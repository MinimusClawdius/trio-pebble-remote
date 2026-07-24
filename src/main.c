#include <pebble.h>
#include "remote_menu.h"
#include "message_keys.auto.h"

static void inbox_received(DictionaryIterator *iter, void *context) {
    (void)context;

    Tuple *t = dict_find(iter, MESSAGE_KEY_KEY_CMD_STATUS);
    /* cstring is a flexible array member — never compare it to NULL (Werror=address). */
    if (t && t->type == TUPLE_CSTRING) {
        remote_menu_set_status(t->value->cstring);
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
    remote_menu_set_status("Phone not ready");
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
    /* Larger buffers — command + status strings */
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
