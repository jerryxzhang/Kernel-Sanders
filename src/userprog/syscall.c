#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/exception.h"
#include "vm/page.h"
#include "filesys/directory.h"
#include <string.h>

#define MAX_GLOBAL_MAPPINGS MAX_PROCESSES * 2

struct mmapping {
    void* addr;
    struct file *file;
};

static struct mmapping all_mmappings[MAX_GLOBAL_MAPPINGS];
static struct lock mmap_lock;

#define MAX_GLOBAL_FILES MAX_PROCESSES * 2

static void syscall_handler(struct intr_frame *);
int getArg(int argnum, struct intr_frame *f);

/*
   Functions to check whether the given user memory address is valid for 
   reading (r), writing(w). Valid for writing means that it's also valid
   for reading, so only need to call w_valid().
 */

static bool r_valid(uint8_t *uaddr);
static bool w_valid(uint8_t *uaddr);
static struct lock filesys_lock;

static struct lock filesys_lock;
static struct file* open_files[MAX_GLOBAL_FILES];

void syscall_init(void) {
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
    lock_init(&filesys_lock);
    lock_init(&mmap_lock);
}

static void syscall_handler(struct intr_frame *f) {
    if (!w_valid(f->esp)) thread_exit(EXIT_FAILURE);
    thread_current()->esp = f->esp;
    thread_current()->in_sc = true;

    int syscall_num = getArg(0, f);
    
    switch(syscall_num) {
        case SYS_HALT:
            halt();
            break;
        case SYS_EXIT:
            exit((int) getArg(1, f));
            break;
        case SYS_EXEC: 
            f->eax = exec((const char*) getArg(1, f));
            break; 
        case SYS_WAIT:
            f->eax = wait((pid_t) getArg(1, f));
            break;
        case SYS_CREATE:
            f->eax = create((const char*) getArg(1, f), 
                (unsigned) getArg(2, f));       
            break;
        case SYS_REMOVE:
            f->eax = remove((const char*) getArg(1, f));       
            break;
        case SYS_OPEN: 
            f->eax = open((const char*) getArg(1, f));       
            break;
        case SYS_FILESIZE: 
            f->eax = filesize(getArg(1, f));       
            break;
        case SYS_READ:
            f->eax = read(getArg(1, f), (void *) getArg(2, f), 
                (unsigned) getArg(3, f));       
            break;
        case SYS_WRITE:
            f->eax = write(getArg(1, f), (void *) getArg(2, f), 
                (unsigned) getArg(3, f));       
            break;
        case SYS_SEEK:
            seek(getArg(1, f), (unsigned)getArg(2, f));
            break;
        case SYS_TELL:
            f->eax = tell(getArg(1, f));
            break;
        case SYS_CLOSE:
            close(getArg(1, f));
            break;
        case SYS_MMAP:
            f->eax = mmap(getArg(1, f), (void *) getArg(2, f));
            break;
        case SYS_MUNMAP:
            munmap((mapid_t)getArg(1, f));
            break;
        case SYS_CHDIR:
            f->eax = chdir((const char*) getArg(1, f));
            break;
        case SYS_MKDIR:
            f->eax = mkdir((const char*) getArg(1, f));
            break;
        case SYS_READDIR:
            f->eax = readdir(getArg(1, f), (char *) getArg(2, f));
            break;
        case SYS_ISDIR:
            f->eax = isdir(getArg(1, f));
            break;
        case SYS_INUMBER:
            f->eax = inumber(getArg(1, f));
            break;
        default:
            printf("Not implemented!\n");
            thread_exit(EXIT_FAILURE);
            break;
    }
    thread_current()->in_sc = false;
}

void halt(void){
    shutdown_power_off();
}

void exit(int status){
    thread_exit(status);
}

pid_t exec(const char *file){
    pid_t pid;
    if (!r_valid((uint8_t*) file))
        return PID_ERROR;
    pid = (int) process_execute(file);
    return pid;
}

int wait(pid_t pid){
    return process_wait(pid);
}

bool create(const char *file, unsigned initial_size){
    DPRINTF("CREATING FILE\n")
    return create_dir_entry(file, initial_size, false);
}

bool create_dir_entry(const char *path, unsigned initial_size, bool is_dir){
    bool status = false;
    if (!r_valid((uint8_t *)path)){
        thread_exit(EXIT_FAILURE);
    }
    else{
        status = filesys_create(path, initial_size, is_dir);
    }
    return status;
}

