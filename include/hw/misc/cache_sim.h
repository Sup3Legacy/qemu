#ifndef HW_CACHE_SIM_H
#define HW_CACHE_SIM_H

#include <inttypes.h>
#include "qom/object.h"

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

    // This points inside the CacheUnit->cache_memory
    // Length is (block_size)
    char *data;
} Block;

typedef struct {
    // Length is (assoc)
    Block *blocks;
    // TODO: instrumentation for stats and replacement decisions
} Set;

typedef struct {
    // Length is (size / (assoc * block_size))
    Set *sets;

    // Array holding the cache line data
    // TODO: alignment?
    char *cache_memory;

    // All three have to be powers of two
    uint64_t size;
    uint8_t assoc;
    uint32_t block_size;

    // Cache policies
    ReplacementPolicy rp;

    // Derived metrics
    uint64_t number_of_sets;

    // log2s
    uint8_t size_log2;
    uint8_t assoc_log2;
    uint8_t block_size_log2;
} CacheUnit;

typedef struct {
    CacheUnit il1;
    CacheUnit dl1;
    CacheUnit l2;
    CacheUnit l3;

    // Write policy
    // INFO: My guess is, this has to be the same accross all cache levels, so
    // it makes more sesn to put it in here.
    // WARN: Same thing for the replacement policy? Probably
    WritePolicy wp;
} CPUCache;

// Structure holding the cache model configuration requested from the guest
// kernel
typedef struct {
    struct {
        bool enable;
        uint64_t size;
        uint8_t assoc;
        uint32_t block_size;
    } il1;
    struct {
        bool enable;
        uint64_t size;
        uint8_t assoc;
        uint32_t block_size;
    } dl1;
    struct {
        bool enable;
        uint64_t size;
        uint8_t assoc;
        uint32_t block_size;
    } l2;
    struct {
        bool enable;
        uint64_t size;
        uint8_t assoc;
        uint32_t block_size;
    } l3;
    WritePolicy wp;
} RequestedCaches;

bool find_in_cache(CacheUnit *cache, uint64_t address);

#endif

