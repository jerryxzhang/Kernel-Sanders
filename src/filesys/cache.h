#ifndef BUFFER_CACHE
#define BUFFER_CACHE

#include <list.h>
#include "threads/synch.h"
#include "devices/block.h"

struct lock cache_lock;

struct cache_block {
    struct list_elem block_elem; // For iterating over buffer cache
    
    char data[BLOCK_SECTOR_SIZE]; // Data being cached
    block_sector_t sector; // in->sector gives incomplete type
    
    uint64_t recent_accesses; // For checking how recently this was accessed
    bool dirty; // Set if block has been written to since last write to memory
    bool accessed; // Set if accessed since last check
};


void cache_init(void); /* Initializes buffer cache. */
void refresh_cache(void); /* Writes all dirty blocks in cache to memory. */
struct cache_block *cache_read_block(block_sector_t sector); /* Reads block from cache. */
struct cache_block *cache_write_block(block_sector_t sector); /* Writes to block in cache. */

#endif // #ifndef BUFFER_CACHE
