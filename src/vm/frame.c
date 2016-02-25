#include <debug.h>
#include <stddef.h>
#include <stdio.h>

#include "frame.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/malloc.h"



struct list frame_table;	/* Frames in the table */


/*! init_frame_table
 * 
 *  @description Initializes the frame table.
 * 
 */
void init_frame_table(void) {
	/* Initialize the frame table list. */
	list_init(&frame_table);
}


/*! create_frame
 *  
 *  @description This gets a new page from the user pool and adds it to the
 *  frame table.
 *  
 *  @return a pointer to the new page
 */
struct frame *create_frame(void *vaddr, int flags) {
	/* Get a page from the user pool. */
	void *kpage = palloc_get_page(flags);
	
	/* Create and update the frame struct so we can add it to our table. */
	struct frame *new_frame = (struct frame *)malloc(sizeof(struct frame));
	new_frame->phys_addr = kpage;
	new_frame->vaddr = vaddr;
	
	/* Add the ne page to the frame table. */
	list_push_back(&frame_table, &new_frame->frame_elem);
	
	/* If no pages, frame full, this will panic. */
	return new_frame;
}


/*! free_frame
 * 
 *  @description This frees a frame and removes it from the frame list.
 *  If no frame found, this does nothing.
 * 
 *  @param page - the page to be freed
 *
 *  @return 0 if no errors, 1 otherwise.
 */
int free_frame(void *page) {
	/* Will point to the frame we are freeing if we find it. */
	struct frame *to_del = NULL;
	/* List iterator for finding frame. */
	struct list_elem *e;
	
	/* Search for the frame with the associated page. */
	for (e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e)) {
		/* Get the frame from the element. */
		to_del = list_entry(e, struct frame, frame_elem);
		ASSERT(to_del != NULL);
		
		/* If it is a match, great we're done. */
		if (page == to_del->phys_addr)
			break;
		else /* Go back to NULL so we can know if we found the frame. */
			to_del = NULL;
	}
	
	/* If we found the frame, delete the page and remove frame from table. */
	if (to_del) {
		/* Remove from table. */
		list_remove(&to_del->frame_elem);
	}
	
	/* Return 0 if successful, 1 otherwise. */
	return (to_del ? 0 : 1);
}
