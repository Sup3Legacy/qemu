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
    bool is_valid;

    // Maybe we can allocate all the block memory in one go and give
    // intermediate pointers.
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
    // All three have to be powers of two
    uint64_t size;
    uint8_t assoc;
    uint32_t block_size;

    // Derived metrics
    uint64_t number_of_sets;

    // log2s
    uint8_t size_log2;
    uint8_t assoc_log2;
    uint8_t block_size_log2;
} CacheUnit;

bool find_in_cache(CacheUnit *cache, uint64_t address) {
    uint64_t address_tag = address >> (cache->block_size_log2 + cache->assoc_log2);

    uint64_t set_idx = (address >> cache->assoc_log2) % cache->numer_of_sets;
    Set *candidate_set = &cache->sets[set_idx];

    for (int i = 0; i < cache->assoc; i++) {
        Block *candidate_block = &candidate_set->blocks[i];
        if ((candidate_block->tag == address tag) && (candidate_block->valid)) {
            return true;
        }
    }

    return false;
}

typedef struct {
    CacheUnit il1;
    CacheUnit dl1;
    CacheUnit l2;
} CPUCache;

#endif

