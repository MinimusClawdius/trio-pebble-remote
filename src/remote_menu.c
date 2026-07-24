#include "remote_menu.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "message_keys.auto.h"
#include "send_anim.h"

#ifndef MESSAGE_KEY_KEY_CMD_TYPE
enum {
    KEY_CMD_TYPE = 7,
    KEY_CMD_AMOUNT = 8,
    KEY_CMD_STATUS = 9,
    KEY_CMD_RESULT = 54,
};
#define MESSAGE_KEY_KEY_CMD_TYPE KEY_CMD_TYPE
#define MESSAGE_KEY_KEY_CMD_AMOUNT KEY_CMD_AMOUNT
#define MESSAGE_KEY_KEY_CMD_STATUS KEY_CMD_STATUS
#define MESSAGE_KEY_KEY_CMD_RESULT KEY_CMD_RESULT
#endif

#ifndef MESSAGE_KEY_KEY_CMD_RESULT
#define MESSAGE_KEY_KEY_CMD_RESULT 54
#endif

enum {
    HOME_BOLUS = 0,
    HOME_CARBS = 1,
    HOME_COUNT = 2
};

enum {
    PICK_MINUS = 0,
    PICK_PLUS = 1,
    PICK_SEND = 2
};

enum {
    PROGRESS_SENDING = 0,
    PROGRESS_ERROR = 1
};

static Window *s_home_window;
static Window *s_pick_window;
static Window *s_progress_window;

static Layer *s_home_canvas;
static int s_focus_home;
static int s_press_home;
static GRect s_home_btn[HOME_COUNT];

static Layer *s_pick_canvas;
static int32_t s_pick_cmd_type;
static int32_t s_pick_amount;
static int s_press_pick;
static GRect s_pick_minus;
static GRect s_pick_plus;
static GRect s_pick_send;
static GRect s_pick_value_box;
static char s_num_buf[16];
static char s_unit_buf[8];

/* Progress / result screen */
static Layer *s_progress_canvas;
static int s_progress_state; /* PROGRESS_* */
static char s_progress_msg[72];
static char s_progress_detail[40];
static AppTimer *s_spin_timer;
static AppTimer *s_min_anim_timer;
static AppTimer *s_timeout_timer;
static int s_spin_frame;
static bool s_awaiting_result;
static bool s_anim_min_elapsed;
static int s_pending_result; /* 0=none, 1=ok, 2=fail */
static char s_pending_msg[72];

static bool s_touch_active;
static int16_t s_touch_down_x;
static int16_t s_touch_down_y;
static int s_touch_down_id;
static bool s_touch_subscribed;
static enum { TOUCH_CTX_NONE, TOUCH_CTX_HOME, TOUCH_CTX_PICK } s_touch_ctx;

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

#define TAP_SLOP_PX 22
/** Always show send animation at least this long on success (and before error UI). */
#define ANIM_MIN_MS 5000
/** If Trio never answers, keep anim this long then show error (5–10s range). */
#define SEND_TIMEOUT_MS 10000
#define SPIN_MS 180

static void open_amount_picker(int32_t cmd_type);
static void pick_adjust(int direction);
static void progress_show_sending(void);
static void progress_show_error(const char *msg);
static void progress_close_app(void);
static void progress_stop_timers(void);
static void progress_try_finalize(void);
static void progress_apply_pending(void);

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

static GFont font_home_label(void) {
    return fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
}

static GFont font_amount_number(void) {
    return fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
}

static GFont font_amount_unit(void) {
    return fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
}

static GFont font_pm(void) {
    return fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
}

static GFont font_send(void) {
    return fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
}

static GFont font_title(void) {
    return fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
}

static GFont font_progress(void) {
    return fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
}

static GFont font_error_msg(void) {
    return fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
}

static void format_amount_parts(void) {
    if (s_pick_cmd_type == 1) {
        int v = (int)s_pick_amount;
        snprintf(s_num_buf, sizeof(s_num_buf), "%d.%d", v / 10, v % 10);
        snprintf(s_unit_buf, sizeof(s_unit_buf), "units");
    } else {
        snprintf(s_num_buf, sizeof(s_num_buf), "%ld", (long)s_pick_amount);
        snprintf(s_unit_buf, sizeof(s_unit_buf), "grams");
    }
}

