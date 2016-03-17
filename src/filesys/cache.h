#ifndef BUFFER_CACHE
#define BUFFER_CACHE

#include <list.h>
#include "threads/synch.h"
#include "devices/block.h"

<<<<<<< Updated upstream
struct lock cache_lock;

=======
/* Locks cache while searching for requested block. */
struct lock cache_lock;

struct rw_lock {
	struct lock r; // Lock used for atomic operations to rw_lock
	struct semaphore g; // Lock acquired by writers to ensure mutual exclusion
	
	int b; // Number of accessors
};


>>>>>>> Stashed changes
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
<<<<<<< Updated upstream
struct cache_block *cache_read_block(block_sector_t sector); /* Reads block from cache. */
struct cache_block *cache_write_block(block_sector_t sector); /* Writes to block in cache. */
=======
void cache_read_block(struct inode *in, block_sector_t sector, char *buffer); /* Reads block from cache. */
void cache_write_block(struct inode *in, block_sector_t sector, char *data); /* Writes to block in cache. */


>>>>>>> Stashed changes

#endif // #ifndef BUFFER_CACHE
