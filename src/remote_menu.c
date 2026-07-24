#include "remote_menu.h"
#include <stdio.h>
#include <string.h>

/* Must match package.json messageKeys and watchface trio_types.h */
enum {
    KEY_CMD_TYPE = 7,
    KEY_CMD_AMOUNT = 8,
    KEY_CMD_STATUS = 9,
};

/* Menu rows */
enum {
    MENU_BOLUS = 0,
    MENU_CARBS = 1,
    MENU_EXIT = 2,
    MENU_COUNT = 3
};

static Window *s_menu_window;
static Window *s_pick_window;
static Window *s_confirm_window;

static TextLayer *s_menu_title;
static TextLayer *s_menu_rows[MENU_COUNT];
static TextLayer *s_menu_status;
static int s_menu_index;

static TextLayer *s_pick_title;
static TextLayer *s_pick_value;
static TextLayer *s_pick_hint;
static int32_t s_pick_cmd_type;
static int32_t s_pick_amount;

static TextLayer *s_confirm_title;
static TextLayer *s_confirm_value;
static TextLayer *s_confirm_hint;

static char s_last_status[64] = "";
static bool s_command_pending = false;
static char s_amount_buf[24];
static char s_status_buf[72];

static GFont font_title(void) {
    return fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
}

static GFont font_value(void) {
    /* Gothic keeps U/g glyphs; Bitham number faces often tofu unit letters. */
    return fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
}

static GFont font_body(void) {
    return fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
}

static GFont font_hint(void) {
    return fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
}

static void format_amount(char *buf, size_t n, int32_t cmd_type, int32_t amount) {
    if (cmd_type == 1) {
        int v = (int)amount;
        snprintf(buf, n, "%d.%d U", v / 10, v % 10);
    } else {
        snprintf(buf, n, "%ld g", (long)amount);
    }
}

static void menu_refresh_highlight(void);

void remote_menu_set_status(const char *status) {
    if (!status) {
        s_last_status[0] = '\0';
    } else {
        strncpy(s_last_status, status, sizeof(s_last_status) - 1);
        s_last_status[sizeof(s_last_status) - 1] = '\0';
    }
    s_command_pending = false;
    APP_LOG(APP_LOG_LEVEL_INFO, "Remote status: %s", s_last_status[0] ? s_last_status : "(empty)");
    if (s_menu_status) {
        if (s_last_status[0]) {
            snprintf(s_status_buf, sizeof(s_status_buf), "%s", s_last_status);
        } else {
            snprintf(s_status_buf, sizeof(s_status_buf), " ");
        }
        text_layer_set_text(s_menu_status, s_status_buf);
    }
}

static void send_watch_command(int32_t cmd_type, int32_t amount) {
    DictionaryIterator *iter;
    AppMessageResult begin = app_message_outbox_begin(&iter);
    if (begin != APP_MSG_OK) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Remote outbox_begin failed: %d", (int)begin);
        vibes_double_pulse();
        remote_menu_set_status("Phone link busy");
        return;
    }
    dict_write_int32(iter, KEY_CMD_TYPE, cmd_type);
    dict_write_int32(iter, KEY_CMD_AMOUNT, amount);
    AppMessageResult sent = app_message_outbox_send();
    if (sent != APP_MSG_OK) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Remote outbox_send failed: %d", (int)sent);
        vibes_double_pulse();
        remote_menu_set_status("Send failed");
        return;
    }
    APP_LOG(APP_LOG_LEVEL_INFO, "Remote sent cmd type=%ld amt=%ld", (long)cmd_type, (long)amount);
    s_command_pending = true;
    remote_menu_set_status(cmd_type == 1 ? "Sending bolus…" : "Sending carbs…");
    vibes_short_pulse();
}

/* ---------- Confirm screen (2nd SELECT required) ---------- */

static void confirm_back(ClickRecognizerRef r, void *c) {
    (void)r;
    (void)c;
    window_stack_pop(true); /* back to amount picker */
}

