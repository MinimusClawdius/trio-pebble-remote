#include "remote_menu.h"
#include <stdio.h>
#include <string.h>

/* Must match both package.json messageKeys and watchface trio_types.h */
enum {
    KEY_CMD_TYPE = 7,
    KEY_CMD_AMOUNT = 8,
};

static Window *s_menu_window;
static Window *s_pick_window;

static SimpleMenuLayer *s_menu_layer;
static SimpleMenuSection s_menu_section;
#define REMOTE_MENU_ITEMS 3
static SimpleMenuItem s_menu_items[REMOTE_MENU_ITEMS];

static TextLayer *s_pick_title;
static TextLayer *s_pick_value;
static TextLayer *s_pick_hint;
static int32_t s_pick_cmd_type;
static int32_t s_pick_amount;

static void send_watch_command(int32_t cmd_type, int32_t amount) {
    DictionaryIterator *iter;
    if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
        vibes_double_pulse();
        return;
    }
    dict_write_int32(iter, KEY_CMD_TYPE, cmd_type);
    dict_write_int32(iter, KEY_CMD_AMOUNT, amount);
    app_message_outbox_send();
    vibes_short_pulse();
}

static void picker_refresh_value_text(void) {
    static char buf[24];
    if (s_pick_cmd_type == 1) {
        int v = (int)s_pick_amount;
        snprintf(buf, sizeof(buf), "%d.%d U", v / 10, v % 10);
    } else {
        snprintf(buf, sizeof(buf), "%ld g", (long)s_pick_amount);
    }
    text_layer_set_text(s_pick_value, buf);
}

static void picker_back_handler(ClickRecognizerRef recognizer, void *context) {
    (void)recognizer;
    (void)context;
    window_stack_pop(true);
}

static void picker_select_handler(ClickRecognizerRef recognizer, void *context) {
    (void)recognizer;
    (void)context;
    send_watch_command(s_pick_cmd_type, s_pick_amount);
    window_stack_pop(true);
}

static void picker_up_handler(ClickRecognizerRef recognizer, void *context) {
    (void)recognizer;
    (void)context;
    if (s_pick_cmd_type == 1) {
        if (s_pick_amount < 300) {
            s_pick_amount++;
        }
    } else {
        if (s_pick_amount < 250) {
            s_pick_amount += 5;
        }
    }
    picker_refresh_value_text();
}

static void picker_down_handler(ClickRecognizerRef recognizer, void *context) {
    (void)recognizer;
    (void)context;
    if (s_pick_cmd_type == 1) {
        if (s_pick_amount > 1) {
            s_pick_amount--;
        }
    } else {
        if (s_pick_amount > 5) {
            s_pick_amount -= 5;
        }
    }
    picker_refresh_value_text();
}

static void picker_click_config(void *context) {
    (void)context;
    window_single_click_subscribe(BUTTON_ID_SELECT, picker_select_handler);
    window_single_click_subscribe(BUTTON_ID_UP, picker_up_handler);
    window_single_click_subscribe(BUTTON_ID_DOWN, picker_down_handler);
    window_single_click_subscribe(BUTTON_ID_BACK, picker_back_handler);
}

static void pick_window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    GRect b = layer_get_bounds(root);

    s_pick_title = text_layer_create(GRect(0, 12, b.size.w, 28));
    text_layer_set_background_color(s_pick_title, GColorClear);
    text_layer_set_text_alignment(s_pick_title, GTextAlignmentCenter);
    text_layer_set_font(s_pick_title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text(s_pick_title, s_pick_cmd_type == 1 ? "Bolus" : "Carbs");
    layer_add_child(root, text_layer_get_layer(s_pick_title));

    s_pick_value = text_layer_create(GRect(0, 48, b.size.w, 44));
    text_layer_set_background_color(s_pick_value, GColorClear);
    text_layer_set_text_alignment(s_pick_value, GTextAlignmentCenter);
    text_layer_set_font(s_pick_value, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
    layer_add_child(root, text_layer_get_layer(s_pick_value));

    s_pick_hint = text_layer_create(GRect(8, b.size.h - 44, b.size.w - 16, 40));
    text_layer_set_background_color(s_pick_hint, GColorClear);
    text_layer_set_text_alignment(s_pick_hint, GTextAlignmentCenter);
    text_layer_set_font(s_pick_hint, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    text_layer_set_text(s_pick_hint, "UP/DOWN adjust\nSELECT send");
    layer_add_child(root, text_layer_get_layer(s_pick_hint));

    window_set_click_config_provider(window, picker_click_config);
    picker_refresh_value_text();
}

static void pick_window_unload(Window *window) {
    (void)window;
    text_layer_destroy(s_pick_title);
    text_layer_destroy(s_pick_value);
    text_layer_destroy(s_pick_hint);
    s_pick_title = s_pick_value = s_pick_hint = NULL;
}

static void open_amount_picker(int32_t cmd_type) {
    s_pick_cmd_type = cmd_type;
    if (cmd_type == 1) {
        s_pick_amount = 20;
    } else {
        s_pick_amount = 15;
    }

    if (!s_pick_window) {
        s_pick_window = window_create();
        window_set_window_handlers(s_pick_window, (WindowHandlers){
            .load = pick_window_load,
            .unload = pick_window_unload,
        });
    }
    window_stack_push(s_pick_window, true);
}

/**
 * Watch app: keep menu under picker so BACK from picker returns to menu.
 * (Watchface pops menu first so the face shows behind the picker.)
 */
static void menu_select_cb(int index, void *context) {
    (void)context;
    if (index == 2) {
        window_stack_pop(true);
        return;
    }
    open_amount_picker(index == 0 ? 1 : 2);
}

static void menu_window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(root);

    s_menu_items[0] = (SimpleMenuItem){
        .title = "Remote bolus",
        .subtitle = "0.1 U",
        .callback = menu_select_cb,
    };
    s_menu_items[1] = (SimpleMenuItem){
        .title = "Remote carbs",
        .subtitle = "5 g",
        .callback = menu_select_cb,
    };
    s_menu_items[2] = (SimpleMenuItem){
        .title = "Cancel",
        .subtitle = NULL,
        .callback = menu_select_cb,
    };

    s_menu_section = (SimpleMenuSection){
        .title = "Trio remote",
        .num_items = REMOTE_MENU_ITEMS,
        .items = s_menu_items,
    };

    s_menu_layer = simple_menu_layer_create(bounds, window, &s_menu_section, 1, NULL);
    layer_add_child(root, simple_menu_layer_get_layer(s_menu_layer));
}

static void menu_window_unload(Window *window) {
    (void)window;
    if (s_menu_layer) {
        simple_menu_layer_destroy(s_menu_layer);
        s_menu_layer = NULL;
    }
}

void remote_menu_init(void) {
    s_menu_window = window_create();
    window_set_window_handlers(s_menu_window, (WindowHandlers){
        .load = menu_window_load,
        .unload = menu_window_unload,
    });
    window_stack_push(s_menu_window, true);
}

void remote_menu_deinit(void) {
    if (s_pick_window) {
        window_destroy(s_pick_window);
        s_pick_window = NULL;
    }
    if (s_menu_window) {
        window_destroy(s_menu_window);
        s_menu_window = NULL;
    }
}
