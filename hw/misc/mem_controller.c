#include "qemu/osdep.h"
#include "qapi/error.h" /* provides error_fatal() handler */
#include "hw/sysbus.h" /* provides all sysbus registering func */
#include "hw/misc/mem_controller.h"

// NOTE: 
// in such a way: "ro ch ra ba bg co"

// CONTRACT: Assumes `val` is within bounds ([0; `controller->topology.size`[).
static void address_to_coords(MemController *controller, uint64_t val, MemCoords *coords) {

}
