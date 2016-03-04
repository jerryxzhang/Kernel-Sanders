#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"

#ifdef VM
#include "vm/page.h"
#define MAX_GLOBAL_MAPPINGS MAX_PROCESSES * 4

struct mmapping {
    void* addr;
    struct file *file;
};

static struct mmapping all_mmappings[MAX_GLOBAL_MAPPINGS];
static struct lock mmap_lock;
#endif

#define MAX_GLOBAL_FILES MAX_PROCESSES * 4

static void syscall_handler(struct intr_frame *);
int getArg(int argnum, struct intr_frame *f);

/*
   Functions to check whether the given user memory address is valid for 
   reading (r), writing(w). Valid for writing means that it's also valid
   for reading, so only need to call w_valid().
 */

bool in_sc;

static bool r_valid(uint8_t *uaddr);
static bool w_valid(uint8_t *uaddr);
static struct lock filesys_lock;

static struct lock filesys_lock;
static struct file* open_files[MAX_GLOBAL_FILES];

void syscall_init(void) {
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
    lock_init(&filesys_lock);
    #ifdef VM
    lock_init(&mmap_lock);
    #endif
    in_sc = false;
}

bool in_syscall(void) {
    return in_sc;
}

static void syscall_handler(struct intr_frame *f) {
    if (!w_valid(f->esp)) thread_exit(-1);

    in_sc = true;

    thread_current()->esp = f->esp;

    int syscall_num = getArg(0, f);
    
    switch(syscall_num) {
        case SYS_HALT:
            shutdown_power_off();
            break;
        case SYS_EXIT:
            thread_exit((int) getArg(1, f));
            break;
        case SYS_EXEC: {
            const char *cmd_line = (const char*) getArg(1, f);
            lock_acquire(&filesys_lock);
            if (r_valid((uint8_t *)cmd_line))
                f->eax = (uint32_t) process_execute(cmd_line);
            else
                f->eax = -1;
            lock_release(&filesys_lock);
            break;
        }
        case SYS_WAIT:
            f->eax = (uint32_t) process_wait((int) getArg(1, f));
            break;
        case SYS_CREATE: {
            const char *file = (const char*) getArg(1, f);
            lock_acquire(&filesys_lock);
            if (r_valid((uint8_t *)file))
                f->eax = filesys_create(file, (off_t) getArg(2, f));
            else
                thread_exit(-1);
            lock_release(&filesys_lock);
            break;
        }
        case SYS_REMOVE: {
            const char *file = (const char*) getArg(1, f);
            lock_acquire(&filesys_lock);
            if (r_valid((uint8_t *)file))
                f->eax = filesys_remove(file);
            else
                thread_exit(-1);
            lock_release(&filesys_lock);
            break;
        }
        case SYS_OPEN: {
            int* files = process_current()->files;
            int process_slot = 2;
            int global_slot = 0;
            struct file* new_file;
            const char *file = (const char*) getArg(1, f);
            if (!r_valid((uint8_t *)file)){
                thread_exit(-1);
                break;
            }
            lock_acquire(&filesys_lock);
            while (process_slot < MAX_FILES && files[process_slot] != -1)
                process_slot++;
            if (process_slot == MAX_FILES)
                goto open_fail;
            while (global_slot < MAX_GLOBAL_FILES && 
                open_files[global_slot] != NULL)
                global_slot++;
            if (global_slot == MAX_GLOBAL_FILES)
                //can't have more open files
                goto open_fail;
            
            new_file = filesys_open(file);
            if (new_file != NULL) {
                open_files[global_slot] = new_file;
                files[process_slot] = global_slot;
                f->eax = process_slot;
                goto done;
            }
            open_fail:
            f->eax = -1;
            done:
            lock_release(&filesys_lock);
            break;
        }
        case SYS_FILESIZE: {
            int fd = (int) getArg(1, f);
            int index;
            if (fd >= 0 && fd < MAX_FILES) {
                lock_acquire(&filesys_lock);
                index = process_current()->files[fd];
                if (index != -1) {
                    f->eax = file_length(open_files[index]);
                    lock_release(&filesys_lock);
                    break;
                }
                lock_release(&filesys_lock);
            }
            f->eax = -1;
            break;
        }
        case SYS_READ:{
            int fd = (int) getArg(1, f);
            void *buffer = (void *) getArg(2, f);
            off_t size = (off_t) getArg(3, f);
            int index;
            if (!w_valid((uint8_t*)buffer) || !w_valid((uint8_t*)((off_t)buffer) + size)) {
                thread_exit(-1);
                break;
            }
            if (fd >= 0 && fd < MAX_FILES) {
                lock_acquire(&filesys_lock);
                index = process_current()->files[fd];
                if (index != -1) {
                    f->eax = file_read(open_files[index], buffer, size);
                    lock_release(&filesys_lock);
                    break;
                }
                lock_release(&filesys_lock);
            }
            f->eax = -1;
            break;
        }
        case SYS_WRITE:{
            int fd = (int) getArg(1, f);
            void *buffer = (void *) getArg(2, f);
            off_t size = (off_t) getArg(3, f);
            int index;
            if (!r_valid((uint8_t*)buffer) || !r_valid((uint8_t*)((off_t)buffer) + size)) {
                thread_exit(-1);
                break;
            }
            if (fd >= 0 && fd < MAX_FILES) {
                if (fd == 1) {
                    putbuf(buffer, size);
                    break;
                }
                else{
                    lock_acquire(&filesys_lock);
                    index = process_current()->files[fd];
                    if (index != -1) {
                        f->eax = file_write(open_files[index], buffer, size);
                        lock_release(&filesys_lock);
                        break;
                    }
                    lock_release(&filesys_lock);
                }
            }
            f->eax = 0;
            break;
        }
        case SYS_SEEK:{
            int fd = (int) getArg(1, f);
            off_t pos = (off_t) getArg(2, f);
            int index;
            if (fd >= 0 && fd < MAX_FILES) {
                lock_acquire(&filesys_lock);
                index = process_current()->files[fd];
                if (index != -1) 
                    file_seek(open_files[index], pos);
                lock_release(&filesys_lock);
            }
            break;
        }
        case SYS_TELL:{
            int fd = (int) getArg(1, f);
            int index;
            if (fd >= 0 && fd < MAX_FILES) {
                lock_acquire(&filesys_lock);
                index = process_current()->files[fd];
                if (index != -1) {
                    f->eax = file_tell(open_files[index]);
                    lock_release(&filesys_lock);
                    break;
                }
                lock_release(&filesys_lock);
            }
            f->eax = -1;
            break;
        }
        case SYS_CLOSE:{
            int fd = (int) getArg(1, f);
            int index;
            if (fd >= 0 && fd < MAX_FILES) {
                lock_acquire(&filesys_lock);
                index = process_current()->files[fd];
                if (index != -1) {
                    file_close(open_files[index]);
                    open_files[index] = NULL;
                }
                process_current()->files[fd] = -1;
                lock_release(&filesys_lock);
            }
            break;
        }
        #ifdef VM
        case SYS_MMAP:{
            int fd = (int) getArg(1, f);
            void *addr = (void *) getArg(2, f);
            int index;
            off_t file_size;
            struct file *handle;
            int num_pages;
            struct process *cur_proc = process_current();
            mapid_t id;
            mapid_t slot;
            uint32_t *pd;
            if ((off_t)addr % PGSIZE)
                goto map_fail;
            if (fd >= 0 && fd < MAX_FILES) {
                lock_acquire(&mmap_lock);
                for (id = 0; id < MAX_MMAPPINGS; id++){
                    if(cur_proc->mmappings[id] == -1)
                        break;
                }
                if (id == MAX_MMAPPINGS){
                    lock_release(&mmap_lock);
                    goto map_fail;
                }
                for (slot = 0; slot < MAX_GLOBAL_MAPPINGS; slot++){
                    if (all_mmappings[slot].file)
                        break;
                }
                if (slot == MAX_GLOBAL_MAPPINGS){
                    lock_release(&mmap_lock);
                    goto map_fail;
                }
                lock_acquire(&filesys_lock);
                index = cur_proc->files[fd];
                if (index == -1){
                    lock_release(&filesys_lock);
                    lock_release(&mmap_lock);
                    goto map_fail;
                }
                handle = open_files[index];
                file_size = file_length(handle);
                if (file_size == 0){
                    lock_release(&filesys_lock);
                    lock_release(&mmap_lock);
                    goto map_fail;
                }
                num_pages = (file_size - 1) / PGSIZE + 1;
                for (index = 0; index < num_pages; index++){
                    if(get_supp_page(&process_current()->supp_page_table, 
                                (void*)((off_t)addr + PGSIZE * index))){
                        lock_release(&filesys_lock);
                        lock_release(&mmap_lock);
                        goto map_fail;
                    }
                }
                all_mmappings[slot].file = file_reopen(handle);
                lock_release(&filesys_lock);
                all_mmappings[slot].addr = addr;
                pd = thread_current()->pagedir;
                for (index = 0; index < num_pages - 1; index++){
                    create_filesys_page(&process_current()->supp_page_table, 
                            (void*)((off_t)addr + PGSIZE * index),
                        pd, NULL, handle, index * PGSIZE, PGSIZE, true);
                }
                create_filesys_page(&process_current()->supp_page_table, 
                        (void*)((off_t)addr + PGSIZE * index),
                        pd, NULL, handle, index * PGSIZE, (off_t)addr %
                        PGSIZE, true);
                cur_proc->mmappings[id] = slot;
                lock_release(&mmap_lock);
                f->eax = id;
                break;
            }
            map_fail:
            f->eax = -1;
            break;

        }
        case SYS_MUNMAP:{

        }
        #endif
        default:
            printf("Not implemented!\n");
            thread_exit(-1);
            break;
    }
    in_sc = false;
}

int getArg(int argnum, struct intr_frame *f) {
    int* addr = (int*) f->esp + argnum;
    if (!r_valid((uint8_t*)addr) || !w_valid((uint8_t*)addr)) thread_exit(-1);
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
