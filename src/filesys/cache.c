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
struct cache_block buffer[CACHE_BLOCKS];

/* Used by read ahead thread to know if reading ahead required. */
bool read_ahead;
/* Block to read ahead. */
block_sector_t next_block;

/* Periodically refreshes cache. */
void refresh_cache_cycle(void *aux UNUSED);
/* Updates last access value for every block in cache. */
void update_accesses(void *aux UNUSED);
/* Chooses a block to evict from the cache and evicts it. */
struct cache_block *cache_evict(void);
/* Finds the block in the cache. */
struct cache_block *find_block(block_sector_t sector);
/* Implemented to read next block from disk on block read. */
void cache_read_ahead(void *args);

void cache_read_begin(struct cache_block *cache_block);
void cache_write_begin(struct cache_block *cache_block);
bool cache_read_try(struct cache_block *cache_block);
bool cache_write_try(struct cache_block *cache_block);
void cache_upgrade(struct cache_block *cache_block);
void cache_downgrade(struct cache_block *cache_block);

/*!
 * init_cache
 * 
 * @descr Initializes the file cache.
 */
void cache_init(void) {
    
    read_ahead = 0;
    
    int i;
    for (i = 0; i < CACHE_BLOCKS; i++) {    
        buffer[i].valid = false;
        buffer[i].sector = 0;

        /* Initialize lock. */
        sema_init(&buffer[i].lock.g, 1);
        lock_init(&buffer[i].lock.r);
        lock_init(&buffer[i].lock.extra);
        buffer[i].lock.b = 0;
        
    }
    
    /* Devote threads to refreshing cache (write dirty blocks to memory) and
     * updating accessed bits. */
//    thread_create("cache_refresh", PRI_DEFAULT, refresh_cache_cycle, NULL);
    thread_create("cache_accesses", PRI_DEFAULT, update_accesses, NULL);
    
    /* Takes care of reading ahead if there is reading ahead to be done. */
//    thread_create("cache_read_ahead", PRI_DEFAULT, cache_read_ahead, NULL);
}

/*!
 * cache_read_block
 * 
 * @descr Reads from a block in the cache.  If the block is not in the cache,
 *        it is loaded into the cache then read from.
 * 
 * @param sector - The sector on the disk to read from
 * @param buffer - Cache read from and loaded into caller's buffer
 * 
 * @return cache_block - Pointer to the cache block read from.
 */

struct cache_block *cache_read_block(block_sector_t sector) {
    /* find_block sets accessed bit */
    struct cache_block *cache_block = find_block(sector);
    
    return cache_block;
}

void cache_read_begin(struct cache_block *cache_block) {
    /* Reading from block.  Want to increment b to mark that there is an access
     * and if the lock is not held by anyone, acquire it to ensure nothing
     * writes to this block while reading. */
    lock_acquire(&cache_block->lock.r); // Ensure this is atomic
    cache_block->lock.b += 1;
    if (cache_block->lock.b == 1) { // Only accessor, lock is free
        lock_acquire(&cache_block->lock.extra);
        sema_down(&cache_block->lock.g);
        lock_release(&cache_block->lock.extra);
    }
    lock_release(&cache_block->lock.r);
}

bool cache_read_try(struct cache_block *cache_block) {
    if (!lock_try_acquire(&cache_block->lock.r)) return false;
    
    cache_block->lock.b += 1;
    if (cache_block->lock.b == 1) {
        if (!sema_try_down(&cache_block->lock.g)) {
            cache_block->lock.b -= 1;
            lock_release(&cache_block->lock.r);
            return false;
        }
    }
    lock_release(&cache_block->lock.r);
    return true;
}

/*!
 * cache_read_ahead
 * 
 * @descr Used to read the next block into the cache.  This should be run on
 *        a thread of its own while the first block is still being read.
 */
void cache_read_ahead(void *aux UNUSED) {
    while(1) {
        if (read_ahead) {
            /* Mark that we have read ahead. */
            read_ahead = 0;            
            find_block(next_block);
        }
    }
}

/*!
 * cache_write_block
 * 
 * @descr Writes to a block in the cache.  If the block is not in the cache,
 *        it is loaded into the cache then written to.
 * 
 * @param sector - The sector on disk to write to
 * @param data - The data to write to the cache
 */

struct cache_block *cache_write_block(block_sector_t sector) {
    /* Find a space for/a pointer to the block in the cache. */
    struct cache_block *cache_block = find_block(sector);
    cache_upgrade(cache_block); 
    return cache_block;
}

void cache_write_begin(struct cache_block *cache_block) {
    /* Must have a lock before writing to cache. */
    lock_acquire(&cache_block->lock.extra);
    sema_down(&cache_block->lock.g);
    lock_release(&cache_block->lock.extra);
}

bool cache_write_try(struct cache_block *cache_block) {
    /* Must have a lock before writing to cache. */
    return sema_try_down(&cache_block->lock.g);
}

void cache_upgrade(struct cache_block *cache_block) {
    lock_acquire(&cache_block->lock.r); // Ensure this is atomic
    cache_block->lock.b -= 1;
    if (cache_block->lock.b == 0) {
        lock_release(&cache_block->lock.r);
        return;
    } else {
        lock_acquire(&cache_block->lock.extra);
        lock_release(&cache_block->lock.r);
        sema_down(&cache_block->lock.g);
        lock_release(&cache_block->lock.extra);
        return;
    }
}

