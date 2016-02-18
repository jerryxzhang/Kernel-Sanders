#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include <stdbool.h>
#include <stdint.h>

#define MAX_PROCESSES 512
#define MAX_NAME_LEN 60
#define MAX_ARGS 100
#define INIT_PID 0
#define MAX_FILES 20

/** A single element in the system process table. */
struct process {

    /** A pointer to the thread structure. Only valid if process is running. */
    struct thread *thread_ptr; 

    /** A buffer storing the process name. **/
    char name[MAX_NAME_LEN + 1];

    /** Ids of this process and its parent. */
    pid_t pid;
    pid_t parent_pid; /** -1 if parent has died. */

    /** A list of pointers to open files **/
    int files[MAX_FILES];

    /** A pointer to this process's executable. */
    struct file* file;

    /** A list of child processes. */
    struct list children;

    /** The return code the process supplies to exit() or -1 if killed by kernel.
     * Only valid if process is not running. */
    int return_code;

    /** Linked list structure. Reused for children list and free_list. */
    struct list_elem elem;

    /** Whether the process is running, or has completed. */
    bool running;

    /** Whether this struct represents a valid process, or is unallocated. */
    bool valid;

    /** Whether the parent process is blocked waiting on this process.  */
    bool blocked;

    /** Whether the process has completed loading */
    bool loaded;

    /** Whether the process successfully loaded */
    bool load_success;

};

void process_init(void);
int process_count(void);
pid_t process_execute(const char *file_name);
int process_wait(pid_t);
void process_exit(int code);
void process_activate(void);
struct process* process_current(void);

#endif /* userprog/process.h */

