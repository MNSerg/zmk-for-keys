/*
 * Copyright (c) 2026 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * PAT9125EL polling input driver.
 * - Does NOT use MOTION GPIO (many modules hold MOTION at 0 V permanently)
 * - Always reads delta registers (does not require MOTION_STATUS bit)
 * - Probes I2C 0x75 / 0x73 / 0x79 (ID_SEL Low / High / NC)
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

#define PAT912X_PRODUCT_ID1   0x00
#define PAT912X_DELTA_X_LO    0x03
#define PAT912X_DELTA_Y_LO    0x04
#define PAT912X_CONFIGURATION 0x06
#define PAT912X_WRITE_PROTECT 0x09
#define PAT912X_RES_X         0x0d
#define PAT912X_RES_Y         0x0e
#define PAT912X_DELTA_XY_HI   0x12

#define PRODUCT_ID_PAT9125EL  0x3191
#define CONFIGURATION_RESET   0x97
#define CONFIGURATION_CLEAR   0x17
#define WRITE_PROTECT_DISABLE 0x5a
#define RES_SCALING_FACTOR    5
#define PAT912X_DATA_SIZE_BITS 12

struct pat912x_poll_config {
	struct i2c_dt_spec i2c;
	uint16_t poll_interval_ms;
	int32_t axis_x;
	int32_t axis_y;
	int16_t res_x_cpi;
	int16_t res_y_cpi;
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

static int pat912x_write(struct pat912x_poll_data *data, uint8_t reg, uint8_t val) {
	return i2c_reg_write_byte_dt(&data->i2c, reg, val);
}

static int pat912x_burst_read(struct pat912x_poll_data *data, uint8_t reg, uint8_t *buf,
			      size_t len) {
	return i2c_burst_read_dt(&data->i2c, reg, buf, len);
}

static void pat912x_status_led(bool on) {
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

static int pat912x_configure_chip(struct pat912x_poll_data *data) {
	const struct pat912x_poll_config *cfg = data->dev->config;
	uint8_t id[2];
	int ret;

	ret = pat912x_burst_read(data, PAT912X_PRODUCT_ID1, id, sizeof(id));
	if (ret < 0) {
		return ret;
	}

	uint16_t pid = sys_get_be16(id);
	if (pid != PRODUCT_ID_PAT9125EL) {
		LOG_WRN("product id %04x (expected %04x) @0x%02x", pid, PRODUCT_ID_PAT9125EL,
			data->i2c.addr);
		if (!cfg->ignore_product_id) {
			return -ENOTSUP;
		}
	} else {
		LOG_INF("PAT9125EL ok @0x%02x", data->i2c.addr);
	}

	(void)pat912x_write(data, PAT912X_CONFIGURATION, CONFIGURATION_RESET);
	k_msleep(2);

	ret = pat912x_write(data, PAT912X_CONFIGURATION, CONFIGURATION_CLEAR);
	if (ret < 0) {
		return ret;
	}

	(void)pat912x_write(data, PAT912X_WRITE_PROTECT, WRITE_PROTECT_DISABLE);

	if (cfg->res_x_cpi >= 0) {
		ret = pat912x_write(data, PAT912X_RES_X, cfg->res_x_cpi / RES_SCALING_FACTOR);
		if (ret < 0) {
			return ret;
		}
	}
	if (cfg->res_y_cpi >= 0) {
		ret = pat912x_write(data, PAT912X_RES_Y, cfg->res_y_cpi / RES_SCALING_FACTOR);
		if (ret < 0) {
			return ret;
		}
	}

	return 0;
}

static int pat912x_probe_address(struct pat912x_poll_data *data, uint16_t addr) {
	data->i2c.addr = addr;
	return pat912x_configure_chip(data);
}

static void pat912x_poll_work_handler(struct k_work *work) {
	struct pat912x_poll_data *data = CONTAINER_OF(work, struct pat912x_poll_data, poll_work);
	const struct pat912x_poll_config *cfg = data->dev->config;
	uint8_t xy[2];
	uint8_t hi;
	int32_t x, y;
	int ret;

	if (!data->ready) {
		return;
	}

	/*
	 * Always read deltas. MOTION pin / MOTION_STATUS are unreliable on some
	 * modules (MOTION stuck at 0 V); deltas still update when the ball moves.
	 */
	ret = pat912x_burst_read(data, PAT912X_DELTA_X_LO, xy, sizeof(xy));
	if (ret < 0) {
		return;
	}
	ret = i2c_reg_read_byte_dt(&data->i2c, PAT912X_DELTA_XY_HI, &hi);
	if (ret < 0) {
		return;
	}

	x = xy[0];
	y = xy[1];
	y |= (hi << 8) & 0xf00;
	x |= (hi << 4) & 0xf00;
	x = sign_extend(x, PAT912X_DATA_SIZE_BITS - 1);
	y = sign_extend(y, PAT912X_DATA_SIZE_BITS - 1);

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
		input_report_rel(data->dev, cfg->axis_x, x, sync, K_NO_WAIT);
	}
	if (cfg->axis_y >= 0) {
		input_report_rel(data->dev, cfg->axis_y, y, true, K_NO_WAIT);
	}
}

