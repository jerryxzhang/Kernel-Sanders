#ifndef VM_SWAP
#define VM_SWAP
#define VM



struct swap_slot {
	void *addr; /* Address of the swap inside the block. */
	bool in_use; /* True if the swap is used, false if the swap is empty. */
};


void init_swap_table(void); /* Initializes empty swap table. */
struct swap_slot *swap_put_page(void *addr); /* Copies page into swap table. */
int swap_retrieve_page(void *dest, struct swap_slot *swap); /* Copies swap to argued address. */
void swap_remove_page(struct swap_slot *swap); /* Marks swap as available. */


#endif // #ifndef VM_SWAP
