#include "cache.h"

#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "devices/timer.h"
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "userprog/process.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"


#define CACHE_BLOCKS 64
#define REFRESH_CACHE_MS 100 /* ms between writing dirty blocks to mem */
#define UPDATE_ACCESS_MS 1 /* ms between updating access bits. */


/* Cache of file blocks */
static struct list buffer_cache;


/* Periodically refreshes cache. */
void refresh_cache_cycle(void *unused);
/* Updates last access value for every block in cache. */
void update_accesses(void *unused);
/* Chooses a block to evict from the cache and evicts it. */
struct cache_block *cache_evict(void);
/* Finds the block in the cache. */
struct cache_block *find_block(struct inode *in, block_sector_t sector);
/* Implemented to read next block from disk on block read. */
void cache_read_ahead(void *args);



/*!
 * init_cache
 * 
 * @descr Initializes the file cache.
 */
void cache_init(void) {
    /* Initialize the cache list. */
    list_init(&buffer_cache);
    lock_init(&cache_lock);
    
    /* Devote threads to refreshing cache (write dirty blocks to memory) and
     * updating accessed bits. */
    thread_create("cache_refresh", PRI_DEFAULT, refresh_cache_cycle, NULL);
    thread_create("cache_accesses", PRI_DEFAULT, update_accesses, NULL);
}




/*!
 * cache_read_block
 * 
 * @descr Reads from a block in the cache.  If the block is not in the cache,
 *        it is loaded into the cache then read from.
 * 
 * @param in - The inode in memory containing info about read
 * @param sector - The sector on the disk to read from
 */
struct cache_block *cache_read_block(struct inode *in, block_sector_t sector) {
    /* Find a space for/ a pointer to the block in the cache. */
    /* find_block sets accessed bit */
    struct cache_block *cache_block = find_block(in, sector);
    
    /* While read happens, load next block from disk into cache. */
    /* This is done on its own thread so it happens in the background. */
    /* First allocate space for the the args array that we will give to the
     * thread function. */
    void *args = malloc(sizeof(struct inode *) + sizeof(block_sector_t));
    
    /* Load the inode and next sector into the args array. */
    *(struct inode **) args = in;
    *(block_sector_t *) (args + sizeof(struct inode *)) = sector + 1;
    
    /* Create the thread that will load block. */
    //thread_create("read_ahead", PRI_DEFAULT, cache_read_ahead, args);
    //cache_read_ahead(args);
    
    return cache_block;
}




/*!
 * cache_read_ahead
 * 
 * @descr Used to read the next block into the cache.  This should be run on
 *        a thread of its own while the first block is still being read.
 * 
 * @param in - The inode in memory containing info about read.  This is the
 *        first sizeof(struct inode *) bytes in args array.
 * @param sector - The sector on disk to read from.  This is the latter
 *        sizeof(block_sector_t) bytes in args array.
 */
void cache_read_ahead(void *args) {
    /* args unpacked into these. */
    struct inode *in;
    block_sector_t sector;
    
    /* Unpack args into our variables. */
    memcpy(&in, args, sizeof(struct inode *));
    memcpy(&sector, args + sizeof(struct inode *), sizeof(block_sector_t));
    
    /* Load next block into cache. */
    find_block(in, sector);
    
    /* This concludes this thread's purpose. */
    //thread_exit(0);
}




/*!
 * cache_write_block
 * 
 * @descr Writes to a block in the cache.  If the block is not in the cache,
 *        it is loaded into the cache then written to.
 * 
 * @param in - The inode in memory containing info about write
 * @param sector - The sector on disk to write to
 */
struct cache_block *cache_write_block(struct inode *in, block_sector_t sector) {
    /* Find a space for/a pointer to the block in the cache. */
    struct cache_block *cache_block = find_block(in, sector);
    
    /* Mark block as dirty for write (access set in find block). */
    cache_block->dirty = 1;
    
    return cache_block;
}




/*!
 * find_block
 * 
 * @descr Finds the block in the cache, or loads it into the cache, evicting
 *        a block if necessary.
 * 
 * @param blk - Block in memory associated with memory access.
 * @param sector - Sector in disk associated with memory access.
 * 
 * @return cache_block - A pointer to the cache_block.
 */
struct cache_block *find_block(struct inode *in, block_sector_t sector) {
    /* List iterator for searching cache. */
    struct list_elem *e;
    
    /* Block in cache (NULL if not found) */
    struct cache_block *cache_block = NULL;
    
    /* Don't attempt to access block outside of device. */
    if (sector >= block_size(fs_device))
        return cache_block;
    
    for(e = list_begin(&buffer_cache); e != list_end(&buffer_cache); e = list_next(e)) {
        cache_block = list_entry(e, struct cache_block, block_elem);
        
        /* If this is a match, stop iterating. */
        if (cache_block->in == in && cache_block->sector == sector)
            break;
            
        /* Otherwise, go back to NULL in case this is the last elem. */
        cache_block = NULL;
    }
    
