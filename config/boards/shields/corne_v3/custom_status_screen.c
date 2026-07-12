/*
 * Copyright (c) 2026 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * Left OLED (128x32): host output, battery, right-half link, active layer index/name.
 */

#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/iterable_sections.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <lvgl.h>

#include <zmk/display.h>
#include <zmk/display/status_screen.h>
#include <zmk/display/widgets/battery_status.h>
#include <zmk/display/widgets/output_status.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/split/transport/central.h>
#include <zmk/split/transport/types.h>

#if IS_ENABLED(CONFIG_ZMK_WIDGET_BATTERY_STATUS)
static struct zmk_widget_battery_status battery_status_widget;
#endif

#if IS_ENABLED(CONFIG_ZMK_WIDGET_OUTPUT_STATUS)
static struct zmk_widget_output_status output_status_widget;
#endif

static lv_obj_t *split_label;
static lv_obj_t *layer_label;

struct layer_ui_state {
    zmk_keymap_layer_index_t index;
    const char *name;
};

static bool right_half_connected(void) {
    bool connected = false;

    STRUCT_SECTION_FOREACH(zmk_split_transport_central, t) {
        if (!t->api || !t->api->get_status) {
            continue;
        }

        struct zmk_split_transport_status st = t->api->get_status();
        if (st.connections != ZMK_SPLIT_TRANSPORT_CONNECTIONS_STATUS_DISCONNECTED) {
            connected = true;
            break;
        }
    }

    return connected;
}

static void refresh_split_label(void) {
    if (!split_label) {
        return;
    }

    lv_label_set_text(split_label, right_half_connected() ? "R:OK" : "R:--");
}

static void split_timer_cb(lv_timer_t *timer) {
    ARG_UNUSED(timer);
    refresh_split_label();
}

static void set_layer_text(lv_obj_t *label, struct layer_ui_state state) {
    char text[24] = {};

    if (state.name != NULL && state.name[0] != '\0') {
        snprintf(text, sizeof(text), "L%u %s", state.index, state.name);
    } else {
        snprintf(text, sizeof(text), "L%u", state.index);
    }

    lv_label_set_text(label, text);
}

static void layer_ui_update_cb(struct layer_ui_state state) {
    if (layer_label) {
        set_layer_text(layer_label, state);
    }
}

static struct layer_ui_state layer_ui_get_state(const zmk_event_t *eh) {
    ARG_UNUSED(eh);
    zmk_keymap_layer_index_t index = zmk_keymap_highest_layer_active();
    return (struct layer_ui_state){
        .index = index,
        .name = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(index)),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_corne_layer_ui, struct layer_ui_state, layer_ui_update_cb,
                            layer_ui_get_state)
ZMK_SUBSCRIPTION(widget_corne_layer_ui, zmk_layer_state_changed);

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
    lv_obj_align(split_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    refresh_split_label();
    lv_timer_create(split_timer_cb, 500, NULL);

    layer_label = lv_label_create(screen);
    lv_obj_set_style_text_font(layer_label, lv_theme_get_font_small(screen), LV_PART_MAIN);
    lv_obj_align(layer_label, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    widget_corne_layer_ui_init();

    return screen;
}
