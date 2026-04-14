#include "remote_send_ui.h"
/* ../ = src/ or src/c/ (CloudPebble) — not same dir as this file in modules/ */
#include "../remote_menu.h"
#include <stdio.h>
#include <string.h>

/* tick/cross icons from pebble-examples/ui-patterns. Bolus/carb confirm: optional PDCS on color
 * watches (basalt/chalk/emery), else procedural art — see docs/CONFIRM_PDC.md.
 * Layout uses safe insets on round (chalk) and letterboxes on tall/wide rectangular (emery). */

/* Progress animation (see Pebble progress_layer example). */
#define CONFIRM_ANIM_TICK_MS 105
#define PROGRESS_TICK_MS 55
#define PROGRESS_IDLE_CAP 92
#define PROGRESS_TIMEOUT_MS 14000
#define TOAST_DISPLAY_MS 1100

/* Color watches only: load PDCS from resources (see docs/CONFIRM_PDC.md). Set to 0 to force procedural. */
#ifndef TRIO_REMOTE_CONFIRM_PDC_ASSETS
#define TRIO_REMOTE_CONFIRM_PDC_ASSETS 1
#endif
#if defined(PBL_COLOR) && TRIO_REMOTE_CONFIRM_PDC_ASSETS
#define TRIO_REMOTE_CONFIRM_PDC_ENABLED 1
#else
#define TRIO_REMOTE_CONFIRM_PDC_ENABLED 0
#endif

static int32_t s_pending_cmd_type;
static int32_t s_pending_amount;

static Window *s_confirm_window;
static Layer *s_confirm_art_layer;
static TextLayer *s_confirm_title_layer;
static TextLayer *s_confirm_amount_layer;
static TextLayer *s_confirm_hint_layer;
static ActionBarLayer *s_confirm_action_bar;
static GBitmap *s_confirm_tick_bitmap;
static GBitmap *s_confirm_cross_bitmap;
static char s_confirm_title_buf[24];
static char s_confirm_amount_buf[24];
static AppTimer *s_confirm_anim_timer;
static uint8_t s_confirm_anim_frame;

#if TRIO_REMOTE_CONFIRM_PDC_ENABLED
static GDrawCommandSequence *s_confirm_pdc_bolus;
static GDrawCommandSequence *s_confirm_pdc_carb;
#endif

/** Map logical size (80 = “classic art height”) to current panel min(w,h); works on round/emery. */
static int16_t confirm_art_dim(int sm_min, int numer_80) {
    int sm = sm_min;
    if (sm < 40) {
        sm = 40;
    }
    return (int16_t)((numer_80 * sm) / 80);
}

static Window *s_progress_window;
static Layer *s_progress_bg_layer;
static Layer *s_progress_bar_layer;
static TextLayer *s_progress_label;
static AppTimer *s_progress_timer;
static int16_t s_progress_value;
static AppTimer *s_timeout_timer;

static Window *s_toast_window;
static TextLayer *s_toast_line1;
static TextLayer *s_toast_line2;
static AppTimer *s_toast_timer;

static bool s_waiting_phone_status;
/** Pops before progress: 2 = confirm+picker (watchface); 3 = +menu under picker (Trio Remote app). */
static int s_confirm_send_pops = 2;

static void cancel_timeout_timer(void);
static void cancel_progress_timer(void);
static void cancel_toast_timer(void);
static void dismiss_toast(void *data);
static void dismiss_progress_show_toast(const char *line1, const char *line2, bool success_vibe);
static void dismiss_progress_quiet(void);
static void push_toast(const char *line1, const char *line2, bool success_vibe);

/** Older CloudPebble SDKs lack window_stack_remove_window — only pop when this window is on top. */
static void remote_send_close_window_if_top(Window *w) {
    if (!w || !window_stack_contains_window(w)) {
        return;
    }
    if (window_stack_get_top_window() == w) {
        window_stack_pop(true);
    }
}

static void progress_bar_update_proc(Layer *layer, GContext *ctx) {
    (void)layer;
    GRect b = layer_get_bounds(layer);
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, b, 2, GCornersAll);
    int fw = (int)b.size.w * (int)s_progress_value / 100;
    if (fw < 1) {
        return;
    }
    if (fw > b.size.w) {
        fw = b.size.w;
    }
#ifdef PBL_COLOR
    graphics_context_set_fill_color(ctx, GColorJaegerGreen);
