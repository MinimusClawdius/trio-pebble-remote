#include <pebble.h>
#include "remote_menu.h"

static void inbox_received(DictionaryIterator *iter, void *context) {
    (void)iter;
    (void)context;
    /* Optional: show KEY_CMD_STATUS on watch — v1 ignores (phone confirms in Trio). */
}

static void init(void) {
    app_message_register_inbox_received(inbox_received);
    app_message_open(4096, 512);
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
