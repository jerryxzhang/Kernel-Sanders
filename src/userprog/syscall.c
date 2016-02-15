#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"

static void syscall_handler(struct intr_frame *);
int getArg(int argnum, struct intr_frame *f);

static bool r_valid(const uint8_t *uaddr);
static bool w_valid(const uint8_t *uaddr);
static bool rw_valid(const uint8_t *uaddr);


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