#else
    graphics_context_set_fill_color(ctx, GColorWhite);
#endif
    graphics_fill_rect(ctx, GRect(0, 0, fw, b.size.h), 2, GCornersLeft);
}

/** Round watches: keep content inside the visible disc (Pebble Time Round, etc.). */
static GRect remote_send_safe_bounds(const GRect *wnd_bounds) {
#ifdef PBL_ROUND
    return grect_inset(*wnd_bounds, (GEdgeInsets){
        .top = 28, .right = 40, .bottom = 28, .left = 40});
#else
    return *wnd_bounds;
#endif
}

/**
 * Rectangular: letterbox to a 144×168 “classic” layout block (diorite/basalt/aplite) or center on emery.
 * Round: same as safe_bounds only.
 */
static GRect remote_send_layout_block(const GRect *wnd_bounds) {
#ifdef PBL_ROUND
    return remote_send_safe_bounds(wnd_bounds);
#else
    GRect safe = *wnd_bounds;
    const int16_t design_h = 168;
    const int16_t design_w = 144;
    if (safe.size.h > design_h) {
        int16_t pad = (int16_t)((safe.size.h - design_h) / 2);
        safe.origin.y = (int16_t)(safe.origin.y + pad);
        safe.size.h = design_h;
    }
    if (safe.size.w > design_w) {
        int16_t xpad = (int16_t)((safe.size.w - design_w) / 2);
        safe.origin.x = (int16_t)(safe.origin.x + xpad);
        safe.size.w = design_w;
    }
    return safe;
#endif
}

GRect remote_send_content_left_of_action_bar(const GRect *window_bounds) {
    GRect blk = remote_send_layout_block(window_bounds);
    int16_t abw = ACTION_BAR_WIDTH;
    if (blk.size.w <= abw) {
        return blk;
    }
    return (GRect){
        .origin = blk.origin,
        .size = {(int16_t)(blk.size.w - abw), blk.size.h},
    };
}

GRect remote_send_right_action_strip_rect(const GRect *window_bounds) {
    GRect blk = remote_send_layout_block(window_bounds);
    int16_t abw = ACTION_BAR_WIDTH;
    return (GRect){
        .origin = {(int16_t)(blk.origin.x + blk.size.w - abw), blk.origin.y},
        .size = {abw, blk.size.h},
    };
}

static void progress_timer_cb(void *data) {
    (void)data;
    if (!s_waiting_phone_status || !s_progress_bar_layer) {
        return;
    }
    if (s_progress_value < PROGRESS_IDLE_CAP) {
        s_progress_value = (int16_t)(s_progress_value + 5);
        if (s_progress_value > PROGRESS_IDLE_CAP) {
            s_progress_value = PROGRESS_IDLE_CAP;
        }
        layer_mark_dirty(s_progress_bar_layer);
    }
    s_progress_timer = app_timer_register(PROGRESS_TICK_MS, progress_timer_cb, NULL);
}

static void timeout_timer_cb(void *data) {
    (void)data;
    s_timeout_timer = NULL;
    if (!s_waiting_phone_status) {
        return;
    }
    s_waiting_phone_status = false;
    cancel_progress_timer();
    remote_send_close_window_if_top(s_progress_window);
    push_toast("No response", "Check Trio + Rebble", false);
}

static void progress_window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    GRect b = layer_get_bounds(root);
    GRect blk = remote_send_layout_block(&b);

    s_progress_bg_layer = layer_create(b);
    layer_add_child(root, s_progress_bg_layer);
#ifdef PBL_COLOR
    window_set_background_color(window, GColorDarkGray);
#else
    window_set_background_color(window, GColorBlack);
