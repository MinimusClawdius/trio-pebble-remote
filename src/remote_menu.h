#pragma once
#include <pebble.h>

/** Push root window with bolus/carbs menu (watch app — full button support). */
void remote_menu_init(void);

/** Tear down before exit. */
void remote_menu_deinit(void);

/** AppMessage bolus/carbs to phone (used by remote_send_ui after confirm). */
void remote_menu_send_to_phone(int32_t cmd_type, int32_t amount);

/** After carbs, JS may send KEY_SUGGESTED_BOLUS_TENTHS — pre-fill bolus picker. */
void remote_menu_open_bolus_picker_preset(int32_t tenths);

/** Defaults and picker steps from phone (KEY_REMOTE_* AppMessage). Pass 0 to skip a field. */
void remote_menu_apply_phone_defaults(int32_t default_bolus_tenths, int32_t default_carb_g,
                                       int32_t bolus_step_tenths, int32_t carb_step_g);
