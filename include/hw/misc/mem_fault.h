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
    uint64_t pullups;
    uint64_t pulldowns;
} FaultModel;

typedef struct {
    FaultModel active_model;

} FaultHandler;

// TODO: move to .c file
// Applies a fault model to the input value
uint64_t apply_fault(FaultHandler *handler, uint64_t val) {
    return (val & (~ handler->active_model.pulldowns)) | handler->active_model.pullups;
}

#endif

