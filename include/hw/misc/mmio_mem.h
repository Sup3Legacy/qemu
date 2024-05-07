#ifndef HW_MMIO_MEM_H
#define HW_MMIO_MEM_H

#include "qom/object.h"
#include "hw/misc/cache_sim.h"

typedef struct MMIOMemState_t {
	SysBusDevice parent_obj;
	MemoryRegion iomem;
	MemoryRegion config_reg;
	uint64_t chip_id;
    unsigned int size;

    RequestedCaches cache_config_req;
    CacheStruct caches;
} MMIOMemState;

DeviceState *mmio_mem_create(hwaddr);
void mmio_mem_instance_init(Object *);

#endif
