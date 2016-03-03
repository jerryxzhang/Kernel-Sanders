#ifndef VM_PAGE
#define VM_PAGE

#include <list.h>

#include "frame.h"


struct list supp_page_table;



/* Type of place the page data can be found in. */
enum page_location_type {
	filesys, /* Incluces zero case (just a filesys where 0 bytes read) */
	swapslot,
	kernel,
};


struct supp_page {
	struct frame *fr; /* Frame this is in. */
	uint32_t *pd; /* Pagedir associated with this page. */
	void *vaddr; /* Virtual address of associated page. */
	enum page_location_type type; /* Tells how to get page data. */
	bool wr; /* True if the memory is writable.  False otherwise. */
	
	struct list_elem supp_page_elem; /* Element to put in lists. */
	
	/* Only useful for filesys type. */
	struct file *fil; /* Pointer to file. */
	int offset; /* Offset from file start to relevant data. */
	int bytes; /* Number of bytes of relevant data. */
	
	/* Only useful for swap type. */
	struct swap_slot *swap; /* Info about location of data in swap. */
};



void init_supp_page_table(void); /* Initializes supplemental page table. */
struct frame *page_to_new_frame(void *vaddr); /* Returns valid frame with vaddr expected data. */

/* Functions to create/remove pages in supplemental page table. */
struct supp_page* get_supp_page(void* vaddr);
int free_supp_page(struct supp_page *spg); /* Removes page from table. */
struct supp_page *create_filesys_page(void *vaddr, uint32_t *pd, struct frame *fr, struct file *file, int offset, int bytes, bool writable);
struct supp_page *create_swapslot_page(void *vaddr, uint32_t *pd, struct frame *fr, bool writable);

#endif // #ifndef VM_PAGE