#endif

    int bar_w = blk.size.w - 40;
    if (bar_w < 60) {
        bar_w = 60;
    }
    int16_t bar_x = (int16_t)(blk.origin.x + (blk.size.w - bar_w) / 2);
    int16_t bar_y = (int16_t)(blk.origin.y + (blk.size.h * 88 / 168));
    if (bar_y > blk.origin.y + blk.size.h - 24) {
        bar_y = (int16_t)(blk.origin.y + blk.size.h / 2);
    }
    s_progress_bar_layer = layer_create(GRect(bar_x, bar_y, bar_w, 8));
    layer_set_update_proc(s_progress_bar_layer, progress_bar_update_proc);
    layer_add_child(root, s_progress_bar_layer);

    int16_t lab_y = (int16_t)(blk.origin.y + (blk.size.h * 24 / 168));
    s_progress_label = text_layer_create(GRect(blk.origin.x + 8, lab_y, blk.size.w - 16, 56));
    text_layer_set_background_color(s_progress_label, GColorClear);
    text_layer_set_text_color(s_progress_label, GColorWhite);
    text_layer_set_text_alignment(s_progress_label, GTextAlignmentCenter);
    text_layer_set_font(s_progress_label, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text(s_progress_label, "Sending…");
    layer_add_child(root, text_layer_get_layer(s_progress_label));

    s_progress_value = 0;
    layer_mark_dirty(s_progress_bar_layer);
}

static void progress_window_unload(Window *window) {
    (void)window;
    cancel_progress_timer();
    cancel_timeout_timer();
    text_layer_destroy(s_progress_label);
    s_progress_label = NULL;
    layer_destroy(s_progress_bar_layer);
    s_progress_bar_layer = NULL;
    layer_destroy(s_progress_bg_layer);
    s_progress_bg_layer = NULL;
}

static void begin_progress_and_send(void) {
    if (!s_progress_window) {
        s_progress_window = window_create();
        window_set_window_handlers(s_progress_window, (WindowHandlers){
            .load = progress_window_load,
            .unload = progress_window_unload,
        });
    }
    window_stack_push(s_progress_window, true);
    s_waiting_phone_status = true;
    s_progress_value = 0;
    cancel_progress_timer();
    s_progress_timer = app_timer_register(PROGRESS_TICK_MS, progress_timer_cb, NULL);
    cancel_timeout_timer();
    s_timeout_timer = app_timer_register(PROGRESS_TIMEOUT_MS, timeout_timer_cb, NULL);
    remote_menu_send_to_phone(s_pending_cmd_type, s_pending_amount);
}

static void confirm_cancel_anim_timer(void) {
    if (s_confirm_anim_timer) {
        app_timer_cancel(s_confirm_anim_timer);
        s_confirm_anim_timer = NULL;
    }
}

static void confirm_anim_timer_cb(void *data) {
    (void)data;
    s_confirm_anim_frame++;
    if (s_confirm_art_layer) {
        layer_mark_dirty(s_confirm_art_layer);
    }
    s_confirm_anim_timer = app_timer_register(CONFIRM_ANIM_TICK_MS, confirm_anim_timer_cb, NULL);
}

#if TRIO_REMOTE_CONFIRM_PDC_ENABLED
static void confirm_pdc_unload(void) {
    if (s_confirm_pdc_bolus) {
        gdraw_command_sequence_destroy(s_confirm_pdc_bolus);
        s_confirm_pdc_bolus = NULL;
    }
    if (s_confirm_pdc_carb) {
        gdraw_command_sequence_destroy(s_confirm_pdc_carb);
        s_confirm_pdc_carb = NULL;
    }
}

/**
 * Draw one frame of the Pebble Draw Command sequence when assets are present.
 * Returns false if sequences are missing or empty so the caller can fall back to procedural art.
 */
static bool confirm_pdc_try_draw(GContext *ctx, const GRect *panel) {
    if (!s_confirm_pdc_bolus) {
        s_confirm_pdc_bolus = gdraw_command_sequence_create_with_resource(RESOURCE_ID_CONFIRM_BOLUS_PDCS);
        s_confirm_pdc_carb = gdraw_command_sequence_create_with_resource(RESOURCE_ID_CONFIRM_CARB_PDCS);
    }
    GDrawCommandSequence *seq = (s_pending_cmd_type == 1) ? s_confirm_pdc_bolus : s_confirm_pdc_carb;
    if (!seq) {
        return false;
    }
    uint32_t n = gdraw_command_sequence_get_num_frames(seq);
    if (n == 0) {
        return false;
    }
    uint32_t idx = (uint32_t)s_confirm_anim_frame % n;
    GDrawCommandFrame *fr = gdraw_command_sequence_get_frame_by_index(seq, idx);
    if (!fr) {
        return false;
    }
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, *panel, 0, GCornerNone);

    GSize sz = gdraw_command_sequence_get_bounds_size(seq);
    int16_t ox = panel->origin.x;
    int16_t oy = panel->origin.y;
    if (sz.w > 0 && sz.h > 0) {
        ox = (int16_t)(panel->origin.x + (panel->size.w - sz.w) / 2);
        oy = (int16_t)(panel->origin.y + (panel->size.h - sz.h) / 2);
    }
    gdraw_command_frame_draw(ctx, seq, fr, GPoint(ox, oy));
    return true;
}
#endif

