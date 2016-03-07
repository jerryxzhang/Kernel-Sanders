#ifndef VM_PAGE
#define VM_PAGE

#include <list.h>
#include <hash.h>

#include "frame.h"
#include "threads/vaddr.h"

#define PAGE_TABLE_SIZE (((uint32_t) PHYS_BASE) >> 12)

/* Type of place the page data can be found in. */
enum page_location_type {
	filesys, /* Incluces zero case (just a filesys where 0 bytes read) */
	swapslot,
};

struct supp_page {
	struct frame *fr; /* Frame this is in. */
	uint32_t *pd; /* Pagedir associated with this page. */
	void *vaddr; /* Virtual address of associated page. */
    enum page_location_type type; /* Tells how to get page data. */
	bool wr; /* True if the memory is writable.  False otherwise. */
	
	struct hash_elem elem;
    
    /* Only useful for filesys type. */
	struct file *fil; /* Pointer to file. */
	int offset; /* Offset from file start to relevant data. */
	int bytes; /* Number of bytes of relevant data. */
	
	/* Only useful for swap type. */
	struct swap_slot *swap; /* Info about location of data in swap. */
};

void init_supp_page_table(struct hash *table); /* Initializes supplemental page table. */
void free_supp_page_table(struct hash *table_addr);
struct frame *page_to_new_frame(struct hash *table, void *vaddr, bool pinned); /* Returns valid frame with vaddr expected data. */

/* Functions to create/remove pages in supplemental page table. */
struct supp_page* get_supp_page(struct hash *table, void* vaddr);
int free_supp_page(struct hash * table, struct supp_page *spg); /* Removes page from table. */
struct supp_page *create_filesys_page(struct hash *table, void *vaddr, uint32_t *pd, struct frame *fr, struct file *file, int offset, int bytes, bool writable);
struct supp_page *create_swapslot_page(struct hash *table, void *vaddr, uint32_t *pd, struct frame *fr, bool writable);


void pin_page(struct hash *table, void *vaddr);
void unpin_page(struct hash *table, void *vaddr);
#endif // #ifndef VM_PAGE
