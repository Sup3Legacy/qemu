#ifndef HW_DDR_H
#define HW_DDR_H

#include <inttypes.h>
#include "qom/object.h"

// Type of a DDR request
// 
// This simulates a very simplified, functional version of the real requests as defined in the DDR spec.
typedef enum {
    Activate,
    Write,
    Read,
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

void fault_model_init(FaultModel *fm);

// Apply the fault model `fm` to the DDR message `msg`
void apply_fault_model_msg(FaultModel *fm, DDRMessage *msg);

void apply_fault_model_data(FaultModel *fm, uint64_t data);

#endif
