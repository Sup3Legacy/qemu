#ifndef HW_CACHE_SIM_H
#define HW_CACHE_SIM_H

#include <inttypes.h>
#include "qom/object.h"
#include "hw/misc/memory_backend/mem_controller.h"

typedef unsigned __int128 uint128_t;

// https://www.wikiwand.com/en/Linear_congruential_generator#Comparison_with_other_PRNGs
#define RNG_a 75
#define RNG_c 74
#define RNG_m ((1 << 16) + 1)
#define RNG_init 12321

typedef void (*lower_read_t)(void *opaque, uint8_t *destination, uint64_t length, uint64_t address);
typedef void (*lower_write_t)(void *opaque, uint8_t *source, uint64_t length, uint64_t address, bool write_through);

// Only used for tracing purposes
typedef enum {
    TYPE_L1I,
    TYPE_L1D,
    TYPE_L2,
    TYPE_L3,
} CacheType;

typedef enum {
    LRU,
    MRU,
    // TODO: how does the random work? Congruential PRNG?
    RANDOM,
    FIFO,
    LIFO,

    // LFU,
} ReplacementPolicy;

typedef enum {
    WRITE_THROUGH,
    WRITE_BACK,
} WritePolicy;

typedef struct {
    uint64_t tag;
    // Whether there is a valid cache line stored here
    bool is_valid;
    // Whether the cache line has been modified but not flushed to the lower
    // level cache/memory
    bool is_dirty;

    uint128_t mlru_gen;

    // This points inside the Cache->cache_memory
    // Length is (block_size)
    uint8_t *data;
} Block;

typedef struct {
    // Length is (assoc)
    Block *blocks;
    // TODO: instrumentation for stats and replacement decisions

    // Implemented as an uint128_t to avoid oveflows
    uint128_t mlru_gen_counter;

    // TODO: init this a a good value
    // FIXME: Should this be part of the cache unit or just set?
    // Side-channel maybe?
    uint128_t rng_state;
} Set;

// NOTE: At some point we might want to add other things, like number of
// evictions, mean age of evicted blocks, etc.
typedef struct {
    uint64_t hits;
    uint64_t misses;
} CacheMetrics;

typedef struct {
    bool enable;

    // Remember where this cache is located. Useful for tracing purposes
    CacheType type;
    
    // Length is (size / (assoc * block_size))
    Set *sets;

    // Array holding the cache line data
    // TODO: alignment?
    uint8_t *cache_memory;

    // Opaque pointer holding a reference to a lower cache level, passed as
    // first argument to lower_{fetch, write_back}
    void *lower_cache;

    // Function to call to fetch data from to populate the cache
    // Will be either the fetch function from the lower cache level or the
    // memory fetch function (plugged to the DRAM controller simulator)
    lower_read_t lower_read;

    // Same thing for writebacks
    lower_write_t lower_write;

    // All three have to be powers of two
    uint64_t size;
    uint8_t assoc;
    uint32_t block_size;

    // TODO: should this go?
    ReplacementPolicy rp;

    // Metrics
    CacheMetrics metrics;

    // Derived cache dimensions
    uint64_t number_of_sets;
    uint64_t set_size;

    // dimensions log2s
    uint8_t size_log2;
    uint8_t assoc_log2;
    uint8_t block_size_log2;
    uint8_t number_of_sets_log2;
} Cache;

// Returns a pointer to the string representing the cache level. Used for
// tracing purposes
static const char *cache_type_str(Cache *cache) {
    switch (cache->type) {
        case TYPE_L1I:
            return "L1I";
        case TYPE_L1D:
            return "L1D";
        case TYPE_L2:
            return "L2";
        case TYPE_L3:
            return "L3";
    }

    // ???
    return NULL;
}

// Temporary backend to test the caches
typedef struct {
    uint64_t size;
    // Address of the first byte in memory
    uint64_t offset;
    char *data;
} MockMemBackend;

typedef struct {
    // All cache levels
    Cache il1;
    Cache dl1;
    Cache l2;
    Cache l3;

    // // Temporary memory backend; just a wrapper around a buffer
    // MockMemBackend mem;

    // The underlying memory controller
    // TODO: call its initializer
    MemController mem_controller;

    // Behaviour toggles
    bool is_active;
    bool is_l2_active;
    bool is_l3_active;

    // Cache policies
    // INFO: My guess is, this has to be the same accross all cache levels, so
    // it makes more sense to put it in here.
    WritePolicy wp;
    ReplacementPolicy rp;

    // Entry call-backs and contexts
    // These CAN be split between Instruction and Data
    void *entry_point_instruction;
    void *entry_point_data;
    lower_read_t read_fct;
    lower_write_t write_fct;
} CacheStruct;

// Struct holding the requested configuration for a cache level
typedef struct {
    bool enable;
    uint64_t size;
    uint8_t assoc;
    uint32_t block_size;
} SingleCacheConfigRequest;

// Structure holding the cache model configuration requested from the guest
// kernel
typedef struct {
    // Cache levels
    SingleCacheConfigRequest il1;
    SingleCacheConfigRequest dl1;
    SingleCacheConfigRequest l2;
    SingleCacheConfigRequest l3;

    // Enables the whole cache unit
    // If disabled, the backing memory module is directly hooked to the
    // callbacks
    bool enable;
    // Enables both the Data and Instruction L1 cache
    bool l1_enable;

    // Requested policies
    WritePolicy wp;
    ReplacementPolicy rp;

    // TEMP: Temporary memory backend config
    uint64_t mem_size;
    uint64_t mem_offset;
} RequestedCaches;

Block *find_in_cache(Cache *cache, uint64_t address);

static uint64_t block_base_from_address(uint64_t block_size_log2, uint64_t address) {
    // TODO: check this, I'm not that confident
    // NOTE: block_size is NOT in log2 form
    //return address & (~ (block_size - 1));
    return (address >> block_size_log2) << block_size_log2;
}

// Converts an `uint64_t` to an 8-long byte array
static void to_bytes(uint64_t val, uint8_t *bytes) {
    for (int i = 0; i < 8; i++) {
        bytes[i] = (val >> (i * 8)) % (1 << 8);
    }
}

// Converts an 8-long byte array to an `uint64_t`
static uint64_t from_bytes(uint8_t *bytes) {
    uint64_t acc = 0;
    for (int i = 0; i < 8; i++) {
        acc = (acc << 8) + bytes[7 - i];
    }
    return acc;
}

// Configures and allocates the caches with respect to the requested
// configuration
//
// NOTE: A lot of things here (and this in particular) are safe only because
// this is single-threaded, so any access being processed here has exclusive
// access to the whole cache/memory simulation simulation backend.
int setup_caches(CacheStruct *caches, RequestedCaches *request);

// Flush all caches to the memory backend
void flush_caches(CacheStruct *caches);

#endif

