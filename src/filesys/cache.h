#ifndef BUFFER_CACHE
#define BUFFER_CACHE

#include <list.h>
#include "threads/synch.h"
#include "devices/block.h"


/*!
 * rw_lock
 * 
 * Used to ensure that nothing is reading from a block while a thread writes
 * to it and that a thread doesn't write to a block being read from.
 */
struct rw_lock {
    struct lock r; // Lock used for atomic operations to rw_lock
    struct semaphore g; // Lock acquired by writers to ensure mutual exclusion
    
    int b; // Number of accessors
};



/*!
 * cache_block
 * 
 * Block in the cache.
 */
struct cache_block {
    struct list_elem block_elem; // For iterating over buffer cache
    
    char data[BLOCK_SECTOR_SIZE]; // Data being cached
    block_sector_t sector; // in->sector gives incomplete type
    
    uint64_t recent_accesses; // For checking how recently this was accessed
    bool dirty; // Set if block has been written to since last write to memory
    bool accessed; // Set if accessed since last check
    
    struct rw_lock lock; // Reader/writer lock
};


void cache_init(void); /* Initializes buffer cache. */
void refresh_cache(void); /* Writes all dirty blocks in cache to memory. */
struct cache_block *cache_read_block(block_sector_t sector); /* Reads block from cache. */
struct cache_block *cache_write_block(block_sector_t sector); /* Writes to block in cache. */
void cache_read_end(struct cache_block *cache_block); /* Unlocks block for writing. */
void cache_write_end(struct cache_block *cache_block); /* Unlocks block. */

#endif // #ifndef BUFFER_CACHE
