/*
 * Copyright (c) 2026 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * Proxy for zmk,underglow: keep the first blank-count LEDs (board underglow)
 * always off; pass the rest through to the real WS2812 strip.
 */

#define DT_DRV_COMPAT corne_led_strip_blank_prefix

#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(led_blank, CONFIG_LED_STRIP_LOG_LEVEL);

struct blank_config {
	const struct device *target;
	size_t chain_length;
	size_t blank_count;
};

static int blank_update_rgb(const struct device *dev, struct led_rgb *pixels, size_t num_pixels) {
	const struct blank_config *cfg = dev->config;
	size_t n = num_pixels;
	size_t blank;

	if (cfg->target == NULL || !device_is_ready(cfg->target)) {
		return -ENODEV;
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
	/* After the real WS2812 strip (LED_STRIP_INIT_PRIORITY). */                         \
	DEVICE_DT_INST_DEFINE(n, blank_init, NULL, NULL, &blank_cfg_##n, POST_KERNEL,              \
			      CONFIG_APPLICATION_INIT_PRIORITY, &blank_api);

DT_INST_FOREACH_STATUS_OKAY(BLANK_INST)
