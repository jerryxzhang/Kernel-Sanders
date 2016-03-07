#ifndef VM_SWAP
#define VM_SWAP

struct swap_slot {
	/* Address of the swap inside the block. */
	int slot_num; 
	/* True if the swap is used, false if the swap is empty. */
	bool in_use; 
};

/* Initializes empty swap table. */
void init_swap_table(void); 
/* Copies page into swap table. */
struct swap_slot *swap_put_page(void *addr); 
/* Copies swap to argued address. */
int swap_retrieve_page(void *dest, struct swap_slot *swap); 
/* Marks swap as available. */
void swap_remove_page(struct swap_slot *swap); 

#endif // #ifndef VM_SWAP