static void confirm_select(ClickRecognizerRef r, void *c) {
    (void)r;
    (void)c;
    /* Second confirmation — send to phone */
    send_watch_command(s_pick_cmd_type, s_pick_amount);
    /* Pop confirm + picker → menu */
    window_stack_pop(false);
    if (s_pick_window && window_stack_contains_window(s_pick_window)) {
        window_stack_pop(true);
    }
}

static void confirm_click_config(void *context) {
    (void)context;
    window_single_click_subscribe(BUTTON_ID_SELECT, confirm_select);
    window_single_click_subscribe(BUTTON_ID_BACK, confirm_back);
    /* Accidental UP/DOWN do nothing on confirm */
}

static void confirm_window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    GRect b = layer_get_bounds(root);
    const int w = b.size.w;
    const int h = b.size.h;

    s_confirm_title = text_layer_create(GRect(4, 8, w - 8, 36));
    text_layer_set_background_color(s_confirm_title, GColorClear);
    text_layer_set_text_alignment(s_confirm_title, GTextAlignmentCenter);
    text_layer_set_font(s_confirm_title, font_title());
    text_layer_set_text(s_confirm_title, s_pick_cmd_type == 1 ? "Confirm bolus?" : "Confirm carbs?");
    layer_add_child(root, text_layer_get_layer(s_confirm_title));

    format_amount(s_amount_buf, sizeof(s_amount_buf), s_pick_cmd_type, s_pick_amount);
    s_confirm_value = text_layer_create(GRect(4, h / 2 - 36, w - 8, 56));
    text_layer_set_background_color(s_confirm_value, GColorClear);
    text_layer_set_text_alignment(s_confirm_value, GTextAlignmentCenter);
    /* Prefer big numbers; fall back if glyph missing for unit letters */
    text_layer_set_font(s_confirm_value, font_value());
    text_layer_set_text(s_confirm_value, s_amount_buf);
    layer_add_child(root, text_layer_get_layer(s_confirm_value));

    s_confirm_hint = text_layer_create(GRect(6, h - 72, w - 12, 64));
    text_layer_set_background_color(s_confirm_hint, GColorClear);
    text_layer_set_text_alignment(s_confirm_hint, GTextAlignmentCenter);
    text_layer_set_font(s_confirm_hint, font_hint());
    text_layer_set_text(s_confirm_hint, "SELECT again to send\nBACK to edit amount");
    layer_add_child(root, text_layer_get_layer(s_confirm_hint));

    window_set_click_config_provider(window, confirm_click_config);
    vibes_short_pulse();
}

static void confirm_window_unload(Window *window) {
    (void)window;
    text_layer_destroy(s_confirm_title);
    text_layer_destroy(s_confirm_value);
    text_layer_destroy(s_confirm_hint);
    s_confirm_title = s_confirm_value = s_confirm_hint = NULL;
}

static void open_confirm(void) {
    if (!s_confirm_window) {
        s_confirm_window = window_create();
        window_set_window_handlers(s_confirm_window, (WindowHandlers){
            .load = confirm_window_load,
            .unload = confirm_window_unload,
        });
    }
    window_stack_push(s_confirm_window, true);
}

/* ---------- Amount picker ---------- */

static void picker_refresh_value_text(void) {
    format_amount(s_amount_buf, sizeof(s_amount_buf), s_pick_cmd_type, s_pick_amount);
    if (s_pick_value) {
        text_layer_set_text(s_pick_value, s_amount_buf);
    }
}

static void picker_back_handler(ClickRecognizerRef recognizer, void *context) {
    (void)recognizer;
    (void)context;
    window_stack_pop(true);
}

static void picker_select_handler(ClickRecognizerRef recognizer, void *context) {
    (void)recognizer;
    (void)context;
    /* First SELECT → confirmation screen (do not send yet) */
    open_confirm();
}

