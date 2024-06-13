#include "qemu/osdep.h"
#include "qapi/error.h" /* provides error_fatal() handler */
#include "hw/sysbus.h" /* provides all sysbus registering func */
#include "hw/misc/memory_backend/memory_channel.h"

// Receive and handle a DDR request. 
//
// CONTRACT: For now, we blindly trust the code calling us here. For example, we
// don't store whether we're in the middle of a read or write burst, or whether
// the burst has actually been initialized. Or even that the addressed bank has
// been ocrrectly activated.
uint64_t memory_channel_instruct(MemChannel *ch, DDRMessage *msg) {
    // TODO 

    switch (msg->type) {
        case Activate:
            // TODO: record selected bank and 
            break;
        case Read:
            // TODO: record selected column

        // Don't break from here, because there is a good logic overlap
        case ReadBurstContinue:
            break;

        case Write:
            // Data to be written is supplied (faulted) in msg->dq
            
        // Same thing here as for Read
        case WriteBurstContinue:
            break;

        // Do we do anything here? We don't really simulate bank behaviour.
        case Precharge:
            break;
    }

    return 0;
}

