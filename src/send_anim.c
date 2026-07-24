#include "send_anim.h"

/* Simple multi-frame vector sequences (same idea as PDC sequences:
 * discrete frames advanced by a timer). Light-bg / dark-fg. */

static void stroke_line(GContext *ctx, GPoint a, GPoint b, int width) {
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_stroke_width(ctx, width);
    graphics_draw_line(ctx, a, b);
}

void send_anim_draw_syringe(GContext *ctx, GRect bounds, int frame) {
    frame = frame % SEND_ANIM_FRAMES;
    int cx = bounds.origin.x + bounds.size.w / 2;
    int top = bounds.origin.y + 4;
    int barrel_w = 28;
    int barrel_h = 70;
    int barrel_x = cx - barrel_w / 2;
    int barrel_y = top + 18;

    /* Plunger descends slowly over frames */
    int plunger_travel = (frame * 10) / (SEND_ANIM_FRAMES - 1);
    int plunger_y = barrel_y - 14 + plunger_travel;

    /* Plunger flange */
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, GRect(barrel_x - 6, plunger_y, barrel_w + 12, 6), 2, GCornersAll);
    /* Plunger rod */
    graphics_fill_rect(ctx, GRect(cx - 3, plunger_y + 6, 6, barrel_y - plunger_y + 8), 0, GCornerNone);

    /* Barrel outline */
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_stroke_width(ctx, 3);
    graphics_draw_round_rect(ctx, GRect(barrel_x, barrel_y, barrel_w, barrel_h), 4);

    /* Liquid level inside barrel drops as plunger descends */
    int liquid_top = barrel_y + 8 + plunger_travel;
    int liquid_h = (barrel_y + barrel_h - 10) - liquid_top;
    if (liquid_h < 4) liquid_h = 4;
#ifdef PBL_COLOR
    graphics_context_set_fill_color(ctx, GColorCyan);
#else
    graphics_context_set_fill_color(ctx, GColorDarkGray);
#endif
    graphics_fill_rect(ctx, GRect(barrel_x + 4, liquid_top, barrel_w - 8, liquid_h), 0, GCornerNone);

    /* Measurement ticks */
    graphics_context_set_stroke_width(ctx, 1);
    for (int i = 0; i < 4; i++) {
        int ty = barrel_y + 14 + i * 12;
        graphics_draw_line(ctx, GPoint(barrel_x + 2, ty), GPoint(barrel_x + 8, ty));
    }

    /* Hub + needle */
    int hub_y = barrel_y + barrel_h;
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, GRect(cx - 6, hub_y, 12, 8), 0, GCornerNone);
    stroke_line(ctx, GPoint(cx, hub_y + 8), GPoint(cx, hub_y + 28), 2);

    /* Drips falling from needle tip — cycle positions by frame */
    int tip_y = hub_y + 28;
#ifdef PBL_COLOR
    graphics_context_set_fill_color(ctx, GColorBlue);
#else
    graphics_context_set_fill_color(ctx, GColorBlack);
#endif
    /* Two staggered drops */
    for (int d = 0; d < 2; d++) {
        int phase = (frame + d * 3) % SEND_ANIM_FRAMES;
        int dy = 6 + phase * 7;
        int drop_y = tip_y + dy;
        if (drop_y > bounds.origin.y + bounds.size.h - 6) continue;
        int r = (phase < 4) ? 4 : 3;
        graphics_fill_radial(ctx, GRect(cx - r, drop_y, r * 2, r * 2 + 2),
                             GOvalScaleModeFitCircle, r + 1, 0, TRIG_MAX_ANGLE);
    }
}

