#ifndef HW_SHARED_H
#define HW_SHARED_H

// Struct holding the requested configuration for a cache level
typedef struct {
    bool enable;
    uint8_t _pad0[7];
    uint64_t size;
    uint8_t assoc;
    uint8_t _pad1[7];
    uint32_t block_size;
    uint8_t _pad2[4];
} SingleCacheConfigRequest;

#endif

