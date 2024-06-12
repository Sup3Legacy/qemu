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
        uint8_t dq;

        // BA0-2 pins
        uint8_t ba;

        // A0-15 pins
        uint16_t a;

        // S0-S1 pins
        // NOTE: From what I understand, this enables to select between both
        // sides of a two-sided (= two-ranked?) DIMM
        uint8_t s;
    } body;
} DDRMessage;

#endif
