#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static struct process process_table[MAX_PROCESSES];
static struct list free_list;

static thread_func start_process NO_RETURN;
static bool load(const char *cmdline, void (**eip)(void), void **esp);
pid_t process_table_get(void);
void process_table_free(struct process *p);
void stack_put_args(void **esp, char **argv, int argc, void *ret);

struct process* process_current(void) {
    return &process_table[thread_current()->pid];
}

/** 
 * Initialize the process table.
 */
void process_init(void) {
    int i;
    ASSERT(intr_get_level() == INTR_OFF);

    list_init(&free_list);

    for (i = MAX_PROCESSES-1; i >= 0; i--) {
        process_table[i].pid = i;
        process_table[i].valid = false;
        list_push_back(&free_list, &process_table[i].elem);
    }

    // Give the init thread the first pid
    pid_t pid = process_table_get();
    ASSERT(pid == INIT_PID);
    thread_current()->pid = pid;
    process_table[INIT_PID].thread_ptr = thread_current();
}

/**
 * Returns the number of process elements in the process table.
 */
int process_count(void) {
    return list_size(&free_list);
}

/** 
 * Allocate and intialize a element in the process table and return
 * the process id.
 */
pid_t process_table_get(void) {
    if (list_empty(&free_list)) {
        return PID_ERROR;
    }
    
    enum intr_level old_level = intr_disable();
    
    struct list_elem *elem = list_pop_back(&free_list);
    struct process *p = list_entry(elem, struct process, elem);
    
    ASSERT(!p->valid);

    p->valid = true;
    p->blocked = false;
    p->running = false;
    p->loaded = false;
    p->file = NULL;

    list_init(&p->children);

    p->parent_pid = thread_current()->pid;

    int i;

    for (i = 0; i < MAX_FILES; i++) {
        p->files[i] = -1;
    }

    for (i = 0; i < MAX_MMAPPINGS; i++) {
        p->mmappings[i] = -1;
    }
    
    intr_set_level(old_level);
    
    return p->pid;
}

/**
 * Free the given process table element and make its pid
 * available again
 */
void process_table_free(struct process *p) {
    ASSERT(p->valid);

    p->valid = false;
    list_remove(&p->elem);

    list_push_back(&free_list, &p->elem);
}

/*! Starts a new thread running a user program loaded from FILENAME.  The new
    thread may be scheduled (and may even exit) before process_execute()
    returns.  Returns the new process's process id, or PID_ERROR if the thread  
    cannot be created. */
pid_t process_execute(const char *file_name) {
    char *fn_copy;
    pid_t pid;
    
    /* Make a copy of FILE_NAME.
       Otherwise there's a race between the caller and load(). */
    fn_copy = palloc_get_page(0);
    if (fn_copy == NULL)
        return PID_ERROR;
    strlcpy(fn_copy, file_name, PGSIZE);

    /** Allocate a process table entry. */
    pid = process_table_get();
    if (pid == PID_ERROR) {
        return pid;
    }

    /* Create a new thread to execute FILE_NAME. */
    process_table[pid].thread_ptr = thread_create_ptr(fn_copy, PRI_DEFAULT, start_process, fn_copy, pid);
    if (process_table[pid].thread_ptr == NULL) {
        palloc_free_page(fn_copy); 
        process_table_free(&process_table[pid]);
        return PID_ERROR;
    }
    
    enum intr_level old_level = intr_disable();
    // Wait till the child finishes loading
    if (!process_table[pid].loaded) {
        process_table[pid].blocked = true;
        thread_block();
    }        
    intr_set_level(old_level);
    
    ASSERT(process_table[pid].loaded);

    // Get whether it loaded successfully
    if (!process_table[pid].load_success) {
        return PID_ERROR;
    }

    // Add new process to current children
    list_push_back(&process_table[thread_current()->pid].children, &process_table[pid].elem);    

    return pid;
}

