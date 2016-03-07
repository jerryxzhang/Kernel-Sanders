/*! \file page.c
 * 
 *  Contains methods for supplementary pages.
 */

#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "userprog/process.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "frame.h"
#include "page.h"
#include "swap.h"

bool valid_page_data(struct hash *table, void *vaddr); /* Returns true if page holds valid data. */
struct supp_page *allocate_supp_page(struct hash *table, void *vaddr);

bool less_func(const struct hash_elem *a, const struct hash_elem *b, void* aux UNUSED);
unsigned hash_func(const struct hash_elem *e, void *aux UNUSED);
void free_action_func(struct hash_elem *elem, void *aux UNUSED);


bool less_func(const struct hash_elem *a, const struct hash_elem *b, void* aux UNUSED) {
    return hash_entry(a, struct supp_page, elem)->vaddr < 
        hash_entry(b, struct supp_page, elem)->vaddr;
}

unsigned hash_func(const struct hash_elem *e, void *aux UNUSED) {
    return hash_int((int) hash_entry(e, struct supp_page, elem)->vaddr);
}

/*! init_supp_page_table
 *  
 *  @description Initializes the supplemental page table.
 */
void init_supp_page_table(struct hash *table) {
    hash_init(table, &hash_func, &less_func, NULL /* aux */);
}


void free_action_func(struct hash_elem *elem, void *aux UNUSED) {
    struct supp_page *page = hash_entry(elem, struct supp_page, elem);
    if (page->fr && page->type == filesys) frame_evict(page->fr);
    else if (page->fr) frame_free(page->fr);

    if (page->type == swapslot && page->swap) swap_remove_page(page->swap);
    pagedir_clear_page(thread_current()->pagedir, page->vaddr);
    free(page);
}

void free_supp_page_table(struct hash *table) {
    hash_apply(table, &free_action_func);
    hash_destroy(table, NULL);
}

/*! locate_page
 * 
 *  @description Uses a virtual address to locate the data that was meant
 *  to have been accessed.  A pointer to the data is returned.  If the vaddr
 *  is an invalid pointer, NULL is returned.
 * 
 *  @param vaddr - User virtual address of a page
 * 
 *  @return pointer to supp page vaddr describes or NULL if it does not exist.
 */
struct supp_page *get_supp_page(struct hash *table, void *vaddr) {
    struct supp_page p;
    struct hash_elem *e;

    ASSERT(is_user_vaddr(vaddr));
    ASSERT(pg_ofs(vaddr) == 0);

    p.vaddr = vaddr;
    e = hash_find(table, &p.elem);
    return e != NULL ? hash_entry(e, struct supp_page, elem) : NULL;
}

/*! valid_page_data
 * 
 *  @description Returns whether the page with argued virtual address has valid
 *  data.  If the page is in the kernel, the data is invalid.
 * 
 *  @param vaddr - Virtual address of a page
 * 
 *  @return boolean that is true if page data valid, false otherwise.
 */
bool valid_page_data(struct hash *table, void *vaddr) {
	/* Get the supplemental page from vaddr. */
	struct supp_page *spg = get_supp_page(table, vaddr);
    return spg != NULL;	
}

/*! page_to_new_frame
 * 
 *  @description Takes a virtual address to a page, searches for its data and
 *  copies it to a new frame, freeing an old one if it exists.
 *  
 *  @param vaddr - Virtual address of the page
 * 
 *  @return A pointer to the new frame or NULL if no frame obtained.
 */
struct frame *page_to_new_frame(struct hash *table, void *vaddr, bool pinned) {
    ASSERT(is_user_vaddr(vaddr));
    ASSERT(pg_ofs(vaddr) == 0);
    
	/* Make sure the address points to valid data. */
	if (!valid_page_data(table, vaddr))
		return NULL;
	
    //printf("Paging %x to new frame\n", vaddr);
	/* Get supplemental page so we know where to look for vaddr data. */
	struct supp_page *spg = get_supp_page(table, vaddr);
	
	ASSERT(!spg->fr || spg->fr->evicting);
    		
	/* Create a new frame to load vaddr's data into. */
    struct frame *new_frame = frame_create(PAL_USER | PAL_ZERO, pinned);
    new_frame->page = spg;
    spg->fr = new_frame;
	
