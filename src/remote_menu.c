#include "remote_menu.h"
#include "modules/remote_send_ui.h"
#include <stdio.h>
#include <string.h>

/* Must match package.json messageKeys */
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
static Layer *s_pick_chrome_layer;
static int32_t s_pick_cmd_type;
static int32_t s_pick_amount;

static int32_t s_default_bolus_tenths = 20;
static int32_t s_default_carb_g = 15;
static int32_t s_step_bolus_tenths = 1;
static int32_t s_step_carb_g = 5;

void remote_menu_apply_phone_defaults(int32_t default_bolus_tenths, int32_t default_carb_g,
                                      int32_t bolus_step_tenths, int32_t carb_step_g) {
    if (default_bolus_tenths > 0 && default_bolus_tenths <= 300) {
        s_default_bolus_tenths = default_bolus_tenths;
    }
    if (default_carb_g > 0 && default_carb_g <= 250) {
        s_default_carb_g = default_carb_g;
    }
    if (bolus_step_tenths > 0 && bolus_step_tenths <= 50) {
        s_step_bolus_tenths = bolus_step_tenths;
    }
    if (carb_step_g > 0 && carb_step_g <= 25) {
        s_step_carb_g = carb_step_g;
    }
}

void remote_menu_send_to_phone(int32_t cmd_type, int32_t amount) {
    DictionaryIterator *iter;
    if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
        remote_send_ui_on_prepare_send_failed();
        return;
    }
    dict_write_int32(iter, KEY_CMD_TYPE, cmd_type);
    dict_write_int32(iter, KEY_CMD_AMOUNT, amount);
    app_message_outbox_send();
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
    remote_send_ui_push_confirm(s_pick_cmd_type, s_pick_amount);
}

static void picker_up_handler(ClickRecognizerRef recognizer, void *context) {
    (void)recognizer;
    (void)context;
    if (s_pick_cmd_type == 1) {
        if (s_pick_amount <= 300 - s_step_bolus_tenths) {
            s_pick_amount += s_step_bolus_tenths;
        }
    } else {
        if (s_pick_amount <= 250 - s_step_carb_g) {
            s_pick_amount += s_step_carb_g;
        }
    }
    picker_refresh_value_text();
}