bool remove(const char *file){
    DPRINTF("Removing %s\n", file)
    bool status = false;
    if (!r_valid((uint8_t *)file)){
        thread_exit(EXIT_FAILURE);
    }
    else{
        status = filesys_remove(file);
    }
    return status;
}   

int open(const char *file){
    int* files = process_current()->files;
    int fd = 2;
    int global_slot = 0;
    struct file* f;
    if (!r_valid((uint8_t *)file)){
        thread_exit(EXIT_FAILURE);
        return EXIT_FAILURE;
    }
    while (fd < MAX_FILES && files[fd] != -1)
        fd++;
    if (fd == MAX_FILES)
        return EXIT_FAILURE;
    if (!(f = filesys_open(file)))
        return EXIT_FAILURE;
    lock_acquire(&filesys_lock);
    while (global_slot < MAX_GLOBAL_FILES && 
        open_files[global_slot] != NULL)
        global_slot++;
    if (global_slot == MAX_GLOBAL_FILES){
        lock_release(&filesys_lock);
        file_close(f);
        return EXIT_FAILURE;
    }
    open_files[global_slot] = f;      
    lock_release(&filesys_lock);
    files[fd] = global_slot;
    return fd;
}

int filesize(int fd){
    int index;
    int fsize = -1;
    if (fd >= 0 && fd < MAX_FILES) {
        index = process_current()->files[fd];
        if (index != -1) {
            if (open_files[index])
                fsize = file_length(open_files[index]);
        }
    }
    return fsize;
}

int read(int fd, void *buffer, unsigned length){
    int index;
    int bytes_read = EXIT_FAILURE;
    int chunk_read;
    off_t to_read;
    off_t page;
    struct hash *supp_table;
    if (fd >= 0 && fd < MAX_FILES) {
        index = process_current()->files[fd];
        if (index != -1 && open_files[index] &&
            !file_is_dir(open_files[index])) {

            bytes_read = 0;
            chunk_read = 1;
            page = 0;
            supp_table = &process_current()->supp_page_table;
            if(pg_ofs(buffer)){
                to_read = PGSIZE - (off_t)pg_ofs(buffer);
                to_read = (off_t)length < to_read? (off_t)length : to_read;
            }
            else{
                to_read = (off_t)length / PGSIZE ? 
                            PGSIZE : (off_t)(length % PGSIZE);
            }
            while (length > 0 && chunk_read > 0){
                if (!w_valid((uint8_t*)buffer)){
                    thread_exit(EXIT_FAILURE);
                    return EXIT_FAILURE;
                }
                pin_page(supp_table, (void*)buffer);
                bytes_read += (chunk_read = file_read(open_files[index],
                    buffer, to_read));
                unpin_page(supp_table, (void*)buffer);
                length -= to_read;
                page ++;
                buffer = (void*)((off_t) buffer + to_read);
                to_read = (off_t)length / PGSIZE ?
                            PGSIZE : (off_t)(length % PGSIZE);
            }
        }
    }
    return bytes_read;
}

int write(int fd, const void *buffer, unsigned length){
    DPRINTF("WRITING TO FILE\n")
    int index;
    int bytes_written = EXIT_FAILURE;
    int chunk_written;
    off_t to_write;
    off_t page;
    struct hash *supp_table;
    if (fd == 1){
        void *addr;
        for (addr = (void*)buffer; addr < (void*)((off_t)buffer + length);
            addr += PGSIZE){
            if (!r_valid((uint8_t*)addr)){
                thread_exit(EXIT_FAILURE);
                return EXIT_FAILURE;
            }
        }
        putbuf(buffer, length);
        return length;
    }
    if (fd >= 0 && fd < MAX_FILES) {
        index = process_current()->files[fd];
        if (index != -1 && open_files[index] &&
            !file_is_dir(open_files[index])) {
            bytes_written = 0;
            chunk_written = 1;
            page = 0;
            supp_table = &process_current()->supp_page_table;
            if(pg_ofs(buffer)){
                to_write = PGSIZE - (off_t)pg_ofs(buffer);
                to_write = (off_t)length < to_write? (off_t)length : to_write;
            }
            else{
                to_write = (off_t)length / PGSIZE ? 
                            PGSIZE : (off_t)(length % PGSIZE);
            }
            while (length > 0 && chunk_written > 0){
                if (!r_valid((uint8_t*)buffer)){
                    thread_exit(EXIT_FAILURE);
                    return EXIT_FAILURE;
                }
                pin_page(supp_table, (void*)buffer);
                bytes_written += (chunk_written = file_write(open_files[index],
                    buffer, to_write));
                unpin_page(supp_table, (void*)buffer);
                length -= to_write;
                page ++;
                buffer = (void*)((off_t) buffer + to_write);
                to_write = (off_t)length / PGSIZE ?
                            PGSIZE : (off_t)(length % PGSIZE);
            }
        }
    }
    return bytes_written;
}

