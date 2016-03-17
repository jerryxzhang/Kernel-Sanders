#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/cache.h"
#include "threads/malloc.h"

/*! Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define NUM_DIRECT 60
#define NUM_SINGLE_INDIRECT 40
#define NUM_DOUBLE_INDIRECT 1

#define ENTRIES_PER_SECTOR (BLOCK_SECTOR_SIZE / (sizeof(block_sector_t)))

/*! On-disk inode.
    Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk {
    block_sector_t direct[NUM_DIRECT];
    block_sector_t single_indirect[NUM_SINGLE_INDIRECT];
    block_sector_t double_indirect;
    off_t length;                       /*!< File size in bytes. */
    unsigned magic;                     /*!< Magic number. */

    /** Unused bytes **/
    unsigned unused[ENTRIES_PER_SECTOR-NUM_DIRECT-NUM_SINGLE_INDIRECT-NUM_DOUBLE_INDIRECT-2];
};

/*! In-memory inode. */
struct inode {
    struct list_elem elem;              /*!< Element in inode list. */
    block_sector_t sector;              /*!< Sector number of disk location. */
    int open_cnt;                       /*!< Number of openers. */
    bool removed;                       /*!< True if deleted, false otherwise. */
    int deny_write_cnt;                 /*!< 0: writes ok, >0: deny writes. */
};


bool is_valid_inode(const struct inode_disk *inode);

/** Returns if the inode is well formed. */
bool is_valid_inode(const struct inode_disk *inode) {
    return inode != NULL && inode->magic == INODE_MAGIC;
}

/**
 * Returns the block index of the block containing data at position. 
 * Traverses the inode tree structure until the index is found.
 * The position must be less than the file size.
 */
static block_sector_t byte_to_sector(const struct inode *inode, off_t pos) {
    ASSERT(inode != NULL);
    printf("looking in file %d\n", inode->sector);
    block_sector_t blocks = pos / BLOCK_SECTOR_SIZE;
    block_sector_t single;
    struct inode_disk *data;
    block_sector_t *single_data;
    block_sector_t * double_data;

    // Get the main node data from cache
    data = (struct inode_disk *)cache_read_block(inode->sector)->data;
    ASSERT(is_valid_inode(data));
    ASSERT(pos < data->length);

    // Check it its in the direct nodes
    if (blocks < NUM_DIRECT) {
        return data->direct[blocks];
    } 
    // Otherwise, skip over them
    blocks -= NUM_DIRECT;

    single = blocks / ENTRIES_PER_SECTOR; 
    blocks = blocks % ENTRIES_PER_SECTOR;
    // Check if it fits in the single indirect nodes.
    if (single < NUM_SINGLE_INDIRECT) {
        single_data = (block_sector_t *) cache_read_block(data->single_indirect[single])->data;
        return single_data[blocks];
    }
    // Skip over single indirects
    single-= NUM_SINGLE_INDIRECT;

    // Only one double indirect, so the rest must fit
    ASSERT(single < ENTRIES_PER_SECTOR);

    double_data = (block_sector_t *) cache_read_block(data->double_indirect)->data;
    single_data = (block_sector_t *) cache_read_block(double_data[single])->data;
    return single_data[blocks];
}

/*! List of open inodes, so that opening a single inode twice
    returns the same `struct inode'. */
static struct list open_inodes;

/*! Initializes the inode module. */
void inode_init(void) {
    list_init(&open_inodes);
}

