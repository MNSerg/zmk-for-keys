/*
 * Trackball feel — edit this file, then rebuild/flash the LEFT half.
 *
 * Speed = MUL/DIV (integers only). Examples:
 *   8/5 = ×1.6   9/5 = ×1.8   2/1 = ×2   1/1 = ×1   1/6 = ÷6
 *
 * Transform flags (OR together), from <dt-bindings/zmk/input_transform.h>:
 *   INPUT_TRANSFORM_XY_SWAP
 *   INPUT_TRANSFORM_X_INVERT
 *   INPUT_TRANSFORM_Y_INVERT
 * Use 0 for no transform.
 *
 * Mouse processors apply on all layers except those listed under scroll {}.
 * Scroll processors apply on RAI (layer 2) and replace the mouse list.
 */

#pragma once

/* --- Mouse (pointer) --- */
#define CORNE_MOUSE_TRANSFORM                                                                      \
	(INPUT_TRANSFORM_XY_SWAP | INPUT_TRANSFORM_Y_INVERT)
#define CORNE_MOUSE_SCALE_MUL 8
#define CORNE_MOUSE_SCALE_DIV 5 /* ×1.6 */

/* --- Scroll (layer 2 / RAI) --- */
#define CORNE_SCROLL_TRANSFORM 0
#define CORNE_SCROLL_SCALE_MUL 1
#define CORNE_SCROLL_SCALE_DIV 6 /* ÷6 */
