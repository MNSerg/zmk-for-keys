/*
 * Copyright (c) 2026 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * Proxy for zmk,underglow: keep the first blank-count LEDs (board underglow)
 * always off; pass the rest through to the real WS2812 strip.
 * Optional override lets batt_bar own the strip without underglow racing.
 */

#define DT_DRV_COMPAT corne_led_strip_blank_prefix

#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <string.h>

#include "led_strip_blank.h"

LOG_MODULE_REGISTER(led_blank, CONFIG_LED_STRIP_LOG_LEVEL);

struct blank_config {
	const struct device *target;
	size_t chain_length;
	size_t blank_count;
};

struct blank_data {
	struct led_rgb override[64];
	size_t override_len;
	bool override_active;
};

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
static struct blank_data *blank_data_ptr;
static const struct blank_config *blank_cfg_ptr;
#endif

int corne_led_strip_set_override(const struct led_rgb *pixels, size_t num_pixels) {
#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
	if (blank_data_ptr == NULL || blank_cfg_ptr == NULL || blank_cfg_ptr->target == NULL) {
		return -ENODEV;
	}
	if (num_pixels > blank_cfg_ptr->chain_length) {
		num_pixels = blank_cfg_ptr->chain_length;
	}
	if (num_pixels > ARRAY_SIZE(blank_data_ptr->override)) {
		num_pixels = ARRAY_SIZE(blank_data_ptr->override);
	}

	memcpy(blank_data_ptr->override, pixels, num_pixels * sizeof(struct led_rgb));
	blank_data_ptr->override_len = num_pixels;
	blank_data_ptr->override_active = true;

	return led_strip_update_rgb(blank_cfg_ptr->target, blank_data_ptr->override, num_pixels);
#else
	ARG_UNUSED(pixels);
	ARG_UNUSED(num_pixels);
	return -ENODEV;
#endif
}

void corne_led_strip_clear_override(void) {
#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
	if (blank_data_ptr != NULL) {
		blank_data_ptr->override_active = false;
		blank_data_ptr->override_len = 0;
	}
#endif
}

bool corne_led_strip_override_active(void) {
#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
	return blank_data_ptr != NULL && blank_data_ptr->override_active;
#else
	return false;
#endif
}

static int blank_update_rgb(const struct device *dev, struct led_rgb *pixels, size_t num_pixels) {
	const struct blank_config *cfg = dev->config;
	struct blank_data *data = dev->data;
	size_t n = num_pixels;
	size_t blank;

	if (cfg->target == NULL || !device_is_ready(cfg->target)) {
		return -ENODEV;
	}

	if (data->override_active) {
		return led_strip_update_rgb(cfg->target, data->override, data->override_len);
	}

	if (n > cfg->chain_length) {
		n = cfg->chain_length;
	}

	blank = cfg->blank_count;
	if (blank > n) {
		blank = n;
	}

	for (size_t i = 0; i < blank; i++) {
		pixels[i] = (struct led_rgb){0};
	}

	return led_strip_update_rgb(cfg->target, pixels, n);
}

static int blank_update_channels(const struct device *dev, uint8_t *channels,
				 size_t num_channels) {
	ARG_UNUSED(dev);
	ARG_UNUSED(channels);
	ARG_UNUSED(num_channels);
	return -ENOTSUP;
}

static const struct led_strip_driver_api blank_api = {
	.update_rgb = blank_update_rgb,
	.update_channels = blank_update_channels,
};

static int blank_init(const struct device *dev) {
	const struct blank_config *cfg = dev->config;
	struct blank_data *data = dev->data;

	data->override_active = false;
	data->override_len = 0;
#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
	blank_data_ptr = data;
	blank_cfg_ptr = cfg;
#endif

	if (cfg->target == NULL) {
		LOG_ERR("blank-prefix: missing target strip");
		return -ENODEV;
	}

	if (!device_is_ready(cfg->target)) {
		LOG_ERR("blank-prefix: target %s not ready", cfg->target->name);
		return -ENODEV;
	}

	LOG_INF("blank-prefix: blank=%u of %u via %s", (unsigned)cfg->blank_count,
		(unsigned)cfg->chain_length, cfg->target->name);
	return 0;
}

#define BLANK_INST(n)                                                                              \
	static const struct blank_config blank_cfg_##n = {                                         \
		.target = DEVICE_DT_GET(DT_INST_PHANDLE(n, target)),                               \
		.chain_length = DT_INST_PROP(n, chain_length),                                     \
		.blank_count = DT_INST_PROP(n, blank_count),                                       \
	};                                                                                         \
	static struct blank_data blank_data_##n;                                                   \
	DEVICE_DT_INST_DEFINE(n, blank_init, NULL, &blank_data_##n, &blank_cfg_##n, POST_KERNEL,   \
			      CONFIG_APPLICATION_INIT_PRIORITY, &blank_api);

DT_INST_FOREACH_STATUS_OKAY(BLANK_INST)
