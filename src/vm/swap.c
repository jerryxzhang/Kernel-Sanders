/*! \file swap.c
 * 
 *  Contains methods for using the swap table. 
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
#include "threads/vaddr.h"
#include "frame.h"
#include "page.h"
#include "swap.h"



struct block *swap_table;
struct swap_slot *swap_slots;




struct swap_slot *swap_find_empty(void); /* Finds empty page in swap table. */



/*! init_swap_table
 *  
 *  @description Initializes an empty swap table.
 */
void init_swap_table (void) {
	/* Initialize swap table to point to swap block. */
	swap_table = block_get_role(BLOCK_SWAP);
	
	/* Create an array of slots so we can easily find free slots.  We want
	 * one entry in the array for every page in the swap block. */
	int num_slots = block_size(swap_table) * sizeof(block_sector_t) / PGSIZE;
	swap_slots = (struct swap_slot *)malloc(num_slots * sizeof(struct swap_slot));
	
	/* Initialize swap array.  Each successive element in the array points
	 * to the next entry in the swap table, each of which are separated by
	 * PGSIZE.  No swap entries should be in_use at this point. */
	void *curr_swap = swap_table;
	int i;
	for (i = 0; i < num_slots; i++) {
		swap_slots[i].addr = curr_swap;
		swap_slots[i].in_use = false;
		curr_swap += PGSIZE;
	}
}




/*! swap_find_empty
 * 
 *  @description Finds the first empty swap page in the swap block.
 * 
 *  @return pointer to first empty swap, or NULL if the swap block is full.
 */
struct swap_slot *swap_find_empty(void) {
	/* Calculate number of swaps so we know how much to iterate. */
	int num_swaps = block_size(swap_table) * sizeof(block_sector_t) / PGSIZE;
	
	/* Iterate over array to see if there are any free swaps.  Return first
	 * one we find because they are all the same size, so there is no chance
	 * of finding a better one by continuing iterating. */
	int i;
	for (i = 0; i < num_swaps; i++) {
		/* Attribute in_use false if the slot is free. */
		if (!swap_slots[i].in_use)
			return &swap_slots[i];
	}
	
	/* If here, found no empty swaps. */
	return NULL;
}



/*! swap_remove_page
 * 
 *  @description Removes a swap page from the swap table by marking it as
 *  not in_use
 * 
 *  @param swap - Pointer to the swap slot entry in the array.
 */
void swap_remove_page(struct swap_slot *swap) {
	ASSERT (swap != NULL);
	
	/* Mark page as unused. */
	swap->in_use = false;
}



/*! swap_retrieve_page
 * 
 *  @description Copies the contents of the argued swap into the argued
 *  address, which is assumed to be PGSIZE bytes long.  Then, it marks the
 *  slot as unused.
 * 
 *  @param addr - The destination for copying swap to address.  It is assumed
 *  that PGSIZE bytes can be written starting at this address.
 *  @param swap - Pointer to the swap that contains desired data.
 * 
 *  @return Number of bytes copied.  Should be PGSIZE unless error occurs.
 */
int swap_retrieve_page(void *dest, struct swap_slot *swap) {
	ASSERT (swap != NULL);
	
	/* Mark slot as unused. */
	swap_remove_page(swap);
	
	/* Copy the page's contents. */
	if (memcpy(dest, swap->addr, PGSIZE))
		return PGSIZE;
	return 0;
}



/*! swap_put_page
 * 
 *  @description Copies the contents at the argued address into an open swap
 *  page, then returns the address of the page.  If no swap page available,
 *  NULL is returned and it is up to the caller to handle accordingly.
 * 
 *  @param addr - Pointer to the page to copy into a swap.
 * 
 *  @return pointer to the swap_slot copied into or NULL if none exists
 */
struct swap_slot *swap_put_page (void *addr) {
	/* Find a place in the swap block to put the page. */
	void *swap_page = swap_find_empty();
	
	/* Copy the contents of the page into the swap if address valid. */
	if (swap_page)
		memcpy(swap_page, addr, PGSIZE);
	
	return swap_page;
}