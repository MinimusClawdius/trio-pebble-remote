#include <pebble.h>
#include "remote_menu.h"
#include "trio_types.h"

#include "modules/remote_cmds.h"

static void inbox_received(DictionaryIterator *iter, void *context) {
    (void)context;

    /* Check for command status from Trio iOS */
    Tuple *t = dict_find(iter, KEY_CMD_STATUS);
    if (t) {
        APP_LOG(APP_LOG_LEVEL_INFO, "Remote: cmd status: %s", t->value->cstring);
        remote_cmds_set_status(t->value->cstring);
    }

    /* Check for CGM data (in case Trio pushes via BLE) */
    t = dict_find(iter, KEY_GLUCOSE);
    if (t) {
        APP_LOG(APP_LOG_LEVEL_INFO, "Remote: received CGM data from Trio");
        /* The remote app could display glucose here, but currently
         * it only handles commands. The watchface shows CGM data. */
    }
}

static void init(void) {
    app_message_register_inbox_received(inbox_received);
    app_message_open(4096, 512);
    remote_cmds_set_watchface_window(NULL); /* No watchface in remote app */
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
