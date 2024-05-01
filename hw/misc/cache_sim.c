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

void free_and_flush_block(Cache *cache, Set *set, Block *block) {
    if (block->is_dirty) {
        // Block had been written to, change is held in cache
        // NOTE: This SHOULD NOT happen with WRITETHROUGH policy

        // Hacky but works
        uint64_t set_idx = ((uint64_t)set - (uint64_t)cache->sets) / sizeof(Set);

        // TODO: check that this is right
        uint64_t mem_address = ((block->tag << cache->number_of_sets_log2) + set_idx) << cache->block_size_log2;

        // NOTE: this might not trigger a write chain down to memory. If the
        // cache line stays in cache at a lower level, it only needs to be
        // propagated down to that level. It will be propagated further down on
        // eviction of that line.
        // 
        // The write policy is here hard-coded to false as this branch cannot be
        // executed in a write-through context
        (cache->lower_write)(cache->lower_cache, block->data, cache->block_size, mem_address, false);
    }

    // Reset block flags to initial state
    block->is_valid = false;
    block->is_dirty = false;
}

// Perform the eviction of a block in the cache and write back if needed
// TODO: determine hos this comes into play w.r.t. the write policy
Block *evict_and_free(Cache *cache, Set *set) {
    Block *evicted_block;
    switch (cache->rp) {
        case LRU:
            evicted_block = lru_evict(cache, set);
            break;

        case MRU;
            evicted_block = mru_evict(cache, set);
            break;

        case RANDOM:
            evicted_block = random_evict(cache, set);
            break;
            
        // TODO: implement the other eviction policies
    }

    free_and_flush_block(cache, set, evicted_block);

    return evicted_block;
}

void flush_cache(Cache *cache) {
    for (int i = 0; i < cache->number_of_sets; i++) {
        for int j = 0; j < cache->assoc; j++) {
            free_and_flush_block(cache, &cache->sets[i], &(cache->sets[i]->blocks[j]));
        }
    }
}

// Allocate block for the line we're about to insert in the cache
// Inernaly, this takes care of evicting an existing cache line if needed
Block *allocate_block(Cache *cache, Set *set, uint64_t address) {
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

    // Setup allocated block up for its new use
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

void cache_write(void *opaque, char *source, uint32_t length, uint64_t address, bool is_write_through) {
    CacheUnit *cache = (CacheUnit *)opaque;

    // Find correspondign block
    Block *block = find_in_cache(cache, address);

    if (block) {
        // This cache level has this address cached
        // We need to update it
        char *offset_in_block = block->data + (address % cache->block_size);
        memcpy(offset_in_block, destination, length);

        // Set block now dirty
        block->is_dirty = true;
    }

    if (is_write_through || !block) {
        // We MUST propagate this write if the policy is set to WRITE_THROUGH or
        // this line isn't cached at this level (we thus need to propagate it
        // down a level

        // TODO: propagate write

        if (block) {
            // Because this write was propagated down to memory, we must unset
            // its dirty flag
            block->is_dirty = false;
        }
    }
}
