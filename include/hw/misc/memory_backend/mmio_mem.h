#ifndef HW_MMIO_MEM_H
#define HW_MMIO_MEM_H

#include "qom/object.h"
#include "hw/misc/memory_backend/cache_sim.h"
#include "hw/misc/memory_backend/mem_controller.h"
#include "hw/misc/memory_backend/mem_backend.h"
#include "hw/misc/memory_backend/mem_fault.h"

typedef struct MMIOMemState_t {
	SysBusDevice parent_obj;
	MemoryRegion iomem;
	MemoryRegion cache_config_reg;
	MemoryRegion fault_config_reg;
	MemoryRegion metrics_reg;
	uint64_t chip_id;
    unsigned int size;

    RequestedCaches cache_config_req;
    CacheStruct caches;

    MemController mem_controller;
    MemBackend mem_backend;
} MMIOMemState;

DeviceState *mmio_mem_create(hwaddr, hwaddr, hwaddr);
void mmio_mem_instance_init(Object *);

#endif
