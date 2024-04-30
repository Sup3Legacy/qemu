#ifndef HW_MMIO_MEM_H
#define HW_MMIO_MEM_H

#include "qom/object.h"

typedef struct MMIOMemState_t {
	SysBusDevice parent_obj;
	MemoryRegion iomem;
	uint64_t chip_id;
    unsigned int size;
    char *internal_memory;
} MMIOMemState;

DeviceState *mmio_mem_create(hwaddr);
void mmio_mem_instance_init(Object *);

#endif
