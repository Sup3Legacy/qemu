#include "qemu/osdep.h"
#include "qapi/error.h" /* provides error_fatal() handler */
#include "hw/sysbus.h" /* provides all sysbus registering func */
#include "hw/misc/mem_fault.h"

static void fault_model_init(FaultModel *fm) {
    fm->dq_pullups = 0;
    fm->dq_pulldowns = 0;

    fm->a_pullups = 0;
    fm->a_pulldowns = 0;

    fm->ba_pullups = 0;
    fm->ba_pulldowns = 0;
}

// Applies a fault model to the input value
// FIXME: Deprecated, to revisit
static uint64_t apply_fault(FaultHandler *handler, uint64_t val) {
    return (val & (~ handler->active_model.pulldowns)) | handler->active_model.pullups;
}

