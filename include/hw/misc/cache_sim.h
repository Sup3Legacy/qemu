#ifndef HW_CACHE_SIM_H
#define HW_CACHE_SIM_H

#include <inttypes.h>
#include "qom/object.h"

// https://www.wikiwand.com/en/Linear_congruential_generator#Comparison_with_other_PRNGs
#define RNG_a 75
#define RNG_c 74
#define RNG_m ((1 << 16) + 1)
#define RNG_init


typedef void (*lower_fetch_t)(void *opaque, char *destination, uint32_t length, uint64_t address);
typedef void (*lower_write_t)(void *opaque, char *source, uint32_t length, uint64_t address, bool write_through);

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

    // This points inside the CacheUnit->cache_memory
    // Length is (block_size)
    char *data;
} Block;

typedef struct {
    // Length is (assoc)
    Block *blocks;
    // TODO: instrumentation for stats and replacement decisions

    // Implemented as an uint128_t to avoid oveflows
    uint128_t mlru_gen_counter;

    // TODO: init this a a good value
    // FIXME: Should this be part of the cahce unit or just set?
    // Side-channel maybe?
    uint128_t rng_state;
} Set;

typedef struct {
    // Length is (size / (assoc * block_size))
    Set *sets;

    // Array holding the cache line data
    // TODO: alignment?
    char *cache_memory;

    // Opaque pointer holding a reference to a lower cache level, passed as
    // first argument to lower_{fetch, write_back}
    void *lower_cache;

    // Function to call to fetch date from to populate the cache
    // Will be either the fetch function from the lower cache level or the
    // memory fetch function (plugged to the DRAM controller ismulator
    lower_fetch_t lower_fetch;

    // Samething for writebacks
    lower_write_t lower_write;

    // All three have to be powers of two
    uint64_t size;
    uint8_t assoc;
    uint32_t block_size;


    // Derived metrics
    uint64_t number_of_sets;
    uint64_t set_size;

    // log2s
    uint8_t size_log2;
    uint8_t assoc_log2;
    uint8_t block_size_log2;
    uint8_t number_of_sets_log2;
} CacheUnit;

typedef struct {
    CacheUnit il1;
    CacheUnit dl1;
    CacheUnit l2;
    CacheUnit l3;

    // Behaviour toggles
    bool is_active;
    bool is_l2_active;
    bool is_l3_active;

    // Cache policies
    // INFO: My guess is, this has to be the same accross all cache levels, so
    // it makes more sense to put it in here.
    WritePolicy wp;
    ReplacementPolicy rp;
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
    ReplacementPolicy rp;
} RequestedCaches;

Block *find_in_cache(CacheUnit *cache, uint64_t address);

static uint64_t block_base_from_address(uint64_t block_size, uint64_t address) {
    // TODO: check this, I'm not that confident
    // NOTE: block_size is NOT in log2 form
    return address & (~ (block_size - 1));
}

#endif

