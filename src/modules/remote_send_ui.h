#pragma once
#include <pebble.h>

/** Layout block used by confirm/progress; left area with action strip reserved on the right. */
GRect remote_send_content_left_of_action_bar(const GRect *window_bounds);

/** Full-height strip at the right of the layout block (for drawn + / ✓ / − affordances). */
GRect remote_send_right_action_strip_rect(const GRect *window_bounds);

/**
 * Remote bolus/carb UX: confirm (choice dialog — ActionBar tick/cross, assets from
 * pebble-examples/ui-patterns) → progress bar while phone JS runs HTTP → toast on status.
 */
void remote_send_ui_push_confirm(int32_t cmd_type, int32_t amount);

/**
 * Trio Remote keeps the main menu under the amount picker; after confirm, pop that many windows
 * before showing progress (default 2). Call with 3 from the Remote app init.
 */
void remote_send_ui_set_confirm_send_pop_count(int pop_count);

/** True while confirm / progress / toast is on screen — blocks opening the remote main menu. */
bool remote_send_ui_blocks_remote(void);

/**
 * Called from main inbox when KEY_CMD_STATUS arrives (closes progress, shows toast).
 * If the same message will open bolus preset (KEY_SUGGESTED_BOLUS_TENTHS), pass true to
 * dismiss progress without a toast so the picker is not covered.
 */
void remote_send_ui_on_cmd_status(const char *status, bool suppress_toast_if_ok);

/** Called from main outbox_failed when AppMessage to phone fails during a send. */
void remote_send_ui_on_outbox_failed(void);

/** Called when app_message_outbox_begin fails while a remote send is in progress. */
void remote_send_ui_on_prepare_send_failed(void);
