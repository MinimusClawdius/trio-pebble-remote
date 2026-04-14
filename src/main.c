#include <pebble.h>
#include "remote_menu.h"
#include "modules/remote_send_ui.h"

enum {
    KEY_CMD_STATUS = 9,
    KEY_SUGGESTED_BOLUS_TENTHS = 45,
    KEY_REMOTE_DEFAULT_BOLUS_TENTHS = 46,
    KEY_REMOTE_DEFAULT_CARB_G = 47,
    KEY_REMOTE_BOLUS_STEP_TENTHS = 48,
    KEY_REMOTE_CARB_STEP_G = 49,
};

static void inbox_received(DictionaryIterator *iter, void *context) {
    (void)context;

    int32_t def_bolus = 0;
    int32_t def_carb = 0;
    int32_t step_bolus = 0;
    int32_t step_carb = 0;
    Tuple *t = dict_find(iter, KEY_REMOTE_DEFAULT_BOLUS_TENTHS);
    if (t && t->type == TUPLE_INT) {
        def_bolus = t->value->int32;
    }
    t = dict_find(iter, KEY_REMOTE_DEFAULT_CARB_G);
    if (t && t->type == TUPLE_INT) {
        def_carb = t->value->int32;
    }
    t = dict_find(iter, KEY_REMOTE_BOLUS_STEP_TENTHS);
    if (t && t->type == TUPLE_INT) {
        step_bolus = t->value->int32;
    }
    t = dict_find(iter, KEY_REMOTE_CARB_STEP_G);
    if (t && t->type == TUPLE_INT) {
        step_carb = t->value->int32;
    }
    if (def_bolus || def_carb || step_bolus || step_carb) {
        remote_menu_apply_phone_defaults(def_bolus, def_carb, step_bolus, step_carb);
    }

    Tuple *sug = dict_find(iter, KEY_SUGGESTED_BOLUS_TENTHS);
    bool sug_opens_bolus = sug && sug->value->int32 > 0;

    Tuple *st = dict_find(iter, KEY_CMD_STATUS);
    if (st && st->type == TUPLE_CSTRING) {
        APP_LOG(APP_LOG_LEVEL_INFO, "Cmd status: %s", st->value->cstring);
        remote_send_ui_on_cmd_status(st->value->cstring, sug_opens_bolus);
    }

    if (sug) {
        int32_t tenths = sug->value->int32;
        if (tenths > 0) {
            remote_menu_open_bolus_picker_preset(tenths);
        }
    }
}

static void outbox_failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {
    (void)iter;
    (void)context;
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox failed: %d", reason);
    remote_send_ui_on_outbox_failed();
}

static void init(void) {
    app_message_register_inbox_received(inbox_received);
    app_message_register_outbox_failed(outbox_failed);
    app_message_open(4096, 512);
    /* Menu stays under picker — confirm+send pops confirm, picker, and menu (3). */
    remote_send_ui_set_confirm_send_pop_count(3);
    remote_menu_init();
}

static void deinit(void) {
    remote_menu_deinit();
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