static void confirm_art_update_proc(Layer *layer, GContext *ctx) {
    GRect r = layer_get_bounds(layer);
#if TRIO_REMOTE_CONFIRM_PDC_ENABLED
    if (confirm_pdc_try_draw(ctx, &r)) {
        return;
    }
#endif
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, r, 0, GCornerNone);

    int w = (int)r.size.w;
    int h = (int)r.size.h;
    int sm = w < h ? w : h;
    int cx = w / 2;
    int cy = h / 2;

    graphics_context_set_fill_color(ctx, GColorBlack);

    if (s_pending_cmd_type == 1) {
        /* Bolus: syringe + plunger stroke + droplet (all scaled from min side length). */
        int16_t bh = confirm_art_dim(sm, 14);
        int16_t bw = confirm_art_dim(sm, 40);
        int16_t nd = confirm_art_dim(sm, 4);
        int16_t nl = confirm_art_dim(sm, 18);
        int16_t fl = confirm_art_dim(sm, 9);
        int16_t rad = confirm_art_dim(sm, 3);
        int16_t pr = confirm_art_dim(sm, 3);
        int16_t plen = confirm_art_dim(sm, 14);
        int phase = (int)(s_confirm_anim_frame % 28);
        int16_t plunge = 0;
        if (phase < 8) {
            plunge = (int16_t)((phase * confirm_art_dim(sm, 5)) / 7);
        } else if (phase < 13) {
            plunge = confirm_art_dim(sm, 5);
        } else {
            plunge = (int16_t)(((27 - phase) * confirm_art_dim(sm, 5)) / 14);
        }

        int16_t bx = (int16_t)(cx - bw / 2);
        int16_t by = (int16_t)(cy - bh / 2);
        graphics_fill_rect(ctx, GRect(bx, by, bw, bh), rad, GCornersAll);
        graphics_fill_rect(ctx, GRect((int16_t)(bx - fl), (int16_t)(cy - bh / 2 - nd / 2), fl, (int16_t)(bh + nd)), rad, GCornersLeft);
        graphics_fill_rect(ctx, GRect((int16_t)(bx - plen - plunge), (int16_t)(cy - pr / 2), plen, pr), 0, GCornerNone);

        int16_t nx = (int16_t)(bx + bw);
        graphics_fill_rect(ctx, GRect(nx, (int16_t)(cy - nd / 2), nl, nd), 0, GCornerNone);

        int dropx = (int)(nx + nl + confirm_art_dim(sm, 2));
        int tdrop = phase % 11;
        int16_t dr = confirm_art_dim(sm, 4);
        if (dr < 2) {
            dr = 2;
        }
        int drop_y = (int)(cy + confirm_art_dim(sm, 6) + (tdrop * confirm_art_dim(sm, 12)) / 10);
        if (drop_y > h - dr - 2) {
            drop_y = (int)(cy + confirm_art_dim(sm, 6));
        }
        graphics_fill_circle(ctx, GPoint((int16_t)dropx, (int16_t)drop_y), dr);
        if (phase > 5 && phase < 11) {
            int16_t dr2 = (int16_t)((dr * 3) / 4);
            graphics_fill_circle(ctx, GPoint((int16_t)(dropx + confirm_art_dim(sm, 5)),
                                               (int16_t)(drop_y - confirm_art_dim(sm, 5))), dr2);
        }
    } else {
        /* Carbs: plate, smile arc, main bite + trailing crumb (scaled). */
        int16_t pr = confirm_art_dim(sm, 24);
        {
            int16_t lim = (int16_t)(w / 2 - confirm_art_dim(sm, 4));
            if (pr > lim) {
                pr = lim;
            }
            lim = (int16_t)(h / 2 - confirm_art_dim(sm, 6));
            if (pr > lim) {
                pr = lim;
            }
            if (pr < 8) {
                pr = 8;
            }
        }
        int py = cy + confirm_art_dim(sm, 3);
#ifdef PBL_COLOR
        graphics_context_set_fill_color(ctx, GColorBlack);
        graphics_fill_circle(ctx, GPoint((int16_t)cx, (int16_t)py), pr);
        graphics_context_set_fill_color(ctx, GColorLightGray);
        graphics_fill_circle(ctx, GPoint((int16_t)cx, (int16_t)py), (int16_t)((pr * 11) / 12));
#else
        graphics_context_set_fill_color(ctx, GColorBlack);
        graphics_fill_circle(ctx, GPoint((int16_t)cx, (int16_t)py), pr);
        graphics_context_set_fill_color(ctx, GColorWhite);
        graphics_fill_circle(ctx, GPoint((int16_t)cx, (int16_t)py), (int16_t)((pr * 11) / 12));
#endif
        graphics_context_set_stroke_color(ctx, GColorBlack);
        {
            int16_t mw = confirm_art_dim(sm, 17);
            int16_t mx0 = (int16_t)(cx - mw);
            int16_t mx1 = (int16_t)(cx + mw);
            int16_t my0 = (int16_t)(py - pr - confirm_art_dim(sm, 3));
            int i;
            for (i = 0; i <= 10; i++) {
                int16_t px = (int16_t)(mx0 + (mx1 - mx0) * i / 10);
                int dq = (int16_t)(i - 5);
                int16_t arc = (int16_t)((dq * dq * confirm_art_dim(sm, 2)) / 12);
                graphics_draw_pixel(ctx, GPoint(px, (int16_t)(my0 + arc)));
            }
        }
        {
            int t = (int)(s_confirm_anim_frame % 20);
            int reach = (t < 15) ? t : 14;
            int16_t fx0 = (int16_t)(cx + confirm_art_dim(sm, 8));
            int16_t fy0 = (int16_t)(py + confirm_art_dim(sm, 10));
            int16_t tx = (int16_t)(cx - confirm_art_dim(sm, 2));
            int16_t ty = (int16_t)(py - pr - confirm_art_dim(sm, 2));
            int16_t fs = confirm_art_dim(sm, 8);
            int16_t fx = (int16_t)(fx0 + (tx - fx0) * reach / 14);
            int16_t fy = (int16_t)(fy0 + (ty - fy0) * reach / 14);
            if (fs < 4) {
                fs = 4;
            }
            graphics_context_set_fill_color(ctx, GColorBlack);
            graphics_fill_rect(ctx, GRect((int16_t)(fx - fs / 2), (int16_t)(fy - fs / 2), fs, fs), (int16_t)(fs / 3), GCornersAll);
        }
        {
            int t2 = (int)((s_confirm_anim_frame + 7) % 20);
            if (t2 < 14) {
                int16_t fx0 = (int16_t)(cx + confirm_art_dim(sm, 12));
                int16_t fy0 = (int16_t)(py + confirm_art_dim(sm, 5));
                int16_t tx = (int16_t)(cx + confirm_art_dim(sm, 6));
                int16_t ty = (int16_t)(py - pr + confirm_art_dim(sm, 3));
                int16_t fs2 = confirm_art_dim(sm, 5);
                int16_t fx = (int16_t)(fx0 + (tx - fx0) * t2 / 13);
                int16_t fy = (int16_t)(fy0 + (ty - fy0) * t2 / 13);
                if (fs2 < 3) {
                    fs2 = 3;
                }
                graphics_fill_rect(ctx, GRect((int16_t)(fx - fs2 / 2), (int16_t)(fy - fs2 / 2), fs2, fs2), (int16_t)(fs2 / 2), GCornersAll);
            }
        }
    }
}

