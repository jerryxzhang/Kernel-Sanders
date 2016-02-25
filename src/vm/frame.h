#ifndef VM_FRAME
#define VM_FRAME

#include <list.h>

struct frame {
	void *phys_addr;		/* Address of the page */
	void *vaddr;			/* Virtual address of page. */
	struct thread *t;		/* Thread that owns page */
	
	struct list_elem frame_elem; /* Element for keeping a list of frames */
};

void init_frame_table(void); /* Initializes the frame table. */
struct frame *create_frame(void *vaddr, int flags); /* Gets a page from user pool and adds it to frame table. */
int free_frame(void *page); /* Frees page and removes frame from table. */

#endif // #ifndef VM_FRAME
