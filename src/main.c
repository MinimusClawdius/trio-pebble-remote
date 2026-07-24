#include <pebble.h>
#include "remote_menu.h"

/* Wire keys — must match package.json messageKeys */
enum {
    KEY_CMD_STATUS = 9,
    KEY_BOLUS_STEP_TENTHS = 50,
    KEY_BOLUS_DEFAULT_TENTHS = 51,
    KEY_CARB_STEP_GRAMS = 52,
    KEY_CARB_DEFAULT_GRAMS = 53
};

static void inbox_received(DictionaryIterator *iter, void *context) {
    (void)context;

    Tuple *t = dict_find(iter, KEY_CMD_STATUS);
    /* cstring is a flexible array member — never compare it to NULL (Werror=address). */
    if (t && t->type == TUPLE_CSTRING) {
        remote_menu_set_status(t->value->cstring);
    }

    Tuple *bs = dict_find(iter, KEY_BOLUS_STEP_TENTHS);
    Tuple *bd = dict_find(iter, KEY_BOLUS_DEFAULT_TENTHS);
    Tuple *cs = dict_find(iter, KEY_CARB_STEP_GRAMS);
    Tuple *cd = dict_find(iter, KEY_CARB_DEFAULT_GRAMS);
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

static void init(void) {
    app_message_register_inbox_received(inbox_received);
    app_message_register_outbox_failed(outbox_failed);
    /* Inbox: status string + prefs ints */
    app_message_open(1024, 256);
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
