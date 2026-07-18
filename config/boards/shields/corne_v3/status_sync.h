/*
 * Copyright (c) 2026 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

struct corne_status_sync {
	uint8_t layer_index;
	uint8_t transport; /* enum zmk_transport */
	uint8_t ble_profile; /* 0-based */
	bool profile_connected;
	bool profile_bonded;
	bool central_active;
	bool valid;
};

const struct corne_status_sync *corne_status_sync_get(void);

typedef void (*corne_status_sync_changed_cb_t)(void);
void corne_status_sync_set_changed_cb(corne_status_sync_changed_cb_t cb);

/* param1 packing helpers (shared by behavior + central relay) */
#define CORNE_SYNC_LAYER(p) ((uint8_t)((p) & 0xff))
#define CORNE_SYNC_TRANSPORT(p) ((uint8_t)(((p) >> 8) & 0xff))
#define CORNE_SYNC_PROFILE(p) ((uint8_t)(((p) >> 16) & 0xff))
#define CORNE_SYNC_FLAGS(p) ((uint8_t)(((p) >> 24) & 0xff))

#define CORNE_SYNC_FLAG_CONN BIT(0)
#define CORNE_SYNC_FLAG_BOND BIT(1)
/* Central half is ACTIVE — peripheral should refresh local idle timer */
#define CORNE_SYNC_FLAG_ACTIVE BIT(2)

#define CORNE_SYNC_PACK(layer, transport, profile, flags)                                          \
	(((uint32_t)(layer)&0xff) | (((uint32_t)(transport)&0xff) << 8) |                          \
	 (((uint32_t)(profile)&0xff) << 16) | (((uint32_t)(flags)&0xff) << 24))
