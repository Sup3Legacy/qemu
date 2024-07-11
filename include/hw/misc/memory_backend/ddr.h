#ifndef HW_DDR_H
#define HW_DDR_H

#include <inttypes.h>
#include "qom/object.h"

// The type of a memory hierarchial element.
// Values are fixed so as th be easier to be passed accross ABI and
// kernel-emulator boundary.
typedef enum {
    Channel = 0,
    Rank    = 1,
    Bank    = 2,
    Row     = 3,
    Column  = 4,
} TopoType;

// (Safely?) cast an integer to a `TopoType`.
static inline TopoType topotype_from_int(uint8_t i) {
    return (TopoType)i;
}

typedef struct {
    uint64_t size;

    uint8_t channels;
    uint8_t ranks;
    uint8_t banks;
    uint32_t rows;

    // CONTRACT: must contain exactly one of each `TopoType` variant
    TopoType topological_order[5];


    // CONTRACT: must be >= (cache line size) x (burst size)
    // i.e. >= 64 x 4 (4 in general)
    uint32_t column_width;

    // NOTE: Derived log2s
    uint8_t channels_log2;
    uint8_t ranks_log2;
    uint8_t banks_log2;
    uint8_t rows_log2;
    uint8_t column_width_log2;

    // NOTE: derived as well, must contain the log2s of coordinates in the same
    // order they appear in `topological_order`
    uint8_t log2s[5];
} MemTopology;

typedef struct {
    uint8_t channel_off;
    uint8_t rank_off;
    uint8_t bank_off;
    uint8_t row_off;
    uint8_t column_off;

    uint32_t channel_mask;
    uint32_t rank_mask;
    uint32_t bank_mask;
    uint32_t row_mask;
    uint32_t column_mask;
} MemTopologyOffsets;


typedef struct {
    uint8_t channel;
    uint8_t rank;
    uint8_t bank;
    uint32_t row;
    uint32_t column;
    // TODO: what next?
} MemCoords;

// Type of a DDR request
//
// This simulates a very simplified, functional version of the real requests as defined in the DDR spec.
typedef enum {
    Activate,
    Read,
    Write,

    // Non-standard DDR request. Serves to emulate the contiguous data read
    // by/written to the channel on a strobe.
    ReadBurstContinue,
    WriteBurstContinue,

    Precharge,
} DDRMessageType;

// A DDR request body
typedef struct {
    DDRMessageType type;

    struct {
        // Data bits
        uint64_t dq;

        // BA0-2 pins
        uint8_t ba;

        // A0-15 pins
        uint16_t a;

        // S0-S1 pins
        // NOTE: From what I understand, this enables to select between both
        // sides of a two-sided (= two-ranked?) DIMM as well as between DIMMs
        // within a channel. At this level, it doesn't really matter how the
        // ranks are distributed accross DIMMS.
        // E.g. a config with 2 channel, each of which having 2 2-sided DIMMs
        // will have 8 ranks in total, 4 per channel
        uint8_t s;
    } body;
} DDRMessage;

#endif
