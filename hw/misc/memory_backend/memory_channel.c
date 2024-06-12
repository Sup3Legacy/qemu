#include "qemu/osdep.h"
#include "qapi/error.h" /* provides error_fatal() handler */
#include "hw/sysbus.h" /* provides all sysbus registering func */
#include "hw/misc/memory_backend/memory_channel.h"

uint64_t memory_channel_instruct(MemChannel *ch, DDRMessage *msg) {
    // TODO 

    switch (msg->type) {
        case Activate:
            break;
        case Read:

            return 0;
        case Write:
            break;
        case Precharge:
            break;
    }

    return 0;
}

