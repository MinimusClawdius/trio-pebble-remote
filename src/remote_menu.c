#include "remote_menu.h"
#include <stdio.h>
#include <string.h>

/* Must match package.json messageKeys */
enum {
    KEY_CMD_TYPE = 7,
    KEY_CMD_AMOUNT = 8,
    KEY_CMD_STATUS = 9,
};

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
static TextLayer *s_pick_value;   /* giant number */
static TextLayer *s_pick_unit;    /* U / g under number */
static TextLayer *s_pick_hint;
static int32_t s_pick_cmd_type;
static int32_t s_pick_amount;

static TextLayer *s_confirm_title;
static TextLayer *s_confirm_value;
static TextLayer *s_confirm_unit;
static TextLayer *s_confirm_hint;

static char s_last_status[64] = "";
static bool s_command_pending = false;
static char s_num_buf[16];
static char s_unit_buf[8];
static char s_status_buf[72];

enum {
    PERSIST_BOLUS_STEP = 1,
    PERSIST_BOLUS_DEFAULT = 2,
    PERSIST_CARB_STEP = 3,
    PERSIST_CARB_DEFAULT = 4
};
static int32_t s_bolus_step_tenths = 1;
static int32_t s_bolus_default_tenths = 20;
static int32_t s_carb_step_grams = 5;
static int32_t s_carb_default_grams = 15;

static void prefs_load_from_persist(void) {
    if (persist_exists(PERSIST_BOLUS_STEP)) s_bolus_step_tenths = persist_read_int(PERSIST_BOLUS_STEP);
    if (persist_exists(PERSIST_BOLUS_DEFAULT)) s_bolus_default_tenths = persist_read_int(PERSIST_BOLUS_DEFAULT);
    if (persist_exists(PERSIST_CARB_STEP)) s_carb_step_grams = persist_read_int(PERSIST_CARB_STEP);
    if (persist_exists(PERSIST_CARB_DEFAULT)) s_carb_default_grams = persist_read_int(PERSIST_CARB_DEFAULT);
    if (s_bolus_step_tenths < 1) s_bolus_step_tenths = 1;
    if (s_bolus_step_tenths > 50) s_bolus_step_tenths = 50;
    if (s_bolus_default_tenths < 1) s_bolus_default_tenths = 20;
    if (s_bolus_default_tenths > 300) s_bolus_default_tenths = 300;
    if (s_carb_step_grams < 1) s_carb_step_grams = 5;
    if (s_carb_step_grams > 50) s_carb_step_grams = 50;
    if (s_carb_default_grams < 1) s_carb_default_grams = 15;
    if (s_carb_default_grams > 250) s_carb_default_grams = 250;
}

void remote_menu_apply_prefs(int32_t bolus_step_tenths, int32_t bolus_default_tenths,
                             int32_t carb_step_grams, int32_t carb_default_grams) {
    if (bolus_step_tenths >= 1 && bolus_step_tenths <= 50) {
        s_bolus_step_tenths = bolus_step_tenths;
        persist_write_int(PERSIST_BOLUS_STEP, s_bolus_step_tenths);
    }
    if (bolus_default_tenths >= 1 && bolus_default_tenths <= 300) {
        s_bolus_default_tenths = bolus_default_tenths;
        persist_write_int(PERSIST_BOLUS_DEFAULT, s_bolus_default_tenths);
    }
    if (carb_step_grams >= 1 && carb_step_grams <= 50) {
        s_carb_step_grams = carb_step_grams;
        persist_write_int(PERSIST_CARB_STEP, s_carb_step_grams);
    }
    if (carb_default_grams >= 1 && carb_default_grams <= 250) {
        s_carb_default_grams = carb_default_grams;
        persist_write_int(PERSIST_CARB_DEFAULT, s_carb_default_grams);
    }
}

/* Largest system faces suitable for safety-critical dosing UI */
static GFont font_screen_title(void) {
    return fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
}

