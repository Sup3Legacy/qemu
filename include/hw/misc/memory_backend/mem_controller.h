#ifndef HW_MEM_CONTROLLER_H
#define HW_MEM_CONTROLLER_H

#include <inttypes.h>
#include "qom/object.h"
#include "hw/misc/memory_backend/mem_fault.h"
#include "hw/misc/memory_backend/ddr.h"
#include "hw/misc/memory_backend/memory_channel.h"


typedef struct {
    char *buf;

    // Number of bytes stored in the wq, starting from buf at index 0
    uint64_t stored_length;

    // Address in memory of the first byte in `buf`
    uint64_t line_offset;

    bool is_empty;
} WriteQueue;

#define WRITE_QUEUE_NUMS 8

typedef struct {
    // Array of write-queues
    WriteQueue *wqs;

    // Number of write-queues
    // TODO: Make this runtime-configurable
    uint64_t total_wq_number;

    // Maximum number of bytes stored in a write-queue
    // TODO: Same thing
    uint64_t max_wq_span;
} WriteBuffer;

typedef struct {
    MemTopology topology;
    MemTopologyOffsets offsets;

    uint8_t channels_count;
    MemChannelController *channels;

    WriteBuffer wbuf;

    // RAM access burst length
    // Typically x4 on DDR2, but may be any reasonnable (as in <= 2^6 == 64)
    // power of two
    uint64_t burst_length;
} MemController;

/*
 * Write cache:
 *
 * Contains an arbitrary number of write queues, each holding a list of cache
 * lines (as a linked-list or an array?)
 *
 * On received:
 * - Write: check all write queue (hint: store important facts to speed up
 *   lookup):
 *   + if cache-line is directly adjacent to the wq's end, enqueue it.
 *   + if already present somewhere (might this happen?), replace present line
 *   + if not present, (flush and) allocate a fresh wq
 * - Read: Check all write queues
 *   + if present somewhere in the write buffer, fetch the data from there (and
 *     register a write-buffer read hit, I guess)
 *   + else fetch from memory and leave write buffer unchanged
 * 
 * On wq flush, stream-write all cache lines.
 * 
 * NOTE: there is no way to bufferize and group reads
 *
 * Should we regularly check whether two wqs could be merged ?
 * Somewhat complexity issue, don't know if it would more closely match the HW
 *
 */

// FIXME: Already defined in cache_sim.h, we should merge these implementations
static inline uint8_t log2i(uint64_t x) {
    return sizeof(uint64_t) * 8 - __builtin_clz(x) - 1 - 32;
}

void mem_controller_init(MemController *mc);

void memory_read(void *mc, unsigned char *destination, uint64_t length, uint64_t address);

void memory_write(void *mc, unsigned char *source, uint64_t lenght, uint64_t address, bool _unused);


/* 
 * DRamsim provides a way to configure the linear-to-topology mapping
 * in such a way: "ro ch ra ba co"
 * i.e. row.channel.rank.bank.group.column
 * https://github.com/umd-memsys/DRAMsim3/blob/29817593b3389f1337235d63cac515024ab8fd6e/configs/DDR3_4Gb_x4_1866.ini#L57
 *
 */

#endif