static void picker_up_handler(ClickRecognizerRef recognizer, void *context) {
    (void)recognizer;
    (void)context;
    if (s_pick_cmd_type == 1) {
        if (s_pick_amount < 300) s_pick_amount++;
    } else {
        if (s_pick_amount < 250) s_pick_amount += 5;
    }
    picker_refresh_value_text();
}

static void picker_down_handler(ClickRecognizerRef recognizer, void *context) {
    (void)recognizer;
    (void)context;
    if (s_pick_cmd_type == 1) {
        if (s_pick_amount > 1) s_pick_amount--;
    } else {
        if (s_pick_amount > 5) s_pick_amount -= 5;
    }
    picker_refresh_value_text();
}

static void picker_click_config(void *context) {
    (void)context;
    window_single_click_subscribe(BUTTON_ID_SELECT, picker_select_handler);
    window_single_click_subscribe(BUTTON_ID_UP, picker_up_handler);
    window_single_click_subscribe(BUTTON_ID_DOWN, picker_down_handler);
    window_single_click_subscribe(BUTTON_ID_BACK, picker_back_handler);
    window_single_repeating_click_subscribe(BUTTON_ID_UP, 80, picker_up_handler);
    window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 80, picker_down_handler);
}

static void pick_window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    GRect b = layer_get_bounds(root);
    const int w = b.size.w;
    const int h = b.size.h;

    s_pick_title = text_layer_create(GRect(4, 6, w - 8, 34));
    text_layer_set_background_color(s_pick_title, GColorClear);
    text_layer_set_text_alignment(s_pick_title, GTextAlignmentCenter);
    text_layer_set_font(s_pick_title, font_title());
    text_layer_set_text(s_pick_title, s_pick_cmd_type == 1 ? "Bolus" : "Carbs");
    layer_add_child(root, text_layer_get_layer(s_pick_title));

    s_pick_value = text_layer_create(GRect(4, h / 2 - 40, w - 8, 56));
    text_layer_set_background_color(s_pick_value, GColorClear);
    text_layer_set_text_alignment(s_pick_value, GTextAlignmentCenter);
    text_layer_set_font(s_pick_value, font_value());
    layer_add_child(root, text_layer_get_layer(s_pick_value));

    s_pick_hint = text_layer_create(GRect(6, h - 64, w - 12, 56));
    text_layer_set_background_color(s_pick_hint, GColorClear);
    text_layer_set_text_alignment(s_pick_hint, GTextAlignmentCenter);
    text_layer_set_font(s_pick_hint, font_hint());
    text_layer_set_text(s_pick_hint, "UP/DOWN adjust\nSELECT to confirm");
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
    s_pick_amount = (cmd_type == 1) ? 20 : 15;

    if (!s_pick_window) {
        s_pick_window = window_create();
        window_set_window_handlers(s_pick_window, (WindowHandlers){
            .load = pick_window_load,
            .unload = pick_window_unload,
        });
    }
    window_stack_push(s_pick_window, true);
}

/* ---------- Main menu (large custom rows) ---------- */

static const char *menu_label(int i) {
    switch (i) {
        case MENU_BOLUS: return "  Bolus";
        case MENU_CARBS: return "  Carbs";
        case MENU_EXIT: return "  Exit";
        default: return "";
    }
}

static void menu_refresh_highlight(void) {
    for (int i = 0; i < MENU_COUNT; i++) {
        if (!s_menu_rows[i]) continue;
        if (i == s_menu_index) {
            text_layer_set_background_color(s_menu_rows[i], GColorBlack);
            text_layer_set_text_color(s_menu_rows[i], GColorWhite);
        } else {
            text_layer_set_background_color(s_menu_rows[i], GColorClear);
            text_layer_set_text_color(s_menu_rows[i], GColorBlack);
        }
        text_layer_set_text(s_menu_rows[i], menu_label(i));
    }
}

