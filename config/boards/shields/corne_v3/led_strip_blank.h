/*
 * Copyright (c) 2026 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stddef.h>
#include <zephyr/drivers/led_strip.h>

/**
 * While override is active, underglow frames are ignored and this buffer
 * is pushed to the strip on every update (and immediately once).
 */
int corne_led_strip_set_override(const struct led_rgb *pixels, size_t num_pixels);

/** Stop overriding; next underglow tick restores normal lighting. */
void corne_led_strip_clear_override(void);

bool corne_led_strip_override_active(void);
