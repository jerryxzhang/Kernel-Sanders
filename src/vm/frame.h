#ifndef VM_FRAME
#define VM_FRAME

#include <list.h>


struct frame {
	void *phys_addr;		/* Address of the page */
	
	struct list_elem frame_elem; /* Element for keeping a list of frames */
};

void init_frame_table(void); /* Initializes the frame table. */
struct frame *frame_create(int flags); /* Gets a page from user pool and adds it to frame table. */
int frame_free(struct frame *fr); /* Frees page and removes frame from table. */
void frame_evict(void); /* Evicts a page from a frame to free it up. */

#endif // #ifndef VM_FRAME