/*! A thread function that loads a user process and starts it running. */
static void start_process(void *file_name_) {
    pid_t pid = thread_current()->pid;
    char *file_name = file_name_;
    struct intr_frame if_;
    bool success;
    
    /* Initialize interrupt frame and load executable. */
    memset(&if_, 0, sizeof(if_));
    if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
    if_.cs = SEL_UCSEG;
    if_.eflags = FLAG_IF | FLAG_MBS;
    
    /* Create our array of pointers to arguments. */
    char *argv[MAX_ARGS], *next;
    int argc = 0, num_chars = 0;
    
    /* Iterate over string.  While doing this, count how long it is, and keep
     * track of the offset of the first character of each token. */
    
    /* Start with next at the very beginning, and the zero-offset of the file
     * name in the first entry of the argv array. */
    next = file_name;
    /* Keep track of the state of our parsing. */
    bool in_word = false;
    while (*next != '\0' && argc < MAX_ARGS - 1) {
        if (!in_word && *next != ' ') { // Found beginning of word
            in_word = true; // Now we are iterating in a word
            argv[argc] = (char *) num_chars; // num_chars is the offset
            argc++;
        }
        else if (in_word && *next == ' ') { // Found end of word
            in_word = false;
            *next = '\0'; // Make NULL so it "looks" like a string.
        }
        // Otherwise, there is nothing special about the character
        
        num_chars++;
        next++;
        
        /* Need to make sure the stack doesn't grow too large.  On the stack,
         * we have argc-many pointers, plus a sentinel, argv, argc and return
         * address which are also 4 bytes.  We also have num_chars characters
         * which are one byte each. */
        if (PGSIZE <= (argc + 5) * sizeof(void*) + num_chars) {
			break;
		}
    }
    
    /* Stopped iterating before counting for the NULL terminator. */
    num_chars++;
    
    /* Want last argv pointer to be a NULL pointer, so put it in the array. */
    argv[argc] = NULL;
    
    /* file_name now has a NULL character after file name. */
    success = load(file_name, &if_.eip, &if_.esp);
    strlcpy(process_table[pid].name, file_name, MAX_NAME_LEN);
    
    process_table[pid].running = true;
    enum intr_level old_level = intr_disable();
    process_table[pid].load_success = success;
    process_table[pid].loaded = true;
    if (process_table[pid].blocked) {
        process_table[pid].blocked = false;
        thread_unblock(process_table[process_table[pid].parent_pid].thread_ptr);
    }
    intr_set_level(old_level);
   
    // Quit if load failed
    if (!success) {
        palloc_free_page(file_name);
        thread_exit(-1);
    }
    
    /* Put the arguments on the stack that was loaded.  The structure of
     * the arguments on the stack will look like:
     *           _____________
     *          |             |
     *          |   kernel    |
     *          |-------------|
     *          |    argv[n]  |
     *          |-------------|
     *          |     ...     |
     *          |-------------|
     *          |    argv[1]  |
     *          |-------------|
     *          |    argv[0]  |
     *          |-------------|
     *          |     '\0'    |
     *          |-------------|
     *          |   &argv[n]  |
     *          |-------------|
     *          |     ...     |
     *          |-------------|
     *          |   &argv[1]  |
     *          |-------------|
     *          |   &argv[0]  |
     *          |-------------|
     *          |    &argv    |
     *          |-------------|
     *          |     argc    |
     *          |-------------|
     *    esp-->| return addr |
     *          |-------------|
     *          |             |
     *          |             |
     */
    /* Decrement stack pointer so we start writing in the correct memory. */
    if_.esp--;
    
    /* Move the stack pointer to where the beginning of the string should
     * be */
    if_.esp = (void *) (((int) if_.esp) - (num_chars * sizeof(char)));
    
    /* Put the filename and argument strings on the stack. */
    memcpy(if_.esp, (void *) file_name, num_chars * sizeof(char));
    /* Make sure the last character is a NULL character, which it currently
     * may not be if the string is of maximum length. */
    *(((char *) if_.esp) + num_chars) = '\0';
    
    palloc_free_page(file_name);
                
    /* Update values in argv to point in stack. */
    int i = 0;
    for (i = 0; i < argc; i++)
        argv[i] = (char *)((int) if_.esp + (int) argv[i]);
    
    /* Move the stack pointer down to the nearest multiple of 4 address. */
    if_.esp = (void *) ((int) if_.esp & 0xFFFFFFFC);
    
    /* Iterate and put each pointer on the stack. */
    for (i = argc; i >= 0; i--) {
        if_.esp -= 4;
        *(char **)(if_.esp) = argv[i];
    }
    
    /* Move stack pointer to put pointer to argv on the stack. */
    if_.esp -= 4;
    /* Put pointer to argv on the stack (points within stack). */
    *(void **) if_.esp = if_.esp + 4;
    
    /* Move stack pointer to where argc goes on the stack. */
    if_.esp -= 4;
    /* Put agrc on the stack. */
    *(int *) if_.esp = argc;
    
    /* Move stack pointer to where return address goes on stack. */
    if_.esp -= 4;
    /* Put return address on stack. */
    *(void **) if_.esp = NULL;
    
    /* Start the user process by simulating a return from an
       interrupt, implemented by intr_exit (in
       threads/intr-stubs.S).  Because intr_exit takes all of its
       arguments on the stack in the form of a `struct intr_frame',
       we just point the stack pointer (%esp) to our stack frame
       and jump to it. */
    asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
    NOT_REACHED();
}

