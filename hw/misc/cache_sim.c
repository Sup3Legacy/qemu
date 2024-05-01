#include "qemu/osdep.h"
#include "qapi/error.h" /* provides error_fatal() handler */
#include "hw/sysbus.h" /* provides all sysbus registering func */
#include "hw/misc/cache_sim.h"

Set *compute_set(Cache *cache, uint64_t address) {
    uint64_t set_idx = (address >> cache->assoc_log2) % cache->number_of_sets;
    Set *candidate_set = &cache->sets[set_idx];

    return candidate_set;
}

Block *find_in_cache(CacheUnit *cache, uint64_t address) {
    uint64_t address_tag = address >> (cache->block_size_log2 + cache->assoc_log2);

    Set *candidate_set = compute_set(cache, address);

    for (int i = 0; i < cache->assoc; i++) {
        Block *candidate_block = &candidate_set->blocks[i];
        if ((candidate_block->tag == address_tag) && (candidate_block->is_valid)) {
            return candidate_block;
        }
    }

    return NULL;
}

Block *find_free_block(Cache *cache, Set *set) {
    for (int i = 0, i < cache->assoc; i++) {
        if (! set->blocks[i].is_valid) {
            // We found a free block :o
            return &set->blocks[i];
        }
    }

    // No free block found
    return NULL;
}

Block *allocate_block(Cache *cache, Set *set, uint64_t address) {
    // TODO: eviction policy, etc.
    uint64_t tag = address >> (cache->block_size_log2 + cache->assoc_log2);

    Block *allocated_block;

    allocated_block = find_free_block(cache, set);

    if (!allocated_block) {
        // No free block was found
        // We have to evict :>

        // TODO properly evict
        // TODO: handle writeback upon eviction of dirty block
        allocated_block = &set->blocks[0];
    }

    allocated_block->tag = tag;
    allocated_block->is_valid = true;
    allocated_block->is_dirty = false;

    return allocated_block;
}

// We assume that `length` is smaller than this cache's block size.
// Also, the alignment should be such that blocks do not cross block
// boundaries of lower cache levels
int cache_fetch(void *opaque, char *destination, uint32_t length, uint64_t address) {
    // Recast opaque pointer to a cache one
    CacheUnit *cache = (CacheUnit *)opaque;


    // Accesses from the CPU should not cross block lines.
    // WARN: this might happen, e.g. with unaligned accesses
    // This will have to be handled in he cachesim toplevel, splitting the
    // access into two
    Block *candidate_block = find_in_cache(cache, address);

    if (!candidate_block) {
        // If line not in cache, fetch from lower
        //
        // TODO: register a cache miss
        Set *destination_set = compute_set(cache, address);
        Block *candidate_block = allocated_block(cache, destination_set, address);

        uint64_t block_base = block_base_from_address(cache->block_size, address);

        (cache->lower_fetch)(candidate_block->data, cache->block_size, block_base);
    }

    // Now that data is in cache, 
    char *offset_in_block = candidate_block->data + (address % cache->block_size);
    memcpy(offset_in_block, destination, length);
}
