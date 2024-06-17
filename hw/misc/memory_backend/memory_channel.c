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

    uint64_t supplied_word, return_word;
    uint64_t *ptr;

    switch (msg->type) {
        case Activate:
            // Record activated bank, row (and rank as a matter of fact)
            ch->activated_bank = msg->body.ba & 0b00000111;
            ch->selected_row = msg->body.a & ((1 << 16) - 1);
            ch->selected_rank = msg->body.s & 0b00000011;
            return 0;

        case Read:
            // FIXME: bit/byte, how??
            ch->current_column = msg->body.a;
            ptr = coords_to_ptr_channel(ch);

            return_word = *ptr;
            return return_word;

        // Don't break from here, because there is a good logic overlap
        case ReadBurstContinue:
            ptr = coords_to_ptr_channel(ch);

            return_word = *ptr;
            return return_word;

        case Write:
            // Data to be written is supplied (faulted) in msg->dq
            // FIXME: bit/byte, how??
            ch->current_column = msg->body.a;
            supplied_word = msg->body.dq;

            ptr = coords_to_ptr_channel(ch);
            *ptr = supplied_word;

            ch->current_column += 1;
            return 0;
            
        // Same thing here as for Read
        case WriteBurstContinue:
            supplied_word = msg->body.dq;

            ptr = coords_to_ptr_channel(ch);
            *ptr = supplied_word;

            ch->current_column += 1;
            return 0;

        // Do we do anything here? We don't really simulate bank behaviour.
        case Precharge:
            break;
    }

    return 0;
}

