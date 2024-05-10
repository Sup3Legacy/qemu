#ifndef HW_MEM_CONTROLLER_H
#define HW_MEM_CONTROLLER_H

#include <inttypes.h>
#include "qom/object.h"

typedef struct {
    uint64_t size;

    uint8_t channels;
    uint8_t ranks;
    uint8_t groups;
    uint8_t banks;
    uint32_t rows;

    // NOTE: derive width of column?

    // NOTE: Derived log2s
    uint8_t channels_log2;
    uint8_t ranks_log2;
    uint8_t groups_log2;
    uint8_t banks_log2;
    uint8_t rows_log2;
} MemTopology;

typedef struct {
    MemTopology topology;

} MemController;

/* 
 * DRamsim provides a way to configure the linear-to-topology mapping
 * in such a way: "ro ch ra ba bg co"
 * i.e. row.channel.rank.bank.group.column
 * https://github.com/umd-memsys/DRAMsim3/blob/29817593b3389f1337235d63cac515024ab8fd6e/configs/DDR3_4Gb_x4_1866.ini#L57
 *
 */

#endif

