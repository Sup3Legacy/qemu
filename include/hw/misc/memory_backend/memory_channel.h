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
    // TODO: make this more elegant
    // For now, will hold a pointer to the same field in the memory controller
    // struct. This is used to index within the `data` host memory segment.
    MemTopology *topology;

    // -1 is none
    uint8_t activated_bank;

    // Row that is currently selected
    uint64_t selected_row;

    // Slightly abusive thing. But might make sense.
    uint8_t selected_rank;

    // TODO: something to keep track of the current burst status?
    uint16_t current_column;

    // Data segment
    // length = ranks * banks * rows * columns / 64
    uint64_t *data;
} MemChannel;

// A wrapper around a memory channel simulator
typedef struct {
    MemChannel channel;

    FaultModel fault_model;

    // The currently activated bank, from the controller's perspective, i.e.
    // without the fault model applied. This thus differs from the memory
    // channel simulator's `activated_channel` field.
    uint8_t activated_bank;
} MemChannelController;

static uint64_t *coords_to_ptr(MemChannel *ch, uint64_t rank, uint64_t bank, uint64_t row, uint64_t reduced_column) {
    MemTopology *topo = ch->topology;

    // TODO: check this
    return &(ch->data[(rank << topo->ranks_log2) + (bank << topo->banks_log2)
                      + (row << topo->rows_log2) + (reduced_column << topo->topo_log2)]);
}

// FIXME: bit/byte?
static uint64_t *coords_to_ptr_channel(MemChannel *ch) {
    return coords_to_ptr(ch, ch->selected_rank, ch->activated_bank, ch->selected_row, ch->current_column);
}

// Simulates the handling of a DDR instruction by a memory channel. The
// instruction is described in `msg`. The simulation function may return some
// data in the case fo a read (NB: will need to be masked by the fault handler.
// Otherwise, `0` is returned.
uint64_t memory_channel_instruct(MemChannel *ch, DDRMessage *msg);

#endif