void send_anim_draw_pizza_face(GContext *ctx, GRect bounds, int frame) {
    frame = frame % SEND_ANIM_FRAMES;
    int cx = bounds.origin.x + bounds.size.w / 2;
    int cy = bounds.origin.y + bounds.size.h / 2 - 4;
    int face_r = 36;

    /* Face */
#ifdef PBL_COLOR
    graphics_context_set_fill_color(ctx, GColorYellow);
#else
    graphics_context_set_fill_color(ctx, GColorWhite);
#endif
    GRect face = GRect(cx - face_r, cy - face_r, face_r * 2, face_r * 2);
    graphics_fill_radial(ctx, face, GOvalScaleModeFitCircle, face_r + 1, 0, TRIG_MAX_ANGLE);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_stroke_width(ctx, 3);
    graphics_draw_arc(ctx, face, GOvalScaleModeFitCircle, 0, TRIG_MAX_ANGLE);

    /* Eyes */
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_radial(ctx, GRect(cx - 14, cy - 12, 8, 10), GOvalScaleModeFitCircle, 5, 0, TRIG_MAX_ANGLE);
    graphics_fill_radial(ctx, GRect(cx + 6, cy - 12, 8, 10), GOvalScaleModeFitCircle, 5, 0, TRIG_MAX_ANGLE);

    /* Mouth opens/closes with frame (eating) */
    int mouth_open = 4 + (frame % 3) * 5; /* 4, 9, 14 */
    graphics_context_set_stroke_width(ctx, 3);
    if (mouth_open <= 6) {
        /* closed-ish smile */
        graphics_draw_arc(ctx, GRect(cx - 14, cy + 2, 28, 22), GOvalScaleModeFitCircle,
                          DEG_TO_TRIGANGLE(20), DEG_TO_TRIGANGLE(160));
    } else {
        /* open oval mouth */
        graphics_context_set_fill_color(ctx, GColorBlack);
        graphics_fill_radial(ctx, GRect(cx - 12, cy + 6, 24, mouth_open + 4),
                             GOvalScaleModeFitCircle, 14, 0, TRIG_MAX_ANGLE);
    }

    /* Pizza slice flies toward mouth from the right, then "eaten" */
    int pizza_start_x = cx + face_r + 8;
    int pizza_end_x = cx + 4;
    int pizza_x = pizza_start_x - ((pizza_start_x - pizza_end_x) * frame) / (SEND_ANIM_FRAMES - 1);
    int pizza_y = cy + 2;
    /* Hide pizza on last frames (swallowed) */
    if (frame < SEND_ANIM_FRAMES - 1) {
#ifdef PBL_COLOR
        graphics_context_set_fill_color(ctx, GColorOrange);
#else
        graphics_context_set_fill_color(ctx, GColorLightGray);
#endif
        GPoint tip = GPoint(pizza_x - 2, pizza_y + 6);
        GPoint a = GPoint(pizza_x + 18, pizza_y - 10);
        GPoint b = GPoint(pizza_x + 18, pizza_y + 22);
        /* Triangle via thick lines + fill center */
        graphics_context_set_stroke_color(ctx, GColorBlack);
        graphics_context_set_stroke_width(ctx, 2);
        graphics_draw_line(ctx, tip, a);
        graphics_draw_line(ctx, a, b);
        graphics_draw_line(ctx, b, tip);
        /* crust block */
#ifdef PBL_COLOR
        graphics_context_set_fill_color(ctx, GColorBrass);
#else
        graphics_context_set_fill_color(ctx, GColorDarkGray);
#endif
        graphics_fill_rect(ctx, GRect(pizza_x + 14, pizza_y - 8, 6, 28), 2, GCornersAll);
        /* pepperoni dots */
        graphics_context_set_fill_color(ctx, GColorBlack);
        graphics_fill_circle(ctx, GPoint(pizza_x + 8, pizza_y + 2), 2);
        graphics_fill_circle(ctx, GPoint(pizza_x + 10, pizza_y + 12), 2);
    }

    /* Chew sparkle on open mouth frames */
    if (frame % 2 == 1) {
        graphics_context_set_stroke_color(ctx, GColorBlack);
        graphics_context_set_stroke_width(ctx, 2);
        graphics_draw_line(ctx, GPoint(cx - 22, cy + 20), GPoint(cx - 16, cy + 14));
        graphics_draw_line(ctx, GPoint(cx + 16, cy + 14), GPoint(cx + 22, cy + 20));
    }
}
