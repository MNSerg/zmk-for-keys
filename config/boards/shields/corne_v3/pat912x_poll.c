/*
 * Copyright (c) 2026 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * PAT9125EL polling driver with Prusa/PixArt tracking-optimization init.
 * MOTION GPIO ignored (often stuck at 0 V). Blue LED solid = I2C probe OK.
 */

#define DT_DRV_COMPAT corne_pat912x_poll

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(pat912x_poll, CONFIG_INPUT_LOG_LEVEL);

#define PAT9125_PID1            0x00
#define PAT9125_PID2            0x01
#define PAT9125_MOTION          0x02
#define PAT9125_DELTA_XL        0x03
#define PAT9125_DELTA_YL        0x04
#define PAT9125_CONFIG          0x06
#define PAT9125_WP              0x09
#define PAT9125_RES_X           0x0d
#define PAT9125_RES_Y           0x0e
#define PAT9125_DELTA_XYH       0x12
#define PAT9125_ORIENTATION     0x19
#define PAT9125_BANK            0x7f

#define PRODUCT_ID_PAT9125EL    0x3191

struct pat912x_poll_config {
	struct i2c_dt_spec i2c;
	uint16_t poll_interval_ms;
	int32_t axis_x;
	int32_t axis_y;
	uint8_t res_x;
	uint8_t res_y;
	bool invert_x;
	bool invert_y;
	bool ignore_product_id;
};

struct pat912x_poll_data {
	const struct device *dev;
	struct i2c_dt_spec i2c;
	struct k_timer poll_timer;
	struct k_work poll_work;
	bool ready;
};

static int wr(struct pat912x_poll_data *data, uint8_t reg, uint8_t val) {
	return i2c_reg_write_byte_dt(&data->i2c, reg, val);
}

static int rd(struct pat912x_poll_data *data, uint8_t reg, uint8_t *val) {
	return i2c_reg_read_byte_dt(&data->i2c, reg, val);
}

static void led_set(bool on) {
#if DT_NODE_HAS_STATUS(DT_NODELABEL(blue_led), okay)
	const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_NODELABEL(blue_led), gpios);
	if (gpio_is_ready_dt(&led)) {
		gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
		gpio_pin_set_dt(&led, on ? 1 : 0);
	}
#else
	ARG_UNUSED(on);
#endif
}

/* Keep LED steady on after probe — activity blink was for bring-up only */

/* PixArt AN / Prusa bank0 tracking optimization (after reset, already in bank0) */
static const uint8_t init_bank0[] = {
	PAT9125_WP, 0x5a,
	/* RES filled at runtime */
	/* ORIENTATION filled at runtime */
	0x5e, 0x08,
	0x20, 0x64,
	0x2b, 0x6d,
	0x32, 0x2f,
};

static const uint8_t init_bank1[] = {
	0x06, 0x28, 0x33, 0xd0, 0x36, 0xc2, 0x3e, 0x01, 0x3f, 0x15, 0x41, 0x32, 0x42, 0x3b,
	0x43, 0xf2, 0x44, 0x3b, 0x45, 0xf2, 0x46, 0x22, 0x47, 0x3b, 0x48, 0xf2, 0x49, 0x3b,
	0x4a, 0xf0, 0x58, 0x98, 0x59, 0x0c, 0x5a, 0x08, 0x5b, 0x0c, 0x5c, 0x08, 0x61, 0x10,
	0x67, 0x9b, 0x6e, 0x22, 0x71, 0x07, 0x72, 0x08,
};

static int wr_seq(struct pat912x_poll_data *data, const uint8_t *seq, size_t len) {
	for (size_t i = 0; i + 1 < len; i += 2) {
		int ret = wr(data, seq[i], seq[i + 1]);
		if (ret < 0) {
			return ret;
		}
		k_msleep(1);
	}
	return 0;
}

static int pat912x_full_init(struct pat912x_poll_data *data) {
	const struct pat912x_poll_config *cfg = data->dev->config;
	uint8_t id[2];
	int ret;

	/* Bank 0 */
	(void)wr(data, PAT9125_BANK, 0x00);

	ret = i2c_burst_read_dt(&data->i2c, PAT9125_PID1, id, sizeof(id));
	if (ret < 0) {
		return ret;
	}

	uint16_t pid = sys_get_be16(id);
	if (pid != PRODUCT_ID_PAT9125EL) {
		LOG_WRN("product id %04x @0x%02x", pid, data->i2c.addr);
		if (!cfg->ignore_product_id) {
			return -ENOTSUP;
		}
	} else {
		LOG_INF("PAT9125EL @0x%02x", data->i2c.addr);
	}

	/* Software reset */
	(void)wr(data, PAT9125_CONFIG, 0x97);
	k_msleep(2);

	(void)wr(data, PAT9125_BANK, 0x00);
	(void)wr(data, PAT9125_WP, 0x5a);
	k_msleep(1);

	ret = wr(data, PAT9125_RES_X, cfg->res_x);
	if (ret < 0) {
		return ret;
	}
	ret = wr(data, PAT9125_RES_Y, cfg->res_y);
	if (ret < 0) {
		return ret;
	}
	/* Bit2 = 12-bit delta format (required for XYH nibble merge) */
	ret = wr(data, PAT9125_ORIENTATION, 0x04);
	if (ret < 0) {
		return ret;
	}

	ret = wr_seq(data, init_bank0, sizeof(init_bank0));
	if (ret < 0) {
		return ret;
	}
	k_msleep(10);

	(void)wr(data, PAT9125_BANK, 0x01);
	ret = wr_seq(data, init_bank1, sizeof(init_bank1));
	if (ret < 0) {
		return ret;
	}

	(void)wr(data, PAT9125_BANK, 0x00);
	(void)wr(data, PAT9125_WP, 0x00);

	/* Clear any pending motion */
	uint8_t discard;
	(void)rd(data, PAT9125_MOTION, &discard);
	(void)rd(data, PAT9125_DELTA_XL, &discard);
	(void)rd(data, PAT9125_DELTA_YL, &discard);
	(void)rd(data, PAT9125_DELTA_XYH, &discard);

	return 0;
}

