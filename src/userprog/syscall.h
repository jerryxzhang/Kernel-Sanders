#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdbool.h>
#include "threads/thread.h"

void syscall_init(void);
bool in_syscall(void);

/*! Map region identifier. */
typedef int mapid_t;
#define MAP_FAILED ((mapid_t) -1)

/*! Maximum characters in a filename written by readdir(). */
#define READDIR_MAX_LEN 14

/*! Typical return values from main() and arguments to exit(). */
#define EXIT_SUCCESS 0          /*!< Successful execution. */
#define EXIT_FAILURE -1          /*!< Unsuccessful execution. */

void halt(void);
void exit(int status);
pid_t exec(const char *file);
int wait(pid_t);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned length);
int write(int fd, const void *buffer, unsigned length);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);

mapid_t mmap(int fd, void *addr);
void munmap(mapid_t);
bool is_stack_access(const void* addr, void* esp);

void free_open_files(void);
void free_mmappings(void);

bool chdir(const char *dir);
bool mkdir(const char *dir);
bool readdir(int fd, char name[READDIR_MAX_LEN + 1]);
bool isdir(int fd);
int inumber(int fd);

bool create_dir_entry(const char *path, unsigned initial_size, bool is_dir);

#endif /* userprog/syscall.h */

