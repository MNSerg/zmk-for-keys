/*
 * Copyright (c) 2020 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * Built-in ZMK widgets use LV_SYMBOL_* (FontAwesome PUA). On this mono
 * Montserrat setup those glyphs become white boxes; ASCII text works.
 *
 * Layout (128×32):
 *   top-left  — USB / BT1 / BT1? / BT1*
 *   top-right — NN%
 *   bot-left  — R:OK/R:-- (central) or L:OK/L:-- (peripheral)
 *   bot-right — layer name (DEF / LOW / …)
 */

#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/iterable_sections.h>
#include <zephyr/sys/util.h>

#include <lvgl.h>

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/display/status_screen.h>
#include <zmk/endpoints.h>
#include <zmk/endpoints_types.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/usb.h>

#if IS_ENABLED(CONFIG_ZMK_SPLIT) && IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#include <zmk/split/transport/central.h>
#include <zmk/split/transport/types.h>
#endif

#if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#include <zmk/split/bluetooth/peripheral.h>
#endif

#include "status_sync.h"

static lv_obj_t *output_label;
static lv_obj_t *battery_label;
static lv_obj_t *split_label;
static lv_obj_t *layer_label;

static bool split_connected_cached;
static struct k_work_delayable split_poll_work;
static struct k_work split_ui_work;
static struct k_work layer_ui_work;
#if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
static struct k_work output_ui_work;
#endif

/* ---- output text (USB / BTn) ---- */

static void format_output_text(char *text, size_t len, enum zmk_transport transport, uint8_t profile,
			       bool connected, bool bonded) {
	switch (transport) {
	case ZMK_TRANSPORT_USB:
		snprintf(text, len, connected ? "USB" : "USB?");
		break;
	case ZMK_TRANSPORT_BLE: {
		int idx = profile + 1;
		if (bonded) {
			snprintf(text, len, connected ? "BT%d" : "BT%d?", idx);
		} else {
			snprintf(text, len, "BT%d*", idx);
		}
		break;
	}
	default:
		snprintf(text, len, "--");
		break;
	}
}

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) || !IS_ENABLED(CONFIG_ZMK_SPLIT)

struct output_ui_state {
	struct zmk_endpoint_instance selected_endpoint;
	enum zmk_transport preferred_transport;
	bool active_profile_connected;
	bool active_profile_bonded;
};

static void set_output_from_central(struct output_ui_state state) {
	char text[16] = {};
	enum zmk_transport transport = state.selected_endpoint.transport;
	bool connected = transport != ZMK_TRANSPORT_NONE;
	uint8_t profile = 0;

	if (!connected) {
		transport = state.preferred_transport;
	}
	if (transport == ZMK_TRANSPORT_BLE) {
		profile = (uint8_t)state.selected_endpoint.ble.profile_index;
	}

	bool out_connected = connected;
	bool bonded = true;
	if (transport == ZMK_TRANSPORT_BLE) {
		out_connected = state.active_profile_connected;
		bonded = state.active_profile_bonded;
	} else if (transport == ZMK_TRANSPORT_USB) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
		out_connected = zmk_usb_is_powered();
#else
		out_connected = connected;
#endif
	}

	format_output_text(text, sizeof(text), transport, profile, out_connected, bonded);
	if (output_label) {
		lv_label_set_text(output_label, text);
	}
}

static void output_ui_update_cb(struct output_ui_state state) { set_output_from_central(state); }

static struct output_ui_state output_ui_get_state(const zmk_event_t *_eh) {
	ARG_UNUSED(_eh);
	return (struct output_ui_state){
		.selected_endpoint = zmk_endpoint_get_selected(),
		.preferred_transport = zmk_endpoint_get_preferred_transport(),
		.active_profile_connected = zmk_ble_active_profile_is_connected(),
		.active_profile_bonded = !zmk_ble_active_profile_is_open(),
	};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_corne_output_ui, struct output_ui_state, output_ui_update_cb,
			    output_ui_get_state)
ZMK_SUBSCRIPTION(widget_corne_output_ui, zmk_endpoint_changed);
#if defined(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(widget_corne_output_ui, zmk_ble_active_profile_changed);
#endif
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_corne_output_ui, zmk_usb_conn_state_changed);
#endif

#else /* peripheral: use status sync from central */

static void refresh_output_from_sync(void) {
	const struct corne_status_sync *st = corne_status_sync_get();
	char text[16] = {};

	if (!st->valid) {
		snprintf(text, sizeof(text), "--");
	} else {
		format_output_text(text, sizeof(text), st->transport, st->ble_profile,
				   st->profile_connected, st->profile_bonded);
	}
	if (output_label) {
		lv_label_set_text(output_label, text);
	}
}

static void output_ui_work_cb(struct k_work *work) {
	ARG_UNUSED(work);
	refresh_output_from_sync();
}

#endif

/* ---- battery percent (no CHG prefix) ---- */

struct battery_ui_state {
	uint8_t level;
};

static void set_battery_text(lv_obj_t *label, struct battery_ui_state state) {
	char text[8] = {};
	snprintf(text, sizeof(text), "%u%%", state.level);
	lv_label_set_text(label, text);
}

static void battery_ui_update_cb(struct battery_ui_state state) {
	if (battery_label) {
		set_battery_text(battery_label, state);
	}
}

static struct battery_ui_state battery_ui_get_state(const zmk_event_t *eh) {
	const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);
	return (struct battery_ui_state){
		.level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
	};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_corne_battery_ui, struct battery_ui_state, battery_ui_update_cb,
			    battery_ui_get_state)
