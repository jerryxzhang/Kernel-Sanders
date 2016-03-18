#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/cache.h"
#include "userprog/process.h"

/*! Partition that contains the file system. */
struct block *fs_device;

block_sector_t filesys_size(void);
static void do_format(void);

/*! Initializes the file system module.
    If FORMAT is true, reformats the file system. */
void filesys_init(bool format) {
    fs_device = block_get_role(BLOCK_FILESYS);
    if (fs_device == NULL)
        PANIC("No file system device found, can't initialize file system.");
	
    inode_init();
    free_map_init();

    if (format) 
        do_format();

    free_map_open();
}

block_sector_t filesys_size(void) {
    return block_size(fs_device);
}

/*! Shuts down the file system module, writing any unwritten data to disk. */
void filesys_done(void) {
	/* Write blocks in cache back to memory. */
	refresh_cache();
	
    free_map_close();
}

static bool resolve_path(const char* path, struct dir **dir, char *name){
    off_t length;
    bool is_dir;
    struct inode *inode;
    if(*path == '\0'){
        return false;
    }
    else if (*path == '/'){
        *dir = dir_open_root();
        path++;
    }
    else
        *dir = dir_reopen(process_current()->working_dir);
    while(true){
        length = 0;
        while (path[length] != '/' && path[length] != '\0'){
            if(++length > NAME_MAX){
                dir_close(*dir);
                return false;
            }
                
        }
        memcpy(name, path, length);
        name[length] = '\0';
        if(path[length] == '/') {
            if(!dir_lookup(*dir, name, &inode, &is_dir) || !is_dir){
                dir_close(*dir);
                return false;
            }
            dir_close(*dir);
            *dir = dir_open(inode);
        }
        else if(path[length] == '\0'){
            if(!length)
                memcpy(name, ".", 2);
            return true;
        }
        path += length + 1;

    }
    return false;
}

/*! Creates a file named NAME with the given INITIAL_SIZE.  Returns true if
    successful, false otherwise.  Fails if a file named NAME already exists,
    or if internal memory allocation fails. */
bool filesys_create(const char *path, off_t initial_size) {
    block_sector_t inode_sector = 0;
    char name[NAME_MAX + 1];
    struct dir *dir = NULL;
    if(!resolve_path(path, &dir, name))
        return false;
    bool success = (free_map_allocate(&inode_sector) &&
                    inode_create(inode_sector, initial_size) &&
                    dir_add(dir, name, inode_sector));
    if (!success && inode_sector != 0) 
        free_map_release(inode_sector);
    dir_close(dir);

    return success;
}

/*! Opens the file with the given NAME.  Returns the new file if successful
    or a null pointer otherwise.  Fails if no file named NAME exists,
    or if an internal memory allocation fails. */
struct file * filesys_open(const char *path) {
    char name[NAME_MAX + 1];
    struct dir *dir = NULL;
    struct inode *inode = NULL;
    bool is_dir;
    if(!resolve_path(path, &dir, name))
        return NULL;

    if (dir != NULL)
        dir_lookup(dir, name, &inode, &is_dir);
    dir_close(dir);

    return file_open(inode, is_dir);
}

/*! Deletes the file named NAME.  Returns true if successful, false on failure.
    Fails if no file named NAME exists, or if an internal memory allocation
    fails. */
bool filesys_remove(const char *path) {
    char name[NAME_MAX + 1];
    struct dir *dir = NULL;
    if(!resolve_path(path, &dir, name))
        return false;
    bool success = dir != NULL && dir_remove(dir, name);
    dir_close(dir);

    return success;
}

/*! Formats the file system. */
static void do_format(void) {
    printf("Formatting file system...");
    free_map_create();
    if (!dir_create(ROOT_DIR_SECTOR, 16, NULL))
        PANIC("root directory creation failed");
    free_map_close();
    printf("done.\n");
}

