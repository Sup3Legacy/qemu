#ifndef HW_MEM_FAULT_H
#define HW_MEM_FAULT_H

#include <inttypes.h>
#include "qom/object.h"

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
typedef struct {
    uint8_t dq_pullups;
    uint8_t dq_pulldowns;

    uint16_t a_pullups;
    uint16_t a_pulldowns;

    uint8_t ba_pullups;
    uint8_t ba_pulldowns;
} FaultModel;

#endif

