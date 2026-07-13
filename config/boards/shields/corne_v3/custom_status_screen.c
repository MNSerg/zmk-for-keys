/*
 * Copyright (c) 2020 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * Built-in ZMK widgets use LV_SYMBOL_* (FontAwesome PUA). On this mono
 * Montserrat setup those glyphs become white boxes; ASCII text works.
 * Layer widget keeps LV_SYMBOL_KEYBOARD (that glyph is present).
 */

#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/iterable_sections.h>

#include <lvgl.h>

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/display/status_screen.h>
#include <zmk/display/widgets/layer_status.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/split/transport/central.h>
#include <zmk/split/transport/types.h>
#include <zmk/usb.h>

#if IS_ENABLED(CONFIG_ZMK_WIDGET_LAYER_STATUS)
static struct zmk_widget_layer_status layer_status_widget;
#endif

static lv_obj_t *output_label;
static lv_obj_t *battery_label;
static lv_obj_t *split_label;

static bool split_connected_cached;
static struct k_work_delayable split_poll_work;
static struct k_work split_ui_work;

/* ---- output (USB / BTn) ---- */

struct output_ui_state {
    struct zmk_endpoint_instance selected_endpoint;
    enum zmk_transport preferred_transport;
    bool active_profile_connected;
    bool active_profile_bonded;
};

static void set_output_text(lv_obj_t *label, struct output_ui_state state) {
    char text[16] = {};

    enum zmk_transport transport = state.selected_endpoint.transport;
    bool connected = transport != ZMK_TRANSPORT_NONE;

    if (!connected) {
        transport = state.preferred_transport;
    }

    switch (transport) {
    case ZMK_TRANSPORT_USB:
        snprintf(text, sizeof(text), connected ? "USB" : "USB?");
        break;
    case ZMK_TRANSPORT_BLE: {
        int idx = state.selected_endpoint.ble.profile_index + 1;
        if (state.active_profile_bonded) {
            snprintf(text, sizeof(text), state.active_profile_connected ? "BT%d" : "BT%d?", idx);
        } else {
            snprintf(text, sizeof(text), "BT%d*", idx);
        }
        break;
    }
    default:
        snprintf(text, sizeof(text), "--");
        break;
    }

    lv_label_set_text(label, text);
}

static void output_ui_update_cb(struct output_ui_state state) {
    if (output_label) {
        set_output_text(output_label, state);
    }
}

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

/* ---- battery percent ---- */

struct battery_ui_state {
    uint8_t level;
    bool usb_present;
};

static void set_battery_text(lv_obj_t *label, struct battery_ui_state state) {
    char text[12] = {};
    if (state.usb_present) {
        snprintf(text, sizeof(text), "CHG %u%%", state.level);
    } else {
        snprintf(text, sizeof(text), "%u%%", state.level);
    }
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
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#else
        .usb_present = false,
#endif
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_corne_battery_ui, struct battery_ui_state, battery_ui_update_cb,
                            battery_ui_get_state)
ZMK_SUBSCRIPTION(widget_corne_battery_ui, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_corne_battery_ui, zmk_usb_conn_state_changed);
#endif

/* ---- right-half link ---- */

static bool right_half_connected(void) {
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

static void split_ui_work_cb(struct k_work *work) {
    ARG_UNUSED(work);
    if (split_label != NULL) {
        lv_label_set_text(split_label, split_connected_cached ? "R:OK" : "R:--");
    }
}

static void split_poll_work_cb(struct k_work *work) {
    ARG_UNUSED(work);
    split_connected_cached = right_half_connected();
    if (zmk_display_is_initialized()) {
        k_work_submit_to_queue(zmk_display_work_q(), &split_ui_work);
    }
    k_work_schedule(&split_poll_work, K_MSEC(1000));
}

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);

    output_label = lv_label_create(screen);
    lv_obj_align(output_label, LV_ALIGN_TOP_LEFT, 0, 0);
    widget_corne_output_ui_init();

    battery_label = lv_label_create(screen);
    lv_obj_align(battery_label, LV_ALIGN_TOP_RIGHT, 0, 0);
    widget_corne_battery_ui_init();

    split_label = lv_label_create(screen);
    lv_label_set_text(split_label, "R:--");
    lv_obj_align(split_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

#if IS_ENABLED(CONFIG_ZMK_WIDGET_LAYER_STATUS)
    zmk_widget_layer_status_init(&layer_status_widget, screen);
    lv_obj_set_style_text_font(zmk_widget_layer_status_obj(&layer_status_widget),
                               lv_theme_get_font_small(screen), LV_PART_MAIN);
    lv_obj_align(zmk_widget_layer_status_obj(&layer_status_widget), LV_ALIGN_BOTTOM_RIGHT, 0, 0);
#endif

    k_work_init(&split_ui_work, split_ui_work_cb);
    k_work_init_delayable(&split_poll_work, split_poll_work_cb);
    k_work_schedule(&split_poll_work, K_MSEC(2500));

    return screen;
}