/* ---------- Result from phone (pkjs) ---------- */

void remote_menu_set_status(const char *status) {
    /* Legacy path: plain status text while awaiting → treat as intermediate only. */
    if (!s_awaiting_result) {
        return;
    }
    if (!status) return;
    /* If JS only sent status without CMD_RESULT, classify heuristically. */
    if (strstr(status, "delivered") || strstr(status, "Bolus OK") || strstr(status, "Carbs OK")) {
        remote_menu_handle_result(1, status);
    } else if (strstr(status, "unreachable") || strstr(status, "timeout") ||
               strstr(status, "Timed out") || strstr(status, "fail") || strstr(status, "error") ||
               strstr(status, "HTTP") || strstr(status, "Error") || strstr(status, "busy") ||
               strstr(status, "Failed")) {
        remote_menu_handle_result(2, status);
    }
}

void remote_menu_handle_result(int result, const char *message) {
    /* Only accept results while sending animation is active (or hard errors). */
    if (s_progress_state != PROGRESS_SENDING && !s_awaiting_result) {
        if (result != 2) return;
    }

    /* Queue result — do not tear down animation early. */
    s_pending_result = (result == 1) ? 1 : 2;
    if (message && message[0]) {
        strncpy(s_pending_msg, message, sizeof(s_pending_msg) - 1);
        s_pending_msg[sizeof(s_pending_msg) - 1] = '\0';
    } else {
        snprintf(s_pending_msg, sizeof(s_pending_msg), "%s",
                 (result == 1) ? "Delivered" : "Send failed");
    }
    s_awaiting_result = false; /* network phase done; may still be in min-anim hold */
    progress_try_finalize();
}

/* ---------- Progress window ---------- */

static void progress_stop_timers(void) {
    if (s_spin_timer) {
        app_timer_cancel(s_spin_timer);
        s_spin_timer = NULL;
    }
    if (s_min_anim_timer) {
        app_timer_cancel(s_min_anim_timer);
        s_min_anim_timer = NULL;
    }
    if (s_timeout_timer) {
        app_timer_cancel(s_timeout_timer);
        s_timeout_timer = NULL;
    }
}

static void progress_close_app(void) {
    progress_stop_timers();
    s_pending_result = 0;
    s_awaiting_result = false;
    window_stack_pop_all(true);
}

static void progress_apply_pending(void) {
    if (s_pending_result == 1) {
        vibes_short_pulse();
        progress_close_app();
        return;
    }
    if (s_pending_result == 2) {
        vibes_double_pulse();
        /* Stop spin; keep window and show sticky error. */
        if (s_spin_timer) {
            app_timer_cancel(s_spin_timer);
            s_spin_timer = NULL;
        }
        if (s_timeout_timer) {
            app_timer_cancel(s_timeout_timer);
            s_timeout_timer = NULL;
        }
        if (s_min_anim_timer) {
            app_timer_cancel(s_min_anim_timer);
            s_min_anim_timer = NULL;
        }
        progress_show_error(s_pending_msg[0] ? s_pending_msg : "Send failed");
        s_pending_result = 0;
    }
}

static void progress_try_finalize(void) {
    if (s_progress_state != PROGRESS_SENDING) return;
    if (!s_anim_min_elapsed) {
        /* Keep anim playing until minimum duration. */
        return;
    }
    if (s_pending_result == 0) {
        /* Min elapsed but still waiting on network — keep spinning until timeout. */
        return;
    }
    progress_apply_pending();
}

static void spin_timer_cb(void *data) {
    (void)data;
    s_spin_timer = NULL;
    if (s_progress_state != PROGRESS_SENDING) return;
    s_spin_frame = (s_spin_frame + 1) % SEND_ANIM_FRAMES;
    if (s_progress_canvas) layer_mark_dirty(s_progress_canvas);
    s_spin_timer = app_timer_register(SPIN_MS, spin_timer_cb, NULL);
}

