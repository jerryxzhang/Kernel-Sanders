#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"

static void syscall_handler(struct intr_frame *);
int getArg(int argnum, struct intr_frame *f);

/*
   Functions to check whether the given user memory address is valid for 
   reading (r), writing(w).
 */

static bool r_valid(uint8_t *uaddr);
static bool w_valid(uint8_t *uaddr);

void syscall_init(void) {
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}


static struct lock filesys_lock;



static void syscall_handler(struct intr_frame *f) {
    if (!r_valid(f->esp) || !w_valid(f->esp)) thread_exit(-1);

    int syscall_num = getArg(0, f);
    
    switch(syscall_num) {
        case SYS_HALT:
            shutdown_power_off();
            break;
        case SYS_EXIT:
            thread_exit((int) getArg(1, f));
            break;
        case SYS_EXEC:
            lock_acquire(&filesys_lock);
            const char *cmd_line = (const char*) getArg(1, f);
            if (r_valid((const uint8_t *)cmd_line))
                f->eax = (uint32_t) process_execute(cmd_line);
            else
                f->eax = -1;
            lock_release(&filesys_lock);
            break;
        case SYS_WAIT:
            f->eax = (uint32_t) process_wait((int) getArg(1, f));
            break;
        case SYS_CREATE:
            lock_acquire(&filesys_lock);
            const char *file = (const char*) getArg(1, f);
            if (r_valid((const uint8_t *)file))
                f->eax = filesys_create(file, (off_t) getArg(2, f));
            else
                f->eax = 0;
            lock_release(&filesys_lock);
            break;
        case SYS_REMOVE:
            lock_acquire(&filesys_lock);
            f->eax = filesys_create((const char*) getArg(1, f), (off_t) getArg(2, f));
            lock_release(&filesys_lock);
            break;
        case SYS_OPEN:
            break;
        case SYS_FILESIZE:
            break;
        case SYS_READ:
            break;
        case SYS_WRITE:
            break;
        case SYS_SEEK:
            break;
        case SYS_TELL:
            break;
        case SYS_CLOSE:
            break;
        default:
            printf("Not implemented!\n");
            thread_exit(-1);
            break;


    }
}

int getArg(int argnum, struct intr_frame *f) {
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
    return is_user_vaddr(uaddr) && get_user(uaddr) != -1;
}

static bool w_valid(uint8_t *uaddr) {
    if (is_kernel_vaddr(uaddr))
        return false;
    int byte = get_user(uaddr);
    if (byte == -1)
        return false;
    return put_user(uaddr, (uint8_t)byte);
}
