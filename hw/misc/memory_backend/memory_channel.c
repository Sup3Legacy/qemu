#include "qemu/osdep.h"
#include "qapi/error.h" /* provides error_fatal() handler */
#include "hw/sysbus.h" /* provides all sysbus registering func */
#include "hw/misc/memory_backend/memory_channel.h"

uint64_t memory_channel_instruct(MemChannel *ch, DDRMessage *msg) {
    // TODO 

    switch (msg->type) {
        case Activate:
            // TODO: record selected bank and 
            break;
        case Read:
            // TODO: record selected column

            return 0;
        case Write:
            // Data to be written is supplied (faulted) in msg->dq
            break;
        case ReadBurstContinue:
            break;
        case WriteBurstContinue:
            break;
        case Precharge:
            break;
    }

    return 0;
}