static void min_anim_timer_cb(void *data) {
    (void)data;
    s_min_anim_timer = NULL;
    s_anim_min_elapsed = true;
    progress_try_finalize();
}

static void timeout_timer_cb(void *data) {
    (void)data;
    s_timeout_timer = NULL;
    if (s_progress_state != PROGRESS_SENDING) return;

    /* Force failure if still no result after full wait window. */
    if (s_pending_result == 0) {
        s_pending_result = 2;
        snprintf(s_pending_msg, sizeof(s_pending_msg), "Timed out waiting for Trio");
        s_awaiting_result = false;
    }
    /* Ensure min elapsed so error can show (timeout is >= min). */
    s_anim_min_elapsed = true;
    progress_try_finalize();
}

static void progress_canvas_update(Layer *layer, GContext *ctx) {
    GRect b = layer_get_bounds(layer);
    /* Light background / dark foreground throughout */
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, b, 0, GCornerNone);

    int cx = b.size.w / 2;
    int anim_top = 18;
    int anim_h = b.size.h / 2 + 10;
    GRect anim_box = GRect(8, anim_top, b.size.w - 16, anim_h);

    if (s_progress_state == PROGRESS_SENDING) {
        /* Frame sequence: syringe drip (bolus) or pizza face (carbs) */
        if (s_pick_cmd_type == 1) {
            send_anim_draw_syringe(ctx, anim_box, s_spin_frame);
        } else {
            send_anim_draw_pizza_face(ctx, anim_box, s_spin_frame);
        }

        graphics_context_set_text_color(ctx, GColorBlack);
        graphics_draw_text(ctx, "Sending to Trio…", font_progress(),
                           GRect(4, anim_top + anim_h + 4, b.size.w - 8, 30),
                           GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
        if (s_progress_detail[0]) {
            graphics_context_set_text_color(ctx, GColorDarkGray);
            graphics_draw_text(ctx, s_progress_detail, font_title(),
                               GRect(4, anim_top + anim_h + 34, b.size.w - 8, 28),
                               GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
        }
    } else {
        /* Error on light bg */
#ifdef PBL_COLOR
        graphics_context_set_fill_color(ctx, GColorRed);
#else
        graphics_context_set_fill_color(ctx, GColorBlack);
#endif
        GRect badge = GRect(cx - 30, 28, 60, 60);
        graphics_fill_radial(ctx, badge, GOvalScaleModeFitCircle, 32, 0, TRIG_MAX_ANGLE);

        graphics_context_set_stroke_color(ctx, GColorWhite);
        graphics_context_set_stroke_width(ctx, 5);
        graphics_draw_line(ctx, GPoint(cx - 14, 44), GPoint(cx + 14, 72));
        graphics_draw_line(ctx, GPoint(cx + 14, 44), GPoint(cx - 14, 72));

        graphics_context_set_text_color(ctx, GColorBlack);
        graphics_draw_text(ctx, "Send failed", font_progress(),
                           GRect(4, 100, b.size.w - 8, 30),
                           GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
        graphics_context_set_text_color(ctx, GColorDarkGray);
        graphics_draw_text(ctx, s_progress_msg, font_error_msg(),
                           GRect(8, 132, b.size.w - 16, 56),
                           GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
        graphics_context_set_text_color(ctx, GColorBlack);
        graphics_draw_text(ctx, "BACK to exit", font_title(),
                           GRect(4, b.size.h - 36, b.size.w - 8, 28),
                           GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    }
}

static void progress_back(ClickRecognizerRef r, void *c) {
    (void)r;
    (void)c;
    if (s_progress_state == PROGRESS_ERROR) {
        progress_close_app();
        return;
    }
    /* While sending: cancel wait and exit */
    if (s_progress_state == PROGRESS_SENDING) {
        s_awaiting_result = false;
        s_pending_result = 0;
        progress_close_app();
    }
}

static void progress_click_config(void *context) {
    (void)context;
    window_single_click_subscribe(BUTTON_ID_BACK, progress_back);
    window_single_click_subscribe(BUTTON_ID_SELECT, progress_back);
}

static void progress_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    GRect b = layer_get_bounds(root);
    window_set_background_color(window, GColorWhite);
    s_progress_canvas = layer_create(b);
    layer_set_update_proc(s_progress_canvas, progress_canvas_update);
    layer_add_child(root, s_progress_canvas);
    window_set_click_config_provider(window, progress_click_config);
}

static void progress_unload(Window *window) {
    (void)window;
    progress_stop_timers();
    layer_destroy(s_progress_canvas);
    s_progress_canvas = NULL;
}

static void progress_ensure_window(void) {
    if (!s_progress_window) {
        s_progress_window = window_create();
        window_set_window_handlers(s_progress_window, (WindowHandlers){
            .load = progress_load,
            .unload = progress_unload,
        });
    }
}

static void progress_show_sending(void) {
    progress_ensure_window();
    s_progress_state = PROGRESS_SENDING;
    s_spin_frame = 0;
    s_anim_min_elapsed = false;
    s_pending_result = 0;
    s_pending_msg[0] = '\0';
    format_amount_parts();
    snprintf(s_progress_detail, sizeof(s_progress_detail), "%s %s", s_num_buf, s_unit_buf);
    s_progress_msg[0] = '\0';

    /* Replace picker with progress (keep home under stack for clean exit). */
    if (s_pick_window && window_stack_contains_window(s_pick_window)) {
        window_stack_remove(s_pick_window, false);
    }
    if (!window_stack_contains_window(s_progress_window)) {
        window_stack_push(s_progress_window, true);
    } else if (s_progress_canvas) {
        layer_mark_dirty(s_progress_canvas);
    }

    progress_stop_timers();
    s_spin_timer = app_timer_register(SPIN_MS, spin_timer_cb, NULL);
    /* Hold animation at least ANIM_MIN_MS so success doesn't flash-exit. */
    s_min_anim_timer = app_timer_register(ANIM_MIN_MS, min_anim_timer_cb, NULL);
    /* No response by SEND_TIMEOUT_MS → error after full anim window. */
    s_timeout_timer = app_timer_register(SEND_TIMEOUT_MS, timeout_timer_cb, NULL);
}

static void progress_show_error(const char *msg) {
    progress_ensure_window();
    s_progress_state = PROGRESS_ERROR;
    s_awaiting_result = false;
    strncpy(s_progress_msg, msg ? msg : "Failed", sizeof(s_progress_msg) - 1);
    s_progress_msg[sizeof(s_progress_msg) - 1] = '\0';

    if (!window_stack_contains_window(s_progress_window)) {
        window_stack_push(s_progress_window, true);
    }
    if (s_progress_canvas) layer_mark_dirty(s_progress_canvas);
}

/* ---------- Send ---------- */

static void send_watch_command(void) {
    if (s_pick_amount <= 0) {
        vibes_double_pulse();
        progress_show_error("Amount required");
        return;
    }

    DictionaryIterator *iter;
    AppMessageResult begin = app_message_outbox_begin(&iter);
    if (begin != APP_MSG_OK) {
        vibes_double_pulse();
        progress_show_error("Phone link busy");
        return;
    }

    dict_write_int32(iter, MESSAGE_KEY_KEY_CMD_TYPE, s_pick_cmd_type);
    dict_write_int32(iter, MESSAGE_KEY_KEY_CMD_AMOUNT, s_pick_amount);
    dict_write_end(iter);

    AppMessageResult sent = app_message_outbox_send();
    if (sent != APP_MSG_OK) {
        vibes_double_pulse();
        progress_show_error("Send failed");
        return;
    }

    s_awaiting_result = true;
    vibes_short_pulse();
    progress_show_sending();
}

static void pick_adjust(int direction) {
    if (s_pick_cmd_type == 1) {
        int step = (int)s_bolus_step_tenths;
        if (direction > 0) {
            if (s_pick_amount + step <= 300) s_pick_amount += step;
            else s_pick_amount = 300;
        } else {
            if (s_pick_amount > step) s_pick_amount -= step;
            else s_pick_amount = step;
        }
    } else {
        int step = (int)s_carb_step_grams;
        if (direction > 0) {
            if (s_pick_amount + step <= 250) s_pick_amount += step;
            else s_pick_amount = 250;
        } else {
            if (s_pick_amount > step) s_pick_amount -= step;
            else s_pick_amount = step;
        }
    }
    if (s_pick_canvas) layer_mark_dirty(s_pick_canvas);
}

static int home_hit(int16_t x, int16_t y) {
    GPoint p = GPoint(x, y);
    for (int i = 0; i < HOME_COUNT; i++) {
        if (grect_contains_point(&s_home_btn[i], &p)) return i;
    }
    return -1;
}

static int pick_hit(int16_t x, int16_t y) {
    GPoint p = GPoint(x, y);
    if (grect_contains_point(&s_pick_minus, &p)) return PICK_MINUS;
    if (grect_contains_point(&s_pick_plus, &p)) return PICK_PLUS;
    if (grect_contains_point(&s_pick_send, &p)) return PICK_SEND;
    return -1;
}

static void activate_home(int btn) {
    if (btn == HOME_BOLUS) open_amount_picker(1);
    else if (btn == HOME_CARBS) open_amount_picker(2);
}

/* ---------- Picker ---------- */

static void pick_layout(GRect b) {
    const int w = b.size.w;
    const int h = b.size.h;
    const int pad = 12;
    const int send_h = 54;
    const int circle = (w < 180) ? 52 : 60;

    s_pick_send = GRect(pad, h - send_h - pad, w - 2 * pad, send_h);
    int pm_y = h - send_h - pad - circle - 16;
    s_pick_minus = GRect(pad + 8, pm_y, circle, circle);
    s_pick_plus = GRect(w - pad - 8 - circle, pm_y, circle, circle);

    int value_bottom = pm_y - 8;
    int value_top = 36;
    int value_h = value_bottom - value_top;
    if (value_h < 56) value_h = 56;
    s_pick_value_box = GRect(pad, value_top, w - 2 * pad, value_h);
}

static void draw_circle_button(GContext *ctx, GRect r, const char *label, bool pressed) {
    /* Light ring buttons, dark labels */
#ifdef PBL_COLOR
    GColor fill = pressed ? GColorMintGreen : GColorWhite;
#else
    GColor fill = pressed ? GColorLightGray : GColorWhite;
#endif
    graphics_context_set_fill_color(ctx, fill);
    graphics_fill_radial(ctx, r, GOvalScaleModeFitCircle, (uint16_t)(r.size.w / 2 + 1), 0, TRIG_MAX_ANGLE);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_stroke_width(ctx, pressed ? 4 : 3);
    graphics_draw_arc(ctx, r, GOvalScaleModeFitCircle, 0, TRIG_MAX_ANGLE);
    graphics_context_set_text_color(ctx, GColorBlack);
    GRect tb = grect_inset(r, GEdgeInsets(r.size.h / 2 - 18, 4, 4, 4));
    graphics_draw_text(ctx, label, font_pm(), tb, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void pick_canvas_update(Layer *layer, GContext *ctx) {
    GRect b = layer_get_bounds(layer);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, b, 0, GCornerNone);

    graphics_context_set_text_color(ctx, GColorBlack);
    const char *title = (s_pick_cmd_type == 1) ? "Bolus" : "Carbs";
    graphics_draw_text(ctx, title, font_title(), GRect(0, 4, b.size.w, 28),
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

    format_amount_parts();
#ifdef PBL_COLOR
    graphics_context_set_text_color(ctx, GColorDarkGreen);
#else
    graphics_context_set_text_color(ctx, GColorBlack);
#endif
    GRect num_box = s_pick_value_box;
    num_box.size.h = (s_pick_value_box.size.h * 2) / 3;
    GRect unit_box = GRect(s_pick_value_box.origin.x,
                           s_pick_value_box.origin.y + num_box.size.h - 4,
                           s_pick_value_box.size.w,
                           s_pick_value_box.size.h - num_box.size.h + 4);
    graphics_draw_text(ctx, s_num_buf, font_amount_number(), num_box,
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    graphics_context_set_text_color(ctx, GColorDarkGray);
    graphics_draw_text(ctx, s_unit_buf, font_amount_unit(), unit_box,
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

    draw_circle_button(ctx, s_pick_minus, "-", s_press_pick == PICK_MINUS);
    draw_circle_button(ctx, s_pick_plus, "+", s_press_pick == PICK_PLUS);

    bool send_p = (s_press_pick == PICK_SEND);
#ifdef PBL_COLOR
    graphics_context_set_fill_color(ctx, send_p ? GColorGreen : GColorMintGreen);
#else
    graphics_context_set_fill_color(ctx, send_p ? GColorLightGray : GColorWhite);
#endif
    graphics_fill_rect(ctx, s_pick_send, s_pick_send.size.h / 2, GCornersAll);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_round_rect(ctx, s_pick_send, s_pick_send.size.h / 2);
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, "Send", font_send(),
                       grect_inset(s_pick_send, GEdgeInsets(s_pick_send.size.h / 2 - 16, 4, 4, 4)),
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void pick_up(ClickRecognizerRef r, void *c) {
    (void)r;
    (void)c;
    pick_adjust(+1);
}
static void pick_down(ClickRecognizerRef r, void *c) {
    (void)r;
    (void)c;
    pick_adjust(-1);
}
static void pick_select(ClickRecognizerRef r, void *c) {
    (void)r;
    (void)c;
    send_watch_command();
}
static void pick_back(ClickRecognizerRef r, void *c) {
    (void)r;
    (void)c;
    window_stack_pop(true);
}

static void pick_click_config(void *context) {
    (void)context;
    window_single_click_subscribe(BUTTON_ID_UP, pick_up);
    window_single_click_subscribe(BUTTON_ID_DOWN, pick_down);
    window_single_click_subscribe(BUTTON_ID_SELECT, pick_select);
    window_single_click_subscribe(BUTTON_ID_BACK, pick_back);
    window_single_repeating_click_subscribe(BUTTON_ID_UP, 80, pick_up);
    window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 80, pick_down);
}

static void pick_activate(int id) {
    if (id == PICK_MINUS) pick_adjust(-1);
    else if (id == PICK_PLUS) pick_adjust(+1);
    else if (id == PICK_SEND) send_watch_command();
}

#if defined(PBL_TOUCH)
static void global_touch_handler(const TouchEvent *event, void *context) {
    (void)context;
    if (!event) return;
    Window *top = window_stack_get_top_window();

    if (s_touch_ctx == TOUCH_CTX_HOME && top == s_home_window) {
        switch (event->type) {
            case TouchEvent_Touchdown:
                s_touch_active = true;
                s_touch_down_x = event->x;
                s_touch_down_y = event->y;
                s_touch_down_id = home_hit(event->x, event->y);
                s_press_home = s_touch_down_id;
                if (s_press_home >= 0) s_focus_home = s_press_home;
                if (s_home_canvas) layer_mark_dirty(s_home_canvas);
                break;
            case TouchEvent_PositionUpdate:
                if (!s_touch_active) break;
                if (home_hit(event->x, event->y) != s_touch_down_id) s_press_home = -1;
                else s_press_home = s_touch_down_id;
                if (s_home_canvas) layer_mark_dirty(s_home_canvas);
                break;
            case TouchEvent_Liftoff: {
                if (!s_touch_active) break;
                s_touch_active = false;
                int dx = event->x - s_touch_down_x;
                int dy = event->y - s_touch_down_y;
                if (dx < 0) dx = -dx;
                if (dy < 0) dy = -dy;
                int up = home_hit(event->x, event->y);
                bool tap = (dx <= TAP_SLOP_PX && dy <= TAP_SLOP_PX && s_touch_down_id >= 0 && up == s_touch_down_id);
                s_press_home = -1;
                if (s_home_canvas) layer_mark_dirty(s_home_canvas);
                if (tap) activate_home(s_touch_down_id);
                s_touch_down_id = -1;
                break;
            }
            default:
                break;
        }
        return;
    }

    if (s_touch_ctx == TOUCH_CTX_PICK && top == s_pick_window) {
        switch (event->type) {
            case TouchEvent_Touchdown:
                s_touch_active = true;
                s_touch_down_x = event->x;
                s_touch_down_y = event->y;
                s_touch_down_id = pick_hit(event->x, event->y);
                s_press_pick = s_touch_down_id;
                if (s_pick_canvas) layer_mark_dirty(s_pick_canvas);
                break;
            case TouchEvent_PositionUpdate:
                if (!s_touch_active) break;
                if (pick_hit(event->x, event->y) != s_touch_down_id) s_press_pick = -1;
                else s_press_pick = s_touch_down_id;
                if (s_pick_canvas) layer_mark_dirty(s_pick_canvas);
                break;
            case TouchEvent_Liftoff: {
                if (!s_touch_active) break;
                s_touch_active = false;
                int dx = event->x - s_touch_down_x;
                int dy = event->y - s_touch_down_y;
                if (dx < 0) dx = -dx;
                if (dy < 0) dy = -dy;
                int up = pick_hit(event->x, event->y);
                bool tap = (dx <= TAP_SLOP_PX && dy <= TAP_SLOP_PX && s_touch_down_id >= 0 && up == s_touch_down_id);
                int id = s_touch_down_id;
                s_press_pick = -1;
                s_touch_down_id = -1;
                if (s_pick_canvas) layer_mark_dirty(s_pick_canvas);
                if (tap) pick_activate(id);
                break;
            }
            default:
                break;
        }
    }
}
#endif

static void touch_resubscribe(int ctx) {
#if defined(PBL_TOUCH)
    s_touch_ctx = ctx;
    s_touch_active = false;
    s_press_home = -1;
    s_press_pick = -1;
    if (!touch_service_is_enabled()) {
        s_touch_subscribed = false;
        return;
    }
    if (!s_touch_subscribed) {
        touch_service_subscribe(global_touch_handler, NULL);
        s_touch_subscribed = true;
    }
#else
    (void)ctx;
#endif
}

static void touch_stop(void) {
#if defined(PBL_TOUCH)
    s_touch_ctx = TOUCH_CTX_NONE;
    s_touch_active = false;
    if (s_touch_subscribed) {
        touch_service_unsubscribe();
        s_touch_subscribed = false;
    }
#endif
}

static void pick_appear(Window *window) {
    (void)window;
    touch_resubscribe(TOUCH_CTX_PICK);
}
static void pick_disappear(Window *window) {
    (void)window;
}

static void pick_window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    GRect b = layer_get_bounds(root);
    pick_layout(b);
    s_press_pick = -1;
    s_pick_canvas = layer_create(b);
    layer_set_update_proc(s_pick_canvas, pick_canvas_update);
    layer_add_child(root, s_pick_canvas);
    window_set_click_config_provider(window, pick_click_config);
    window_set_background_color(window, GColorWhite);
}

static void pick_window_unload(Window *window) {
    (void)window;
    layer_destroy(s_pick_canvas);
    s_pick_canvas = NULL;
}

static void open_amount_picker(int32_t cmd_type) {
    s_pick_cmd_type = cmd_type;
    s_pick_amount = (cmd_type == 1) ? s_bolus_default_tenths : s_carb_default_grams;
    if (!s_pick_window) {
        s_pick_window = window_create();
        window_set_window_handlers(s_pick_window, (WindowHandlers){
            .load = pick_window_load,
            .unload = pick_window_unload,
            .appear = pick_appear,
            .disappear = pick_disappear,
        });
    }
    window_stack_push(s_pick_window, true);
}

/* ---------- Home (no status footer) ---------- */

static void home_layout(GRect bounds) {
    const int pad = 8;
    const int gap = 12;
    int usable = bounds.size.h - pad * 2 - gap;
    int btn_h = usable / 2;
    int w = bounds.size.w - pad * 2;
    s_home_btn[HOME_BOLUS] = GRect(pad, pad, w, btn_h);
    s_home_btn[HOME_CARBS] = GRect(pad, pad + btn_h + gap, w, btn_h);
}

static void home_canvas_update(Layer *layer, GContext *ctx) {
    GRect b = layer_get_bounds(layer);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, b, 0, GCornerNone);

    for (int i = 0; i < HOME_COUNT; i++) {
        GRect f = s_home_btn[i];
        bool pressed = (s_press_home == i);
        bool focused = (s_focus_home == i);
        /* Light mint pills, dark text */
#ifdef PBL_COLOR
        GColor fill = pressed ? GColorMintGreen : GColorMintGreen;
        if (pressed) fill = GColorGreen;
        GColor ink = GColorBlack;
#else
        GColor fill = pressed ? GColorLightGray : GColorWhite;
        GColor ink = GColorBlack;
#endif
        graphics_context_set_fill_color(ctx, fill);
        graphics_fill_rect(ctx, f, 14, GCornersAll);
        graphics_context_set_stroke_color(ctx, GColorBlack);
        graphics_context_set_stroke_width(ctx, focused || pressed ? 4 : 2);
        graphics_draw_round_rect(ctx, f, 14);
        const char *label = (i == HOME_BOLUS) ? "Bolus" : "Carbs";
        graphics_context_set_text_color(ctx, ink);
        int text_h = 56;
        if (text_h > f.size.h - 16) text_h = f.size.h - 16;
        if (text_h < 42) text_h = f.size.h - 8;
        int text_y = f.origin.y + (f.size.h - text_h) / 2 + 2;
        GRect text_box = GRect(f.origin.x + 4, text_y, f.size.w - 8, text_h);
        graphics_draw_text(ctx, label, font_home_label(), text_box,
                           GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    }
}

static void home_up(ClickRecognizerRef r, void *c) {
    (void)r;
    (void)c;
    s_focus_home = HOME_BOLUS;
    if (s_home_canvas) layer_mark_dirty(s_home_canvas);
}
static void home_down(ClickRecognizerRef r, void *c) {
    (void)r;
    (void)c;
    s_focus_home = HOME_CARBS;
    if (s_home_canvas) layer_mark_dirty(s_home_canvas);
}
static void home_select(ClickRecognizerRef r, void *c) {
    (void)r;
    (void)c;
    activate_home(s_focus_home);
}
static void home_back(ClickRecognizerRef r, void *c) {
    (void)r;
    (void)c;
    window_stack_pop(true);
}

static void home_click_config(void *context) {
    (void)context;
    window_single_click_subscribe(BUTTON_ID_UP, home_up);
    window_single_click_subscribe(BUTTON_ID_DOWN, home_down);
    window_single_click_subscribe(BUTTON_ID_SELECT, home_select);
    window_single_click_subscribe(BUTTON_ID_BACK, home_back);
}

static void home_appear(Window *window) {
    (void)window;
    touch_resubscribe(TOUCH_CTX_HOME);
    s_press_home = -1;
    if (s_home_canvas) layer_mark_dirty(s_home_canvas);
}
static void home_disappear(Window *window) {
    (void)window;
}

static void home_window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    GRect b = layer_get_bounds(root);
    home_layout(b);
    s_focus_home = HOME_BOLUS;
    s_press_home = -1;
    window_set_background_color(window, GColorWhite);
    s_home_canvas = layer_create(b);
    layer_set_update_proc(s_home_canvas, home_canvas_update);
    layer_add_child(root, s_home_canvas);
    window_set_click_config_provider(window, home_click_config);
}

static void home_window_unload(Window *window) {
    (void)window;
    touch_stop();
    layer_destroy(s_home_canvas);
    s_home_canvas = NULL;
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
    progress_stop_timers();
    touch_stop();
    if (s_progress_window) {
        window_destroy(s_progress_window);
        s_progress_window = NULL;
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
