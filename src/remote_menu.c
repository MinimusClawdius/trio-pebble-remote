#include "remote_menu.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Must match package.json messageKeys */
enum {
    KEY_CMD_TYPE = 7,
    KEY_CMD_AMOUNT = 8,
    KEY_CMD_STATUS = 9,
};

enum {
    BTN_BOLUS = 0,
    BTN_CARBS = 1,
    BTN_COUNT = 2
};

static Window *s_home_window;
static Window *s_pick_window;
static Window *s_confirm_window;

static Layer *s_home_canvas;
static TextLayer *s_home_status;
static int s_focus_btn; /* hardware-button focus 0=bolus 1=carbs */
static int s_press_btn; /* -1 none, else button under finger */
static GRect s_btn_frame[BTN_COUNT];

static TextLayer *s_pick_title;
static TextLayer *s_pick_value;
static TextLayer *s_pick_unit;
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

/* Touch tap tracking */
static bool s_touch_active;
static int16_t s_touch_down_x;
static int16_t s_touch_down_y;
static int s_touch_down_btn;
static bool s_touch_subscribed;

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

#define TAP_SLOP_PX 18

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

static GFont font_screen_title(void) {
    return fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
}

static GFont font_home_btn(void) {
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

static void open_amount_picker(int32_t cmd_type);

void remote_menu_set_status(const char *status) {
    if (!status) {
        s_last_status[0] = '\0';
    } else {
        strncpy(s_last_status, status, sizeof(s_last_status) - 1);
        s_last_status[sizeof(s_last_status) - 1] = '\0';
    }
    s_command_pending = false;
    if (s_home_status) {
        if (s_last_status[0]) {
            snprintf(s_status_buf, sizeof(s_status_buf), "%s", s_last_status);
        } else {
            snprintf(s_status_buf, sizeof(s_status_buf), " ");
        }
        text_layer_set_text(s_home_status, s_status_buf);
    }
    if (s_home_canvas) {
        layer_mark_dirty(s_home_canvas);
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

static int hit_test_button(int16_t x, int16_t y) {
    GPoint p = GPoint(x, y);
    for (int i = 0; i < BTN_COUNT; i++) {
        if (grect_contains_point(&s_btn_frame[i], &p)) {
            return i;
        }
    }
    return -1;
}

static void activate_button(int btn) {
    if (btn == BTN_BOLUS) {
        open_amount_picker(1);
    } else if (btn == BTN_CARBS) {
        open_amount_picker(2);
    }
}

/* ---------- Confirm ---------- */

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

/* ---------- Home: two big buttons (+ touch) ---------- */

static void home_canvas_update(Layer *layer, GContext *ctx) {
    GRect b = layer_get_bounds(layer);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, b, 0, GCornerNone);

    for (int i = 0; i < BTN_COUNT; i++) {
        GRect f = s_btn_frame[i];
        bool pressed = (s_press_btn == i);
        bool focused = (s_focus_btn == i);

        GColor fill = pressed ? GColorBlack : GColorWhite;
        GColor ink = pressed ? GColorWhite : GColorBlack;
        graphics_context_set_fill_color(ctx, fill);
        graphics_fill_rect(ctx, f, 8, GCornersAll);

        graphics_context_set_stroke_color(ctx, GColorBlack);
        graphics_context_set_stroke_width(ctx, focused || pressed ? 4 : 2);
        graphics_draw_round_rect(ctx, f, 8);

        const char *label = (i == BTN_BOLUS) ? "BOLUS" : "CARBS";
        graphics_context_set_text_color(ctx, ink);
        GRect text_box = grect_inset(f, GEdgeInsets(f.size.h / 2 - 18, 8, 8, 8));
        graphics_draw_text(ctx, label, font_home_btn(), text_box,
                           GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    }
}

static void home_layout_buttons(GRect bounds) {
    const int status_h = 40;
    const int gap = 10;
    const int pad = 8;
    int usable_h = bounds.size.h - status_h - pad * 2 - gap;
    int btn_h = usable_h / 2;
    int w = bounds.size.w - pad * 2;
    s_btn_frame[BTN_BOLUS] = GRect(pad, pad, w, btn_h);
    s_btn_frame[BTN_CARBS] = GRect(pad, pad + btn_h + gap, w, btn_h);
}

static void home_up(ClickRecognizerRef r, void *c) {
    (void)r;
    (void)c;
    s_focus_btn = BTN_BOLUS;
    if (s_home_canvas) layer_mark_dirty(s_home_canvas);
}

static void home_down(ClickRecognizerRef r, void *c) {
    (void)r;
    (void)c;
    s_focus_btn = BTN_CARBS;
    if (s_home_canvas) layer_mark_dirty(s_home_canvas);
}

static void home_select(ClickRecognizerRef r, void *c) {
    (void)r;
    (void)c;
    activate_button(s_focus_btn);
}

static void home_back(ClickRecognizerRef r, void *c) {
    (void)r;
    (void)c;
    window_stack_pop(true); /* leave app */
}

static void home_click_config(void *context) {
    (void)context;
    window_single_click_subscribe(BUTTON_ID_UP, home_up);
    window_single_click_subscribe(BUTTON_ID_DOWN, home_down);
    window_single_click_subscribe(BUTTON_ID_SELECT, home_select);
    window_single_click_subscribe(BUTTON_ID_BACK, home_back);
}

#if defined(PBL_TOUCH)
static void home_touch_handler(const TouchEvent *event, void *context) {
    (void)context;
    if (!event) return;
    /* Only handle home while it is top window */
    if (!s_home_window || window_stack_get_top_window() != s_home_window) {
        return;
    }

    switch (event->type) {
        case TouchEvent_Touchdown: {
            s_touch_active = true;
            s_touch_down_x = event->x;
            s_touch_down_y = event->y;
            s_touch_down_btn = hit_test_button(event->x, event->y);
            s_press_btn = s_touch_down_btn;
            if (s_press_btn >= 0) s_focus_btn = s_press_btn;
            if (s_home_canvas) layer_mark_dirty(s_home_canvas);
            break;
        }
        case TouchEvent_PositionUpdate: {
            if (!s_touch_active) break;
            int btn = hit_test_button(event->x, event->y);
            /* cancel press highlight if finger leaves original button */
            if (btn != s_touch_down_btn) {
                s_press_btn = -1;
            } else {
                s_press_btn = btn;
            }
            if (s_home_canvas) layer_mark_dirty(s_home_canvas);
            break;
        }
        case TouchEvent_Liftoff: {
            if (!s_touch_active) break;
            s_touch_active = false;
            int dx = event->x - s_touch_down_x;
            int dy = event->y - s_touch_down_y;
            if (dx < 0) dx = -dx;
            if (dy < 0) dy = -dy;
            int up_btn = hit_test_button(event->x, event->y);
            bool is_tap = (dx <= TAP_SLOP_PX && dy <= TAP_SLOP_PX &&
                           s_touch_down_btn >= 0 && up_btn == s_touch_down_btn);
            s_press_btn = -1;
            if (s_home_canvas) layer_mark_dirty(s_home_canvas);
            if (is_tap) {
                activate_button(s_touch_down_btn);
            }
            s_touch_down_btn = -1;
            break;
        }
        default:
            break;
    }
}
#endif

static void home_touch_subscribe_if_needed(void) {
#if defined(PBL_TOUCH)
    if (s_touch_subscribed) return;
    if (!touch_service_is_enabled()) {
        /* Still usable with side buttons */
        return;
    }
    touch_service_subscribe(home_touch_handler, NULL);
    s_touch_subscribed = true;
#endif
}

static void home_touch_unsubscribe_if_needed(void) {
#if defined(PBL_TOUCH)
    if (!s_touch_subscribed) return;
    touch_service_unsubscribe();
    s_touch_subscribed = false;
#endif
}

static void home_appear(Window *window) {
    (void)window;
    home_touch_subscribe_if_needed();
    s_press_btn = -1;
    if (s_home_canvas) layer_mark_dirty(s_home_canvas);
}

static void home_disappear(Window *window) {
    (void)window;
    home_touch_unsubscribe_if_needed();
    s_touch_active = false;
    s_press_btn = -1;
}

static void home_window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    GRect b = layer_get_bounds(root);
    const int w = b.size.w;
    const int h = b.size.h;
    const int status_h = 40;

    home_layout_buttons(b);
    s_focus_btn = BTN_BOLUS;
    s_press_btn = -1;
    s_touch_down_btn = -1;

    s_home_canvas = layer_create(GRect(0, 0, w, h - status_h));
    layer_set_update_proc(s_home_canvas, home_canvas_update);
    layer_add_child(root, s_home_canvas);

    s_home_status = text_layer_create(GRect(4, h - status_h, w - 8, status_h - 2));
    text_layer_set_background_color(s_home_status, GColorClear);
    text_layer_set_text_alignment(s_home_status, GTextAlignmentCenter);
    text_layer_set_font(s_home_status, font_status());
    if (s_last_status[0]) {
        snprintf(s_status_buf, sizeof(s_status_buf), "%s", s_last_status);
    } else {
#if defined(PBL_TOUCH)
        snprintf(s_status_buf, sizeof(s_status_buf), "Tap BOLUS or CARBS");
#else
        snprintf(s_status_buf, sizeof(s_status_buf), "UP/DOWN + SELECT");
#endif
    }
    text_layer_set_text(s_home_status, s_status_buf);
    layer_add_child(root, text_layer_get_layer(s_home_status));

    /* Buttons always available; touch is additive on capable hardware */
    window_set_click_config_provider(window, home_click_config);
}

static void home_window_unload(Window *window) {
    (void)window;
    home_touch_unsubscribe_if_needed();
    layer_destroy(s_home_canvas);
    s_home_canvas = NULL;
    text_layer_destroy(s_home_status);
    s_home_status = NULL;
}

void remote_menu_init(void) {
    prefs_load_from_persist();
    s_home_window = window_create();
    window_set_window_handlers(s_home_window, (WindowHandlers){
        .load = home_window_load,
        .unload = home_window_unload,
        .appear = home_appear,
        .disappear = home_disappear,
    });
    window_stack_push(s_home_window, true);
}

void remote_menu_deinit(void) {
    home_touch_unsubscribe_if_needed();
    if (s_confirm_window) {
        window_destroy(s_confirm_window);
        s_confirm_window = NULL;
    }
    if (s_pick_window) {
        window_destroy(s_pick_window);
        s_pick_window = NULL;
    }
    if (s_home_window) {
        window_destroy(s_home_window);
        s_home_window = NULL;
    }
}
