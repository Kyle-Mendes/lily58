/*
 * Custom text-only status screen for the 128x32 corne OLED.
 *
 * The stock status screen renders battery/output icons as LVGL symbol glyphs,
 * which come out as garbage blocks on this panel. This screen uses plain text
 * only (no symbols), plus a kaomoji that reflects the active layer.
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/display/status_screen.h>
#include <zmk/event_manager.h>

#include <zmk/battery.h>
#include <zmk/usb.h>
#include <zmk/keymap.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/events/layer_state_changed.h>

static lv_obj_t *battery_label;
static lv_obj_t *layer_label;
static lv_obj_t *face_label;

// --- Battery (text only, no charge symbol) --------------------------------

struct batt_state {
    uint8_t level;
    bool usb_present;
};

static void batt_update_cb(struct batt_state state) {
    if (battery_label == NULL) {
        return;
    }
    char text[16] = {};
    snprintf(text, sizeof(text), "%s %u%%", state.usb_present ? "USB" : "BAT", state.level);
    lv_label_set_text(battery_label, text);
}

static struct batt_state batt_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);
    return (struct batt_state){
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#else
        .usb_present = false,
#endif
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(status_batt, struct batt_state, batt_update_cb, batt_get_state)
ZMK_SUBSCRIPTION(status_batt, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(status_batt, zmk_usb_conn_state_changed);
#endif

// --- Layer (name + a mood face) -------------------------------------------

struct layer_state {
    zmk_keymap_layer_index_t index;
    const char *label;
};

static const char *face_for_layer(zmk_keymap_layer_index_t index) {
    switch (index) {
    case 0:
        return "(o.o)";
    case 1:
        return "(>_>)";
    case 2:
        return "(^o^)";
    default:
        return "(._.)";
    }
}

static void layer_update_cb(struct layer_state state) {
    if (layer_label != NULL) {
        if (state.label == NULL || strlen(state.label) == 0) {
            char text[16] = {};
            snprintf(text, sizeof(text), "L%u", state.index);
            lv_label_set_text(layer_label, text);
        } else {
            lv_label_set_text(layer_label, state.label);
        }
    }
    if (face_label != NULL) {
        lv_label_set_text(face_label, face_for_layer(state.index));
    }
}

static struct layer_state layer_get_state(const zmk_event_t *eh) {
    zmk_keymap_layer_index_t index = zmk_keymap_highest_layer_active();
    return (struct layer_state){
        .index = index,
        .label = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(index)),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(status_layer, struct layer_state, layer_update_cb, layer_get_state)
ZMK_SUBSCRIPTION(status_layer, zmk_layer_state_changed);

// --- Screen assembly ------------------------------------------------------

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);

    battery_label = lv_label_create(screen);
    lv_obj_align(battery_label, LV_ALIGN_TOP_LEFT, 0, 0);

    face_label = lv_label_create(screen);
    lv_obj_align(face_label, LV_ALIGN_TOP_RIGHT, 0, 0);

    layer_label = lv_label_create(screen);
    lv_obj_align(layer_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    status_batt_init();
    status_layer_init();

    return screen;
}