static void menu_up(ClickRecognizerRef r, void *c) {
    (void)r;
    (void)c;
    if (s_menu_index > 0) s_menu_index--;
    menu_refresh_highlight();
}

static void menu_down(ClickRecognizerRef r, void *c) {
    (void)r;
    (void)c;
    if (s_menu_index < MENU_COUNT - 1) s_menu_index++;
    menu_refresh_highlight();
}

static void menu_select(ClickRecognizerRef r, void *c) {
    (void)r;
    (void)c;
    if (s_menu_index == MENU_EXIT) {
        window_stack_pop(true);
        return;
    }
    open_amount_picker(s_menu_index == MENU_BOLUS ? 1 : 2);
}

static void menu_back(ClickRecognizerRef r, void *c) {
    (void)r;
    (void)c;
    window_stack_pop(true);
}

static void menu_click_config(void *context) {
    (void)context;
    window_single_click_subscribe(BUTTON_ID_UP, menu_up);
    window_single_click_subscribe(BUTTON_ID_DOWN, menu_down);
    window_single_click_subscribe(BUTTON_ID_SELECT, menu_select);
    window_single_click_subscribe(BUTTON_ID_BACK, menu_back);
}

static void menu_window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    GRect b = layer_get_bounds(root);
    const int w = b.size.w;
    const int h = b.size.h;
    const int row_h = 40;
    const int row_y0 = 44;

    s_menu_title = text_layer_create(GRect(4, 4, w - 8, 36));
    text_layer_set_background_color(s_menu_title, GColorClear);
    text_layer_set_text_alignment(s_menu_title, GTextAlignmentCenter);
    text_layer_set_font(s_menu_title, font_title());
    text_layer_set_text(s_menu_title, "Trio Remote");
    layer_add_child(root, text_layer_get_layer(s_menu_title));

    for (int i = 0; i < MENU_COUNT; i++) {
        s_menu_rows[i] = text_layer_create(GRect(8, row_y0 + i * row_h, w - 16, row_h - 4));
        text_layer_set_text_alignment(s_menu_rows[i], GTextAlignmentLeft);
        text_layer_set_font(s_menu_rows[i], font_body());
        text_layer_set_overflow_mode(s_menu_rows[i], GTextOverflowModeTrailingEllipsis);
        layer_add_child(root, text_layer_get_layer(s_menu_rows[i]));
    }

    s_menu_status = text_layer_create(GRect(6, h - 40, w - 12, 36));
    text_layer_set_background_color(s_menu_status, GColorClear);
    text_layer_set_text_alignment(s_menu_status, GTextAlignmentCenter);
    text_layer_set_font(s_menu_status, font_hint());
    if (s_last_status[0]) {
        snprintf(s_status_buf, sizeof(s_status_buf), "%s", s_last_status);
    } else {
        snprintf(s_status_buf, sizeof(s_status_buf), "Pick bolus or carbs");
    }
    text_layer_set_text(s_menu_status, s_status_buf);
    layer_add_child(root, text_layer_get_layer(s_menu_status));

    s_menu_index = 0;
    menu_refresh_highlight();
    window_set_click_config_provider(window, menu_click_config);
}

static void menu_window_unload(Window *window) {
    (void)window;
    text_layer_destroy(s_menu_title);
    s_menu_title = NULL;
    for (int i = 0; i < MENU_COUNT; i++) {
        text_layer_destroy(s_menu_rows[i]);
        s_menu_rows[i] = NULL;
    }
    text_layer_destroy(s_menu_status);
    s_menu_status = NULL;
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
    if (s_confirm_window) {
        window_destroy(s_confirm_window);
        s_confirm_window = NULL;
    }
    if (s_pick_window) {
        window_destroy(s_pick_window);
        s_pick_window = NULL;
    }
    if (s_menu_window) {
        window_destroy(s_menu_window);
        s_menu_window = NULL;
    }
}
