#include "qemu/osdep.h"
#include "qapi/error.h" /* provides error_fatal() handler */
#include "hw/sysbus.h" /* provides all sysbus registering func */
#include "hw/misc/memory_backend/mmio_mem.h"

#define TYPE_MMIO_MEM "mmio_mem"
//typedef struct MMIOMmemState MMIOMemState;
DECLARE_INSTANCE_CHECKER(MMIOMemState, MMIO_MEM, TYPE_MMIO_MEM)

#define REG_ID 	0x0
#define CHIP_ID	0xBA000001

union uint64_bytes {
    uint64_t integer;
    char bytes[8];
};

/* 
 * Simulated memory MMIO region
 */

static uint64_t mmio_mem_read(void *opaque, hwaddr addr, unsigned int size) {
	MMIOMemState *s = opaque;

    uint8_t bytes[8] = {0};

    printf("Received read @%lx with size %x\n", addr, size);
    
    // INFO: for now, only handle `data` read, not `instruction` ones
    (s->caches.read_fct)(s->caches.entry_point_data, bytes, size, addr);

    uint64_t ret = from_bytes(bytes);

    printf("Read: %lX.\n", ret);

	return ret;
}

static void mmio_mem_write(void *opaque, hwaddr addr, uint64_t val, unsigned int size) {
	MMIOMemState *s = opaque;
    uint8_t bytes[8];

    to_bytes(val, bytes);

    printf("Received write @%lx with size %x\n", addr, size);

    (s->caches.write_fct)(s->caches.entry_point_data, bytes, size, addr, s->caches.wp == WRITE_THROUGH);
}

static const MemoryRegionOps mmio_mem_ops = {
	.read = mmio_mem_read,
    .write = mmio_mem_write,
	.endianness = DEVICE_NATIVE_ENDIAN,
};

/*
 * Cache configuration MMIO segment
 */

// INFO: This isn't very useful to implement, as this configuration is done by
// the guest kernel; so it can simply inspect its own requested cache
// configuration.
static uint64_t mmio_cache_config_read(void *opaque, hwaddr addr, unsigned int size) {
	//MMIOMemState *s = opaque;

    return 0;
}

// Write to the configuration request of a single cache
static void mmio_single_cache_config_write(SingleCacheConfigRequest *creq, hwaddr addr, uint64_t val) {
    switch (addr) {
        case 0:
            creq->enable = ((val & 0xff) == 1);
            break;
        case 4:
            creq->size = val;
            break;
        case 8:
            creq->assoc = val;
            break;
        case 12:
            creq->block_size = val;
            break;
    }
}

static void mmio_cache_config_write(void *opaque, hwaddr addr, uint64_t val, unsigned int size) {
	MMIOMemState *s = opaque;
    RequestedCaches *req = &s->cache_config_req;

    switch (addr) {
        case 0:
            req->enable = ((val & 0xff) == 1);
            break;
        case 1:
            req->l1_enable = ((val & 0xff) == 1);
            break;
        case 2:
            // (re)init caches
            // TODO: free memory if reiniting
            printf("Reseting caches on guest request.\n");
            setup_caches(&s->caches, &s->cache_config_req);
            break;
        case 3:
            // Flush all caches to main memory
            printf("Flushing all caches on guest request.\n");
            flush_caches(&s->caches);
            break;
        case 4:
            req->mem_size = val;
            break;
        case 8:
            req->mem_offset = val;
            break;
        case 12:
            req->wp = ((val & 0xff) == 0 ? WRITE_BACK : WRITE_THROUGH);
            break;
        case 16:
            char rp = val & 0xff;
            req->rp = (rp == 0 ? RANDOM : (rp == 1 ? LRU : MRU));
            break;
        case 32:
            mmio_single_cache_config_write(&req->il1, addr - 20, val);
            break;
        case 48:
            mmio_single_cache_config_write(&req->dl1, addr - 24, val);
            break;
        case 64:
            mmio_single_cache_config_write(&req->l2, addr - 28, val);
            break;
        case 80:
            mmio_single_cache_config_write(&req->l3, addr - 32, val);
            break;
    }
}

