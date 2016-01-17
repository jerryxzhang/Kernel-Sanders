#include "interrupts.h"
#include "keyboard.h"
#include "timer.h"

/* This is the entry-point for the game! */
void c_start(void) {
    /* TODO:  You will need to initialize various subsystems here.  This
     *        would include the interrupt handling mechanism, and the various
     *        systems that use interrupts.  Once this is done, you can call
     *        enable_interrupts() to start interrupt handling, and go on to
     *        do whatever else you decide to do!
     */
    
    /* Initialize our interrupt vector. */
    init_interrupts();
    
    /* Initialize peripherals. */
    init_keyboard();
    
    /* Initialize timers. */
    init_timer();

	/* After setting up the game, we can enable interrupts again so the game
	 * can run as expected. */
	enable_interrupts();
	 
    /* Loop forever, so that we don't fall back into the bootloader code. */
    while (1) { continue; }
}

