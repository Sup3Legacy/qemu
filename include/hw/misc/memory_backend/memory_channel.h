#ifndef HW_MEM_CHANNEL_H
#define HW_MEM_CHANNEL_H

#include <inttypes.h>
#include "qom/object.h"
#include "hw/misc/memory_backend/ddr.h"
#include "hw/misc/memory_backend/mem_fault.h"

typedef enum {
    Ready,
    RowSelected,

    // ?
    Precharged,
} ChannelState;

// A struct containing the data and logic held by a physical memory channel
//
// Because the faults we target in the end happen on the per-channel traces, we
// directly assign a fault handler to each channel.
typedef struct {
    FaultHandler fault_handler;

    // Row that is currently selected
    uint64_t selected_row;

    // Data segment
    char *data;

    // TODO: fill-in the rest
} MemChannel;

// Simulates the handling of a DDR instruction by a memory channel. The
// instruction is described in `msg`. The simulation function may return some
// data in the case fo a read (NB: will need to be masked by the fault handler.
// Otherwise, `0` is returned.
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

#endif