/*! Waits for thread TID to die and returns its exit status.  If it was
    terminated by the kernel (i.e. killed due to an exception), returns -1.
    If TID is invalid or if it was not a child of the calling process, or if
    process_wait() has already been successfully called for the given TID,
    returns -1 immediately, without waiting.

    This function will be implemented in problem 2-2.  For now, it does
    nothing. */
int process_wait(pid_t child_id) {
    // Ensure that child_id is valid
    if (child_id < 0 || child_id > MAX_PROCESSES - 1 || !process_table[child_id].valid 
            || process_table[child_id].parent_pid != thread_current()->pid) {
        return -1;
    }

    int ret;

    enum intr_level old_level = intr_disable();
    if (process_table[child_id].running) {
        // Block until the child process exiting wakes it up
        process_table[child_id].blocked = true;
        thread_block();
    }
    intr_set_level(old_level);

    // The child process must be done at this point
    ASSERT(!process_table[child_id].running);

    ret = process_table[child_id].return_code;

    process_table_free(&process_table[child_id]);

    return ret;
}

/*! Free the current process's resources. */
void process_exit(int code) {
    struct thread *cur = thread_current();
    struct process *p = &process_table[cur->pid];
    
    ASSERT(!intr_context());
    printf("%s: exit(%d)\n", process_table[thread_current()->pid].name, code);

    uint32_t *pd;

    p->return_code = code;
    p->running = false;
    if (p->file) file_close(p->file);

    // Unblock the parent if it is waiting
    enum intr_level old_level = intr_disable();
    if (p->blocked) {
        p->blocked = false;
        thread_unblock(process_table[p->parent_pid].thread_ptr);
    }
    intr_set_level(old_level);

    // Deal with any remaining children
    if (!list_empty(&p->children)) {
        struct list_elem *e;
        for (e = list_begin(&p->children); e != list_end(&p->children); 
                e = list_next(e)) {
            struct process *child = list_entry(e, struct process, elem);
            if (child->running) {
                // Inform the child the parent is dead
                child->parent_pid = -1;
            } else {
                // The parent did not care about these children. 
                process_table_free(child);
            }
        }
    }

    // Parent is dead, so noone will wait for this child
    if (p->parent_pid == -1) {
        process_table_free(p);
    }

    /* Destroy the current process's page directory and switch back
       to the kernel-only page directory. */
    pd = cur->pagedir;
    if (pd != NULL) {
        /* Correct ordering here is crucial.  We must set
           cur->pagedir to NULL before switching page directories,
           so that a timer interrupt can't switch back to the
           process page directory.  We must activate the base page
           directory before destroying the process's page
           directory, or our active page directory will be one
           that's been freed (and cleared). */
        cur->pagedir = NULL;
        pagedir_activate(NULL);
        pagedir_destroy(pd);
    }
}

/*! Sets up the CPU for running user code in the current thread.
    This function is called on every context switch. */
void process_activate(void) {
    struct thread *t = thread_current();

    /* Activate thread's page tables. */
    pagedir_activate(t->pagedir);

    /* Set thread's kernel stack for use in processing interrupts. */
    tss_update();
}

/*! We load ELF binaries.  The following definitions are taken
    from the ELF specification, [ELF1], more-or-less verbatim.  */

/*! ELF types.  See [ELF1] 1-2. @{ */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;
/*! @} */

/*! For use with ELF types in printf(). @{ */
#define PE32Wx PRIx32   /*!< Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /*!< Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /*!< Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /*!< Print Elf32_Half in hexadecimal. */
/*! @} */

/*! Executable header.  See [ELF1] 1-4 to 1-8.
    This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
};

/*! Program header.  See [ELF1] 2-2 to 2-4.  There are e_phnum of these,
    starting at file offset e_phoff (see [ELF1] 1-6). */
struct Elf32_Phdr {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
};

