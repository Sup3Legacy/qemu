#include "qemu/osdep.h"
#include "qapi/error.h" /* provides error_fatal() handler */
#include "hw/sysbus.h" /* provides all sysbus registering func */
#include "hw/misc/cache_sim.h"

Set *compute_set(Cache *cache, uint64_t address) {
    uint64_t set_idx = (address >> cache->assoc_log2) % cache->number_of_sets;
    Set *candidate_set = &cache->sets[set_idx];

    return candidate_set;
}

// Find block associated with address in the cache
Block *find_in_cache(CacheUnit *cache, uint64_t address) {
    uint64_t address_tag = address >> (cache->block_size_log2 + cache->number_of_sets_log2);

    Set *candidate_set = compute_set(cache, address);

    for (int i = 0; i < cache->assoc; i++) {
        Block *candidate_block = &candidate_set->blocks[i];
        if ((candidate_block->tag == address_tag) && (candidate_block->is_valid)) {
            return candidate_block;
        }
    }

    return NULL;
}

// Finds the (possible) first free block in the set
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

Block *random_evict(Cache *cache, Set *set) {
    uint64_t new_value = (RNG_a * set->rng_state + RNG_c) % RNG_m;
    set->rng_state = new_value;

    return &set->blocks[new_value % cache->assoc];
}

// Find the block to evict from set according to the LRU policy
// Also tick an mlru gen and update this block's counter
Block *lru_evict(Cache *cache, Set *set) {
    Block *min_gen_block;
    uint128_t min_gen = -1;

    for (int i = 0; i < cache->assoc; i++) {
        if (set->blocks[i]->mlru_gen < min_gen) {
            min_gen_block = &set->blocks[i];
            min_gen = min_gen_block->mlru_gen;
        }
    }

    set->mlru_gen_counter += 1;

    // TODO: also update this upon subsequent access to the block
    min_gen_block->mlru_gen = set->mlru_gen_counter;
    return min_gen_block;
}

// Find the block to evict from set according to the MRU policy
// Also tick an mlru gen and update this block's counter
Block *mru_evict(Cache *cache, Set *set) {
    Block *max_gen_block;
    uint128_t max_gen = 0;

    for (int i = 0; i < cache->assoc; i++) {
        if (set->blocks[i]->mlru_gen > max_gen) {
            max_gen_block = &set->blocks[i];
            max_gen = max_gen_block->mlru_gen;
        }
    }

    set->mlru_gen_counter += 1;

    // TODO: also update this upon subsequent access to the block
    max_gen_block->mlru_gen = set->mlru_gen_counter;
    return max_gen_block;
}

// Perform the eviction of a block in the cache and write back if needed
// TODO: determine hos this comes into play w.r.t. the write policy
Block *evict_and_free(Cache *cache, Set *set) {
    Block *freed_block;
    switch (cache->rp) {
        case LRU:
            freed_block = lru_evict(cache, set);
            break;

        case MRU;
            freed_block = mru_evict(cache, set);
            break;

        case RANDOM:
            freed_block = random_evict(cache, set);
            break;
            
        // TODO: implement the other eviction policies
    }

    // Now, we have the block to evict
    if (freed_block->is_dirty) {
        // Block had been written to, change is held in cache
        // NOTE: This SHOULD NOT happen with WRITETHROUGH policy

        // Hacky but works
        uint64_t set_idx = ((uint64_t)set - (uint64_t)cache->sets) / sizeof(Set);

        // TODO: check that this is true
        uint64_t mem_address = ((freed_block->tag << cache->number_of_sets_log2) + set_idx) << cache->block_size_log2;
        (cache->lower_write_back)(cache->lower_cache, freed_block->data, cache->block_size, mem_address);
    }
}

// Allocate block for the line we're about to insert in the cache
// Inernaly, this takes care of evicting an existing cache line if needed
Block *allocate_block(Cache *cache, Set *set, uint64_t address) {
    // TODO: eviction policy, etc.
    
    // FIXME: should be shifted of block_size_log2 + number_of_sets_log2
    // NOTE: We could instead shift it by only block_size_log2 (removing the
    // block offset bits). This would makes things a bit simpler, without
    // hurting anything because we're storing and handling u64s anyway
    uint64_t tag = address >> (cache->block_size_log2 + cache->number_of_sets_log2);

    Block *allocated_block;

    allocated_block = find_free_block(cache, set);

    if (!allocated_block) {
        // No free block was found
        // We have to evict :>

        allocated_block = evict_and_free(cache, set);
    }

    allocated_block->tag = tag;
    allocated_block->is_valid = true;
    allocated_block->is_dirty = false;

    return allocated_block;
}

// We assume that `length` is smaller than this cache's block size.
// Also, the alignment should be such that blocks do not cross block
// boundaries of lower cache levels
void cache_fetch(void *opaque, char *destination, uint32_t length, uint64_t address) {
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

        (cache->lower_fetch)(cache->lower_cache, candidate_block->data, cache->block_size, block_base);
    }

    // Now that data is in cache, copy over the data
    char *offset_in_block = candidate_block->data + (address % cache->block_size);
    memcpy(offset_in_block, destination, length);
}