void cache_downgrade(struct cache_block *cache_block) {
    lock_acquire(&cache_block->lock.r); // Ensure this is atomic
    ASSERT(cache_block->lock.b == 0);
    cache_block->lock.b += 1;
    lock_release(&cache_block->lock.r);    
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
struct cache_block *find_block(block_sector_t sector) {
//    printf("Looking for block %d\n", sector);
    
    /* Block in cache (NULL if not found) */
    struct cache_block *cache_block = NULL;
    
    /* Don't attempt to access block outside of device. */
    ASSERT(sector < block_size(fs_device));
    
    int i;
    for (i = 0; i < CACHE_BLOCKS; i++) {  
        /* If this is a match, stop iterating. */
        if (buffer[i].sector == sector) {
//            printf("found block %d in spot %d\n", sector, i);
            cache_read_begin(buffer+i);
            ASSERT(buffer[i].sector == sector);
            cache_block = buffer + i;
//            cache_read_end(cache_block);
  //          cache_write_begin(cache_block);
            break;
        }
    }
    
    if (cache_block == NULL) {
        /* Block was not in the cache, retrieve it from memory. */
        
        // See if there is free space
        for (i = 0; i < CACHE_BLOCKS; i++) {    
            if (!cache_write_try(buffer+i)) continue;
            if (!buffer[i].valid) {
                cache_block = buffer + i;
                cache_block->valid = true;
                break;
            }
            cache_write_end(buffer+i);
        }
        
        /* Need to evict.  Retain space from eviction. */
        if (cache_block == NULL) {
            cache_block = cache_evict();
        }        
            
        /* Import block. */
        block_read(fs_device, sector, (uint8_t *) cache_block->data);
        cache_block->sector = sector;
        
        /* Caller will set these accordingly, but should start cleared. */
        cache_block->dirty = 0;
        cache_block->recent_accesses = 0;

        cache_downgrade(cache_block); 
    }
    
    /* While read happens, load next block from disk into cache. */
    /* This is done on its own thread so it happens in the background. */
    next_block = sector + 1;
    read_ahead = 1;
    
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
    int i;
    
    /* Block to evict. */
    struct cache_block *victim = NULL;
    struct cache_block *temp;
    uint64_t oldest = ~0;
    for(i = 0; i < CACHE_BLOCKS; i++) {
        if (!cache_write_try(buffer+i)) continue;

        /* Get next eviction candidate. */
        temp = buffer + i;
        ASSERT(temp->valid);
        /* Checking <= oldest because if every block has been accessed every
         * clock for the past 64 clocks, then this would otherwise fail to
         * return a victim when indeed there was a victim. */

        if (temp->recent_accesses <= oldest) {
            if (victim) cache_write_end(victim);
            oldest = temp->recent_accesses;
            victim = temp;
            continue;
        }
        cache_write_end(buffer+i);
    }

    /* If no victim could have been chosen, don't try to remove anything. */
    if (victim != NULL) {
        /* If this is dirty, write it to memory before removing. */
        if (victim->dirty)
            block_write(fs_device, victim->sector, (uint8_t *) victim->data);
    } else {
        PANIC("Cache is full and completely locked down!\n");
    }
    //printf("Evicted %d\n", victim->sector);
    /* Return the block for the caller to handle. */
    return victim;
}

/*!
 * refresh_cache
 * 
 * @descr Rewrites all dirty blocks in cache to memory.
 */
void refresh_cache(void) {
    int i; 
    /* Temp for writing blocks to memory. */
    struct cache_block *blk;
    
    for(i = 0; i < CACHE_BLOCKS; i++) {
        /* Get the next cache file block. */
        blk = buffer + i;
        if (!blk->valid) continue;
        if (!cache_read_try(blk)) continue;
        
        /* Only care about writing to memory if dirty. */
        if (blk->dirty) {
            block_write(fs_device, blk->sector, (void *) blk->data);
            blk->dirty = 0;
        }
        cache_read_end(blk);
    }
}

/*!
 * refresh_cache_cycle
 * 
 * @descr Periodically refreshes cache. 
 */
void refresh_cache_cycle(void *aux UNUSED) {
    while (1) {
        /* Write everything that's dirty back to memory. */
        refresh_cache();
        
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
void update_accesses(void *aux UNUSED) {
    int i;
    struct cache_block *blk;
    
    while (1) {
        for (i = 0; i < CACHE_BLOCKS; i++) {
            /* Use iterator to get next block in cache. */
            blk = buffer + i;
            if (!blk->valid) continue; 
            if (!cache_read_try(blk)) continue;
            /* Make space for access bit on left. */
            blk->recent_accesses >>= 1;
            
            /* Put access bit at most significant bit.  This ensures most recently
             * accessed blocks have largest recent access value. */
            /*    NOTE: recent_accesses 64 bits; shifting left accordingly. */
            blk->recent_accesses |= ((uint64_t) blk->accessed << 63);
            
            /* Clear the accessed bit so next time we accurately add to its access
             * recency, but only if nothing is still accessing it. */
            if (blk->lock.b > 0)
                blk->accessed = 0;

            cache_read_end(blk);
        }
        
        /* Sleep until next time we want to update. */
        timer_msleep(UPDATE_ACCESS_MS);
    }
}
void cache_read_end(struct cache_block *cache_block) {
    
    /* Release lock and account for stopping reading from cache. */
    lock_acquire(&cache_block->lock.r);
    cache_block->lock.b -= 1;
    if (cache_block->lock.b == 0) // Was last accesor, free lock
        sema_up(&cache_block->lock.g);
    lock_release(&cache_block->lock.r);
    
}

void cache_write_end(struct cache_block *cache_block) {
    /* Done writing, free lock. */
    sema_up(&cache_block->lock.g);
    
    /* Mark as dirty to show that it has changed and is done changing. */
    cache_block->dirty = 1;
}

