#include <pebble.h>
#include "remote_menu.h"

/* Wire key — must match package.json KEY_CMD_STATUS and remote_menu.c */
enum { KEY_CMD_STATUS = 9 };

static void inbox_received(DictionaryIterator *iter, void *context) {
    (void)context;
    Tuple *t = dict_find(iter, KEY_CMD_STATUS);
    /* cstring is a flexible array member — never compare it to NULL (Werror=address). */
    if (t && t->type == TUPLE_CSTRING) {
        remote_menu_set_status(t->value->cstring);
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
    /* Large inbox for status strings; modest outbox for cmd type+amount */
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
