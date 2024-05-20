#include "qemu/osdep.h"
#include "qapi/error.h" /* provides error_fatal() handler */
#include "hw/sysbus.h" /* provides all sysbus registering func */
#include "hw/misc/mem_controller.h"

// NOTE: 
// in such a way: "ro ch ra ba bg co"

// CONTRACT: Assumes `val` is within bounds ([0; `controller->topology.size`[).
//           Also, the controller must have been completely initialized
static void address_to_coords(MemController *controller, uint64_t val, MemCoords *coords) {
    MemTopologyOffsts *offsets = &controller->offsets;
    // TODO: here are some implicit casts to uint8_t
    //       In practice, these should not overflow (no more than 2^8 channels,
    //       ranks or groups)
    coords->channel = (val >> offsets->channel_off) & offsets->channel_mask;
    coords->rank = (val >> offsets->rank_off) & offsets->rank_mask;
    coords->group = (val >> offsets->group_off) & offsets->group_mask;
    coords->bank = (val >> offsets->bank_off) & offsets->bank_mask;
    coords->row = (val >> offsets->row_off) & offsets->row_mask;
    coords->column = (val >> offsets->column_off) & offsets->column_mask;
}
