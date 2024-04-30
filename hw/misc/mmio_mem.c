#include "qemu/osdep.h"
#include "qapi/error.h" /* provides error_fatal() handler */
#include "hw/sysbus.h" /* provides all sysbus registering func */
#include "hw/misc/mmio_mem.h"

#define TYPE_MMIO_MEM "mmio_mem"
//typedef struct MMIOMmemState MMIOMemState;
DECLARE_INSTANCE_CHECKER(MMIOMemState, MMIO_MEM, TYPE_MMIO_MEM)

#define REG_ID 	0x0
#define CHIP_ID	0xBA000001


static uint64_t mmio_mem_read(void *opaque, hwaddr addr, unsigned int size)
{
    if (size != 1) {
        return 0;
    }
	MMIOMemState *s = opaque;

    return (uint64_t)(s->internal_memory[addr % 0x100]);

	switch (addr) {
	case REG_ID:
		return s->chip_id;
		break;
	
	default:
		return 0xDEADBEEF;
		break;
	}

	return 0;
}

static void mmio_mem_write(void *opaque, hwaddr addr, uint64_t val, unsigned int size) {
    if (size != 1) {
        return;
    }

	MMIOMemState *s = opaque;

    s->internal_memory[addr % 0x100] = (char)(val);

}

// TODO: dissociate this into Data and Instruction segments
static const MemoryRegionOps mmio_mem_ops = {
	.read = mmio_mem_read,
    .write = mmio_mem_write,
	.endianness = DEVICE_NATIVE_ENDIAN,
};

// Configuration MMIO segment
// Will be used to reconfigure the cache architecture at runtime
// And also to get stats, e.g. cache miss rates, etc.
// Could also be used to configure the attacks at runtime
static const MemoryRegionOps config_reg_ops = {
    // TODO: write io handler for these
	.read = mmio_mem_read,
    .write = mmio_mem_write,
	.endianness = DEVICE_NATIVE_ENDIAN,
};

void mmio_mem_instance_init(Object *obj)
{
	MMIOMemState *s = MMIO_MEM(obj);

	/* allocate memory map region */
	memory_region_init_io(&s->iomem, obj, &mmio_mem_ops, s, TYPE_MMIO_MEM, 0x100);
	memory_region_init_io(&s->config_reg, obj, &config_reg_ops, s, TYPE_MMIO_MEM, 0x100);
	sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->iomem);

	s->chip_id = CHIP_ID;
    s->size = 0x100;
    // TODO: check for NULL return
    s->internal_memory = g_malloc(0x100);
}

/* create a new type to define the info related to our device */
static const TypeInfo mmio_mem_info = {
	.name = TYPE_MMIO_MEM,
	.parent = TYPE_SYS_BUS_DEVICE,
	.instance_size = sizeof(MMIOMemState),
	.instance_init = mmio_mem_instance_init,
};

static void mmio_mem_register_types(void)
{
    type_register_static(&mmio_mem_info);
}

type_init(mmio_mem_register_types)

/*
 * Create the device.
 */
DeviceState *mmio_mem_create(hwaddr addr)
{
	DeviceState *dev = qdev_new(TYPE_MMIO_MEM);
	sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
	sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, addr);
	return dev;
}
