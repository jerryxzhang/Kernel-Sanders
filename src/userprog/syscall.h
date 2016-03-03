#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdbool.h>

void syscall_init(void);
bool in_syscall(void);

typedef int mapid_t;
#define MAP_FAILED ((mapid_t) -1)

#endif /* userprog/syscall.h */