ZMK_SUBSCRIPTION(widget_corne_battery_ui, zmk_battery_state_changed);

/* ---- layer name ---- */

#if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
/* Peripheral has no keymap runtime — names come from DT display-name order. */
#define CORNE_LAYER_NAME_ENTRY(node) DT_PROP_OR(node, display_name, ""),
static const char *const corne_layer_names[] = {
	DT_FOREACH_CHILD_STATUS_OKAY(DT_INST(0, zmk_keymap), CORNE_LAYER_NAME_ENTRY)
};
#undef CORNE_LAYER_NAME_ENTRY
#endif

static void refresh_layer_label(void) {
	char text[16] = {};
	uint8_t index;
	const char *name = NULL;

#if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
	const struct corne_status_sync *st = corne_status_sync_get();
	index = st->valid ? st->layer_index : 0;
	if (index < ARRAY_SIZE(corne_layer_names)) {
		name = corne_layer_names[index];
	}
#else
	index = zmk_keymap_highest_layer_active();
	name = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(index));
#endif

	if (name == NULL || name[0] == '\0') {
		snprintf(text, sizeof(text), "L%u", (unsigned)index);
	} else {
		snprintf(text, sizeof(text), "%s", name);
	}

	if (layer_label) {
		lv_label_set_text(layer_label, text);
	}
}

static void layer_ui_work_cb(struct k_work *work) {
	ARG_UNUSED(work);
	refresh_layer_label();
}

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) || !IS_ENABLED(CONFIG_ZMK_SPLIT)
static int layer_event_listener(const zmk_event_t *eh) {
	ARG_UNUSED(eh);
	if (zmk_display_is_initialized()) {
		k_work_submit_to_queue(zmk_display_work_q(), &layer_ui_work);
	}
	return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(corne_layer_ui, layer_event_listener);
ZMK_SUBSCRIPTION(corne_layer_ui, zmk_layer_state_changed);
#endif

/* ---- split link ---- */

#if IS_ENABLED(CONFIG_ZMK_SPLIT) && IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
static bool peer_connected(void) {
	STRUCT_SECTION_FOREACH(zmk_split_transport_central, t) {
		if (!t->api || !t->api->get_status) {
			continue;
		}
		struct zmk_split_transport_status st = t->api->get_status();
		if (st.connections != ZMK_SPLIT_TRANSPORT_CONNECTIONS_STATUS_DISCONNECTED) {
			return true;
		}
	}
	return false;
}
#define SPLIT_OK_TEXT "R:OK"
#define SPLIT_BAD_TEXT "R:--"
#elif IS_ENABLED(CONFIG_ZMK_SPLIT)
static bool peer_connected(void) { return zmk_split_bt_peripheral_is_connected(); }
#define SPLIT_OK_TEXT "L:OK"
#define SPLIT_BAD_TEXT "L:--"
#else
static bool peer_connected(void) { return false; }
#define SPLIT_OK_TEXT "--"
#define SPLIT_BAD_TEXT "--"
#endif

static void split_ui_work_cb(struct k_work *work) {
	ARG_UNUSED(work);
	if (split_label != NULL) {
		lv_label_set_text(split_label, split_connected_cached ? SPLIT_OK_TEXT : SPLIT_BAD_TEXT);
	}
}

static void split_poll_work_cb(struct k_work *work) {
	ARG_UNUSED(work);
	split_connected_cached = peer_connected();
	if (zmk_display_is_initialized()) {
		k_work_submit_to_queue(zmk_display_work_q(), &split_ui_work);
	}
	k_work_schedule(&split_poll_work, K_MSEC(1000));
}

#if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
static void on_status_sync_changed(void) {
	if (!zmk_display_is_initialized()) {
		return;
	}
	k_work_submit_to_queue(zmk_display_work_q(), &layer_ui_work);
	k_work_submit_to_queue(zmk_display_work_q(), &output_ui_work);
}
#endif

lv_obj_t *zmk_display_status_screen(void) {
	lv_obj_t *screen = lv_obj_create(NULL);

	k_work_init(&layer_ui_work, layer_ui_work_cb);
	k_work_init(&split_ui_work, split_ui_work_cb);
#if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
	k_work_init(&output_ui_work, output_ui_work_cb);
	corne_status_sync_set_changed_cb(on_status_sync_changed);
#endif

	output_label = lv_label_create(screen);
	lv_obj_align(output_label, LV_ALIGN_TOP_LEFT, 0, 0);
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) || !IS_ENABLED(CONFIG_ZMK_SPLIT)
	widget_corne_output_ui_init();
#else
	lv_label_set_text(output_label, "--");
	refresh_output_from_sync();
#endif

	battery_label = lv_label_create(screen);
	lv_obj_align(battery_label, LV_ALIGN_TOP_RIGHT, 0, 0);
	widget_corne_battery_ui_init();

	split_label = lv_label_create(screen);
	lv_label_set_text(split_label, SPLIT_BAD_TEXT);
	lv_obj_align(split_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

	layer_label = lv_label_create(screen);
	lv_obj_set_style_text_font(layer_label, lv_theme_get_font_small(screen), LV_PART_MAIN);
	lv_obj_align(layer_label, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
	refresh_layer_label();

	k_work_init_delayable(&split_poll_work, split_poll_work_cb);
	k_work_schedule(&split_poll_work, K_MSEC(2500));

	return screen;
}
