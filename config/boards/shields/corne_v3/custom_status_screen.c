/*
 * Copyright (c) 2026 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * Left OLED (128x32): host output, battery, right-half link, active layer.
 * Linked into `app` (see CMakeLists.txt) so this overrides the weak stub.
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/iterable_sections.h>

#include <lvgl.h>

#include <zmk/display.h>
#include <zmk/display/status_screen.h>
#include <zmk/display/widgets/battery_status.h>
#include <zmk/display/widgets/layer_status.h>
#include <zmk/display/widgets/output_status.h>
#include <zmk/split/transport/central.h>
#include <zmk/split/transport/types.h>

#if IS_ENABLED(CONFIG_ZMK_WIDGET_BATTERY_STATUS)
static struct zmk_widget_battery_status battery_status_widget;
#endif

#if IS_ENABLED(CONFIG_ZMK_WIDGET_OUTPUT_STATUS)
static struct zmk_widget_output_status output_status_widget;
#endif

#if IS_ENABLED(CONFIG_ZMK_WIDGET_LAYER_STATUS)
static struct zmk_widget_layer_status layer_status_widget;
#endif

static lv_obj_t *split_label;
static int split_poll_ticks;

static bool right_half_connected(void) {
    STRUCT_SECTION_FOREACH(zmk_split_transport_central, t) {
        if (t == NULL || t->api == NULL || t->api->get_status == NULL) {
            continue;
        }

        struct zmk_split_transport_status st = t->api->get_status();
        if (st.connections != ZMK_SPLIT_TRANSPORT_CONNECTIONS_STATUS_DISCONNECTED) {
            return true;
        }
    }

    return false;
}

static void split_timer_cb(lv_timer_t *timer) {
    ARG_UNUSED(timer);

    if (split_label == NULL) {
        return;
    }

    /* ~2s delay before first real poll (timer period 500ms) */
    if (split_poll_ticks < 4) {
        split_poll_ticks++;
        return;
    }

    lv_label_set_text(split_label, right_half_connected() ? "R:OK" : "R:--");
}

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);

#if IS_ENABLED(CONFIG_ZMK_WIDGET_OUTPUT_STATUS)
    zmk_widget_output_status_init(&output_status_widget, screen);
    lv_obj_align(zmk_widget_output_status_obj(&output_status_widget), LV_ALIGN_TOP_LEFT, 0, 0);
#endif

#if IS_ENABLED(CONFIG_ZMK_WIDGET_BATTERY_STATUS)
    zmk_widget_battery_status_init(&battery_status_widget, screen);
    lv_obj_align(zmk_widget_battery_status_obj(&battery_status_widget), LV_ALIGN_TOP_RIGHT, 0, 0);
#endif

    split_label = lv_label_create(screen);
    lv_label_set_text(split_label, "R:--");
    lv_obj_align(split_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    split_poll_ticks = 0;
    lv_timer_create(split_timer_cb, 500, NULL);

#if IS_ENABLED(CONFIG_ZMK_WIDGET_LAYER_STATUS)
    zmk_widget_layer_status_init(&layer_status_widget, screen);
    lv_obj_set_style_text_font(zmk_widget_layer_status_obj(&layer_status_widget),
                               lv_theme_get_font_small(screen), LV_PART_MAIN);
    lv_obj_align(zmk_widget_layer_status_obj(&layer_status_widget), LV_ALIGN_BOTTOM_RIGHT, 0, 0);
#endif

    return screen;
}
