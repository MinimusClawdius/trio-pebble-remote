#pragma once
#include <pebble.h>

/** Number of frames in each send sequence (loop). */
#define SEND_ANIM_FRAMES 6

/**
 * Draw one frame of the bolus "syringe drip" sequence (PDC-style frame index).
 * Light background expected; draws dark strokes/fills.
 */
void send_anim_draw_syringe(GContext *ctx, GRect bounds, int frame);

/**
 * Draw one frame of the carbs "face eating pizza" sequence.
 */
void send_anim_draw_pizza_face(GContext *ctx, GRect bounds, int frame);
