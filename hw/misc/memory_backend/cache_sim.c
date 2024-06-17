#include "qemu/osdep.h"
#include "qapi/error.h" /* provides error_fatal() handler */
#include "hw/sysbus.h" /* provides all sysbus registering func */
#include "hw/misc/memory_backend/cache_sim.h"

// Returns the pointer to the set `address` belongs to within `cache`
static Set *compute_set(Cache *cache, uint64_t address) {
    uint64_t set_idx = (address >> (cache->assoc_log2 + cache->block_size_log2)) % cache->number_of_sets;
    Set *candidate_set = &cache->sets[set_idx];

    return candidate_set;
}

// Returns the tag corresponding to the address within `cache`
static uint64_t compute_address_tag(Cache *cache, uint64_t address) {
    return address >> (cache->block_size_log2);
}

// Find block associated with address in the cache
Block *find_in_cache(Cache *cache, uint64_t address) {
    uint64_t address_tag = compute_address_tag(cache, address);

    Set *candidate_set = compute_set(cache, address);

    for (int i = 0; i < cache->assoc; i++) {
        Block *candidate_block = &candidate_set->blocks[i];
        if ((candidate_block->tag == address_tag) && (candidate_block->is_valid)) {
            // TODO: Maybe we can update the MLRU generation here
            //printf("Found block of index %d, tag %ld, block_size_log2 %d, address %ld.\n", i, address_tag, cache->block_size_log2, address);
            //printf("assoc_log2 %d, size_log2 %d, block_size %d.\n", cache->assoc_log2, cache->size_log2, cache->block_size);
            //printf("truc %d.\n", log2i(64));

            // We have found the block; this is a hit
            cache->metrics.hits += 1;

            return candidate_block;
        }
    }

    // We haven't found this block in the set; this is a miss
    cache->metrics.misses += 1;
    return NULL;
}

// Finds the (possible) first free block in the set
//
// NOTE: in an ideal world, we would store a pointer to the possible next free
// block, such as to spare a few cycles iterating over the whole set. Now, with
// low enough (say < 32) associativity this is not an issue at all.
static Block *find_free_block(Cache *cache, Set *set) {
    for (int i = 0; i < cache->assoc; i++) {
        if (! set->blocks[i].is_valid) {
            // We found a free block to allocate
            return &set->blocks[i];
        }
    }

    // No free block found
    return NULL;
}

// `set`'s embedded PRNG's `next` method
static uint64_t set_rng_next(Set *set) {
    uint64_t new_value = (RNG_a * set->rng_state + RNG_c) % RNG_m;
    set->rng_state = new_value;
    return new_value;
}

static Block *random_evict(Cache *cache, Set *set) {
    // Get next random value from PRNG
    uint64_t new_value = set_rng_next(set) % cache->assoc;

    printf("Evicting block at index %lx.\n", new_value);
    return &set->blocks[new_value];
}