static GFont font_menu_row(void) {
    return fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
}

static GFont font_amount_number(void) {
#ifdef PBL_COLOR
    return fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49);
#else
    return fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
#endif
}

static GFont font_amount_unit(void) {
    return fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
}

static GFont font_hint(void) {
    return fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
}

static GFont font_status(void) {
    return fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
}

/** Split dose into giant digits + unit label (Bitham/Roboto lack reliable U/g). */
static void format_amount_parts(int32_t cmd_type, int32_t amount) {
    if (cmd_type == 1) {
        int v = (int)amount;
        snprintf(s_num_buf, sizeof(s_num_buf), "%d.%d", v / 10, v % 10);
        snprintf(s_unit_buf, sizeof(s_unit_buf), "units");
    } else {
        snprintf(s_num_buf, sizeof(s_num_buf), "%ld", (long)amount);
        snprintf(s_unit_buf, sizeof(s_unit_buf), "grams");
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
        vibes_double_pulse();
        remote_menu_set_status("Phone link busy");
        return;
    }
    dict_write_int32(iter, KEY_CMD_TYPE, cmd_type);
    dict_write_int32(iter, KEY_CMD_AMOUNT, amount);
    AppMessageResult sent = app_message_outbox_send();
    if (sent != APP_MSG_OK) {
        vibes_double_pulse();
        remote_menu_set_status("Send failed");
        return;
    }
    s_command_pending = true;
    remote_menu_set_status(cmd_type == 1 ? "Sending bolus…" : "Sending carbs…");
    vibes_short_pulse();
}

/* ---------- Confirm (2nd SELECT) ---------- */

static void confirm_back(ClickRecognizerRef r, void *c) {
    (void)r;
    (void)c;
    window_stack_pop(true);
}

static void confirm_select(ClickRecognizerRef r, void *c) {
    (void)r;
    (void)c;
    send_watch_command(s_pick_cmd_type, s_pick_amount);
    window_stack_pop(false);
    if (s_pick_window && window_stack_contains_window(s_pick_window)) {
        window_stack_pop(true);
    }
}

static void confirm_click_config(void *context) {
    (void)context;
    window_single_click_subscribe(BUTTON_ID_SELECT, confirm_select);
    window_single_click_subscribe(BUTTON_ID_BACK, confirm_back);
}

static void confirm_window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    GRect b = layer_get_bounds(root);
    const int w = b.size.w;
    const int h = b.size.h;
    /* Fill screen: title, huge value, unit, hint — minimal dead space */
    const int title_h = 40;
    const int unit_h = 36;
    const int hint_h = 56;
    const int value_h = h - title_h - unit_h - hint_h - 12;
    int value_y = title_h + 2;

    s_confirm_title = text_layer_create(GRect(2, 2, w - 4, title_h));
    text_layer_set_background_color(s_confirm_title, GColorClear);
    text_layer_set_text_alignment(s_confirm_title, GTextAlignmentCenter);
    text_layer_set_font(s_confirm_title, font_screen_title());
    text_layer_set_text(s_confirm_title, s_pick_cmd_type == 1 ? "SEND BOLUS?" : "SEND CARBS?");
    layer_add_child(root, text_layer_get_layer(s_confirm_title));

    format_amount_parts(s_pick_cmd_type, s_pick_amount);

    s_confirm_value = text_layer_create(GRect(2, value_y, w - 4, value_h));
    text_layer_set_background_color(s_confirm_value, GColorClear);
    text_layer_set_text_alignment(s_confirm_value, GTextAlignmentCenter);
    text_layer_set_font(s_confirm_value, font_amount_number());
    text_layer_set_text(s_confirm_value, s_num_buf);
    layer_add_child(root, text_layer_get_layer(s_confirm_value));

    s_confirm_unit = text_layer_create(GRect(2, value_y + value_h - 4, w - 4, unit_h));
    text_layer_set_background_color(s_confirm_unit, GColorClear);
    text_layer_set_text_alignment(s_confirm_unit, GTextAlignmentCenter);
    text_layer_set_font(s_confirm_unit, font_amount_unit());
    text_layer_set_text(s_confirm_unit, s_unit_buf);
    layer_add_child(root, text_layer_get_layer(s_confirm_unit));

    s_confirm_hint = text_layer_create(GRect(4, h - hint_h - 2, w - 8, hint_h));
    text_layer_set_background_color(s_confirm_hint, GColorClear);
    text_layer_set_text_alignment(s_confirm_hint, GTextAlignmentCenter);
    text_layer_set_font(s_confirm_hint, font_hint());
    text_layer_set_text(s_confirm_hint, "SELECT = send now\nBACK = edit");
    layer_add_child(root, text_layer_get_layer(s_confirm_hint));

    window_set_click_config_provider(window, confirm_click_config);
    vibes_short_pulse();
}