void seek(int fd, unsigned position){
    int index;
    if (fd >= 0 && fd < MAX_FILES &&
        (index = process_current()->files[fd]) != -1) {
        if (!open_files[index])
            return
        file_seek(open_files[index], position);
    }
}

unsigned tell(int fd){
    int index;
    unsigned position = -1;
    if (fd >= 0 && fd < MAX_FILES) {
        index = process_current()->files[fd];
        if (index != -1 && open_files[index]) {
            position = file_tell(open_files[index]);
        }
    }
    return position;
}

void close(int fd){
    int index;
    if (fd >= 0 && fd < MAX_FILES) {
        index = process_current()->files[fd];
        if (index != -1) {
            file_close(open_files[index]);
            open_files[index] = NULL;
        }
        process_current()->files[fd] = -1;
    }
}

/* For cleaning up exiting process */
void free_open_files(){
    int fd;
    for (fd = 0; fd < MAX_FILES; fd++){
        close(fd);
    }
}

mapid_t mmap(int fd, void *addr){
    int index;
    off_t file_size;
    struct file *handle;
    int num_whole_pages;
    off_t last_page_bytes;
    off_t it;
    struct process *cur_proc;
    mapid_t mapping = 0;
    int *mappings;
    mapid_t slot = 0;
    uint32_t *pd;
    struct hash *supp_table;

    if (pg_ofs(addr) != 0 || fd < 2 || fd >= MAX_FILES || addr == NULL 
            || is_kernel_vaddr(addr))
        return MAP_FAILED;

    
    cur_proc = process_current();
    mappings = cur_proc->mmappings;
    while (mapping < MAX_MMAPPINGS && mappings[mapping] != -1)
        mapping++;
    if (mapping == MAX_MMAPPINGS){
        return MAP_FAILED;
    }
    index = cur_proc->files[fd];
    if (index == -1){
        return MAP_FAILED;
    }
    handle = open_files[index];;
    file_size = file_length(handle);
    if (file_size == 0){
        return MAP_FAILED;
    }
    num_whole_pages = file_size / PGSIZE;
    last_page_bytes = file_size % PGSIZE;
    supp_table = &process_current()->supp_page_table;
    for (it = (off_t)addr; it < (off_t)addr + file_size; it += PGSIZE){
        if(get_supp_page(supp_table, (void*)it)){
            return MAP_FAILED;
        }
    }
    lock_acquire(&mmap_lock);
    while (slot < MAX_GLOBAL_MAPPINGS && all_mmappings[slot].addr)
        slot++;
    if (slot == MAX_GLOBAL_MAPPINGS){
        return MAP_FAILED;
    }
    handle = file_reopen(handle);
    all_mmappings[slot].file = handle;
    all_mmappings[slot].addr = addr;
    lock_release(&mmap_lock);
    pd = thread_current()->pagedir;
    for (index = 0; index < num_whole_pages; index++){
        create_filesys_page(supp_table, (void*)((off_t)addr + PGSIZE * index),
            pd, NULL, handle, index * PGSIZE, PGSIZE, true);
    }
    if(last_page_bytes)
        create_filesys_page(supp_table, (void*)((off_t)addr + PGSIZE * index),
            pd, NULL, handle, index * PGSIZE, last_page_bytes, true);
    cur_proc->mmappings[mapping] = slot;
    return mapping;
}

void munmap(mapid_t mapping){
    int index;
    struct process *cur_proc;
    struct mmapping mm;
    off_t file_size;
    off_t i;
    struct hash *supp_table;

    if (mapping >= 0 && mapping < MAX_MMAPPINGS) {
        cur_proc = process_current();
        index = cur_proc->mmappings[mapping];
        if (index != -1) {
            mm = all_mmappings[index];
            all_mmappings[index] = (struct mmapping){NULL,NULL};
            cur_proc->mmappings[mapping] = -1;

            file_size = file_length(mm.file);
            supp_table = &process_current()->supp_page_table;
            for (i = (off_t)mm.addr; i < (off_t)mm.addr + file_size;
                i += PGSIZE){
                ASSERT(is_user_vaddr((void*)i));
                struct supp_page *p = get_supp_page(supp_table, (void*)i);

                if (p->fr) {
                    frame_evict(p->fr);
                }
                free_supp_page(supp_table, p);
            }
            file_close(mm.file);         
        }
    }            
}