static void confirm_double_up_send(ClickRecognizerRef recognizer, void *context) {
    (void)recognizer;
    (void)context;
    for (int i = 0; i < s_confirm_send_pops; i++) {
        window_stack_pop(true);
    }
    begin_progress_and_send();
}

static void confirm_click_down(ClickRecognizerRef recognizer, void *context) {
    (void)recognizer;
    (void)context;
    window_stack_pop(true);
}

static void confirm_action_bar_click_config(void *context) {
    (void)context;
    /* Two quick presses on UP to send (Rebble / CloudPebble: 2-arg click handlers). */
    window_multi_click_subscribe(BUTTON_ID_UP, 2, 2, 450, true, confirm_double_up_send);
    window_single_click_subscribe(BUTTON_ID_DOWN, confirm_click_down);
}

static void confirm_window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(root);
    GRect blk = remote_send_layout_block(&bounds);

#ifdef PBL_COLOR
    window_set_background_color(window, GColorWhite);
#else
    window_set_background_color(window, GColorWhite);
#endif

    if (s_pending_cmd_type == 1) {
        snprintf(s_confirm_title_buf, sizeof(s_confirm_title_buf), "Send bolus?");
        int v = (int)s_pending_amount;
        snprintf(s_confirm_amount_buf, sizeof(s_confirm_amount_buf), "%d.%d U", v / 10, v % 10);
    } else {
        snprintf(s_confirm_title_buf, sizeof(s_confirm_title_buf), "Send carbs?");
        snprintf(s_confirm_amount_buf, sizeof(s_confirm_amount_buf), "%ld g", (long)s_pending_amount);
    }

