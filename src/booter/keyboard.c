#include "interrupts.h"
#include "handlers.h"
#include "keyboard.h"
#include "ports.h"
#include "game.h"
#include "video.h"

/* This is the IO port of the PS/2 controller, where the keyboard's scan
 * codes are made available.  Scan codes can be read as follows:
 *
 *     unsigned char scan_code = inb(KEYBOARD_PORT);
 *
 * Most keys generate a scan-code when they are pressed, and a second scan-
 * code when the same key is released.  For such keys, the only difference
 * between the "pressed" and "released" scan-codes is that the top bit is
 * cleared in the "pressed" scan-code, and it is set in the "released" scan-
 * code.
 *
 * A few keys generate two scan-codes when they are pressed, and then two
 * more scan-codes when they are released.  For example, the arrow keys (the
 * ones that aren't part of the numeric keypad) will usually generate two
 * scan-codes for press or release.  In these cases, the keyboard controller
 * fires two interrupts, so you don't have to do anything special - the
 * interrupt handler will receive each byte in a separate invocation of the
 * handler.
 *
 * See http://wiki.osdev.org/PS/2_Keyboard for details.
 */
#define KEYBOARD_PORT 0x60

/**
 * @brief Handles keyboard interrupts.
 */
void keyboard_ISR(void) {
    unsigned char scan_code = inb(KEYBOARD_PORT);
    
    switch (scan_code) {
        case SPACE_PRESS:
            if (game_lost) {
                display_background();
                init_game();
            }

            if (!game_started) {
                game_started = 1;
                display_background();
                init_game();
            }
            bird_direction = -1;
            bird_countdown = 3;
            break;

        default:
            break;
    }
	return;
}


void init_keyboard(void) {
    install_interrupt_handler(KEYBOARD_INTERRUPT, keyboard_handler);
}

