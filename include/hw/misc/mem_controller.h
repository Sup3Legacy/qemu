#ifndef HW_MEM_CONTROLLER_H
#define HW_MEM_CONTROLLER_H

#include <inttypes.h>
#include "qom/object.h"
#include "hw/misc/mem_fault.h"

typedef struct {
    uint64_t size;

    uint8_t channels;
    uint8_t ranks;
    uint8_t groups;
    uint8_t banks;
    uint32_t rows;

    // NOTE: derive width of column?
    uint32_t column_width;

    // NOTE: Derived log2s
    uint8_t channels_log2;
    uint8_t ranks_log2;
    uint8_t groups_log2;
    uint8_t banks_log2;
    uint8_t rows_log2;
    uint8_t column_width_log2;
} MemTopology;

typedef struct {
    uint8_t channel_off;
    uint8_t rank_off;
    uint8_t group_off;
    uint8_t bank_off;
    uint8_t row_off;
    uint8_t column_off;

    uint32_t channel_mask;
    uint32_t rank_mask;
    uint32_t group_mask;
    uint32_t bank_mask;
    uint32_t row_mask;
    uint32_t column_mask;
} MemTopologyOffsets;


typedef struct {
    uint8_t channel;
    uint8_t rank;
    uint8_t group;
    uint8_t bank;
    uint32_t row;
    uint32_t column;
    // TODO: what next?
} MemCoords;

typedef struct {
    char *buf;

    // Number of bytes stored in the wq, starting from buf at index 0
    uint64_t stored_length;

    // Address in memory of the first byte in `buf`
    uint64_t line_offset;

    bool is_empty;
} WriteQueue;

#define WRITE_QUEUE_NUMS
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

    WriteBuffer wbuf;
} MemController;

/*
 * Write cache:
 *
 * Contains an arbitrary number of write queues, each holding a list of cache
 * lines (as a linked-list or an array?)
 *
 * On received:
 * - Write: check all write queue (hint: store important fats to speed up
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


/* 
 * DRamsim provides a way to configure the linear-to-topology mapping
 * in such a way: "ro ch ra ba bg co"
 * i.e. row.channel.rank.bank.group.column
 * https://github.com/umd-memsys/DRAMsim3/blob/29817593b3389f1337235d63cac515024ab8fd6e/configs/DDR3_4Gb_x4_1866.ini#L57
 *
 */

#endif