#ifdef PBL_ROUND
    const GEdgeInsets art_insets = {.top = 8, .right = 36, .bottom = 88, .left = 22};
    const GEdgeInsets title_insets = {.top = 62, .right = ACTION_BAR_WIDTH, .left = ACTION_BAR_WIDTH / 2, .bottom = 0};
    const GEdgeInsets amt_insets = {.top = 86, .right = ACTION_BAR_WIDTH, .left = ACTION_BAR_WIDTH / 2, .bottom = 0};
    const GEdgeInsets hint_insets = {.top = 112, .right = ACTION_BAR_WIDTH, .left = ACTION_BAR_WIDTH / 2, .bottom = 0};
    const char *title_font = FONT_KEY_GOTHIC_18_BOLD;
    const char *amount_font = FONT_KEY_GOTHIC_24_BOLD;
#else
    const GEdgeInsets art_insets = {.top = 6, .right = 30, .bottom = 78, .left = 14};
    const GEdgeInsets title_insets = {.top = 78, .right = ACTION_BAR_WIDTH, .left = ACTION_BAR_WIDTH / 2, .bottom = 0};
    const GEdgeInsets amt_insets = {.top = 104, .right = ACTION_BAR_WIDTH, .left = ACTION_BAR_WIDTH / 2, .bottom = 0};
    const GEdgeInsets hint_insets = {.top = 132, .right = ACTION_BAR_WIDTH, .left = ACTION_BAR_WIDTH / 2, .bottom = 0};
    const char *title_font = FONT_KEY_GOTHIC_24_BOLD;
    const char *amount_font = FONT_KEY_GOTHIC_24_BOLD;
#endif
    GRect art_rect = grect_inset(blk, art_insets);
    s_confirm_art_layer = layer_create(art_rect);
    layer_set_update_proc(s_confirm_art_layer, confirm_art_update_proc);
    layer_add_child(root, s_confirm_art_layer);

    s_confirm_anim_frame = 0;
    confirm_cancel_anim_timer();
    s_confirm_anim_timer = app_timer_register(CONFIRM_ANIM_TICK_MS, confirm_anim_timer_cb, NULL);

    s_confirm_title_layer = text_layer_create(grect_inset(blk, title_insets));
    text_layer_set_text(s_confirm_title_layer, s_confirm_title_buf);
    text_layer_set_background_color(s_confirm_title_layer, GColorClear);
    text_layer_set_text_color(s_confirm_title_layer, GColorBlack);
    text_layer_set_text_alignment(s_confirm_title_layer, GTextAlignmentCenter);
    text_layer_set_font(s_confirm_title_layer, fonts_get_system_font(title_font));
    layer_add_child(root, text_layer_get_layer(s_confirm_title_layer));

    s_confirm_amount_layer = text_layer_create(grect_inset(blk, amt_insets));
    text_layer_set_text(s_confirm_amount_layer, s_confirm_amount_buf);
    text_layer_set_background_color(s_confirm_amount_layer, GColorClear);
    text_layer_set_text_color(s_confirm_amount_layer, GColorBlack);
    text_layer_set_text_alignment(s_confirm_amount_layer, GTextAlignmentCenter);
    text_layer_set_font(s_confirm_amount_layer, fonts_get_system_font(amount_font));
    layer_add_child(root, text_layer_get_layer(s_confirm_amount_layer));

    s_confirm_hint_layer = text_layer_create(grect_inset(blk, hint_insets));
    text_layer_set_text(s_confirm_hint_layer, "Double-tap UP to send");
    text_layer_set_background_color(s_confirm_hint_layer, GColorClear);
    text_layer_set_text_color(s_confirm_hint_layer, GColorDarkGray);
    text_layer_set_text_alignment(s_confirm_hint_layer, GTextAlignmentCenter);
    text_layer_set_font(s_confirm_hint_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    layer_add_child(root, text_layer_get_layer(s_confirm_hint_layer));

    s_confirm_tick_bitmap = gbitmap_create_with_resource(RESOURCE_ID_REMOTE_DIALOG_TICK);
    s_confirm_cross_bitmap = gbitmap_create_with_resource(RESOURCE_ID_REMOTE_DIALOG_CROSS);
    s_confirm_action_bar = action_bar_layer_create();
    action_bar_layer_set_icon(s_confirm_action_bar, BUTTON_ID_UP, s_confirm_tick_bitmap);
    action_bar_layer_set_icon(s_confirm_action_bar, BUTTON_ID_DOWN, s_confirm_cross_bitmap);
    action_bar_layer_set_click_config_provider(s_confirm_action_bar, confirm_action_bar_click_config);
    action_bar_layer_add_to_window(s_confirm_action_bar, window);
}

