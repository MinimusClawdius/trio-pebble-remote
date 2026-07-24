#include <pebble.h>
#include "remote_menu.h"
#include "message_keys.auto.h"

#ifndef MESSAGE_KEY_KEY_CMD_RESULT
#define MESSAGE_KEY_KEY_CMD_RESULT 54
#endif

static void inbox_received(DictionaryIterator *iter, void *context) {
    (void)context;

    Tuple *result_t = dict_find(iter, MESSAGE_KEY_KEY_CMD_RESULT);
    Tuple *status_t = dict_find(iter, MESSAGE_KEY_KEY_CMD_STATUS);

    const char *status = NULL;
    if (status_t && status_t->type == TUPLE_CSTRING) {
        status = status_t->value->cstring;
    }

    if (result_t) {
        int result = (int)result_t->value->int32;
        remote_menu_handle_result(result, status);
    } else if (status) {
        remote_menu_set_status(status);
    }

    Tuple *bs = dict_find(iter, MESSAGE_KEY_KEY_BOLUS_STEP_TENTHS);
    Tuple *bd = dict_find(iter, MESSAGE_KEY_KEY_BOLUS_DEFAULT_TENTHS);
    Tuple *cs = dict_find(iter, MESSAGE_KEY_KEY_CARB_STEP_GRAMS);
    Tuple *cd = dict_find(iter, MESSAGE_KEY_KEY_CARB_DEFAULT_GRAMS);
    if (bs || bd || cs || cd) {
        int32_t bolus_step = bs ? bs->value->int32 : -1;
        int32_t bolus_def = bd ? bd->value->int32 : -1;
        int32_t carb_step = cs ? cs->value->int32 : -1;
        int32_t carb_def = cd ? cd->value->int32 : -1;
        remote_menu_apply_prefs(bolus_step, bolus_def, carb_step, carb_def);
    }
}

static void outbox_failed(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
    (void)iterator;
    (void)context;
    APP_LOG(APP_LOG_LEVEL_ERROR, "Remote outbox failed: %d", (int)reason);
    remote_menu_handle_result(2, "Phone not ready");
}

static void outbox_sent(DictionaryIterator *iterator, void *context) {
    (void)iterator;
    (void)context;
    APP_LOG(APP_LOG_LEVEL_INFO, "Remote outbox sent OK");
}

static void init(void) {
    app_message_register_inbox_received(inbox_received);
    app_message_register_outbox_failed(outbox_failed);
    app_message_register_outbox_sent(outbox_sent);
    app_message_open(1024, 512);
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