static void pat912x_poll_timer_handler(struct k_timer *timer) {
	struct pat912x_poll_data *data = CONTAINER_OF(timer, struct pat912x_poll_data, poll_timer);
	k_work_submit(&data->poll_work);
}

static int pat912x_poll_init(const struct device *dev) {
	const struct pat912x_poll_config *cfg = dev->config;
	struct pat912x_poll_data *data = dev->data;
	static const uint16_t alt_addrs[] = {0x75, 0x73, 0x79};
	int ret;
	bool found = false;

	data->dev = dev;
	data->i2c = cfg->i2c;
	data->ready = false;

	if (!i2c_is_ready_dt(&cfg->i2c)) {
		LOG_ERR("I2C bus not ready");
		pat912x_status_led(false);
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
		ret = pat912x_probe_address(data, try_order[i]);
		if (ret == 0) {
			found = true;
			break;
		}
		LOG_DBG("probe 0x%02x failed (%d)", try_order[i], ret);
	}

	if (!found) {
		LOG_ERR("PAT912x not found on I2C (0x75/0x73/0x79) — check 3.3V VCC, SDA/SCL, GND");
		/* LED off = probe failed */
		pat912x_status_led(false);
		return -ENODEV;
	}

	k_work_init(&data->poll_work, pat912x_poll_work_handler);
	k_timer_init(&data->poll_timer, pat912x_poll_timer_handler, NULL);

	data->ready = true;
	k_timer_start(&data->poll_timer, K_MSEC(cfg->poll_interval_ms),
		      K_MSEC(cfg->poll_interval_ms));

	/* LED on = sensor probed OK */
	pat912x_status_led(true);
	LOG_INF("polling every %u ms @0x%02x (no MOTION GPIO)", cfg->poll_interval_ms,
		data->i2c.addr);
	return 0;
}

#define PAT912X_POLL_INIT(n)                                                                       \
	static const struct pat912x_poll_config pat912x_poll_cfg_##n = {                           \
		.i2c = I2C_DT_SPEC_INST_GET(n),                                                    \
		.poll_interval_ms = DT_INST_PROP(n, poll_interval_ms),                             \
		.axis_x = DT_INST_PROP_OR(n, zephyr_axis_x, -1),                                   \
		.axis_y = DT_INST_PROP_OR(n, zephyr_axis_y, -1),                                   \
		.res_x_cpi = DT_INST_PROP(n, res_x_cpi),                                           \
		.res_y_cpi = DT_INST_PROP(n, res_y_cpi),                                           \
		.invert_x = DT_INST_PROP(n, invert_x),                                             \
		.invert_y = DT_INST_PROP(n, invert_y),                                             \
		.ignore_product_id = DT_INST_PROP(n, ignore_product_id),                           \
	};                                                                                         \
	static struct pat912x_poll_data pat912x_poll_data_##n;                                     \
	DEVICE_DT_INST_DEFINE(n, pat912x_poll_init, NULL, &pat912x_poll_data_##n,                  \
			      &pat912x_poll_cfg_##n, POST_KERNEL, CONFIG_INPUT_INIT_PRIORITY,      \
			      NULL);

DT_INST_FOREACH_STATUS_OKAY(PAT912X_POLL_INIT)
