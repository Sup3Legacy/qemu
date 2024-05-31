#include "qemu/osdep.h"
#include "qapi/error.h" /* provides error_fatal() handler */
#include "hw/sysbus.h" /* provides all sysbus registering func */
#include "hw/misc/memory_backend/mem_fault.h"

void fault_model_init(FaultModel *fm) {
    fm->dq_pullups = 0;
    fm->dq_pulldowns = 0;

    fm->a_pullups = 0;
    fm->a_pulldowns = 0;

    fm->ba_pullups = 0;
    fm->ba_pulldowns = 0;
}

// Apply the fault model `fm` to the DDR message `msg`
void apply_fault_model(FaultModel *fm, DDRMessage *msg) {
    msg->body.dq = (msg->body.dq | fm->dq_pullups) & (~ fm->dq_pulldowns);
    msg->body.a = (msg->body.a | fm->a_pullups) & (~ fm->a_pulldowns);
    msg->body.ba = (msg->body.ba | fm->ba_pullups) & (~ fm->ba_pulldowns);
}
