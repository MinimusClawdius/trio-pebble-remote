#pragma once
#include <pebble.h>

/** Push root window with bolus/carbs menu (watch app — full button support). */
void remote_menu_init(void);

/** Tear down before exit. */
void remote_menu_deinit(void);
