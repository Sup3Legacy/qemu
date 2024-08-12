#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qapi/error.h" /* provides error_fatal() handler */
#include "hw/sysbus.h" /* provides all sysbus registering func */
#include "hw/misc/memory_backend/mem_controller.h"

// CONTRACT: Assumes `address` is within bounds ([0; `controller->topology.size`[).
//           Also, the controller must have been completely initialized
static void address_to_coords(MemController *controller, uint64_t address, MemCoords *coords) {
    MemTopologyOffsets *offsets = &controller->offsets;
    // TODO: here are some implicit casts to uint8_t
    //       In practice, these should not overflow (no more than 2^8 channels,
    //       ranks)
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

// CONTRACT: assumes `topo`'s *_log2 have been filled
static void fill_offsets(MemTopologyOffsets *offsets, MemTopology *topo) {
    int8_t offset = 0;
    TopoType topo_type;

    // Loop over the coordinates in their given order and compute all their
    // offsets. This is a topo-order-agnostic implementation
    for (int i = 0; i < 5; i++) {
        topo_type = topo->topological_order[i];

        switch (topo_type) {
            case Channel:
                offsets->channel_off = offset;
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
                offset += topo->rows_log2;

                topo->log2s[i] = topo->rows_log2;
                break;
            case Column:
                offsets->column_off = offset;
                offset += topo->column_width_log2;

                topo->log2s[i] = topo->column_width_log2;
                break;
            default:
                tracing_report("Invalid thing.\n");
        }

    }

    // Compute all masks
    offsets->channel_mask = (1 << topo->channels_log2) - 1;
    offsets->rank_mask = (1 << topo->ranks_log2) - 1;
    offsets->bank_mask = (1 << topo->banks_log2) - 1;
    offsets->row_mask = (1 << topo->rows_log2) - 1;
    offsets->column_mask = (1 << topo->column_width_log2) - 1;
}

static void mem_channel_controller_init(MemChannelController *mcc) {
    MemTopology *topo = mcc->channel.topology;
    
    // Compute channel memory segment size
    // FIXME: Is this correct?
    uint64_t mem_segment_size = 
        topo->ranks * topo->banks * topo->rows * topo->column_width;

    char *data_segment = g_malloc(mem_segment_size);
    for (int i = 0; i < mem_segment_size; i++) {
        data_segment[i] = 0x69;
    }
    if (!data_segment) {
        tracing_report("Failed mem segment allocation.\n");
    }
    // TODO: return on `NULL`

    mcc->channel.data = (uint64_t *)data_segment;

    // Initialize state-machine registers
    mcc->activated_bank = (uint8_t)(int8_t)(-1);
    mcc->channel.activated_bank = (uint8_t)(int8_t)(-1);
    mcc->channel.selected_rank = (uint8_t)(int8_t)(-1);
    mcc->channel.selected_row = (uint64_t)(int64_t)(-1);
    mcc->channel.current_column = (uint64_t)(int64_t)(-1);

    // TODO: give a way to change a model, or at least apply a static one
    fault_model_init(&mcc->fault_model);
}

// CONTRACT: Only assumes `mc->topology` has been set.
void mem_controller_init(MemController *mc) {
    // Compute and fill log2s and offsets/masks
    fill_log2s(&mc->topology);
    fill_offsets(&mc->offsets, &mc->topology);

    mc->channels_count = mc->topology.channels;

    // Allocate and initialize channel controller
    MemChannelController *channel_controller_array = 
        g_malloc(mc->channels_count * sizeof(MemChannelController));
    if (!channel_controller_array) {
        tracing_report("Failed mem channel controllers allocation");
    }
    // TODO: return on `NULL`

    mc->channels = channel_controller_array;

    MemChannelController *channel_controller;
    for (int i = 0; i < mc->channels_count; i++) {
        channel_controller = &(mc->channels[i]);

        // Give pointer to topology struct to the channel
        channel_controller->channel.topology = &mc->topology;

        // Recursively init channel controller
        mem_channel_controller_init(channel_controller);
    }

    tracing_report("Memory controller initialized");
}

// TODO: place this elsewhere
// CONTRACT: length must be such that the requested memory segment can be handed
// out in one go, contiguously
//
// CONTRACT: `length` must be a multiple of 8
//
// TODO: this is the `read` implementation but will be easily generalized to
// the `write` operation.
//
// TODO: rename `channel` to avoid confusion with `channel->channel`...
static void mem_channel_read(
        MemController *mc, 
        MemChannelController *channel_controller, 
        char *destination, 
        MemCoords *coords, 
        uint64_t length) {
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
    // memory controller and the memory channel.
    DDRMessage msg;

    // FIXME: this requires that the memory topology has <= 4 ranks per channel.
    msg.body.s = coords->rank;

    // If we've switched to a different bank since the last time this channel
    // was used, send a `bank activate` request.
    FaultModel *fm = &channel_controller->fault_model;
    // printf("a %x ba %x dq %lx s %x\n", fm->a_pullups, fm->ba_pullups, fm->dq_pullups, fm->s_pullups);
    if (channel_controller->activated_bank != coords->bank) {
        msg.type = Activate;
        // TODO: fill-in the bank (and row?)
        
        // FIXME: also there can only be 8 banks per chip.
        msg.body.ba = coords->bank;
        msg.body.a = coords->row;

        // Apply the fault on the DDR request message
        apply_fault_model_msg(fm, &msg);

        // send an Activate DDR request
        memory_channel_instruct(&channel_controller->channel, &msg);

        // Store the currently active bank
        channel_controller->activated_bank = coords->bank;
    }

    // Divide the length by 8 because were handling a 64-bit = 8-byte wide bus
    for (int i = 0; i < length / 8; i++) {
        
        msg.type = (i == 0 ? Read : ReadBurstContinue);
        msg.body.a = coords->column;

        // Apply the fault on the DDR request message
        // NOTE: the currently defined and implemented fault model is
        // involutive, so it's okay to apply the model multiple times on the
        // same request register.
        apply_fault_model_msg(fm, &msg);

        uint64_t returned_value = memory_channel_instruct(&channel_controller->channel, &msg);

        // Apply the fault model on the returned data
        uint64_t faulted_returned_data = 
            apply_fault_model_data(&channel_controller->fault_model, returned_value);

        // Move each byte to the destination buffer
        //
        // This should be endianess-agnostic, as the R and W operations are
        // implemented in the same way and all accesses are always 64-bit wide.
        //
        // TODO: the backing memory should be a uint64_t[] for simplicity.
        for (int j = 0; j < 8; j++) {
            destination[i * 8 + j] = (char)(uint8_t)((faulted_returned_data >> (8 * j)) & 0xFF);
        }
    }
}

// TODO: merge this with the read implementation
//
// NOTE: there are many similar comments in the read implementation that are not
// present here.
static void mem_channel_write(
        MemController *mc, 
        MemChannelController *channel_controller, 
        char *source, 
        MemCoords *coords, 
        uint64_t length) {

    // DDR message value. Will be used extensively back-and-forth between this
    // memory controller and the memory channel.
    DDRMessage msg;

    msg.body.s = coords->rank;

    // If we've switched to a different bank since the last time this channel
    // was used, send a `bank activate` request.
    if (channel_controller->activated_bank != coords->bank) {
        msg.type = Activate;

        msg.body.ba = coords->bank;
        msg.body.a = coords->row;

        // Apply the fault on the DDR request message
        apply_fault_model_msg(&channel_controller->fault_model, &msg);

        // send an Activate DDR request
        memory_channel_instruct(&channel_controller->channel, &msg);

        // Store the currently active bank
        channel_controller->activated_bank = coords->bank;
    }

    // Divide the length by 8 because were handling a 64-bit = 8-byte wide bus
    for (int i = 0; i < length / 8; i++) {
        uint64_t to_send = 0;

        for (int j = 0; j < 8; j++) {
            // destination[i * 8 + j] = (char)(uint8_t)((faulted_returned_data >> (8 * j)) & 0xFF);
            to_send |= (uint64_t)(uint8_t)(source[i * 8 + j]) << (8 * j);
        }
        
        msg.type = (i == 0 ? Write : WriteBurstContinue);

        msg.body.a = coords->column;
        msg.body.dq = to_send;

        // Apply the fault on the DDR request message
        // NOTE: the currently defined and implemented fault model is
        // involutive, so it's okay to apply the model multiple times on the
        // same request register.
        apply_fault_model_msg(&channel_controller->fault_model, &msg);

        memory_channel_instruct(&channel_controller->channel, &msg);
    }
}


// CONTRACT: Assumes the RAM topology has been registered and all offsets and
// masks have been computed; i.e. `*mc` was correctly initialized
//
// CONTRACT: `address` has to be 8-byte aligned and `length` a multiple of 8
void memory_read(void *opaque, unsigned char *destination, uint64_t length, uint64_t address) {
    tracing_report("memory read: %lx @%lx", length, address);
    MemController *mc = opaque;
    MemCoords coords;
    uint8_t channel_idx;
    MemChannelController *channel;


    // We make steps of at most the lowest topological dimension's size.
    // NOTE: in principle, this will always be the column dimension...
    uint64_t step_size_bound = 1 << mc->topology.log2s[0];
    // Within this bound, we can can read either 64 bits or a burst of length
    // `mc->burst_length`. It is expressed in *bytes*
    uint64_t step_size_burst = 8 * (mc->topology.topological_order[0] == Column ? mc->burst_length : 1);
    uint64_t step_delta;

    // Store initial data pointer and memory segment information.
    // Will get updated on-the-fly
    uint64_t current_address = address;
    uint64_t current_length = length;
    char *current_destination = (char *)destination;

    while (current_length > 0) {
        // TODO: check that the address is withing bounds of the ram controller.
        // this then ensures all coordinates valid (assuming the
        // address-to-coordinates conversion is correct itself)

        // Convert linear address to DDR2 coordinates
        address_to_coords(mc, current_address, &coords);
        channel_idx = coords.channel;
        channel = &mc->channels[channel_idx];

        // Minimum (in *bytes*) of:
        // - current remaining left from the requested operation
        // - size that is permitted by the burst policy
        // - size that is contiguously mapped to memory as in memory mapping
        //   order
        step_delta = MIN(current_length, 
                MIN(step_size_burst, step_size_bound - 
                    (current_address % step_size_bound))
            );

        // TODO: request a transfer of size step_delta
        mem_channel_read(mc, channel, current_destination, &coords, step_delta);

        current_destination = (char *)((size_t)current_destination + (size_t)step_delta);
        current_address += step_delta;
        current_length -= step_delta;
    }
    
    // TODO: check whether some parts have to be overlaid by data in the write
    // buffer

    return;
}

// CONTRACT: Assumes the RAM topology has been registered and all offsets and
// masks have been computed; i.e. `*mc` was correctly initialized
//
// CONTRACT: `address` has to be 8-byte aligned and `length` a multiple of 8
void memory_write(void *opaque, unsigned char *source, uint64_t length, uint64_t address, bool _unused) {
    tracing_report("memory write: %lx @ %lx", length, address);
    MemController *mc = opaque;
    MemCoords coords;
    uint8_t channel_idx;
    MemChannelController *channel;


    // We make steps of at most the lowest topological dimension's size.
    // NOTE: in principle, this will always be the column dimension...
    uint64_t step_size_bound = 1 << (mc->topology.log2s[0] - 3);
    // Within this bound, we can can read either 64 bits or a burst of length
    // `mc->burst_length`. It is expressed in *bytes*
    uint64_t step_size_burst = 8 * (mc->topology.topological_order[0] == Column ? mc->burst_length : 1);
    uint64_t step_delta;

    // Store initial data pointer and memory segment information.
    // Will get updated on-the-fly
    uint64_t current_address = address;
    uint64_t current_length = length;
    char *current_source = (char *)source;

    while (current_length > 0) {
        // TODO: check that the address is withing bounds of the ram controller.
        // this then ensures all coordinates valid (assuming the
        // address-to-coordinates conversion is correct itself)

        // Convert linear address to DDR2 coordinates
        address_to_coords(mc, current_address, &coords);
        channel_idx = coords.channel;
        channel = &mc->channels[channel_idx];

        // Minimum (in *bytes*) of:
        // - current remaining left from the requested operation
        // - size that is permitted by the burst policy
        // - size that is contiguously mapped to memory as in memory mapping
        //   order
        step_delta = MIN(current_length, 
                MIN(step_size_burst, step_size_bound - 
                    (current_address % step_size_bound))
            );

        // request a transfer of size step_delta
        mem_channel_write(mc, channel, current_source, &coords, step_delta);

        current_source = (char *)((size_t)current_source + (size_t)step_delta);
        current_address += step_delta;
        current_length -= step_delta;
    }
    
    // TODO: check whether some parts have to be overlaid by data in the write
    // buffer

    return;
}

/* NOTE: This is the WIP write-queue implementation.
 *       It should be moved out of here anyway.

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
*/
