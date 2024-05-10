#ifndef HW_MEM_FAULT_H
#define HW_MEM_FAULT_H

#include <inttypes.h>
#include "qom/object.h"

/*
 * Make a struct that holds a reference to the backing memory backend.
 * This should have callbacks to change the fault model configuration.
 *
 */

typedef struct {

} FaultHandler;

#endif