    if (cache_block == NULL) {
        /* Block was not in the cache, retrieve it from memory. */
        
        /* First make space for the block. */
        if (list_size(&buffer_cache) < CACHE_BLOCKS)
            /* Room in cache, create new block. */
            cache_block = (struct cache_block *)malloc(sizeof(struct cache_block));
        else
            /* Need to evict.  Retain space from eviction. */
            cache_block = cache_evict();
        
        ASSERT (cache_block != NULL);
            
        /* Add the block to our cache (evict removes it) */
        list_push_back(&buffer_cache, &cache_block->block_elem);
        
        /* Import block. */
        block_read(fs_device, sector, (uint8_t *) cache_block->data);
        cache_block->in = in;
        cache_block->sector = sector;
        
        /* Caller will set these accordingly, but should start cleared. */
        cache_block->dirty = 0;
        cache_block->recent_accesses = 0;
    }
    
    /* Only entering this function if an access is being made. */
    cache_block->accessed = 1;
    /* Update recent_access so we don't accidentally immediately evict this */
    cache_block->recent_accesses |= ((uint64_t) 1 << 63);
    
    return cache_block;
}




/*!
 * cache_evict
 * 
 * @descr Finds the block in the cache whose last access is in the most distant
 *        past, then evicts it from the cache.
 */
struct cache_block *cache_evict(void) {
    /* Iterator for cache. */
    struct list_elem *e;
    
    /* Block to evict. */
    struct cache_block *victim = NULL, *temp;
    uint64_t oldest = -1; // -1 typecasts to largest 64-bit int
    
    for(e = list_begin(&buffer_cache); e != list_end(&buffer_cache); e = list_next(e)) {
        /* Get next eviction candidate. */
        temp = list_entry(e, struct cache_block, block_elem);
        
        /* Update accordingly to keep track of oldest block. */
        if (temp->recent_accesses < oldest) {
            oldest = temp->recent_accesses;
            victim = temp;
        }
        
        /* Boundary case for optimization: If recent_accesses is 0, it has not
         * been accessed in longer than we can tell.  Nothing can be, as far as
         * we know, older, so we can stop iteration here. */
        if (oldest == 0)
            break;
    }
    
    /* victim should never be NULL, but make sure just in case. */
    ASSERT(victim != NULL);
        
    /* Evict the page from the cache. */
    list_remove(&victim->block_elem);
    
    /* If this is dirty, write it to memory before removing. */
    if (victim->dirty)
        block_write(fs_device, victim->sector, (uint8_t *) victim->data);
    
    /* Return the block for the caller to handle. */
    return victim;
}




/*!
 * refresh_cache
 * 
 * @descr Rewrites all dirty blocks in cache to memory.
 */
void refresh_cache(void) {
    /* Iterator for buffer cache list. */
    struct list_elem *e;
    
    /* Temp for writing blocks to memory. */
    struct cache_block *blk;
    
    for(e = list_begin(&buffer_cache); e != list_end(&buffer_cache); e = list_next(e)) {
        /* Get the next cache file block. */
        blk = list_entry(e, struct cache_block, block_elem);
        
        /* Only care about writing to memory if dirty. */
        if (blk->dirty) {
            block_write(fs_device, blk->sector, (void *) blk->data);
            blk->dirty = 0;
        }
    }
}




/*!
 * refresh_cache_cycle
 * 
 * @descr Periodically refreshes cache. 
 */
void refresh_cache_cycle(void *unused) {
    while (1) {
        /* Write everything that's dirty back to memory. */
        lock_acquire(&cache_lock);
        refresh_cache();
        lock_release(&cache_lock);
        
        /* Sleep until next time we want to refresh. */
        timer_msleep(REFRESH_CACHE_MS);
    }
}




/*!
 * update_accesses
 * 
 * @descr Periodically updates the access values for all blocks in cache.  This
 *        is done by shifting right the recent_access value and or-ing in the
 *        accessed bit to the leftmost bit.  This way, larger values have been
 *        accessed more recently than others, or more frequently if their last
 *        access is the same.
 */
void update_accesses(void *unused) {
    /* Iterator for our buffer cache list. */
    struct list_elem *e;
    struct cache_block *blk;
    
    while (1) {
        for (e = list_begin(&buffer_cache); e != list_end(&buffer_cache); e = list_next(e)) {
            /* Use iterator to get next block in cache. */
            blk = list_entry(e, struct cache_block, block_elem);
            
            /* Make space for access bit on left. */
            blk->recent_accesses >>= 1;
            
            /* Put access bit at most significant bit.  This ensures most recently
             * accessed blocks have largest recent access value. */
            /*    NOTE: recent_accesses 64 bits; shifting left accordingly. */
            blk->recent_accesses |= ((uint64_t) blk->accessed << 63);
            
            /* Clear the accessed bit so next time we accurately add to its access
             * recency. */
            blk->accessed = 0;
        }
        
        /* Sleep until next time we want to update. */
        timer_msleep(UPDATE_ACCESS_MS);
    }
}
