#include "qemu/osdep.h"
#include "qapi/error.h" /* provides error_fatal() handler */
#include "hw/sysbus.h" /* provides all sysbus registering func */
#include "hw/misc/memory_backend/mem_controller.h"

// CONTRACT: Assumes `address` is within bounds ([0; `controller->topology.size`[).
//           Also, the controller must have been completely initialized
static void address_to_coords(MemController *controller, uint64_t address, MemCoords *coords) {
    MemTopologyOffsets *offsets = &controller->offsets;
    // TODO: here are some implicit casts to uint8_t
    //       In practice, these should not overflow (no more than 2^8 channels,
    //       ranks or groups)
    coords->channel = (address >> offsets->channel_off) & offsets->channel_mask;
    coords->rank = (address >> offsets->rank_off) & offsets->rank_mask;
    coords->bank = (address >> offsets->bank_off) & offsets->bank_mask;
    coords->row = (address >> offsets->row_off) & offsets->row_mask;
    coords->column = (address >> offsets->column_off) & offsets->column_mask;
}

// TODO: Re-order these methods

// Derives the log2 fields
static void fill_log2s(MemTopology *topo) {
    topo->channels_log2 = log2i(topo->channels);
    topo->ranks_log2 = log2i(topo->ranks);
    topo->banks_log2 = log2i(topo->banks);
    topo->rows_log2 = log2i(topo->rows);
    topo->column_width_log2 = log2i(topo->column_width);
}

// CONTRACT: assumes `topo`'s log2s have been filled
static void fill_offsets(MemTopologyOffsets *offsets, MemTopology *topo) {
    int8_t offset = 0;
    TopoType topo_type;

    // Loop over the coordinates in their given order and compute all their
    // offsets. This is a topo-order-agnostic implementation
    for (int i = 0; i < 5; i ++) {
        topo_type = topo->topological_order[i];

        switch (topo_type) {
            case Channel:
                offsets->channels_off = offset;
                offset += topo->channels_log2;

                topo->log2s[i] = topo->channels_log2;
                break;
            case Rank:
                offsets->rank_off = offset;
                offset += topo->ranks_log2;

                topo->log2s[i] = topo->channels_log2;
                break;
            case Bank:
                offsets->bank_off = offset;
                offset += topo->banks_log2;

                topo->log2s[i] = topo->banks_log2;
                break;
            case Row:
                offsets->row_off = offset;
                offsets += topo->rows_log2;

                topo->log2s[i] = topo->rows_log2;
                break;
            case Column:
                offsets->column_off = offset;
                offset += topo->column_width_log2;

                topo->log2s[i] = topo->column_width_log2;
                break;
        }

    }

    // Compute all masks
    offsets->channel_mask = (1 << topo->channels_log2) - 1;
    offsets->rank_mask = (1 << topo->ranks_log2) - 1;
    offsets->bank_mask = (1 << topo->banks_log2) - 1;
    offsets->row_mask = (1 << topo->rows_log2) - 1;
    offsets->column_mask = (1 << topo->column_width_log2) - 1;
}

// TODO: place this elsewhere
// CONTRACT: length must be such that the requested memory segment can be handed
// out in one go, contiguously
//
// CONTRACT: `length` must be a multiple of 64
void mem_channel_read(MemController *mc, MemChannel *channel, char *destination, MemCoords *start_coords, uint64_t length) {
    // NOTE: Okay, here I'm stuck. How do I work from here? I understand how a
    // single RAM DIMM receives bank/row/column information. But what am I
    // supposed to do with rank/group dimensions? I'm still a bit confused about
    // RAM topology tho

    // NOTE: One subtle thing: data burst transfer (typically 4 on DDR2, I'll
    // want to support different modes, including x1, x2 and x8) will only have
    // its start address affected by the fault. Subsequent reads will be normal,
    // contiguous reads

    // Okay, now we are here, within a memory channel controller, with precise
    // coordinates, a total length to transfer and a max burst length value

    // We need to send a number of DDR requests, tamper them with the fault
    // handler and pass them down to the memory channel simulator, which will
    // interpret said commands and perform requested action

    // DDR message value. Will be used extensively back-and-forth between this
    // memory controller and the memory channel
    DDRMessage msg;

    // DDR burst length
    uint64_t burst_length = mc->burst_length;


    // TODO: fill this
    uint64_t current_address;

    MemCoords coords = start_coords;

    // We only allow to use the DDR burst feature if the column is the first
    // coordinate in the mapping.
    //
    // NOTE: This might be refined later, though, because there is nothing (?)
    // preventing us to make bursts as long as the mapping of the row coordinate
    // is higher than that of the column
    //
    // Can a bank's burst be paused while another bank is activated and be
    // resumed as soon as we go back to it?
    bool can_burst = (mc->offsets.column_off == 0);

    while () {
        address_to_coords(mc, current_address, &coords);

        if (can_burst) {
            // Well, if we can burst... Let's burst
            // Thanks to the assumption made on bust capability, we know we are
            // in such a state that addressing is column-first, so this should
            // be semi-straight-forward.
            // TODO 
        }
    }

    return;
}