	/* Populate new frame based on what vaddr supposedly pointed to. */
	switch (spg->type) {
		case filesys : /* Read from file for specified number of bytes. */
            //printf("Reading in from file\n");
			if (file_read_at(spg->fil, new_frame->phys_addr, spg->bytes, spg->offset) != (int) spg->bytes) {
				PANIC("Error with page fault\n");
			}
            if (spg->wr && spg->fil == process_current()->file) {
                // If the page is linked to the current executable, but
                // is also writable, then turn it into a swap page
                spg->type = swapslot;
                spg->fil = NULL;
                spg->offset = 0;
                spg->bytes = 0;
                spg->swap = NULL;
            //    printf("Paging in data page \n");
            }
			break;
		case swapslot : /* Read from swap slot. */
            //printf("Reading in from slot\n");
			swap_retrieve_page(new_frame->phys_addr, spg->swap);
			break;
		default : /* Something went terribly wrong if not one of the enums. */
			PANIC("Unknown error when handling page fault.\n");
			break;
	}
		
	/* Map vaddr to the physical address we just created. */
	pagedir_set_page(spg->pd, vaddr, new_frame->phys_addr, spg->wr);
	
	return new_frame;
}

/*! remove_page
 * 
 *  @description Removes a page from the supplemental page table and frees
 *  the page.
 * 
 *  @param vaddr - pointer to the virtual page that is to be removed.
 * 
 *  @return 0 if successful, nonzero otherwise.
 */
int free_supp_page(struct hash *table, struct supp_page *spg) {
    ASSERT(get_supp_page(table, spg->vaddr) != NULL);
    hash_delete(table, &spg->elem);
    free((void*)spg);
	return 0;
}

struct supp_page *allocate_supp_page(struct hash *table, void *vaddr) {
    ASSERT(is_user_vaddr(vaddr));
    ASSERT(pg_ofs(vaddr) == 0);
    ASSERT(get_supp_page(table, vaddr) == NULL);

    struct supp_page *p = malloc(sizeof(struct supp_page));
    p->vaddr = vaddr;
    pagedir_set_dirty(thread_current()->pagedir, vaddr, false);

    hash_insert(table, &p->elem);
    return p;
}

/*! create_filesys_page
 * 
 *  @description Creates a page in the supplemental page table for data that
 *  comes from a file.
 * 
 *  @param vaddr - pointer to a user virtual page
 *  @param file - pointer to the file data is being read from
 *  @param offset - offset in the file the data is read from
 *  @param bytes - number of bytes written from file
 *  @param writable - true if read/write, false if read-only
 * 
 *  @return a pointer to the supplemental page
 */
struct supp_page *create_filesys_page(struct hash *table, void *vaddr, uint32_t *pd, struct frame *fr, struct file *file, int offset, int bytes, bool writable) {
	ASSERT(file != NULL);

	/* Create and populate the page. */
	struct supp_page *new_page = allocate_supp_page(table, vaddr);
	new_page->fr = fr;
	new_page->type = filesys;
	new_page->vaddr = vaddr;
	new_page->fil = file;
	new_page->offset = offset;
	new_page->bytes = bytes;
	new_page->wr = writable;
	new_page->pd = pd;
	
	new_page->swap = NULL;

	return new_page;
}

/*! create_swapslot_page
 * 
 *  @description Creates a page in the supplemental page table for a newly
 *  allocated memory page that will be written back to swap.
 * 
 *  @param vaddr - pointer to user virtual page
 * 
 *  @return a pointer to the supplemental page
*/
struct supp_page *create_swapslot_page(struct hash *table, void *vaddr, uint32_t *pd, struct frame *fr, bool writable) {
	/* Create and populate the page. */
	struct supp_page *new_page = allocate_supp_page(table, vaddr);
	new_page->fr = fr;
	new_page->type = swapslot;
	new_page->vaddr = vaddr;
	new_page->fil = NULL;
	new_page->offset = 0;
	new_page->bytes = 0;
	new_page->wr = writable;
	new_page->pd = pd;

    new_page->swap = NULL;
	
	return new_page;
}


void pin_page(struct hash *table, void *vaddr){
	void *upage = pg_round_down(vaddr);
	struct supp_page *spg = get_supp_page(table, upage);
	while (spg->fr && spg->fr->evicting) {

	}
	ASSERT(spg);
	if (spg->fr){
		ASSERT(spg->fr->pinned >= 0);
		spg->fr->pinned++;
	}
	else
		page_to_new_frame(table, upage, true);
}

void unpin_page(struct hash *table, void *vaddr){
	void *upage = pg_round_down(vaddr);
	struct supp_page *spg = get_supp_page(table, upage);
	ASSERT(spg);
	ASSERT(spg->fr);
	ASSERT(spg->fr->pinned > 0);
	ASSERT(!spg->fr->evicting);
	spg->fr->pinned--;
}