#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdbool.h>

void syscall_init(void);
bool in_syscall(void);

#endif /* userprog/syscall.h */

