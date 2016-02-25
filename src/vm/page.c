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
#include "threads/vaddr.h"
#include "vm/frame.h"



struct list supp_page_table;


struct supp_page *get_supp_page(void *vaddr); /* Returns physical address of page. */
bool valid_page_data(void *vaddr); /* Returns true if page holds valid data. */



/*! init_supp_page_table
 *  
 *  @description Initializes the supplemental page table.
 */
void init_supp_page_table(void) {
	/* Initialize the list. */
	list_init(&supp_page_table);
}



/*! locate_page
 * 
 *  @description Uses a virtual address to locate the data that was meant
 *  to have been accessed.  A pointer to the data is returned.  If the vaddr
 *  is an invalid pointer, NULL is returned.
 * 
 *  @param vaddr - Virtual address of a page
 * 
 *  @return pointer to supp page vaddr describes or NULL if it does not exist.
 */
struct supp_page *get_supp_page(void *vaddr) {
	/* Find page in supplemental page table. */
	struct list_elem *e;
	struct supp_page *spg = NULL;
	for (e = list_begin(&supp_page_table); e != list_end(&supp_page_table); e = list_next(e)) {
		spg = list_entry(e, struct supp_page, supp_page_elem);
		ASSERT(spg != NULL);
		
		/* If address found, job is done. */
		if (vaddr == spg->vaddr)
			break;
		else
			spg = NULL; /* Otherwise, revert so we can check if pg found after. */
	}
	
	return spg;
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
bool valid_page_data(void *vaddr) {
	/* Get the supplemental page from vaddr. */
	struct supp_page *spg = get_supp_page(vaddr);
	
	/* Determine whether data is valid. Obviously not if spg is NULL. */
	if (spg)
		return (spg->type == kernel ? false : true);
	return false;
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
struct frame *page_to_new_frame(void *vaddr) {
	/* Make sure the address points to valid data. */
	if (!valid_page_data(vaddr))
		return NULL;
	
	/* Get supplemental page so we know where to look for vaddr data. */
	struct supp_page *spg = get_supp_page(vaddr);
	
	free_frame(vaddr);
		
	/* Create a new frame to load vaddr's data into. */
	struct frame *new_frame = create_frame(vaddr, PAL_USER);
	
	/* Declare info we will use to do copy after switch structure. */
	char *start = NULL; /* Where the data starts to copy. */
	int bytes = 0; /* Number of bytes to copy (rest are zeroes). */
	
	/* Populate new frame based on what vaddr supposedly pointed to. */
	switch (spg->type) {
		case filesys : /* Read from file for specified number of bytes. */
			if (file_read(spg->fil, new_frame->phys_addr, spg->bytes) != (int) spg->bytes) {
				PANIC("Error with page fault\n");
				return NULL;
			}
			memset(new_frame->phys_addr + spg->bytes, 0, PGSIZE - spg->bytes);
			break;
			
		case swapslot : /* Read from swap slot. */
			start = NULL; /*! WILL HAVE TO CHANGE */
			bytes = 0;    /*! WILL HAVE TO CHANGE */
			break;
			
		case kernel : /* Shouldn't be here because valid_page_data was true. */
			PANIC("Unable to handle page fault.\n");
			break;
			
		default : /* Something went terribly wrong if not one of the enums. */
			PANIC("Unknown error when handling page fault.\n");
			break;
	}
		
	/* Clear the mapping from vaddr to any physical address (if it existed). */
	pagedir_clear_page(spg->pd, vaddr);
	/* Map vaddr to the physical address we just created. */
	pagedir_set_page(spg->pd, vaddr, new_frame->phys_addr, spg->wr);
	
	return new_frame;
}




/*! remove_page
 * 
 *  @description Removes a page from the supplemental page table.
 * 
 *  @param vaddr - pointer to the virtual page that is to be removed.
 * 
 *  @return 0 if successful, nonzero otherwise.
 */
int free_supp_page(struct supp_page *spg) {
	free_supp_page(spg);
	return 0;
}




/*! create_filesys_page
 * 
 *  @description Creates a page in the supplemental page table for data that
 *  comes from a file.
 * 
 *  @param vaddr - pointer to virtual page
 *  @param type - the type of location the data comes from
 *  @param file - pointer to the file data is being read from
 *  @param offset - offset in the file the data is read from
 *  @param bytes - number of bytes written from file
 *  @param writable - true if read/write, false if read-only
 * 
 *  @return a pointer to the supplemental page
 */
struct supp_page *create_filesys_page(void *vaddr, struct file *file, int offset, int bytes, bool writable) {
	ASSERT(vaddr != NULL);
	ASSERT(file != NULL);
	
	/* Create and populate the page. */
	struct supp_page *new_page = (struct supp_page *)malloc(sizeof(struct supp_page));
	new_page->type = filesys;
	new_page->vaddr = vaddr;
	new_page->fil = file;
	new_page->offset = offset;
	new_page->bytes = bytes;
	new_page->wr = writable;
	new_page->pd = thread_current()->pagedir;
	
	/* Add page to table. */
	list_push_back(&supp_page_table, &new_page->supp_page_elem);
	
	return new_page;
}
