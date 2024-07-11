#ifndef HW_MEM_FAULT_H
#define HW_MEM_FAULT_H

#include <inttypes.h>
#include "qom/object.h"
#include "hw/misc/memory_backend/ddr.h"

/*
 * Make a struct that holds a reference to the backing memory backend.
 * This should have callbacks to change the fault model configuration.
 *
 */

// Defines a fault model configuration
//
// `pullups` is a bitfield corresponding to the pins that should be shortcut to
//     HIGH. -> active on 1
//
//  `pulldowns` is a bitfield corresponding to the pins that should be shortcut
//     to LOW -> active on 1. 
typedef struct __attribute__((packed)) {
    uint64_t dq_pullups;
    uint64_t dq_pulldowns;

    uint16_t a_pullups;
    uint16_t a_pulldowns;

    uint8_t ba_pullups;
    uint8_t ba_pulldowns;

    uint8_t s_pullups;
    uint8_t s_pulldowns;
} FaultModel;

void fault_model_init(FaultModel *fm);

// Apply the fault model `fm` to the DDR message `msg`
void apply_fault_model_msg(FaultModel *fm, DDRMessage *msg);

uint64_t apply_fault_model_data(FaultModel *fm, uint64_t data);

#endif