// Configuration MMIO segment
// Will be used to reconfigure the cache architecture at runtime
// And also to get stats, e.g. cache miss rates, etc.
// Could also be used to configure the attacks at runtime
static const MemoryRegionOps cache_reg_ops = {
	.read = mmio_cache_config_read,
    .write = mmio_cache_config_write,
	.endianness = DEVICE_NATIVE_ENDIAN,
};

/* 
 * Cache Metrics MMIO segment
 */

// Reads to this region return a metric value related to one of the caches
// It consists of [uint64_t hits; uint64_t misses] for all caches (l1I, l1D, l2, l3)
//
// FIXME: very hacky and ad-hoc
static uint64_t mmio_metrics_read(void *opaque, hwaddr addr, unsigned int size) {
	MMIOMemState *s = opaque;
    CacheStruct *caches = &s->caches;
    Cache *cache_array[4] = {&caches->il1, &caches->dl1, &caches->l2, &caches->l3};
    Cache *cache;

    // Rejects reads beyond limit
    if (addr >= 4 * 2 * 8) {
        return 0;
    }

    cache = cache_array[addr / (8 * 2)];

    switch (addr % 16) {
        case 0:
            // First field
            return cache->metrics.hits;
        case 8:
            // Second field
            return cache->metrics.misses;
        default:
            // Missaligned access, return default 0
            return 0;
    }
}

static void mmio_metrics_write(void *opaque, hwaddr addr, uint64_t val, unsigned int size) {
	//MMIOMemState *s = opaque;
    // NOTE: writing to this region does nothing

    return;
}

static const MemoryRegionOps metrics_reg_ops = {
	.read = mmio_metrics_read,
    .write = mmio_metrics_write,
	.endianness = DEVICE_NATIVE_ENDIAN,
};

/* 
 * Fault configuration MMIO segment
 */

static uint64_t mmio_fault_config_read(void *opaque, hwaddr addr, unsigned int size) {
	//MMIOMemState *s = opaque;

    return 0;
}

static void mmio_fault_config_write(void *opaque, hwaddr addr, uint64_t val, unsigned int size) {
	//MMIOMemState *s = opaque;

    return;
}

static const MemoryRegionOps fault_reg_ops = {
	.read = mmio_fault_config_read,
    .write = mmio_fault_config_write,
	.endianness = DEVICE_NATIVE_ENDIAN,
};

// TEMP: This is statically defined until we make it runtime-configurable
static RequestedCaches cache_request = {
    .enable = true,
    .l1_enable = true,
    .mem_size = 0x8000,
    .mem_offset = 0,
    // .mem_offset = 0xfffff00,
    .wp = WRITE_THROUGH,
    //.wp = WRITE_BACK,
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
        .size = 2 * 64,
        .assoc = 2,
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
    // FIXME: This size won't be changed after initialization, so
    // cache_request->
	memory_region_init_io(&s->iomem, obj, &mmio_mem_ops, s, TYPE_MMIO_MEM, 0x1000);
	memory_region_init_io(&s->cache_config_reg, obj, &cache_reg_ops, s, TYPE_MMIO_MEM, 0x100);
	memory_region_init_io(&s->fault_config_reg, obj, &fault_reg_ops, s, TYPE_MMIO_MEM, 0x100);
	memory_region_init_io(&s->metrics_reg, obj, &metrics_reg_ops, s, TYPE_MMIO_MEM, 0x100);
	sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->iomem);
	sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->cache_config_reg);
    // TODO: same thing for the fautl config region

    // TODO: remove
	s->chip_id = CHIP_ID;

    // TODO: remove
    s->size = 0x100;

    // TEMP: this will go at some point
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
DeviceState *mmio_mem_create(hwaddr mem_addr, hwaddr config_addr, hwaddr metrics_addr)
{
	DeviceState *dev = qdev_new(TYPE_MMIO_MEM);
	sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
	sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, mem_addr);
	sysbus_mmio_map(SYS_BUS_DEVICE(dev), 1, config_addr);
	sysbus_mmio_map(SYS_BUS_DEVICE(dev), 2, metrics_addr);
	return dev;
}