void free_mmappings(){
    mapid_t i;
    for (i = 0; i < MAX_MMAPPINGS; i++){
        munmap(i);
    }
}

bool chdir(const char *dir) {
    // use open, which already has the checks
    int fd = open(dir);
    struct dir* new_dir;
    struct dir* old_dir;
    struct file* file;
    struct process* cur_proc;

    if (fd == EXIT_FAILURE)
        return false;

    cur_proc = process_current();
    file = open_files[cur_proc->files[fd]];
    if (!file_is_dir(file)){
        close(fd);
        return false;
    }
    new_dir = dir_reopen((struct dir*)file);
    close(fd);

    if(!dir)
        return false;

    old_dir = cur_proc->working_dir;
    cur_proc->working_dir = new_dir;
    dir_close(old_dir);
    return true;
}

bool mkdir(const char *dir){
    DPRINTF("MAKING DIRECTORY\n")
    return create_dir_entry(dir, 4, true);
}

bool readdir(int fd, char name[READDIR_MAX_LEN + 1]){
    int index;
    bool status = false;
    if(!w_valid((uint8_t*)name)){
        thread_exit(EXIT_FAILURE);
        return false;
    }
    if (fd >= 0 && fd < MAX_FILES) {
        index = process_current()->files[fd];
        if (index != -1 && open_files[index] &&
            file_is_dir(open_files[index])) {
            do{
                status = dir_readdir((struct dir *)(open_files[index]), name);
            }while (status && (!strcmp(name, PATH_WD) ||
                !strcmp(name, PATH_PARENT)));
        }
    }

    return status;
}

bool isdir(int fd){
    int index;
    bool status = false;
    if (fd >= 0 && fd < MAX_FILES) {
        index = process_current()->files[fd];
        if (index != -1 && open_files[index]) {
            status = file_is_dir(open_files[index]);
        }
    }

    return status;
}

int inumber(int fd){
    int index;
    int inumber = -1;
    if (fd >= 0 && fd < MAX_FILES) {
        index = process_current()->files[fd];
        if (index != -1 && open_files[index]) {
            inumber = filesys_get_inumber((struct dir *)(open_files[index]));
        }
    }

    return inumber;
}


int getArg(int argnum, struct intr_frame *f) {
    int* addr = (int*) f->esp + argnum;
    if (!r_valid((uint8_t*)addr) || !w_valid((uint8_t*)addr)) thread_exit(EXIT_FAILURE);
    return *(((int*) f->esp) + argnum);
}

/*! Reads a byte at user virtual address UADDR.
    UADDR must be below PHYS_BASE.
    Returns the byte value if successful, -1 if a segfault occurred. */
static int get_user(const uint8_t *uaddr) {
    int result;
    asm ("movl $1f, %0; movzbl %1, %0; 1:"
         : "=&a" (result) : "m" (*uaddr));
    return result;
}

/*! Writes BYTE to user address UDST.
    UDST must be below PHYS_BASE.
    Returns true if successful, false if a segfault occurred. */
static bool put_user (uint8_t *udst, uint8_t byte) {
    int error_code;
    asm ("movl $1f, %0; movb %b2, %1; 1:"
         : "=&a" (error_code), "=m" (*udst) : "q" (byte));
    return error_code != -1;
}

static bool r_valid(uint8_t *uaddr) {
    return uaddr != NULL && is_user_vaddr(uaddr) && get_user(uaddr) != -1;
}

static bool w_valid(uint8_t *uaddr) {
    if (uaddr == NULL || is_kernel_vaddr(uaddr))
        return false;
    int byte = get_user(uaddr);
    if (byte == -1)
        return false;
    return put_user(uaddr, (uint8_t)byte);
}

/**
 * Return if the given access is a reasonable stack access.
 */
bool is_stack_access(const void* addr, void* esp) {
    return ((uint32_t) addr > ((uint32_t) esp - 8) && (uint32_t) addr <
        (uint32_t) PHYS_BASE);
}