// CONTRACT: Assumes the RAM topology has been registered and all offsets and
// masks have been computed; i.e. `*mc` was correctly initialized
//
// CONTRACT: `address` has to be 64 aligned and `length` a multiple of 64
void memory_read(MemController *mc, char *destination, uint64_t address, uint64_t length) {
    MemCoords coords;
    uint8_t channel_idx;
    MemChannel *channel;

    if (mc->topology.channels == 1) {
        // Edge-case: there is only one memory channel. We can directly call the
        // channel access routine

        address_to_coords(mc, address, &coords);
        channel = &mc->channels[0];

        // Hand-off the read request to the memory channel controller
        mem_channel_read(mc, channel, destination, &coords, length);
    } else {
        // Memory topology has more than 1 channel. We need to split

        // The idea is that we make an abstraction over memory channels, so we split
        // the memory segment on channel boundaries to pass to the channel logic.
        uint64_t step_size = 1 << (mc->offsets.channel_off);
        uint64_t step_delta;

        // Store initial data pointer and memory segment information.
        // Will get updated on-the-fly
        uint64_t current_address = address;
        uint64_t current_length = length;
        char *current_destination = destination;

        while (current_length > 0) {
            // TODO: check that the address is withing bounds of the ram controller.
            // this then ensures all coordinates valid (assuming the
            // address-to-coordinates conversion is correct itself)

            // Convert linear address to DDR2 coordinates
            address_to_coords(mc, current_address, &coords);
            channel_idx = coords.channel;
            channel = &mc->channels[channel_idx];

            step_delta = min(current_length, step_size - (current_address % step_size));

            // TODO: request a transfer of size step_delta

            current_destination = (char *)((size_t)current_destination + (size_t)step_delta);
            current_address += step_delta;
            current_length -= step_delta;
        }

    }



    
    // TODO: check whether some parts have to be overlaid by data in the write
    // buffer

    return;
}

// NOTE: a full write-queue should be directly written in the same invocation

// TEMP: remove this, useless?
void overwrite_write_queues(WriteBuffer *wb, char *date, uint64_t address, uint64_t length) {
    WriteQueue *wq;
    for (int i = 0; i < wb->total_wq_number; i++) {
        wq = &wb->wqs[i];

        // We skip any empty write queue; these will be looked through elsewhere
        if (wq->is_empty)
            continue;

        if (address <= wq->line_offset + wq->stored_length && address + length >= wq->line_offset) {
            // There is an overlap between the stored cache line and the
            // incoming one
             
        }
    }
}

WriteQueue *try_find_nonempty_write_queue(WriteBuffer *wb, uint64_t address) {
    WriteQueue *wq;
    for (int i = 0; i < wb->total_wq_number; i++) {
        wq = &wb->wqs[i];

        // We skip any empty write queue; these will be looked through elsewhere
        if (wq->is_empty)
            continue;

        // BUG: If the memory segment given as argument completely contains the
        // stored buffer line, there WILL be a false negative in this search
        if (wq->line_offset <= address && wq->line_offset + wq->stored_length >= address) {
            // There is an overlap between the stored cache line and the
            // incoming one
            return wq;
        }
    }
    
    return NULL;
}

WriteQueue *try_find_empty_write_queue(WriteBuffer *wb, uint64_t address) {
    WriteQueue *wq;
    for (int i = 0; i < wb->total_wq_number; i++) {
        wq = &wb->wqs[i];
        if (wq->is_empty) {
            return wq;
        }
    }
    
    return NULL;
}

WriteQueue *try_find_write_queue(WriteBuffer *wb, uint64_t address) {
    WriteQueue *candidate_wq;
    candidate_wq = try_find_nonempty_write_queue(wb, address);

    if (!candidate_wq)
        candidate_wq = try_find_empty_write_queue(wb, address);

    return candidate_wq;
}

WriteQueue *find_fullest_queue(WriteBuffer *wb) {
    uint64_t max_length = 0;
    WriteQueue *q = NULL;

    for (int i = 0; i < wq->total_wq_number; i++) {
        WriteQueue *queue = &wq->wqs[i];
        if (!queue->is_empty && queue->stored_length > max_length) {
            q = queue;
        }
    }

    return q;
}

// CONTRACT: will always return a valid pointer
WriteQueue *find_write_queue(WriteBuffer *wb, uint64_t address) {
    WriteQueue *candidate_wq;
    candidate_wq = try_find_write_queue(wb, address);
    // Here, `candidate_wq` is either an empty wq or a non-empty one that
    // intersects with the incoming data

    if (!candidate_wq) {
        // Find the fullest queue in the buffer
        candidate_queue = find_fullest_queue(wb);

        // TODO: flush it and carry-on with the operation
    }

    return candate_queue;
}

void flush_write_queue(MemoryController *mc, WriteQueue *wq) {
    // TODO: Write the actual flushing once DDR backend written

    // Now, mark queue as empty and reusable
    wq->stored_length = 0;
    wq->is_empty = true;
}

// CONTRACT: `address` must be within the wq data or juste after
uint64_t write_to_queue_single(WriteQueue *wq, char *data, uint64_t address, uint64_t length, uint64_t max_length) {

    uint64_t wq_end = wq->line_offset + wq->stored_length;
    uint64_t overlap = wq_end - address;
    uint64_t remaining_space = max_length - wq->stored_length;
    uint64_t total_to_write = length - overlap;
    uint64_t to_write = min(remaining_space, total_to_write);

    // WARN: Hmhmmm...? What do you mean "ugly code"?
    memcpy((char *)((size_t)wq->buf + (size_t)wq->stored_length - (size_t)overlap), data, to_write);

    wq->is_empty = false;

    if (to_write == remaining_space) {
        // TODO: queue is now full, flush to memory (finally!)
    }

    // Return number of bytes written
    return to_write;
}

// TODO: find better name
void buffer_write(MemController *mc, char *data, uint64_t address, uint64_t length) {
    uint64_t written = 0;

    while (written != length) {
        WriteQueue *queue = find_write_queue(&mc->wbuf, address);
        // TODO; check that returned value is not 0
        written += write_to_queue_single(queue, (char *)((size_t)char + (size_t)written), address + written, length - written, mc->wbuf.max_wq_span);
    }
}

void buffer_read(MemController *mc, char *destination, uint64_t address, uint64_t length) {

}