static void confirm_window_unload(Window *window) {
    (void)window;
    text_layer_destroy(s_confirm_title);
    text_layer_destroy(s_confirm_value);
    text_layer_destroy(s_confirm_unit);
    text_layer_destroy(s_confirm_hint);
    s_confirm_title = s_confirm_value = s_confirm_unit = s_confirm_hint = NULL;
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
    format_amount_parts(s_pick_cmd_type, s_pick_amount);
    if (s_pick_value) text_layer_set_text(s_pick_value, s_num_buf);
    if (s_pick_unit) text_layer_set_text(s_pick_unit, s_unit_buf);
}

static void picker_back_handler(ClickRecognizerRef recognizer, void *context) {
    (void)recognizer;
    (void)context;
    window_stack_pop(true);
}

static void picker_select_handler(ClickRecognizerRef recognizer, void *context) {
    (void)recognizer;
    (void)context;
    open_confirm();
}

static void picker_up_handler(ClickRecognizerRef recognizer, void *context) {
    (void)recognizer;
    (void)context;
    if (s_pick_cmd_type == 1) {
        if (s_pick_amount + s_bolus_step_tenths <= 300)
            s_pick_amount += s_bolus_step_tenths;
        else
            s_pick_amount = 300;
    } else {
        if (s_pick_amount + s_carb_step_grams <= 250)
            s_pick_amount += s_carb_step_grams;
        else
            s_pick_amount = 250;
    }
    picker_refresh_value_text();
}

