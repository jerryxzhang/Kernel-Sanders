/* The BIOS always loads the boot sector at address 0x07c00 (almost always as
 * the real address 0000:7c00).  Being 512 bytes, the sector ends at address
 * 0x07e00.
 */
#define BOOT_BASE_ADDR 0x7c00
#define BOOT_END_ADDR  0x7e00

/* This is where the bootloader signature goes.  It's 2 bytes, 0xaa55. */
#define BOOT_SIG_ADDR (BOOT_END_ADDR - 2)

/* The main program expects to be loaded at address 0x00020000 */
#define PROGRAM_BASE_ADDR 0x00020000

/* Flags in control register 0. */
#define CR0_PE 0x00000001      /* Protection Enable. */
#define CR0_EM 0x00000004      /* (Floating-point) Emulation. */
#define CR0_PG 0x80000000      /* Paging. */
#define CR0_WP 0x00010000      /* Write-Protect enable in kernel mode. */

/* Global Descriptor Table selectors. */
#define SEL_NULL          0x00    /* Null selector. */
#define SEL_CODESEG       0x08    /* Code selector. */
#define SEL_DATASEG       0x10    /* Data selector. */

/* Op code to perform extended read operation. */
#define EXT_READ_OP_CODE  0x42    /* Extended read operation */

/* Interrupt number used to do extended read operation. */
#define EXT_READ_INT	  0x13

/* Total number of bytes that make up the DAP. */
#define DAP_SIZE_BYTES	  0x20    /* DAP is 32 bytes. */

/* Data address packet byte values. */
#define DAP_PACKET_SIZE   0x10	  /* First byte, size of packet in bytes. */
#define DAP_UNUSED		  0x00	  /* Second byte, unused */
#define DAP_NUM_SECTORS_L 0x00    /* Third byte, low byte of number of
									 sectors to read. */
#define DAP_NUM_SECTORS_H 0x00	  /* Fourth byte, high byte of number of
									 sectors to read. */
								  /* Next four bytes are destination, or 
								     program base address, in little endian. */
#define DAP_DEST_L 		  (PROGRAM_BASE_ADDR & 0xFF)
#define DAP_DEST_ML       ((PROGRAM_BASE_ADDR >> 8) & 0xFF)
#define DAP_DEST_MH		  ((PROGRAM_BASE_ADDR >> 16) & 0xFF)
#define DAP_DEST_H		  ((PROGRAM_BASE_ADDR >> 24) & 0xFF)
								     
/* Compile list of 16-bit numbers to push onto the stack using the above
 * defined DAP byte values. */
#define DAP_WORD_1		  ((DAP_UNUSED << 16) | (DAP_PACKET_SIZE))
#define DAP_WORD_2		  ((DAP_NUM_SECTORS_H << 16) | (DAP_NUM_SECTORS_L))
#define DAP_WORD_3		  ((DAP_DEST_ML << 16) | (DAP_DEST_L))
#define DAP_WORD_4		  ((DAP_DEST_H << 16) | (DAP_DEST_MH))
#define DAP_WORD_5		  0x0000  /* Last 8 bytes are sector number. */
#define DAP_WORD_6		  0x0000
#define DAP_WORD_7		  0x0000
#define DAP_WORD_8		  0x0000

/* Entry point offset. */
#define ENTRY_POINT_OFFSET 0x18

