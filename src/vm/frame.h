#ifndef VM_FRAME
#define VM_FRAME

#include <list.h>


struct frame {
	void *phys_addr;		/* Address of the page */
    struct supp_page *page; /* Supplemental page entry. Since there is 
                               no memory sharing
                               each frame corresponds to at most one page. */
	struct list_elem frame_elem; /* Element for keeping a list of frames */
    int pinned;
   	bool evicting;
};

void init_frame_table(void); /* Initializes the frame table. */
struct frame *frame_create(int flags, bool pinned); /* Gets a page from user pool and adds it to frame table. */
int frame_free(struct frame *fr); /* Frees page and removes frame from table. */
void frame_evict(struct frame *fr); /* Evicts a page from a frame to free it up. */

#endif // #ifndef VM_FRAME