static void picker_down_handler(ClickRecognizerRef recognizer, void *context) {
    (void)recognizer;
    (void)context;
    if (s_pick_cmd_type == 1) {
        if (s_pick_amount > s_bolus_step_tenths)
            s_pick_amount -= s_bolus_step_tenths;
        else
            s_pick_amount = s_bolus_step_tenths;
    } else {
        if (s_pick_amount > s_carb_step_grams)
            s_pick_amount -= s_carb_step_grams;
        else
            s_pick_amount = s_carb_step_grams;
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
    const int title_h = 40;
    const int unit_h = 36;
    const int hint_h = 52;
    const int value_h = h - title_h - unit_h - hint_h - 8;
    int value_y = title_h;

    s_pick_title = text_layer_create(GRect(2, 2, w - 4, title_h));
    text_layer_set_background_color(s_pick_title, GColorClear);
    text_layer_set_text_alignment(s_pick_title, GTextAlignmentCenter);
    text_layer_set_font(s_pick_title, font_screen_title());
    text_layer_set_text(s_pick_title, s_pick_cmd_type == 1 ? "BOLUS" : "CARBS");
    layer_add_child(root, text_layer_get_layer(s_pick_title));

    /* Giant dose number — primary safety surface */
    s_pick_value = text_layer_create(GRect(2, value_y, w - 4, value_h));
    text_layer_set_background_color(s_pick_value, GColorClear);
    text_layer_set_text_alignment(s_pick_value, GTextAlignmentCenter);
    text_layer_set_font(s_pick_value, font_amount_number());
    layer_add_child(root, text_layer_get_layer(s_pick_value));

    s_pick_unit = text_layer_create(GRect(2, value_y + value_h - 2, w - 4, unit_h));
    text_layer_set_background_color(s_pick_unit, GColorClear);
    text_layer_set_text_alignment(s_pick_unit, GTextAlignmentCenter);
    text_layer_set_font(s_pick_unit, font_amount_unit());
    layer_add_child(root, text_layer_get_layer(s_pick_unit));

    s_pick_hint = text_layer_create(GRect(4, h - hint_h - 2, w - 8, hint_h));
    text_layer_set_background_color(s_pick_hint, GColorClear);
    text_layer_set_text_alignment(s_pick_hint, GTextAlignmentCenter);
    text_layer_set_font(s_pick_hint, font_hint());
    text_layer_set_text(s_pick_hint, "UP/DOWN  adjust\nSELECT  confirm");
    layer_add_child(root, text_layer_get_layer(s_pick_hint));

    window_set_click_config_provider(window, picker_click_config);
    picker_refresh_value_text();
}

static void pick_window_unload(Window *window) {
    (void)window;
    text_layer_destroy(s_pick_title);
    text_layer_destroy(s_pick_value);
    text_layer_destroy(s_pick_unit);
    text_layer_destroy(s_pick_hint);
    s_pick_title = s_pick_value = s_pick_unit = s_pick_hint = NULL;
}

static void open_amount_picker(int32_t cmd_type) {
    s_pick_cmd_type = cmd_type;
    s_pick_amount = (cmd_type == 1) ? s_bolus_default_tenths : s_carb_default_grams;
    if (!s_pick_window) {
        s_pick_window = window_create();
        window_set_window_handlers(s_pick_window, (WindowHandlers){
            .load = pick_window_load,
            .unload = pick_window_unload,
        });
    }
    window_stack_push(s_pick_window, true);
}

/* ---------- Main menu — full-height rows ---------- */

static const char *menu_label(int i) {
    switch (i) {
        case MENU_BOLUS: return "BOLUS";
        case MENU_CARBS: return "CARBS";
        case MENU_EXIT: return "EXIT";
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
    const int title_h = 40;
    const int status_h = 44;
    const int rows_area = h - title_h - status_h - 4;
    const int row_h = rows_area / MENU_COUNT;
    const int row_y0 = title_h + 2;

    s_menu_title = text_layer_create(GRect(2, 2, w - 4, title_h));
    text_layer_set_background_color(s_menu_title, GColorClear);
    text_layer_set_text_alignment(s_menu_title, GTextAlignmentCenter);
    text_layer_set_font(s_menu_title, font_screen_title());
    text_layer_set_text(s_menu_title, "TRIO REMOTE");
    layer_add_child(root, text_layer_get_layer(s_menu_title));

    for (int i = 0; i < MENU_COUNT; i++) {
        int y = row_y0 + i * row_h;
        s_menu_rows[i] = text_layer_create(GRect(6, y, w - 12, row_h - 4));
        text_layer_set_text_alignment(s_menu_rows[i], GTextAlignmentCenter);
        text_layer_set_font(s_menu_rows[i], font_menu_row());
        text_layer_set_overflow_mode(s_menu_rows[i], GTextOverflowModeTrailingEllipsis);
        layer_add_child(root, text_layer_get_layer(s_menu_rows[i]));
    }

    s_menu_status = text_layer_create(GRect(4, h - status_h, w - 8, status_h - 2));
    text_layer_set_background_color(s_menu_status, GColorClear);
    text_layer_set_text_alignment(s_menu_status, GTextAlignmentCenter);
    text_layer_set_font(s_menu_status, font_status());
    if (s_last_status[0]) {
        snprintf(s_status_buf, sizeof(s_status_buf), "%s", s_last_status);
    } else {
        snprintf(s_status_buf, sizeof(s_status_buf), "Select dose type");
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
    prefs_load_from_persist();
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
