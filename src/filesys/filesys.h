#ifndef FILESYS_FILESYS_H
#define FILESYS_FILESYS_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "filesys/directory.h"

/*! Sectors of system file inodes. @{ */
#define DEBUG_SECTOR 0       /*!< Reserved for debugging. */
#define FREE_MAP_SECTOR 1       /*!< Free map file inode sectors. */
#define ROOT_DIR_SECTOR 2       /*!< Root directory file inode sector. */
/*! @} */

/*! Block device that contains the file system. */
struct block *fs_device;

void filesys_init(bool format);
void filesys_done(void);
bool filesys_create(const char *name, off_t initial_size, bool is_dir);
struct file *filesys_open(const char *name);
bool filesys_remove(const char *name);
int filesys_get_inumber(struct dir*);
#endif /* filesys/filesys.h */

