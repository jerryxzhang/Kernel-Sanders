#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"

static void syscall_handler(struct intr_frame *);
int getArg(int argnum, struct intr_frame *f);

/*
   Functions to check whether the given user memory address is valid for 
   reading (r), writing(w).
 */
static bool r_valid(const uint8_t *uaddr);
static bool w_valid(const uint8_t *uaddr);

void syscall_init(void) {
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}



static void syscall_handler(struct intr_frame *f) {
    int syscall_num = getArg(0, f);;
    
    switch(syscall_num) {
        case SYS_HALT:
            shutdown_power_off();
            break;
        case SYS_EXIT:
            thread_exit((int) getArg(1, f));
            break;
        case SYS_EXEC:
            f->eax = (uint32_t) process_execute((const char*) getArg(1, f));
            break;
        case SYS_WAIT:
            f->eax = (uint32_t) process_wait((int) getArg(1, f));
            break;
        case SYS_CREATE:
            break;
        case SYS_REMOVE:
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

static bool r_valid(const uint8_t *uaddr) {
    return is_user_vaddr(uaddr) && get_user(uaddr) != -1;
}

static bool w_valid(const uint8_t *uaddr) {
    if (is_kernel_vaddr(uaddr))
        return false;
    uint8_t byte = get_user(uaddr);
    if (byte == -1)
        return false;
    return put_user(uaddr, byte);
}