/*! Values for p_type.  See [ELF1] 2-3. @{ */
#define PT_NULL    0            /*!< Ignore. */
#define PT_LOAD    1            /*!< Loadable segment. */
#define PT_DYNAMIC 2            /*!< Dynamic linking info. */
#define PT_INTERP  3            /*!< Name of dynamic loader. */
#define PT_NOTE    4            /*!< Auxiliary info. */
#define PT_SHLIB   5            /*!< Reserved. */
#define PT_PHDR    6            /*!< Program header table. */
#define PT_STACK   0x6474e551   /*!< Stack segment. */
/*! @} */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. @{ */
#define PF_X 1          /*!< Executable. */
#define PF_W 2          /*!< Writable. */
#define PF_R 4          /*!< Readable. */
/*! @} */

static bool setup_stack(void **esp);
static bool validate_segment(const struct Elf32_Phdr *, struct file *);
static bool load_segment(struct file *file, off_t ofs, uint8_t *upage,
                         uint32_t read_bytes, uint32_t zero_bytes,
                         bool writable);

/*! Loads an ELF executable from FILE_NAME into the current thread.  Stores the
    executable's entry point into *EIP and its initial stack pointer into *ESP.
    Returns true if successful, false otherwise. */
bool load(const char *file_name, void (**eip) (void), void **esp) {
    struct thread *t = thread_current();
    struct Elf32_Ehdr ehdr;
    struct file *file = NULL;
    off_t file_ofs;
    bool success = false;
    int i;

    /* Allocate and activate page directory. */
    t->pagedir = pagedir_create();
    if (t->pagedir == NULL) 
        goto done;
    process_activate();

    /* Open executable file. */
    file = filesys_open(file_name);
    if (file == NULL) {
        printf("load: %s: open failed\n", file_name);
        goto done; 
    }
    file_deny_write(file);

    /* Read and verify executable header. */
    if (file_read(file, &ehdr, sizeof ehdr) != sizeof ehdr ||
        memcmp(ehdr.e_ident, "\177ELF\1\1\1", 7) || ehdr.e_type != 2 ||
        ehdr.e_machine != 3 || ehdr.e_version != 1 ||
        ehdr.e_phentsize != sizeof(struct Elf32_Phdr) || ehdr.e_phnum > 1024) {
        printf("load: %s: error loading executable\n", file_name);
        goto done; 
    }

    /* Read program headers. */
    file_ofs = ehdr.e_phoff;
    for (i = 0; i < ehdr.e_phnum; i++) {
        struct Elf32_Phdr phdr;

        if (file_ofs < 0 || file_ofs > file_length(file))
            goto done;
        file_seek(file, file_ofs);

        if (file_read(file, &phdr, sizeof phdr) != sizeof phdr)
            goto done;

        file_ofs += sizeof phdr;

        switch (phdr.p_type) {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
            /* Ignore this segment. */
            break;

        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
            goto done;

        case PT_LOAD:
            if (validate_segment(&phdr, file)) {
                bool writable = (phdr.p_flags & PF_W) != 0;
                uint32_t file_page = phdr.p_offset & ~PGMASK;
                uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
                uint32_t page_offset = phdr.p_vaddr & PGMASK;
                uint32_t read_bytes, zero_bytes;
                if (phdr.p_filesz > 0) {
                    /* Normal segment.
                       Read initial part from disk and zero the rest. */
                    read_bytes = page_offset + phdr.p_filesz;
                    zero_bytes = (ROUND_UP(page_offset + phdr.p_memsz, PGSIZE) -
                                 read_bytes);
                }
                else {
                    /* Entirely zero.
                       Don't read anything from disk. */
                    read_bytes = 0;
                    zero_bytes = ROUND_UP(page_offset + phdr.p_memsz, PGSIZE);
                }
                if (!load_segment(file, file_page, (void *) mem_page,
                                  read_bytes, zero_bytes, writable))
                    goto done;
            }
            else {
                goto done;
            }
            break;
        }
    }

    /* Set up stack. */
    if (!setup_stack(esp))
        goto done;

    /* Start address. */
    *eip = (void (*)(void)) ehdr.e_entry;

    success = true;

done:
    /* We arrive here whether the load is successful or not. */
    if (!success) {
        file_close(file);
    } else {
        process_current()->file = file;
    }

    return success;
}

/*! Checks whether PHDR describes a valid, loadable segment in
    FILE and returns true if so, false otherwise. */
static bool validate_segment(const struct Elf32_Phdr *phdr, struct file *file) {
    /* p_offset and p_vaddr must have the same page offset. */
    if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
        return false; 

    /* p_offset must point within FILE. */
    if (phdr->p_offset > (Elf32_Off) file_length(file))
        return false;

    /* p_memsz must be at least as big as p_filesz. */
    if (phdr->p_memsz < phdr->p_filesz)
        return false; 

    /* The segment must not be empty. */
    if (phdr->p_memsz == 0)
        return false;
  
    /* The virtual memory region must both start and end within the
       user address space range. */
    if (!is_user_vaddr((void *) phdr->p_vaddr))
        return false;
    if (!is_user_vaddr((void *) (phdr->p_vaddr + phdr->p_memsz)))
        return false;

    /* The region cannot "wrap around" across the kernel virtual
       address space. */
    if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
        return false;

    /* Disallow mapping page 0.
       Not only is it a bad idea to map page 0, but if we allowed it then user
       code that passed a null pointer to system calls could quite likely panic
       the kernel by way of null pointer assertions in memcpy(), etc. */
    if (phdr->p_vaddr < PGSIZE)
        return false;

    /* It's okay. */
    return true;
}

/*! Loads a segment starting at offset OFS in FILE at address UPAGE.  In total,
    READ_BYTES + ZERO_BYTES bytes of virtual memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

    The pages initialized by this function must be writable by the user process
    if WRITABLE is true, read-only otherwise.

    Return true if successful, false if a memory allocation error or disk read
    error occurs. */
static bool load_segment(struct file *file, off_t ofs, uint8_t *upage,
                         uint32_t read_bytes, uint32_t zero_bytes,
                         bool writable) {
    ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
    ASSERT(pg_ofs(upage) == 0);
    ASSERT(ofs % PGSIZE == 0);

    file_seek(file, ofs);
    int total_ofs = ofs;
    while (read_bytes > 0 || zero_bytes > 0) {
        /* Calculate how to fill this page.
           We will read PAGE_READ_BYTES bytes from FILE
           and zero the final PAGE_ZERO_BYTES bytes. */
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;

        /* Load this page. */
        struct frame *new_fr = frame_create(PAL_USER);
        if (file_read(file, new_fr->phys_addr, page_read_bytes) != (int) page_read_bytes) {
            frame_free(new_fr);
            return false;
        }
        memset(new_fr->phys_addr + page_read_bytes, 0, page_zero_bytes);

        /* Get a page of memory. */
        struct supp_page *spg = create_filesys_page(\
				upage, thread_current()->pagedir, new_fr,\
				file, total_ofs, page_read_bytes, writable);
        new_fr->page = spg;
		if (!spg) {
            frame_free(new_fr);
			free_supp_page(spg);
			return false;
		}
        
        /* Add the page to the process's address space. */
        if (!install_page(upage, new_fr->phys_addr, writable)) {
            frame_free(new_fr);
            free_supp_page(spg);
            return false; 
        }


        /* Advance. */
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        upage += PGSIZE;
        total_ofs += PGSIZE;
    }
    return true;
}

/*! Create a minimal stack by mapping a zeroed page at the top of
    user virtual memory. */
static bool setup_stack(void **esp) {
	struct frame *new_fr;
    uint8_t *kpage;
    bool success = false;

    void *upage = (void *)(PHYS_BASE - PGSIZE);
    new_fr = frame_create(PAL_USER | PAL_ZERO);
    new_fr->page = create_swapslot_page(upage, thread_current()->pagedir,
          new_fr, true);

    kpage = new_fr->phys_addr;
    if (kpage != NULL) {
        success = install_page(upage, kpage, true);
        if (success)
            *esp = PHYS_BASE - 1;
        else
            frame_free(new_fr);
    }
    return success;
}

/*! Adds a mapping from user virtual address UPAGE to kernel
    virtual address KPAGE to the page table.
    If WRITABLE is true, the user process may modify the page;
    otherwise, it is read-only.
    UPAGE must not already be mapped.
    KPAGE should probably be a page obtained from the user pool
    with palloc_get_page().
    Returns true on success, false if UPAGE is already mapped or
    if memory allocation fails. */
bool install_page(void *upage, void *kpage, bool writable) {
    struct thread *t = thread_current();

    /* Verify that there's not already a page at that virtual
       address, then map our page there. */
    return (pagedir_get_page(t->pagedir, upage) == NULL &&
            pagedir_set_page(t->pagedir, upage, kpage, writable));
}

