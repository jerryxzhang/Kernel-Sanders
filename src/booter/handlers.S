# Interrupt handlers


.align 4
.globl timer_handler
timer_handler:
	pusha				 # Save machine state.
	cld					 # Calling C code; clear direction flag.
	
	call interrupt_timer # Call our C function for timer interrupts.
	
	mov $0x20, %al 		 # Acknowledge the interrupt.
	out %al, $0x20		 # Send interrupt ack to hardware.
	
	popa				 # Restore machine state.
	iret				 # Return from interrupt.
	
	
	
.globl keyboard_handler
keyboard_handler:
	pusha				# Save machine state.
	cld					# Calling C code; clear direction flag.
	
	call interrupt_keyboard # Call our C function for keyboard interrupts.
	
	mov $0x20, %al		# Acknowledge the interrupt.
	out %al, $0x20		# Send interrupt ack to hardware.
	
	popa				# Restore machine state.
	iret				# Return from interrupt.
