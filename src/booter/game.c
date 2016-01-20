#include "interrupts.h"
#include "keyboard.h"
#include "timer.h"
#include "video.h"
#include "game.h"
#include "prng.h"

volatile int game_started = 0;
volatile unsigned timer_tick = 0;
volatile int pipespawn = 0;
volatile int bird_height = 0;
volatile int bird_direction = 0;
volatile int bird_countdown = 0;
volatile int game_lost = 0;
volatile int score = 0;
volatile int maxscore = 0;

volatile Pipe pipes[NUM_PIPES];

/**
 * Resets the game state. Used at the start of games, or after losing.
 */
void init_game(void) {
    unsigned i;

    prng_seed_time(timer_tick);

    pipespawn = PIPE_TIME;
    for (i = 0; i < NUM_PIPES; i++) {
        Pipe pipe;
        pipe.position = -1;
        pipe.height = -1;
        pipes[i] = pipe;
    }

    bird_height = 4;
    bird_direction = 1;
    bird_countdown = 0;
    game_lost = 0;
    score = 0;
}

/**
 * Returns a 1 if the given coordinates collide with an object, 0 otherwise.
 */
int detect_collision(int row, int col)
{
    if ((row < 0) || (row > SKY_HEIGHT - 1)) return 1;

    unsigned i;
    for (i = 0; i < NUM_PIPES; i++) {
        if (pipes[i].position == -1) continue;
        if (col != pipes[i].position && col != pipes[i].position - 1 && col != pipes[i].position + 1) continue;

        if (row > pipes[i].height && row < pipes[i].height + PIPE_GAP) {
            continue;
        }   
        return 1;
    }   
    return 0;
}


/**
 * Returns 1 if the bird at height h collides with an object, 0 otherwise.
 */
int bird_collide(int h) {
    int ret = detect_collision(h, BIRD_POS+3) ||
    detect_collision(h, BIRD_POS) ||
    detect_collision(h+1, BIRD_POS+4) ||
    detect_collision(h+1, BIRD_POS+3) ||
    detect_collision(h+1, BIRD_POS);

    return ret;
}


/**
 * Updates the locations of game objects after one timestep. 
 * Checks for collisions and updates state if we lost.
 */
void game_step() {
    if (game_lost) return;

    unsigned i;
    if (timer_tick % 5 == 0) {
        for (i = 0; i < NUM_PIPES; i++) {
            if (pipes[i].position != -1) {
                pipes[i].position--;
                if (pipes[i].position == BIRD_POS - 1) score++;
            }   
        }

        pipespawn--;
        if (pipespawn == 0) {
            for (i = 0; i < NUM_PIPES; i++) {
                if (pipes[i].position == -1) {
                    pipes[i].position = SCREEN_WIDTH - 1;
                    pipes[i].height = prng_get_byte() % (SKY_HEIGHT - PIPE_GAP);
                    break;
                }
            }
            pipespawn = PIPE_TIME;
        }   
    }

    if (timer_tick % 7 == 0) {
        if (bird_collide(bird_height)) {
            game_lost = 1;
            if (score > maxscore) maxscore = score;
            return;
        }
        bird_height += bird_direction;
        if (bird_countdown) {
            bird_countdown--;
            if (!bird_countdown) {
                bird_direction = 1;
            }
        }
    }
}

/* This is the entry-point for the game! */
void c_start(void) {
    /* Initialize our interrupt vector. */
    init_interrupts();
    
    /* Initialize peripherals. */
    init_keyboard();
    
    /* Initialize timers. */
    init_timer();

    display_background();
    game_started = 0;
    game_lost = 0;

	/* After setting up the game, we can enable interrupts again so the game
	 * can run as expected. */
	enable_interrupts();
	

    /* Loop forever, so that we don't fall back into the bootloader code. */
    while (1) {
    }
}

