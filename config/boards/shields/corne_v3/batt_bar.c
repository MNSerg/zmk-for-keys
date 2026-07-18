/*
 * Copyright (c) 2026 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * Show local battery % as a green bar on the middle per-key LED row.
 * Uses led_strip_blank override so underglow cannot overwrite the bar.
 *
 * LED indices (0-based, per half) from foostan/QMK Corne chain — middle
 * row SW7–SW12 left→right: 25, 22, 19, 16, 11, 8 (not contiguous).
 */

#define DT_DRV_COMPAT corne_behavior_batt_bar

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#if DT_HAS_CHOSEN(zmk_underglow) && DT_NODE_HAS_STATUS(DT_CHOSEN(zmk_underglow), okay)

#include <zephyr/drivers/led_strip.h>
#include <zmk/battery.h>

#if IS_ENABLED(CONFIG_ZMK_RGB_UNDERGLOW)
#include <zmk/rgb_underglow.h>
#endif

#include "led_strip_blank.h"

#define STRIP_CHOSEN DT_CHOSEN(zmk_underglow)
#define STRIP_LEN DT_PROP(STRIP_CHOSEN, chain_length)
#define REFRESH_MS 40

/* Corne middle row, outer→inner (SW7…SW12 / mirrored right). */
static const uint8_t middle_row_leds[] = {25, 22, 19, 16, 11, 8};

struct batt_bar_config {
	uint16_t hold_ms;
};

struct batt_bar_data {
	struct k_work_delayable paint_work;
	int64_t deadline_ms;
	bool active;
};

static struct led_rgb pixels[STRIP_LEN];
static struct batt_bar_data batt_bar_data;
static const struct batt_bar_config *active_cfg;

static uint8_t lit_from_soc(uint8_t soc, uint8_t count) {
	if (count == 0 || soc == 0) {
		return 0;
	}
	if (soc >= 100) {
		return count;
	}

	/* Floor buckets: 49% of 6 → 2 LEDs. */
	uint8_t lit = (uint8_t)((soc * count) / 100);
	return lit == 0 ? 1 : lit;
}

static void paint_bar(void) {
	uint8_t soc = zmk_battery_state_of_charge();
	uint8_t count = ARRAY_SIZE(middle_row_leds);
	uint8_t lit = lit_from_soc(soc, count);

	for (uint8_t i = 0; i < STRIP_LEN; i++) {
		pixels[i] = (struct led_rgb){0};
	}

	for (uint8_t i = 0; i < count; i++) {
		uint8_t idx = middle_row_leds[i];
		if (idx >= STRIP_LEN) {
			continue;
		}
		if (i < lit) {
			pixels[idx] = (struct led_rgb){.r = 0, .g = 90, .b = 0};
		}
	}

	int err = corne_led_strip_set_override(pixels, STRIP_LEN);
	if (err < 0) {
		LOG_ERR("batt_bar: override failed (%d)", err);
	} else {
		LOG_DBG("batt_bar: soc=%u lit=%u/%u", soc, lit, count);
	}
}

static void end_bar(void) {
	corne_led_strip_clear_override();

#if IS_ENABLED(CONFIG_ZMK_RGB_UNDERGLOW)
	{
		bool on = false;
		if (zmk_rgb_underglow_get_state(&on) == 0 && !on) {
			for (uint8_t i = 0; i < STRIP_LEN; i++) {
				pixels[i] = (struct led_rgb){0};
			}
			const struct device *strip = DEVICE_DT_GET(STRIP_CHOSEN);
			if (device_is_ready(strip)) {
				(void)led_strip_update_rgb(strip, pixels, STRIP_LEN);
			}
		}
	}
#else
	{
		const struct device *strip = DEVICE_DT_GET(STRIP_CHOSEN);
		for (uint8_t i = 0; i < STRIP_LEN; i++) {
			pixels[i] = (struct led_rgb){0};
		}
		if (device_is_ready(strip)) {
			(void)led_strip_update_rgb(strip, pixels, STRIP_LEN);
		}
	}
#endif
}

static void paint_work_handler(struct k_work *work) {
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct batt_bar_data *data = CONTAINER_OF(dwork, struct batt_bar_data, paint_work);

	if (!data->active || active_cfg == NULL) {
		return;
	}

	if (k_uptime_get() >= data->deadline_ms) {
		data->active = false;
		active_cfg = NULL;
		end_bar();
		return;
	}

	paint_bar();
	k_work_schedule(&data->paint_work, K_MSEC(REFRESH_MS));
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
				     struct zmk_behavior_binding_event event) {
	const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
	const struct batt_bar_config *cfg = dev->config;
	struct batt_bar_data *data = dev->data;

	ARG_UNUSED(event);

	active_cfg = cfg;
	data->active = true;
	data->deadline_ms = k_uptime_get() + cfg->hold_ms;
	paint_bar();
	k_work_schedule(&data->paint_work, K_MSEC(REFRESH_MS));

	return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
				      struct zmk_behavior_binding_event event) {
	ARG_UNUSED(binding);
	ARG_UNUSED(event);
	return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_batt_bar_driver_api = {
	.binding_pressed = on_keymap_binding_pressed,
	.binding_released = on_keymap_binding_released,
	.locality = BEHAVIOR_LOCALITY_EVENT_SOURCE,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
	.get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

static int behavior_batt_bar_init(const struct device *dev) {
	struct batt_bar_data *data = dev->data;

	k_work_init_delayable(&data->paint_work, paint_work_handler);
	data->active = false;
	return 0;
}

#define BATT_BAR_INST(n)                                                                           \
	static const struct batt_bar_config batt_bar_cfg_##n = {                                   \
		.hold_ms = DT_INST_PROP(n, hold_ms),                                               \
	};                                                                                         \
	BEHAVIOR_DT_INST_DEFINE(n, behavior_batt_bar_init, NULL, &batt_bar_data,                   \
				&batt_bar_cfg_##n, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, \
				&behavior_batt_bar_driver_api);

DT_INST_FOREACH_STATUS_OKAY(BATT_BAR_INST)

#else /* no usable underglow strip — register a no-op so the keymap still builds */

static int on_keymap_binding_pressed_noop(struct zmk_behavior_binding *binding,
					  struct zmk_behavior_binding_event event) {
	ARG_UNUSED(binding);
	ARG_UNUSED(event);
	return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released_noop(struct zmk_behavior_binding *binding,
					   struct zmk_behavior_binding_event event) {
	ARG_UNUSED(binding);
	ARG_UNUSED(event);
	return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_batt_bar_noop_api = {
	.binding_pressed = on_keymap_binding_pressed_noop,
	.binding_released = on_keymap_binding_released_noop,
	.locality = BEHAVIOR_LOCALITY_EVENT_SOURCE,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
	.get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

#define BATT_BAR_NOOP_INST(n)                                                                      \
	BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                            \
				CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_batt_bar_noop_api);

DT_INST_FOREACH_STATUS_OKAY(BATT_BAR_NOOP_INST)

#endif /* strip okay */

#endif /* DT_HAS_COMPAT_STATUS_OKAY */