static int pat912x_probe_address(struct pat912x_poll_data *data, uint16_t addr) {
	data->i2c.addr = addr;
	return pat912x_full_init(data);
}

static void report_xy(struct pat912x_poll_data *data, int32_t x, int32_t y) {
	const struct pat912x_poll_config *cfg = data->dev->config;

	if (cfg->invert_x) {
		x = -x;
	}
	if (cfg->invert_y) {
		y = -y;
	}
	if (x == 0 && y == 0) {
		return;
	}

	if (cfg->axis_x >= 0) {
		bool sync = cfg->axis_y < 0;
		input_report_rel(data->dev, cfg->axis_x, x, sync, K_MSEC(20));
	}
	if (cfg->axis_y >= 0) {
		input_report_rel(data->dev, cfg->axis_y, y, true, K_MSEC(20));
	}
}

static void pat912x_poll_work_handler(struct k_work *work) {
	struct pat912x_poll_data *data = CONTAINER_OF(work, struct pat912x_poll_data, poll_work);
	uint8_t motion = 0;
	uint8_t xl = 0, yl = 0, xyh = 0;
	int32_t x, y;
	int ret;

	if (!data->ready) {
		return;
	}

	ret = rd(data, PAT9125_MOTION, &motion);
	if (ret < 0) {
		return;
	}
	ARG_UNUSED(motion);

	/* Always sample deltas (MOTION pad may be stuck; register bit may lag). */
	ret = rd(data, PAT9125_DELTA_XL, &xl);
	if (ret < 0) {
		return;
	}
	ret = rd(data, PAT9125_DELTA_YL, &yl);
	if (ret < 0) {
		return;
	}
	ret = rd(data, PAT9125_DELTA_XYH, &xyh);
	if (ret < 0) {
		return;
	}

	x = xl | ((xyh << 4) & 0xf00);
	y = yl | ((xyh << 8) & 0xf00);
	if (x & 0x800) {
		x -= 4096;
	}
	if (y & 0x800) {
		y -= 4096;
	}

	report_xy(data, x, y);
}

static void pat912x_poll_timer_handler(struct k_timer *timer) {
	struct pat912x_poll_data *data = CONTAINER_OF(timer, struct pat912x_poll_data, poll_timer);
	k_work_submit(&data->poll_work);
}

static int pat912x_poll_init(const struct device *dev) {
	const struct pat912x_poll_config *cfg = dev->config;
	struct pat912x_poll_data *data = dev->data;
	static const uint16_t alt_addrs[] = {0x75, 0x73, 0x79};
	bool found = false;

	data->dev = dev;
	data->i2c = cfg->i2c;
	data->ready = false;

	if (!i2c_is_ready_dt(&cfg->i2c)) {
		led_set(false);
		return -ENODEV;
	}

	uint16_t try_order[4];
	size_t n = 0;
	try_order[n++] = cfg->i2c.addr;
	for (size_t i = 0; i < ARRAY_SIZE(alt_addrs); i++) {
		bool dup = false;
		for (size_t j = 0; j < n; j++) {
			if (try_order[j] == alt_addrs[i]) {
				dup = true;
				break;
			}
		}
		if (!dup) {
			try_order[n++] = alt_addrs[i];
		}
	}

	for (size_t i = 0; i < n; i++) {
		if (pat912x_probe_address(data, try_order[i]) == 0) {
			found = true;
			break;
		}
	}

	if (!found) {
		led_set(false);
		LOG_ERR("PAT9125 init failed on all addresses");
		return -ENODEV;
	}

	k_work_init(&data->poll_work, pat912x_poll_work_handler);
	k_timer_init(&data->poll_timer, pat912x_poll_timer_handler, NULL);
	data->ready = true;
	led_set(true);
	k_timer_start(&data->poll_timer, K_MSEC(cfg->poll_interval_ms),
		      K_MSEC(cfg->poll_interval_ms));

	LOG_INF("PAT9125 ready @0x%02x poll=%ums", data->i2c.addr, cfg->poll_interval_ms);
	return 0;
}

#define PAT912X_POLL_INIT(n)                                                                       \
	static const struct pat912x_poll_config pat912x_poll_cfg_##n = {                           \
		.i2c = I2C_DT_SPEC_INST_GET(n),                                                    \
		.poll_interval_ms = DT_INST_PROP(n, poll_interval_ms),                             \
		.axis_x = DT_INST_PROP_OR(n, zephyr_axis_x, -1),                                   \
		.axis_y = DT_INST_PROP_OR(n, zephyr_axis_y, -1),                                   \
		.res_x = DT_INST_PROP(n, res_x),                                                   \
		.res_y = DT_INST_PROP(n, res_y),                                                   \
		.invert_x = DT_INST_PROP(n, invert_x),                                             \
		.invert_y = DT_INST_PROP(n, invert_y),                                             \
		.ignore_product_id = DT_INST_PROP(n, ignore_product_id),                           \
	};                                                                                         \
	static struct pat912x_poll_data pat912x_poll_data_##n;                                     \
	DEVICE_DT_INST_DEFINE(n, pat912x_poll_init, NULL, &pat912x_poll_data_##n,                  \
			      &pat912x_poll_cfg_##n, POST_KERNEL, CONFIG_INPUT_INIT_PRIORITY,      \
			      NULL);

DT_INST_FOREACH_STATUS_OKAY(PAT912X_POLL_INIT)