static void confirm_window_unload(Window *window) {
    (void)window;
    confirm_cancel_anim_timer();
#if TRIO_REMOTE_CONFIRM_PDC_ENABLED
    confirm_pdc_unload();
#endif
    if (s_confirm_action_bar) {
        action_bar_layer_destroy(s_confirm_action_bar);
        s_confirm_action_bar = NULL;
    }
    if (s_confirm_hint_layer) {
        text_layer_destroy(s_confirm_hint_layer);
        s_confirm_hint_layer = NULL;
    }
    if (s_confirm_title_layer) {
        text_layer_destroy(s_confirm_title_layer);
        s_confirm_title_layer = NULL;
    }
    if (s_confirm_amount_layer) {
        text_layer_destroy(s_confirm_amount_layer);
        s_confirm_amount_layer = NULL;
    }
    if (s_confirm_art_layer) {
        layer_destroy(s_confirm_art_layer);
        s_confirm_art_layer = NULL;
    }
    if (s_confirm_tick_bitmap) {
        gbitmap_destroy(s_confirm_tick_bitmap);
        s_confirm_tick_bitmap = NULL;
    }
    if (s_confirm_cross_bitmap) {
        gbitmap_destroy(s_confirm_cross_bitmap);
        s_confirm_cross_bitmap = NULL;
    }
}

static void toast_timer_cb(void *data) {
    (void)data;
    s_toast_timer = NULL;
    remote_send_close_window_if_top(s_toast_window);
}

static void dismiss_toast(void *data) {
    (void)data;
    cancel_toast_timer();
    remote_send_close_window_if_top(s_toast_window);
}

