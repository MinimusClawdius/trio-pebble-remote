#include <pebble.h>
#include "remote_menu.h"
#include "modules/remote_send_ui.h"

enum {
    KEY_CMD_STATUS = 9,
    KEY_SUGGESTED_BOLUS_TENTHS = 45,
};

static void inbox_received(DictionaryIterator *iter, void *context) {
    (void)context;

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
