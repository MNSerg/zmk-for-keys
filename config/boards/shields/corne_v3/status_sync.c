/*
 * Copyright (c) 2026 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * GLOBAL behavior: central packs layer + host endpoint + activity into param1
 * and invokes on peripherals so the right OLED can show DEF/USB/BT1 and the
 * right half stays awake while the left is in use (ZMK otherwise only wakes
 * central from peripheral activity, not vice versa).
 */

#define DT_DRV_COMPAT corne_behavior_status_sync

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <drivers/behavior.h>
#include <zmk/activity.h>
#include <zmk/behavior.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/endpoints_types.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/usb.h>

#if IS_ENABLED(CONFIG_INPUT)
#include <zephyr/input/input.h>
#endif

#include "status_sync.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static struct corne_status_sync sync_state;
static corne_status_sync_changed_cb_t changed_cb;

const struct corne_status_sync *corne_status_sync_get(void) { return &sync_state; }

void corne_status_sync_set_changed_cb(corne_status_sync_changed_cb_t cb) { changed_cb = cb; }

#if IS_ENABLED(CONFIG_INPUT)
/* Virtual input device: refreshes activity idle timer without HID / keymap side effects. */
static int activity_poke_init(const struct device *dev) {
	ARG_UNUSED(dev);
	return 0;
}

DEVICE_DEFINE(corne_activity_poke, "corne_activity_poke", activity_poke_init, NULL, NULL, NULL,
	      POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, NULL);

static void poke_local_activity(void) {
	(void)input_report_rel(DEVICE_GET(corne_activity_poke), INPUT_REL_X, 0, true, K_NO_WAIT);
}
#endif

static void apply_sync_param(uint32_t param1) {
	sync_state.layer_index = CORNE_SYNC_LAYER(param1);
	sync_state.transport = CORNE_SYNC_TRANSPORT(param1);
	sync_state.ble_profile = CORNE_SYNC_PROFILE(param1);
	uint8_t flags = CORNE_SYNC_FLAGS(param1);
	sync_state.profile_connected = (flags & CORNE_SYNC_FLAG_CONN) != 0;
	sync_state.profile_bonded = (flags & CORNE_SYNC_FLAG_BOND) != 0;
	sync_state.central_active = (flags & CORNE_SYNC_FLAG_ACTIVE) != 0;
	sync_state.valid = true;

#if IS_ENABLED(CONFIG_INPUT)
	if (sync_state.central_active) {
		poke_local_activity();
	}
#endif

	if (changed_cb) {
		changed_cb();
	}
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
				     struct zmk_behavior_binding_event event) {
	ARG_UNUSED(event);
	apply_sync_param(binding->param1);
	return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
				      struct zmk_behavior_binding_event event) {
	ARG_UNUSED(binding);
	ARG_UNUSED(event);
	return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_status_sync_driver_api = {
	.binding_pressed = on_keymap_binding_pressed,
	.binding_released = on_keymap_binding_released,
	.locality = BEHAVIOR_LOCALITY_GLOBAL,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
	.get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

#define ST_SYNC_INST(n)                                                                            \
	BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                            \
				CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                               \
				&behavior_status_sync_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ST_SYNC_INST)

#if IS_ENABLED(CONFIG_ZMK_SPLIT) && IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

static uint32_t pack_current_status(void) {
	struct zmk_endpoint_instance ep = zmk_endpoint_get_selected();
	enum zmk_transport transport = ep.transport;
	uint8_t profile = 0;
	uint8_t flags = 0;

	if (transport == ZMK_TRANSPORT_NONE) {
		transport = zmk_endpoint_get_preferred_transport();
	}

	if (transport == ZMK_TRANSPORT_BLE) {
		profile = (uint8_t)ep.ble.profile_index;
#if IS_ENABLED(CONFIG_ZMK_BLE)
		if (zmk_ble_active_profile_is_connected()) {
			flags |= CORNE_SYNC_FLAG_CONN;
		}
		if (!zmk_ble_active_profile_is_open()) {
			flags |= CORNE_SYNC_FLAG_BOND;
		}
#endif
	} else if (transport == ZMK_TRANSPORT_USB) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
		if (zmk_usb_is_powered()) {
			flags |= CORNE_SYNC_FLAG_CONN;
		}
#endif
		flags |= CORNE_SYNC_FLAG_BOND; /* USB has no bond concept */
	}

	if (zmk_activity_get_state() == ZMK_ACTIVITY_ACTIVE) {
		flags |= CORNE_SYNC_FLAG_ACTIVE;
	}

	return CORNE_SYNC_PACK(zmk_keymap_highest_layer_active(), transport, profile, flags);
}

static void invoke_status_sync(void) {
	struct zmk_behavior_binding binding = {
		.behavior_dev = "st_sync",
		.param1 = pack_current_status(),
		.param2 = 0,
	};
	struct zmk_behavior_binding_event event = {
		.layer = 0,
		.position = 0,
		.timestamp = k_uptime_get(),
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
		.source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
#endif
	};

	int err = zmk_behavior_invoke_binding(&binding, event, true);
	if (err) {
		LOG_DBG("st_sync invoke failed: %d", err);
	}
}

static int status_sync_event_listener(const zmk_event_t *eh) {
	ARG_UNUSED(eh);
	invoke_status_sync();
	return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(corne_status_sync_relay, status_sync_event_listener);
ZMK_SUBSCRIPTION(corne_status_sync_relay, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(corne_status_sync_relay, zmk_endpoint_changed);
#if IS_ENABLED(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(corne_status_sync_relay, zmk_ble_active_profile_changed);
#endif
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(corne_status_sync_relay, zmk_usb_conn_state_changed);
#endif
ZMK_SUBSCRIPTION(corne_status_sync_relay, zmk_split_peripheral_status_changed);
ZMK_SUBSCRIPTION(corne_status_sync_relay, zmk_activity_state_changed);

static void status_sync_boot_work(struct k_work *work) {
	ARG_UNUSED(work);
	invoke_status_sync();
}

static struct k_work_delayable status_sync_boot;
static struct k_work_delayable status_sync_periodic;

static void status_sync_periodic_work(struct k_work *work) {
	ARG_UNUSED(work);
	invoke_status_sync();
	/* Faster than IDLE_TIMEOUT/2 so an active central keeps the peripheral awake. */
	k_work_schedule(&status_sync_periodic, K_SECONDS(4));
}

static int status_sync_central_init(void) {
	k_work_init_delayable(&status_sync_boot, status_sync_boot_work);
	k_work_init_delayable(&status_sync_periodic, status_sync_periodic_work);
	k_work_schedule(&status_sync_boot, K_MSEC(3000));
	k_work_schedule(&status_sync_periodic, K_SECONDS(4));
	return 0;
}

SYS_INIT(status_sync_central_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#endif /* central */

#endif /* DT_HAS_COMPAT_STATUS_OKAY */