// Find the block to evict from set according to the LRU policy
// Also tick an mlru gen and update this block's counter
static Block *lru_evict(Cache *cache, Set *set) {
    Block *min_gen_block = NULL;
    uint128_t min_gen = -1;

    for (int i = 0; i < cache->assoc; i++) {
        if (set->blocks[i].mlru_gen < min_gen) {
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
static Block *mru_evict(Cache *cache, Set *set) {
    Block *max_gen_block = NULL;
    uint128_t max_gen = 0;

    for (int i = 0; i < cache->assoc; i++) {
        if (set->blocks[i].mlru_gen > max_gen) {
            max_gen_block = &set->blocks[i];
            max_gen = max_gen_block->mlru_gen;
        }
    }

    set->mlru_gen_counter += 1;

    // TODO: also update this upon subsequent access to the block
    max_gen_block->mlru_gen = set->mlru_gen_counter;
    return max_gen_block;
}

static void free_and_flush_block(Cache *cache, Set *set, Block *block) {
    if (block->is_dirty) {
        // Block had been written to, change is held in cache
        // NOTE: This SHOULD NOT happen with WRITETHROUGH policy
        //       (because no block in a write-through cache can be marked dirty)

        // HACK: Hacky but works
        // We retreive the set index from the set pointer.
        uint64_t set_idx = ((uint64_t)set - (uint64_t)cache->sets) / sizeof(Set);

        // TODO: check that this is right
        uint64_t mem_address = ((block->tag) + set_idx) << cache->block_size_log2;

        // NOTE: this might not trigger a write chain down to memory. If the
        // cache line stays in cache at a lower level, it only needs to be
        // propagated down to that level. It will be propagated further down on
        // eviction of that line.
        // 
        // The write policy is here hard-set to false as this branch cannot be
        // executed in a write-through context
        (cache->lower_write)(cache->lower_cache, block->data, cache->block_size, mem_address, false);
    }

    // If `block` was not dirty, there is no need to write it back to the next
    // cache level, we can simply discard the block

    // Reset block flags to initial state
    // NOTE: There is no need to touch anything in the block data segment. No
    // data could be leaked, as this block will only be read after having been
    // re-filled with a segment from lower.
    block->is_valid = false;
    block->is_dirty = false;
}

// Perform the eviction of a block in the cache and write back if needed
// TODO: determine how this comes into play w.r.t. the write policy
//
// POST: Will always return a valid `Block *`
static Block *evict_and_free(Cache *cache, Set *set) {
    Block *evicted_block;
    switch (cache->rp) {
        case LRU:
            evicted_block = lru_evict(cache, set);
            break;

        case MRU:
            evicted_block = mru_evict(cache, set);
            break;

        case RANDOM:
            evicted_block = random_evict(cache, set);
            break;

        default:
            // Fallback in case of misconfigured cache unit:
            // evict the first block. That way, nothing is broken further down,
            // even though the high-level cache behaviour is... weird
            evicted_block = &set->blocks[0];
            break;
            
        // TODO: implement the other eviction policies
    }

    free_and_flush_block(cache, set, evicted_block);

    return evicted_block;
}

// Flushes the whole cache, by flushing all its blocks one-by-one
static void flush_cache(Cache *cache) {
    for (int i = 0; i < cache->number_of_sets; i++) {
        for (int j = 0; j < cache->assoc; j++) {
            free_and_flush_block(cache, &cache->sets[i], &(cache->sets[i].blocks[j]));
        }
    }
}

// Allocate block for the line we're about to insert into the cache
// Internaly, this takes care of evicting an existing cache line if needed
//
// POST: Will always return a valid `Block *`
static Block *allocate_block(Cache *cache, Set *set, uint64_t address) {
    uint64_t tag = compute_address_tag(cache, address);

    // Might return `NULL` on full set
    Block *allocated_block = find_free_block(cache, set);

    if (!allocated_block) {
        // No free block was found, set was full
        // We have to evict :>

        printf("Line eviction needed.\n");
        allocated_block = evict_and_free(cache, set);
    }

    // Setup allocated block up for its new use
    allocated_block->tag = tag;
    allocated_block->is_valid = true;
    allocated_block->is_dirty = false;

    return allocated_block;
}

// Cache level read callback
//
// We assume that `length` is smaller than this cache's block size.
// Also, the alignment should be such that blocks do not cross block
// boundaries of lower cache levels
static void cache_read(void *opaque, uint8_t *destination, uint64_t length, uint64_t address) {
    // Recast opaque pointer to a cache one
    Cache *cache = (Cache *)opaque;

    //printf("Try and read from cache @%lx with size %x.\n", address, length);

    // NOTE: Accesses from the CPU should not cross block lines.
    // WARN: this might happen, e.g. with unaligned accesses
    // This will have to be handled in the cachesim toplevel, splitting the
    // access into two
    Block *candidate_block = find_in_cache(cache, address);

    // By this point, the hit/miss in this level has been registered
    if (!candidate_block) {
        // If line not in cache, fetch from lower
        printf("Data not in cache.\n");

        Set *destination_set = compute_set(cache, address);
        candidate_block = allocate_block(cache, destination_set, address);

        uint64_t block_base = block_base_from_address(cache->block_size_log2, address);

        // Call the next level's read callback to populate this freshly
        // allocated cache block
        (cache->lower_read)(cache->lower_cache, candidate_block->data, cache->block_size, block_base);
        //printf("Data fetched from lower level cache.\n");
    } else {
        //printf("Data in cache.\n");
    }

    // Now that data is in cache, copy over the data
    char *offset_in_block = (char *)((uint64_t)candidate_block->data + (address % cache->block_size));
    //printf("Going to copy from %lx to %lx with length %x.\n", (uint64_t)offset_in_block, (uint64_t)destination, length);

    memcpy(destination, offset_in_block, length);
}

// Cache level read callback
//
// If the policy is write-back and the line is present in this cache, we simply
// apply this write in the cache block. In all other cases, we pass this write
// forward to the lower cache level.
//
// In particular, if the write location is nowhere in the cache, the write
// percolates down to memory.
static void cache_write(void *opaque, uint8_t *source, uint64_t length, uint64_t address, bool is_write_through) {
    Cache *cache = (Cache *)opaque;

    //printf("Try and write to cache @%lx with size %x.\n", address, length);

    // Find corresponding block
    Block *block = find_in_cache(cache, address);

    if (!block && !is_write_through) {
        // If line not in cache, fetch from lower before performing the write

        Set *destination_set = compute_set(cache, address);
        block = allocate_block(cache, destination_set, address);

        uint64_t block_base = block_base_from_address(cache->block_size_log2, address);

        (cache->lower_read)(cache->lower_cache, block->data, cache->block_size, block_base);
    }

    if (block) {
        // This cache level has this address cached
        // We need to update it
        char *offset_in_block = (char *)((uint64_t)block->data + (address % cache->block_size));
        memcpy(offset_in_block, source, length);

        if (is_write_through) {
            // If write-through, we still propagate the write to lower cache
            // levels, but encompasing a whole block
            uint64_t block_base = block_base_from_address(cache->block_size, address);
            (cache->lower_write)(cache->lower_cache, block->data, cache->block_size, block_base, is_write_through);

            // NOTE: we can mark the block non-dirty, as it was written back in
            // its entirety
            block->is_dirty = false;
        } else {
            // Set block now dirty
            if (!block->is_dirty) {
                printf("Marking block dirty.\n");
            }
            block->is_dirty = true;
        }
    } else {
        // Else, we simply propagate the write as-is to the lower level
        //
        // WARN: Do we need to pass the whole cache, or just what was given to
        // us?
        // Edge-case: if is_write_through and no level has the location cached,
        // only [address; address + length[ gets written back to memory, so ~8
        // bytes instead of l3->block_size...
        (cache->lower_write)(cache->lower_cache, source, length, address, is_write_through);
    }
}

// Mock memory backend read callback
static void mem_read(void *opaque, uint8_t *destination, uint64_t length, uint64_t address) {
    MockMemBackend *mem = opaque;

    printf("Try and read from mem @%lx with size %lx.\n", address, length);
    if (address < mem->offset) {
        // Read below memory segment
        printf("Read below memory segment: %lx @%lx.\n", length, address);
        return;
    }
    if (address + length >= mem->offset + mem->size) {
        // Read over the memory segment
        printf("Read over memory segment: %lx @%lx.\n", length, address);
        return;
    }

    memcpy(destination, (char *)(address - mem->offset + (uint64_t)(mem->data)), length);
}

// Mock memory backend write callback
static void mem_write(void *opaque, uint8_t *source, uint64_t length, uint64_t address, bool _is_write_through) {
    MockMemBackend *mem = opaque;

    printf("Try and write to mem @%lx with size %lx.\n", address, length);

    if (address < mem->offset) {
        // Write below memory segment
        printf("Write below memory segment: %lx @%lx.\n", length, address);
        return;
    }
    if (address + length >= mem->offset + mem->size) {
        // Write over the memory segment
        printf("Write over memory segment: %lx @%lx.\n", length, address);
        return;
    }
    memcpy((char *)(address - mem->offset + (uint64_t)(mem->data)), source, length);
}

// Initializes block
static void init_block(Cache *cache, Set *set, Block *block, uint32_t set_id, uint32_t block_id) {
    // Take pointer to cache memory at correct offset
    block->data = &cache->cache_memory[set_id * cache->set_size + block_id * cache->block_size];
    block->is_valid = false;
    block->is_dirty = false;
    block->mlru_gen = 0;
    block->tag = 0;
}

// Initializes set
static int init_set(Cache *cache, Set *set, uint32_t set_id) {
    set->rng_state = RNG_init;
    set->mlru_gen_counter = 0;

    Block *blocks = g_malloc(cache->assoc * sizeof(Block));
    set->blocks = blocks;

    if (!blocks) 
        return 1;

    for (int i = 0; i < cache->assoc; i++) {
        init_block(cache, set, &set->blocks[i], set_id, i);
    }

    return 0;
}

/*
 *
 * NOTE: What follows next is ugly
 *
 */

// TODO: not static
static int setup_cache (Cache *cache, uint64_t size, uint32_t block_size, uint8_t assoc, ReplacementPolicy rp, void *lower_cache, lower_read_t lower_read, lower_write_t lower_write) {
    cache->lower_cache = lower_cache;
    cache->lower_read = lower_read;
    cache->lower_write = lower_write;

    cache->rp = rp;

    cache->assoc = assoc;
    cache->size = size;
    cache->block_size = block_size;

    // Reset cache metrics
    // TODO: maybe provide a way to reset these at any time without resetting
    // the caches, for better instrumentation granularity
    cache->metrics.hits = 0;
    cache->metrics.misses = 0;

    // Derive other fields
    uint32_t number_of_sets = size / (assoc * block_size);
    cache->number_of_sets = number_of_sets;
    cache->set_size = assoc * block_size;
    // TODO: log2s
    
    cache->size_log2 = log2i(size);
    cache->assoc_log2 = log2i(assoc);
    cache->block_size_log2 = log2i(block_size);
    cache->number_of_sets_log2 = log2i(number_of_sets);

    // Allocate bulk cache memory
    uint8_t *cache_memory = g_malloc(size);
    cache->cache_memory = cache_memory;
    if (!cache_memory) 
        goto error;

    // Allcoate array of sets
    Set *sets = g_malloc(number_of_sets * sizeof(Set));
    cache->sets = sets;
    if (!sets) 
        goto error;

    // Init all sets
    bool set_failed = false;
    for (int i = 0; i < number_of_sets; i++) {
        if (init_set(cache, &cache->sets[i], i)) {
            // If one fails, still go through all sets, to cleanly dealloc
            // everything afterwards
            // This way, we know that all set->blocks is either NULL or a good
            // pointer, not garbage
            set_failed = true;
        }
    }

    if (set_failed)
        goto error;

    return 0;

    error:
    // TODO: move this out in a standalone `deinit` function
    if (cache->cache_memory) {
        g_free(cache->cache_memory);
    }
    if (cache->sets) {
        for (int i = 0; i < cache->number_of_sets; i++) {
            Set *set = &cache->sets[i];
            if (set->blocks) {
                g_free(set->blocks);
                // Don't free the memory segment held by blocks, as it is merely
                // a segment taken from cache->cache_memory
            }
        }
        g_free(cache->sets);
    }

    return 1;
}

static int setup_mem_backend(MockMemBackend *mem, uint64_t size, uint64_t offset) {
    mem->size = size;
    mem->offset = offset;

    char *data = g_malloc(size);
    mem->data = data;
    if (!data) {
        printf("Could not allocate memory backend memory.\n");
        return 1;
    }

    // TEMP:
    for (int i = 0; i < size; i++) {
        data[i] = 0x42;
    }

    return 0;
}

void flush_caches(CacheStruct *caches) {
    if (caches->il1.enable) {
        // NOTE: il1 and dl1 are always enabled together
        flush_cache(&caches->il1);
        flush_cache(&caches->dl1);
    }
    if (caches->l2.enable)
        flush_cache(&caches->l2);
    if (caches->l3.enable)
        flush_cache(&caches->l3);
}

int setup_caches(CacheStruct *caches, RequestedCaches *request) {
    Cache *il1 = &caches->il1;
    Cache *dl1 = &caches->dl1;
    Cache *l2 = &caches->l2;
    Cache *l3 = &caches->l3;

    il1->enable = request->l1_enable;
    dl1->enable = request->l1_enable;
    l2->enable = request->l2.enable;
    l3->enable = request->l3.enable;

    // MockMemBackend *mem = &caches->mem;
    MemController *mem = &caches->mem_controller;

    int enabled_caches[3];
    int nb_enabled_caches = 0;
    int current_cache = 0;

    if (request->l1_enable)
        enabled_caches[nb_enabled_caches++] = 1;

    if (request->l2.enable)
        enabled_caches[nb_enabled_caches++] = 2;

    if (request->l3.enable)
        enabled_caches[nb_enabled_caches++] = 3;

    if (request->l1_enable) {
        current_cache++;

        void *next_opaque = NULL;
        lower_read_t next_read;
        lower_write_t next_write;

        if (current_cache < nb_enabled_caches) {
            // There is a cache afterwards
            switch (enabled_caches[current_cache]) {
                // NOTE: one has to hold
                case 2:
                    next_opaque = l2;
                    break;
                case 3:
                    next_opaque = l3;
                    break;
            }
            next_read = cache_read;
            next_write = cache_write;
        } else {
            // Memory just after
            next_opaque = mem;
            next_read = memory_read;
            next_write = memory_write;
        }

        setup_cache(il1, request->il1.size, request->il1.block_size, request->il1.assoc, request->rp, next_opaque, next_read, next_write);
        setup_cache(dl1, request->dl1.size, request->dl1.block_size, request->dl1.assoc, request->rp, next_opaque, next_read, next_write);
    }

    if (request->l2.enable) {
        current_cache++;

        void *next_opaque = NULL;
        lower_read_t next_read;
        lower_write_t next_write;

        if (current_cache < nb_enabled_caches) {
            // There is a cache afterwards
            switch (enabled_caches[current_cache]) {
                case 3:
                    next_opaque = l3;
                    break;
            }
            next_read = cache_read;
            next_write = cache_write;
        } else {
            // Memory just after
            next_opaque = mem;
            next_read = memory_read;
            next_write = memory_write;
        }

        setup_cache(l2, request->l2.size, request->l2.block_size, request->l2.assoc, request->rp, next_opaque, next_read, next_write);
    }

    if (request->l3.enable) {
        current_cache++;

        void *next_opaque;
        lower_read_t next_read;
        lower_write_t next_write;

        // Memory just after
        next_opaque = mem;
        next_read = memory_read;
        next_write = memory_write;

        setup_cache(l3, request->l3.size, request->l3.block_size, request->l3.assoc, request->rp, next_opaque, next_read, next_write);
    }

    // Populate the methods and opaque pointers
    caches->read_fct = cache_read;
    caches->write_fct = cache_write;
    if (request->l1_enable) {
        caches->entry_point_instruction = il1;
        caches->entry_point_data = dl1;
    } else if (request->l2.enable) {
        caches->entry_point_instruction = l2;
        caches->entry_point_data = l2;
    } else if (request->l3.enable) {
        caches->entry_point_instruction = l3;
        caches->entry_point_data = l3;
    } else {
        // Replace the cache methods with the memory one
        caches->read_fct = mem_read;
        caches->write_fct = mem_write;
        caches->entry_point_instruction = mem;
        caches->entry_point_data = mem;
    }

    MemController *mc = &caches->mem_controller;

    // NOTE: temporary hardcoded RAM topology config.
    //       will be runtime-configurable.
    mc->topology.channels = 2;
    mc->topology.ranks = 4;
    mc->topology.banks = 8;
    mc->topology.rows = 1024;
    mc->topology.column_width = 1024;
    // mc->topology.topological_order = {Column, Row, Bank, Rank, Channel};
    
    mc->topology.topological_order[0] = Column;
    mc->topology.topological_order[1] = Row;
    mc->topology.topological_order[2] = Bank;
    mc->topology.topological_order[3] = Rank;
    mc->topology.topological_order[4] = Channel;

    mem_controller_init(mc);
    return 0;
}