bool allocate_direct(block_sector_t *sector, off_t *length) {
    ASSERT(*length > 0);
    if (!free_map_allocate(sector)) return false;
    *length -= BLOCK_SECTOR_SIZE;
    block_clear(fs_device, *sector);
    printf("Allocating direct sector %d %d\n", *sector, *length);
    return true;
}
bool allocate_indirect(block_sector_t *sector, off_t *length) {
    ASSERT(*length > 0);
    if (!free_map_allocate(sector)) return false;
    printf("Allocating indirect sector %d %d\n", *sector, *length);
    int i;
    block_clear(fs_device, *sector);
    struct cache_block *cache_block = cache_read_block(*sector);
    block_sector_t *sectors = (block_sector_t*) cache_block->data;
    for (i = 0; i < ENTRIES_PER_SECTOR; i++) {
        if (!allocate_direct(sectors + i, length)) return false;
        if (*length <= 0) break;
    }   
    cache_block->dirty = true;
    return true;
}
bool allocate_double_indirect(block_sector_t *sector, off_t *length) {
    ASSERT(*length > 0);
    if (!free_map_allocate(sector)) return false;
    printf("Allocating double indirect sector %d %d\n", *sector, *length);
    int i;
    block_clear(fs_device, *sector);
    struct cache_block *cache_block = cache_read_block(*sector);
    block_sector_t *sectors = (block_sector_t*) cache_block->data;
    for (i = 0; i < ENTRIES_PER_SECTOR; i++) {
        if (!allocate_indirect(sectors + i, length)) return false;
        if (*length <= 0) break;
    }   
    cache_block->dirty = true;
    return true;
}
void free_direct(block_sector_t sector, off_t *length) {
    ASSERT(*length > 0);
    free_map_release(sector);
    *length -= BLOCK_SECTOR_SIZE;
}
void free_indirect(block_sector_t sector, off_t *length) {
    ASSERT(*length > 0);
    struct cache_block *cache_block = cache_read_block(sector);
    int i;
    block_sector_t *sectors = (block_sector_t*) cache_block->data;
    for (i = 0; i < ENTRIES_PER_SECTOR; i++) {
        free_direct(sectors[i], length);
        if (*length <= 0) break;
    }
    free_map_release(sector);
}
void free_double_indirect(block_sector_t sector, off_t *length) {
    ASSERT(*length > 0);
    struct cache_block *cache_block = cache_read_block(sector);
    int i;
    block_sector_t *sectors = (block_sector_t*) cache_block->data;
    for (i = 0; i < ENTRIES_PER_SECTOR; i++) {
        free_indirect(sectors[i], length);
        if (*length <= 0) break;
    }
    free_map_release(sector);
}

/*! Initializes an inode with LENGTH bytes of data and
    writes the new inode to sector SECTOR on the file system
    device.
    Returns true if successful.
    Returns false if memory or disk allocation fails. */
bool inode_create(block_sector_t sector, off_t length) {
    struct inode_disk *disk_inode = NULL;
    ASSERT(length >= 0);
    printf("Creating inode at %d\n", sector);
    /* If this assertion fails, the inode structure is not exactly
       one sector in size, and you should fix that. */
    ASSERT(sizeof *disk_inode == BLOCK_SECTOR_SIZE);

    block_clear(fs_device, sector);
    struct cache_block *cache_block = cache_read_block(sector);
    int i;

    disk_inode = (struct inode_disk *) cache_block->data;
    disk_inode->length = length;
    disk_inode->magic = INODE_MAGIC;

    // Allocate necessary direct nodes
    for (i = 0; i < NUM_DIRECT; i++) {
        printf("%d\n", length);
        if (length <= 0) {
            break;
        }
        if (!allocate_direct(&disk_inode->direct[i], &length)) return false;
    }

    // Allocate necessary single indirect nodes
    for (i = 0; i < NUM_SINGLE_INDIRECT; i++) {
        if (length <= 0)
            break;
        if (!allocate_indirect(&disk_inode->single_indirect[i], &length)) return false;
    }

    // Allocate a double indirect node if necessary
    if (length > 0 && !allocate_double_indirect(&disk_inode->double_indirect, &length)) return false;

    cache_block->dirty = true;
    return true;
}

/*! Reads an inode from SECTOR
    and returns a `struct inode' that contains it.
    Returns a null pointer if memory allocation fails. */
struct inode * inode_open(block_sector_t sector) {
    struct list_elem *e;
    struct inode *inode;
    printf("Opening an inode\n");
    /* Check whether this inode is already open. */
    for (e = list_begin(&open_inodes); e != list_end(&open_inodes);
         e = list_next(e)) {
        inode = list_entry(e, struct inode, elem);
        if (inode->sector == sector) {
            inode_reopen(inode);
            return inode; 
        }
    }

    /* Allocate memory. */
    inode = malloc(sizeof *inode);
    if (inode == NULL)
        return NULL;

    /* Initialize. */
    list_push_front(&open_inodes, &inode->elem);
    inode->sector = sector;
    inode->open_cnt = 1;
    inode->deny_write_cnt = 0;
    inode->removed = false;
    return inode;
}

/*! Reopens and returns INODE. */
struct inode * inode_reopen(struct inode *inode) {
    if (inode != NULL)
        inode->open_cnt++;
    return inode;
}

/*! Returns INODE's inode number. */
block_sector_t inode_get_inumber(const struct inode *inode) {
    return inode->sector;
}

/*! Closes INODE and writes it to disk.
    If this was the last reference to INODE, frees its memory.
    If INODE was also a removed inode, frees its blocks. */
