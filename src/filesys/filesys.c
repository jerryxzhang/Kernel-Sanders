#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
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
    DPRINTF("resolving path %s\n", path)
    off_t length;
    bool is_dir;
    struct inode *inode;
    if(*path == '\0'){
        DPRINTF("EMPTY PATH\n")
        return false;
    }
    else if (*path == '/'){
        DPRINTF("Base dir is ROOT\n")
        *dir = dir_open_root();
        path++;
    }
    else{
        DPRINTF("BASE DIR is WORKING DIR\n")
        *dir = dir_reopen(process_current()->working_dir);
    }
    while(true){
        length = 0;
        while (path[length] != '/' && path[length] != '\0'){
            if(++length > NAME_MAX){
                DPRINTF("FAILED PARSE\n");
                dir_close(*dir);
                return false;
            }
                
        }
        memcpy(name, path, length);
        name[length] = '\0';
        DPRINTF("NEXT TARGET IS %s\n", name)
        if(path[length] == '/') {
            DPRINTF("TARGET IS DIR\n")
            if(!dir_lookup(*dir, name, &inode, &is_dir) || !is_dir){
                DPRINTF("FAILED TO FIND IT\n");
                dir_close(*dir);
                return false;
            }
            dir_close(*dir);
            *dir = dir_open(inode);
            if(dir){
                DPRINTF("OPENED\n")
            }
            else{
                DPRINTF("FAILED\n")
            }
        }
        else if(path[length] == '\0'){
            if(!length)
                memcpy(name, ".", 2);
            DPRINTF("FINAL TARGET IS %s\n", name)
            return true;
        }
        path += length + 1;

    }
    return false;
}

/*! Creates a file named NAME with the given INITIAL_SIZE.  Returns true if
    successful, false otherwise.  Fails if a file named NAME already exists,
    or if internal memory allocation fails. */
bool filesys_create(const char *path, off_t initial_size, bool is_dir) {
    block_sector_t inode_sector = 0;
    char name[NAME_MAX + 1];
    struct dir *dir = NULL;
    if(!resolve_path(path, &dir, name))
        return false;
    bool success = (free_map_allocate(&inode_sector) &&
                    (is_dir ? dir_create(inode_sector, initial_size,
                        filesys_get_inumber(dir)): 
                        inode_create(inode_sector, initial_size)) &&
                    dir_add(dir, name, inode_sector, is_dir));
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
    if (!dir_create(ROOT_DIR_SECTOR, 4, ROOT_DIR_SECTOR))
        PANIC("root directory creation failed");
    free_map_close();
    printf("done.\n");
}

/*! Returns the sector number of a given directory or file */
int filesys_get_inumber(struct dir* dir){
    return inode_get_inumber(dir_get_inode(dir));
}