static void picker_down_handler(ClickRecognizerRef recognizer, void *context) {
    (void)recognizer;
    (void)context;
    if (s_pick_cmd_type == 1) {
        if (s_pick_amount > s_step_bolus_tenths) {
            s_pick_amount -= s_step_bolus_tenths;
        } else if (s_pick_amount > 1) {
            s_pick_amount = 1;
        }
    } else {
        if (s_pick_amount > s_step_carb_g) {
            s_pick_amount -= s_step_carb_g;
        } else if (s_pick_amount > 1) {
            s_pick_amount = 1;
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

static void pick_chrome_update_proc(Layer *layer, GContext *ctx) {
    GRect r = layer_get_bounds(layer);
#ifdef PBL_COLOR
    graphics_context_set_fill_color(ctx, GColorDarkGray);
#else
    graphics_context_set_fill_color(ctx, GColorBlack);
#endif
    graphics_fill_rect(ctx, r, 0, GCornerNone);

    graphics_context_set_fill_color(ctx, GColorWhite);
    int cx = (int)r.size.w / 2;
    int h = (int)r.size.h;
    int y_plus = h * 17 / 100;
    int y_check = h * 48 / 100;
    int y_minus = h * 81 / 100;
    const int arm = 6;
    const int thick = 3;

    graphics_fill_rect(ctx, GRect((int16_t)(cx - arm), (int16_t)(y_plus - 1), (int16_t)(arm * 2), thick), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect((int16_t)(cx - 1), (int16_t)(y_plus - arm), thick, (int16_t)(arm * 2)), 0, GCornerNone);

    graphics_fill_rect(ctx, GRect((int16_t)(cx - arm), (int16_t)(y_minus - 1), (int16_t)(arm * 2), thick), 0, GCornerNone);

    graphics_fill_rect(ctx, GRect((int16_t)(cx - 8), (int16_t)(y_check - 1), 6, thick), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect((int16_t)(cx - 3), (int16_t)(y_check - 5), thick, 10), 0, GCornerNone);
}

static void pick_window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    GRect b = layer_get_bounds(root);
    GRect content = remote_send_content_left_of_action_bar(&b);
    GRect strip = remote_send_right_action_strip_rect(&b);

    window_set_background_color(window, GColorWhite);

    int16_t title_h = 40;
    int16_t title_y = (int16_t)(content.origin.y + 4);
    s_pick_title = text_layer_create(GRect(content.origin.x, title_y, content.size.w, title_h));
    text_layer_set_background_color(s_pick_title, GColorClear);
    text_layer_set_text_color(s_pick_title, GColorBlack);
    text_layer_set_text_alignment(s_pick_title, GTextAlignmentCenter);
    text_layer_set_font(s_pick_title, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
    text_layer_set_text(s_pick_title, s_pick_cmd_type == 1 ? "Bolus" : "Carbs");
    layer_add_child(root, text_layer_get_layer(s_pick_title));

    int16_t val_y = (int16_t)(title_y + title_h - 2);
    int16_t val_h = (int16_t)(content.origin.y + content.size.h - val_y - 6);
    if (val_h < 52) {
        val_h = 52;
    }
    s_pick_value = text_layer_create(GRect(content.origin.x, val_y, content.size.w, val_h));
    text_layer_set_background_color(s_pick_value, GColorClear);
    text_layer_set_text_color(s_pick_value, GColorBlack);
    text_layer_set_text_alignment(s_pick_value, GTextAlignmentCenter);
    text_layer_set_font(s_pick_value, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
    layer_add_child(root, text_layer_get_layer(s_pick_value));

    s_pick_chrome_layer = layer_create(strip);
    layer_set_update_proc(s_pick_chrome_layer, pick_chrome_update_proc);
    layer_add_child(root, s_pick_chrome_layer);

    window_set_click_config_provider(window, picker_click_config);
    picker_refresh_value_text();
}

static void pick_window_unload(Window *window) {
    (void)window;
    text_layer_destroy(s_pick_title);
    text_layer_destroy(s_pick_value);
    layer_destroy(s_pick_chrome_layer);
    s_pick_title = s_pick_value = NULL;
    s_pick_chrome_layer = NULL;
}

static void push_amount_picker(int32_t cmd_type, int32_t amount) {
    s_pick_cmd_type = cmd_type;
    s_pick_amount = amount;

    if (!s_pick_window) {
        s_pick_window = window_create();
        window_set_window_handlers(s_pick_window, (WindowHandlers){
            .load = pick_window_load,
            .unload = pick_window_unload,
        });
    }
    window_stack_push(s_pick_window, true);
}

static void open_amount_picker(int32_t cmd_type) {
    if (cmd_type == 1) {
        push_amount_picker(1, s_default_bolus_tenths);
    } else {
        push_amount_picker(2, s_default_carb_g);
    }
}

void remote_menu_open_bolus_picker_preset(int32_t tenths) {
    if (tenths < 1) {
        return;
    }
    if (s_pick_window && window_stack_contains_window(s_pick_window)) {
        s_pick_cmd_type = 1;
        s_pick_amount = tenths;
        picker_refresh_value_text();
        return;
    }
    push_amount_picker(1, tenths);
}

static void menu_select_cb(int index, void *context) {
    (void)context;
    if (index == 2) {
        window_stack_pop(true);
        return;
    }
    if (remote_send_ui_blocks_remote()) {
        return;
    }
    open_amount_picker(index == 0 ? 1 : 2);
}

static void menu_window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(root);

    s_menu_items[0] = (SimpleMenuItem){
        .title = "Bolus",
        .subtitle = NULL,
        .callback = menu_select_cb,
    };
    s_menu_items[1] = (SimpleMenuItem){
        .title = "Carbs",
        .subtitle = NULL,
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
