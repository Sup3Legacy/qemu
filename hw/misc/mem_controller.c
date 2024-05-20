#include "qemu/osdep.h"
#include "qapi/error.h" /* provides error_fatal() handler */
#include "hw/sysbus.h" /* provides all sysbus registering func */
#include "hw/misc/mem_controller.h"

// CONTRACT: Assumes `val` is within bounds ([0; `controller->topology.size`[).
//           Also, the controller must have been completely initialized
static void address_to_coords(MemController *controller, uint64_t val, MemCoords *coords) {
    MemTopologyOffsts *offsets = &controller->offsets;
    // TODO: here are some implicit casts to uint8_t
    //       In practice, these should not overflow (no more than 2^8 channels,
    //       ranks or groups)
    coords->channel = (val >> offsets->channel_off) & offsets->channel_mask;
    coords->rank = (val >> offsets->rank_off) & offsets->rank_mask;
    coords->group = (val >> offsets->group_off) & offsets->group_mask;
    coords->bank = (val >> offsets->bank_off) & offsets->bank_mask;
    coords->row = (val >> offsets->row_off) & offsets->row_mask;
    coords->column = (val >> offsets->column_off) & offsets->column_mask;
}

// TODO: Re-order these methods

// Derives the log2 fields
static void fill_log2s(MemTopology *topo) {
    topo->channels_log2 = log2i(topo->channels);
    topo->ranks_log2 = log2i(topo->ranks);
    topo->groups_log2 = log2i(topo->groups);
    topo->banks_log2 = log2i(topo->banks);
    topo->rows_log2 = log2i(topo->rows);
    topo->column_width_log2 = log2i(topo->column_width);
}

// CONTRACT: assumes `topo`'s log2s have been filled
static void fill_offsets(MemTopologyOffsets *offsets, MemTopology *topo) {
    int8_t offset = 0;

    // Compute all offsets
    // NOTE: Currently hardcoded as: "ro ch ra ba bg co"
    offsets->column_off = offset;
    offset += topo->column_width_log2;
    offsets->group_off = offset;
    offset += topo->groups_log2;
    offsets->bank_off = offset;
    offset += topo->banks_log2;
    offsets->rank_off = offset;
    offset += topo->ranks_log2;
    offsets->channel_off = offset;
    offset += topo->channels_log2;
    offsets->row_off = offset;

    // Compute all masks
    offsets->channel_mask = (1 << topo->channels_log2) - 1;
    offsets->rank_mask = (1 << topo->ranks_log2) - 1;
    offsets->group_mask = (1 << topo->groups_log2) - 1;
    offsets->bank_mask = (1 << topo->banks_log2) - 1;
    offsets->row_mask = (1 << topo->rows_log2) - 1;
    offsets->column_mask = (1 << topo->columns_log2) - 1;
}