void inode_close(struct inode *inode) {
    /* Ignore null pointer. */
    if (inode == NULL)
        return;

    /* Release resources if this was the last opener. */
    if (--inode->open_cnt == 0) {
        /* Remove from inode list and release lock. */
        list_remove(&inode->elem);
 
        /* Deallocate blocks if removed. */
        if (inode->removed) {
            struct cache_block *cache_block = cache_read_block(inode->sector);
            struct inode_disk *disk_inode = (struct inode_disk *) cache_block->data;
            int i;

            off_t length = disk_inode->length;
            for (i = 0; i < NUM_DIRECT; i++) {
                if (length <= 0) break;
                free_direct(disk_inode->direct[i], &length);
            }
            // Allocate necessary single indirect nodes
            for (i = 0; i < NUM_SINGLE_INDIRECT; i++) {
                if (length <= 0) break;
                free_indirect(disk_inode->single_indirect[i], &length);
            }
        
            // Allocate a double indirect node if necessary
            if (length > 0) free_double_indirect(disk_inode->double_indirect, &length);
            
            free_map_release(inode->sector);
        }
        free(inode); 
    }
}

/*! Marks INODE to be deleted when it is closed by the last caller who
    has it open. */
void inode_remove(struct inode *inode) {
    ASSERT(inode != NULL);
    inode->removed = true;
}

off_t inode_length(const struct inode *inode) {
    struct inode_disk* inode_disk = (struct inode_disk*) cache_read_block(inode->sector)->data;
    return inode_disk->length;
}

/*! Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t inode_read_at(struct inode *inode, void *buffer_, off_t size, off_t offset) {
    uint8_t *buffer = buffer_;
    struct cache_block *cache_block;
    off_t bytes_read = 0;

    while (size > 0) {
        /* Disk sector to read, starting byte offset within sector. */
        block_sector_t sector_idx = byte_to_sector (inode, offset);
        int sector_ofs = offset % BLOCK_SECTOR_SIZE;

        /* Bytes left in inode, bytes left in sector, lesser of the two. */
        off_t inode_left = inode_length(inode) - offset;
        int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
        int min_left = inode_left < sector_left ? inode_left : sector_left;

        /* Number of bytes to actually copy out of this sector. */
        int chunk_size = size < min_left ? size : min_left;
        if (chunk_size <= 0)
            break;
            
        lock_acquire(&cache_lock); // Need to move
        /* Load data into cache. */
        cache_block = cache_read_block(sector_idx);
        
        /* Copy from cache into caller's buffer. */
        memcpy(buffer + bytes_read, cache_block->data + sector_ofs, chunk_size);
        cache_read_end(cache_block);
        lock_release(&cache_lock); // Need to move
      
        /* Advance. */
        size -= chunk_size;
        offset += chunk_size;
        bytes_read += chunk_size;
    }

    return bytes_read;
}

/*! Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
    Returns the number of bytes actually written, which may be
    less than SIZE if end of file is reached or an error occurs.
    (Normally a write at end of file would extend the inode, but
    growth is not yet implemented.) */
off_t inode_write_at(struct inode *inode, const void *buffer_, off_t size, off_t offset) {
    const uint8_t *buffer = buffer_;
    struct cache_block *cache_block;
    off_t bytes_written = 0;

    if (inode->deny_write_cnt)
        return 0;

    while (size > 0) {
        /* Sector to write, starting byte offset within sector. */
        block_sector_t sector_idx = byte_to_sector(inode, offset);
        int sector_ofs = offset % BLOCK_SECTOR_SIZE;

        /* Bytes left in inode, bytes left in sector, lesser of the two. */
        off_t inode_left = inode_length(inode) - offset;
        int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
        int min_left = inode_left < sector_left ? inode_left : sector_left;
        
        printf("Wrote %d bytes\n", inode_left);

        /* Number of bytes to actually write into this sector. */
        int chunk_size = size < min_left ? size : min_left;
        if (chunk_size <= 0)
            break;

        /* Get the data from the cache and write to it. */
        lock_acquire(&cache_lock); // Need to move...
        cache_block = cache_write_block(sector_idx);
        memcpy(cache_block->data + sector_ofs, buffer + bytes_written, chunk_size);
        cache_write_end(cache_block);
        lock_release(&cache_lock); // Need to move...

        /* Advance. */
        size -= chunk_size;
        offset += chunk_size;
        bytes_written += chunk_size;
    }


    return bytes_written;
}

/*! Disables writes to INODE.
    May be called at most once per inode opener. */
void inode_deny_write (struct inode *inode) {
    inode->deny_write_cnt++;
    ASSERT(inode->deny_write_cnt <= inode->open_cnt);
}

/*! Re-enables writes to INODE.
    Must be called once by each inode opener who has called
    inode_deny_write() on the inode, before closing the inode. */
void inode_allow_write (struct inode *inode) {
    ASSERT(inode->deny_write_cnt > 0);
    ASSERT(inode->deny_write_cnt <= inode->open_cnt);
    inode->deny_write_cnt--;
}

