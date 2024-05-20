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
    MemTopology topology;
    MemTopologyOffsets offsets;

} MemController;

typedef struct {
    uint8_t channel;
    uint8_t rank;
    uint8_t group;
    uint8_t bank;
    uint32_t row;
    uint32_t column;
    // TODO: what next?
} MemCoords;

// FIXME: Already defined in cache_sim.h, we should merge these implementations
static inline uint8_t log2i(uint64_t x) {
    return sizeof(uint64_t) * 8 - __builtin_clz(x) - 1 - 32;
}

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

/* 
 * DRamsim provides a way to configure the linear-to-topology mapping
 * in such a way: "ro ch ra ba bg co"
 * i.e. row.channel.rank.bank.group.column
 * https://github.com/umd-memsys/DRAMsim3/blob/29817593b3389f1337235d63cac515024ab8fd6e/configs/DDR3_4Gb_x4_1866.ini#L57
 *
 */

#endif

