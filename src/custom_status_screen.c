/*
 * Custom text-only status screen for the 128x32 corne OLED.
 *
 * The stock status screen renders battery/output icons as LVGL symbol glyphs,
 * which come out as garbage blocks on this panel. This screen uses plain text
 * only (no symbols), plus a kaomoji face.
 *
 * Layer state lives on the split central, so the layer name + reactive face are
 * central-only; the peripheral shows battery and a static face.
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/display/status_screen.h>
#include <zmk/event_manager.h>
#include <zmk/battery.h>
#include <zmk/events/battery_state_changed.h>

#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
#include <zmk/usb.h>
#include <zmk/events/usb_conn_state_changed.h>
#endif

#define HAS_LAYER (IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) || !IS_ENABLED(CONFIG_ZMK_SPLIT))

#if HAS_LAYER
#include <zmk/keymap.h>
#include <zmk/events/layer_state_changed.h>
#endif

static lv_obj_t *battery_label;
static lv_obj_t *face_label;
#if HAS_LAYER
static lv_obj_t *layer_label;
#endif

// --- Battery (text only, no charge symbol); runs on both halves ------------

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

// --- Layer name + mood face; central-only ---------------------------------

#if HAS_LAYER

struct layer_state_ext {
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

static void layer_update_cb(struct layer_state_ext state) {
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

static struct layer_state_ext layer_get_state(const zmk_event_t *eh) {
    zmk_keymap_layer_index_t index = zmk_keymap_highest_layer_active();
    return (struct layer_state_ext){
        .index = index,
        .label = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(index)),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(status_layer, struct layer_state_ext, layer_update_cb, layer_get_state)
ZMK_SUBSCRIPTION(status_layer, zmk_layer_state_changed);

#endif // HAS_LAYER

// --- Endpoint / Bluetooth profile (text only); central + BLE only ---------

#if HAS_LAYER && IS_ENABLED(CONFIG_ZMK_BLE)
#include <zmk/endpoints.h>
#include <zmk/ble.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/ble_active_profile_changed.h>

static lv_obj_t *endpoint_label;

struct endpoint_state_ext {
    enum zmk_transport transport;
    uint8_t profile_index;
    bool connected;
    bool bonded;
};

static void endpoint_update_cb(struct endpoint_state_ext state) {
    if (endpoint_label == NULL) {
        return;
    }
    char text[12] = {};
    switch (state.transport) {
    case ZMK_TRANSPORT_USB:
        snprintf(text, sizeof(text), "USB");
        break;
    case ZMK_TRANSPORT_BLE:
        // e.g. "BT1" connected, "BT1 x" bonded but disconnected, "BT1 o" unpaired
        snprintf(text, sizeof(text), "BT%u%s", state.profile_index + 1,
                 state.connected ? "" : (state.bonded ? " x" : " o"));
        break;
    default:
        snprintf(text, sizeof(text), "---");
        break;
    }
    lv_label_set_text(endpoint_label, text);
}

static struct endpoint_state_ext endpoint_get_state(const zmk_event_t *eh) {
    struct zmk_endpoint_instance ep = zmk_endpoint_get_selected();
    return (struct endpoint_state_ext){
        .transport = ep.transport,
        .profile_index = ep.ble.profile_index,
        .connected = zmk_ble_active_profile_is_connected(),
        .bonded = !zmk_ble_active_profile_is_open(),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(status_endpoint, struct endpoint_state_ext, endpoint_update_cb,
                            endpoint_get_state)
ZMK_SUBSCRIPTION(status_endpoint, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(status_endpoint, zmk_ble_active_profile_changed);

#endif // HAS_LAYER && CONFIG_ZMK_BLE

// --- Screen assembly ------------------------------------------------------

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);

    battery_label = lv_label_create(screen);
    lv_obj_align(battery_label, LV_ALIGN_TOP_LEFT, 0, 0);

    face_label = lv_label_create(screen);
    lv_obj_align(face_label, LV_ALIGN_TOP_RIGHT, 0, 0);

#if HAS_LAYER
    layer_label = lv_label_create(screen);
    lv_obj_align(layer_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);
#else
    lv_label_set_text(face_label, "(o.o)");
#endif

#if HAS_LAYER && IS_ENABLED(CONFIG_ZMK_BLE)
    endpoint_label = lv_label_create(screen);
    lv_obj_align(endpoint_label, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
#endif

    status_batt_init();
#if HAS_LAYER
    status_layer_init();
#endif
#if HAS_LAYER && IS_ENABLED(CONFIG_ZMK_BLE)
    status_endpoint_init();
#endif

    return screen;
}
