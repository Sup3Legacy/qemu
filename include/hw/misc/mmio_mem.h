#ifndef HW_MMIO_MEM_H
#define HW_MMIO_MEM_H

#include "qom/object.h"

typedef struct MMIOMemState_t {
	SysBusDevice parent_obj;
	MemoryRegion iomem;
	uint64_t chip_id;
} MMIOMemState;

DeviceState *mmio_mem_create(hwaddr);
void mmio_mem_instance_init(Object *);

#endif