static void toast_window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    GRect b = layer_get_bounds(root);
    GRect safe = remote_send_safe_bounds(&b);
    window_set_background_color(window, GColorWhite);

    int16_t mid_y = (int16_t)(safe.origin.y + (safe.size.h - 72) / 2);
    if (mid_y < safe.origin.y) {
        mid_y = safe.origin.y;
    }
    s_toast_line1 = text_layer_create(GRect(safe.origin.x + 6, mid_y, safe.size.w - 12, 32));
    text_layer_set_background_color(s_toast_line1, GColorClear);
    text_layer_set_text_color(s_toast_line1, GColorBlack);
    text_layer_set_text_alignment(s_toast_line1, GTextAlignmentCenter);
    text_layer_set_font(s_toast_line1, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    layer_add_child(root, text_layer_get_layer(s_toast_line1));

    s_toast_line2 = text_layer_create(GRect(safe.origin.x + 6, (int16_t)(mid_y + 34), safe.size.w - 12, 40));
    text_layer_set_background_color(s_toast_line2, GColorClear);
    text_layer_set_text_color(s_toast_line2, GColorDarkGray);
    text_layer_set_text_alignment(s_toast_line2, GTextAlignmentCenter);
    text_layer_set_font(s_toast_line2, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    layer_add_child(root, text_layer_get_layer(s_toast_line2));
}

static void toast_window_unload(Window *window) {
    (void)window;
    text_layer_destroy(s_toast_line1);
    text_layer_destroy(s_toast_line2);
    s_toast_line1 = s_toast_line2 = NULL;
}

static void cancel_timeout_timer(void) {
    if (s_timeout_timer) {
        app_timer_cancel(s_timeout_timer);
        s_timeout_timer = NULL;
    }
}

static void cancel_progress_timer(void) {
    if (s_progress_timer) {
        app_timer_cancel(s_progress_timer);
        s_progress_timer = NULL;
    }
}

static void cancel_toast_timer(void) {
    if (s_toast_timer) {
        app_timer_cancel(s_toast_timer);
        s_toast_timer = NULL;
    }
}

static void push_toast(const char *line1, const char *line2, bool success_vibe) {
    cancel_toast_timer();
    dismiss_toast(NULL);

    if (!s_toast_window) {
        s_toast_window = window_create();
        window_set_window_handlers(s_toast_window, (WindowHandlers){
            .load = toast_window_load,
            .unload = toast_window_unload,
        });
    }
    window_stack_push(s_toast_window, true);
    text_layer_set_text(s_toast_line1, line1);
    text_layer_set_text(s_toast_line2, line2);
    if (success_vibe) {
        vibes_double_pulse();
    } else {
        vibes_long_pulse();
    }
    s_toast_timer = app_timer_register(TOAST_DISPLAY_MS, toast_timer_cb, NULL);
}

static void dismiss_progress_show_toast(const char *line1, const char *line2, bool success_vibe) {
    s_waiting_phone_status = false;
    cancel_progress_timer();
    cancel_timeout_timer();
    if (s_progress_bar_layer) {
        s_progress_value = 100;
        layer_mark_dirty(s_progress_bar_layer);
    }
    remote_send_close_window_if_top(s_progress_window);
    push_toast(line1, line2, success_vibe);
}

static void dismiss_progress_quiet(void) {
    s_waiting_phone_status = false;
    cancel_progress_timer();
    cancel_timeout_timer();
    if (s_progress_bar_layer) {
        s_progress_value = 100;
        layer_mark_dirty(s_progress_bar_layer);
    }
    remote_send_close_window_if_top(s_progress_window);
    vibes_short_pulse();
}

void remote_send_ui_push_confirm(int32_t cmd_type, int32_t amount) {
    s_pending_cmd_type = cmd_type;
    s_pending_amount = amount;

    if (!s_confirm_window) {
        s_confirm_window = window_create();
        window_set_window_handlers(s_confirm_window, (WindowHandlers){
            .load = confirm_window_load,
            .unload = confirm_window_unload,
        });
    }
    window_stack_push(s_confirm_window, true);
}

bool remote_send_ui_blocks_remote(void) {
    if (s_confirm_window && window_stack_contains_window(s_confirm_window)) {
        return true;
    }
    if (s_progress_window && window_stack_contains_window(s_progress_window)) {
        return true;
    }
    if (s_toast_window && window_stack_contains_window(s_toast_window)) {
        return true;
    }
    return false;
}

void remote_send_ui_on_cmd_status(const char *status, bool suppress_toast_if_ok) {
    if (!s_waiting_phone_status) {
        return;
    }
    bool ok = status && (
        strstr(status, "saved") != NULL ||
        strstr(status, "Sent") != NULL ||
        strstr(status, "deliver") != NULL ||
        strstr(status, "Bolus sent") != NULL ||
        strstr(status, "Carbs saved") != NULL);
    bool bad = status && (
        strstr(status, "unreachable") != NULL ||
        strstr(status, "fail") != NULL ||
        strstr(status, "error") != NULL ||
        strstr(status, "Error") != NULL);

    if (bad && !ok) {
        dismiss_progress_show_toast("Trio", status ? status : "Error", false);
    } else if (suppress_toast_if_ok) {
        dismiss_progress_quiet();
    } else {
        dismiss_progress_show_toast("Done", status ? status : "OK", true);
    }
}

void remote_send_ui_on_outbox_failed(void) {
    if (!s_waiting_phone_status) {
        return;
    }
    dismiss_progress_show_toast("Watch send", "Open Rebble + Trio", false);
}

void remote_send_ui_on_prepare_send_failed(void) {
    if (!s_waiting_phone_status) {
        return;
    }
    dismiss_progress_show_toast("Watch send", "Can't queue", false);
}

void remote_send_ui_set_confirm_send_pop_count(int pop_count) {
    if (pop_count >= 1 && pop_count <= 5) {
        s_confirm_send_pops = pop_count;
    }
}
