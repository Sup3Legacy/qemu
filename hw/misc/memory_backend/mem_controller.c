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
    coords->group = (address >> offsets->group_off) & offsets->group_mask;
    coords->bank = (address >> offsets->bank_off) & offsets->bank_mask;
    coords->row = (address >> offsets->row_off) & offsets->row_mask;
    coords->column = (address >> offsets->column_off) & offsets->column_mask;
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
    offsets->column_mask = (1 << topo->column_width_log2) - 1;
}

// TODO: move elsewhere
// CONTRACT: Assumes the RAM topology has been registered and all offsets and
// masks have been computed; i.e. `*mc` was correctly initialized
void memory_read(MemController *mc, char *destination, uint64_t address, uint64_t length) {
    MemCoords coords;
    uint8_t channel_idx;
    MemChannel *channel;

    // TODO: check that the address is withing bounds of the ram controller.
    // this then ensures all coordinates valid (assuming the
    // address-to-coordinates conversion is correct itself)

    // Convert linear address to DDR2 coordinates
    address_to_coords(mc, address, &coords);
    channel_idx = coords.channel;
    channel = &mc->channels[channel_idx];


    
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
