#pragma once
#include <pebble.h>

/** Push root window with bolus/carbs menu (watch app — full button support). */
void remote_menu_init(void);

/** Tear down before exit. */
void remote_menu_deinit(void);

/**
 * Legacy status text from phone. While a send is in flight, success/failure
 * phrases are classified; otherwise ignored (no home footer).
 */
void remote_menu_set_status(const char *status);

/**
 * Structured result from pkjs (KEY_CMD_RESULT).
 * result: 1 = success (exit app → watchface), 2 = failure (error screen).
 */
void remote_menu_handle_result(int result, const char *message);

/**
 * Apply picker preferences from phone settings.
 */
void remote_menu_apply_prefs(int32_t bolus_step_tenths, int32_t bolus_default_tenths,
                             int32_t carb_step_grams, int32_t carb_default_grams);
