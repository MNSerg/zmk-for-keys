/*
 * Copyright (c) 2020 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * Custom status screen = ZMK built-in layout + right-half link (R:OK / R:--).
 *
 * Why the earlier custom screen bricked boot:
 * CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM does NOT select the LVGL fonts,
 * theme, or LV_Z_MEM_POOL_SIZE=4096 that BUILT_IN enables. Creating labels
 * without those settings crashes LVGL → OLED noise, USB/RGB dead.
 * See corne_v3_left.conf for the required LVGL Kconfig mirror.
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
static bool split_connected_cached;
static struct k_work_delayable split_poll_work;
static struct k_work split_ui_work;

static bool right_half_connected(void) {
    STRUCT_SECTION_FOREACH(zmk_split_transport_central, t) {
        if (!t->api || !t->api->get_status) {
            continue;
        }

        struct zmk_split_transport_status st = t->api->get_status();
        if (st.connections != ZMK_SPLIT_TRANSPORT_CONNECTIONS_STATUS_DISCONNECTED) {
            return true;
        }
    }

    return false;
}

static void split_ui_work_cb(struct k_work *work) {
    ARG_UNUSED(work);

    if (split_label == NULL) {
        return;
    }

    lv_label_set_text(split_label, split_connected_cached ? "R:OK" : "R:--");
}

static void split_poll_work_cb(struct k_work *work) {
    ARG_UNUSED(work);

    split_connected_cached = right_half_connected();

    if (zmk_display_is_initialized()) {
        k_work_submit_to_queue(zmk_display_work_q(), &split_ui_work);
    }

    k_work_schedule(&split_poll_work, K_MSEC(1000));
}

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);

#if IS_ENABLED(CONFIG_ZMK_WIDGET_BATTERY_STATUS)
    zmk_widget_battery_status_init(&battery_status_widget, screen);
    lv_obj_align(zmk_widget_battery_status_obj(&battery_status_widget), LV_ALIGN_TOP_RIGHT, 0, 0);
#endif

#if IS_ENABLED(CONFIG_ZMK_WIDGET_OUTPUT_STATUS)
    zmk_widget_output_status_init(&output_status_widget, screen);
    lv_obj_align(zmk_widget_output_status_obj(&output_status_widget), LV_ALIGN_TOP_LEFT, 0, 0);
#endif

    /* Bottom-left: right-half link (built-in puts layer here; layer moved right). */
    split_label = lv_label_create(screen);
    lv_label_set_text(split_label, "R:--");
    lv_obj_align(split_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

#if IS_ENABLED(CONFIG_ZMK_WIDGET_LAYER_STATUS)
    zmk_widget_layer_status_init(&layer_status_widget, screen);
    lv_obj_set_style_text_font(zmk_widget_layer_status_obj(&layer_status_widget),
                               lv_theme_get_font_small(screen), LV_PART_MAIN);
    lv_obj_align(zmk_widget_layer_status_obj(&layer_status_widget), LV_ALIGN_BOTTOM_RIGHT, 0, 0);
#endif

    k_work_init(&split_ui_work, split_ui_work_cb);
    k_work_init_delayable(&split_poll_work, split_poll_work_cb);
    /* Defer first poll until split BT stack is up */
    k_work_schedule(&split_poll_work, K_MSEC(2500));

    return screen;
}
