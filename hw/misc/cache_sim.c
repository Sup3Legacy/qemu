#include "qemu/osdep.h"
#include "qapi/error.h" /* provides error_fatal() handler */
#include "hw/sysbus.h" /* provides all sysbus registering func */
#include "hw/misc/cache_sim.h"

bool find_in_cache(CacheUnit *cache, uint64_t address) {
    uint64_t address_tag = address >> (cache->block_size_log2 + cache->assoc_log2);

    uint64_t set_idx = (address >> cache->assoc_log2) % cache->number_of_sets;
    Set *candidate_set = &cache->sets[set_idx];

    for (int i = 0; i < cache->assoc; i++) {
        Block *candidate_block = &candidate_set->blocks[i];
        if ((candidate_block->tag == address_tag) && (candidate_block->is_valid)) {
            return true;
        }
    }

    return false;
}
