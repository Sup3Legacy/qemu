#include "qemu/osdep.h"
#include "qapi/error.h" /* provides error_fatal() handler */
#include "hw/sysbus.h" /* provides all sysbus registering func */
#include "hw/misc/mmio_mem.h"

#define TYPE_MMIO_MEM "mmio_mem"
//typedef struct MMIOMmemState MMIOMemState;
DECLARE_INSTANCE_CHECKER(MMIOMemState, MMIO_MEM, TYPE_MMIO_MEM)

#define REG_ID 	0x0
#define CHIP_ID	0xBA000001

// WARN: endianness: we assume a little-endian host platform
union uint64_bytes {
    uint64_t integer;
    char bytes[8];
};

static uint64_t mmio_mem_read(void *opaque, hwaddr addr, unsigned int size) {
	MMIOMemState *s = opaque;

    char bytes[8] = {0};

    printf("Received read @%lx with size %x\n", addr, size);
    
    // INFO: for now, only handle `data` read, not `instruction` ones
    (s->caches.read_fct)(s->caches.entry_point_data, bytes, size, addr);

    uint64_t ret = from_bytes(bytes);

    printf("Read: %lX.\n", ret);

	return ret;
}

static void mmio_mem_write(void *opaque, hwaddr addr, uint64_t val, unsigned int size) {
	MMIOMemState *s = opaque;
    char bytes[8];

    to_bytes(val, bytes);

    printf("Received write @%lx with size %x\n", addr, size);

    (s->caches.write_fct)(s->caches.entry_point_data, bytes, size, addr, s->caches.wp);
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

// TEMP: This is staticcally defined until we make it runtime-configurable
static RequestedCaches cache_request = {
    .enable = true,
    .l1_enable = true,
    .mem_size = 0x8000,
    .mem_offset = 0,
    // .mem_offset = 0xfffff00,
    .wp = WRITE_BACK,
    .rp = RANDOM,
    .il1 = {
        .size = 1 * 1024,
        .assoc = 4,
        .block_size = 64,
    },
    //.dl1 = {
    //    .size = 1 * 1024,
    //    .assoc = 4,
    //    .block_size = 64,
    //},
    .dl1 = {
        .size = 4 * 64,
        .assoc = 4,
        .block_size = 64,
    },
    .l2 = {
        .enable = true,
        .size = 4 * 1024,
        .assoc = 8,
        .block_size = 128,
    },
    .l3 = {
        .enable = true,
        .size = 16 * 1024,
        .assoc = 8,
        .block_size = 256,
    },
};

void mmio_mem_instance_init(Object *obj)
{
	MMIOMemState *s = MMIO_MEM(obj);

	/* allocate memory map region */
	memory_region_init_io(&s->iomem, obj, &mmio_mem_ops, s, TYPE_MMIO_MEM, 0x100);
	memory_region_init_io(&s->config_reg, obj, &config_reg_ops, s, TYPE_MMIO_MEM, 0x100);
	sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->iomem);

	s->chip_id = CHIP_ID;

    // TODO: remove
    s->size = 0x100;

    setup_caches(&s->caches, &cache_request);
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
