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
    // INFO: size of this should be held in parent set
    char *data;
} Block;

typedef struct {
    Block *blocks;
    // TODO: instrumentation for stats and replacement decisions
} Set;

typedef struct {
    Set *set;
    uint64_t size;
    uint8_t assoc;
    uint32_t block_size;
} CacheUnit;

typedef struct {
    CacheUnit il1;
    CacheUnit dl1;
    CacheUnit l2;
} CPUCache;

#endif

