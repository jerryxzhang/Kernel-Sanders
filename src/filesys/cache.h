#ifndef BUFFER_CACHE
#define BUFFER_CACHE


#include <list.h>
#include "devices/block.h"


struct file_block {
    struct list_elem block_elem; // For iterating over buffer cache
    
    char data[BLOCK_SECTOR_SIZE]; // Data being cached
    struct inode *in; // Corresponding inode in memory
    block_sector_t sector; // in->sector gives incomplete type
    
    uint64_t recent_accesses; // For checking how recently this was accessed
    bool dirty; // Set if block has been written to since last write to memory
    bool accessed; // Set if accessed since last check
};


void cache_init(void); /* Initializes buffer cache. */
void cache_read_block(struct inode *in, block_sector_t sector); /* Reads block from cache. */
void cache_write_block(struct inode *in, block_sector_t sector); /* Writes to block in cache. */



#endif // #ifndef BUFFER_CACHE
