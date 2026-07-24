#pragma once
#include <pebble.h>

/** Push root window with bolus/carbs menu (watch app — full button support). */
void remote_menu_init(void);

/** Tear down before exit. */
void remote_menu_deinit(void);

/** Status text from phone pkjs (KEY_CMD_STATUS) — shown on menu footer. */
void remote_menu_set_status(const char *status);

/**
 * Apply picker preferences from phone settings.
 * bolus_step_tenths: UP/DOWN step for bolus (1 = 0.1 U).
 * carb_step_grams: UP/DOWN step for carbs.
 * defaults: starting amount when opening picker.
 */
void remote_menu_apply_prefs(int32_t bolus_step_tenths, int32_t bolus_default_tenths,
                             int32_t carb_step_grams, int32_t carb_default_grams);
