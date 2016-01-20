#include "video.h"
#include "game.h"

/* This is the address of the VGA text-mode video buffer.  Note that this
 * buffer actually holds 8 pages of text, but only the first page (page 0)
 * will be displayed.
 *
 * Individual characters in text-mode VGA are represented as two adjacent
 * bytes:
 *     Byte 0 = the character value
 *     Byte 1 = the color of the character:  the high nibble is the background
 *              color, and the low nibble is the foreground color
 *
 * See http://wiki.osdev.org/Printing_to_Screen for more details.
 *
 * Also, if you decide to use a graphical video mode, the active video buffer
 * may reside at another address, and the data will definitely be in another
 * format.  It's a complicated topic.  If you are really intent on learning
 * more about this topic, go to http://wiki.osdev.org/Main_Page and look at
 * the VGA links in the "Video" section.
 */
#define VIDEO_BUFFER ((void *) 0xB8000)
volatile char *video = (volatile char*) VIDEO_BUFFER;

/**
 * Writes a character to the display.
 */
void write_char(int fcolor, int bcolor, int row, int col, char toprint)
{
    unsigned index = ((row * SCREEN_WIDTH) + col) * 2;

    video[index] = toprint;
    video[index+1] = (char) (bcolor << 4) + fcolor;
}

char * itoa( int value, char * str, int base );
void print_score(int n, int x, int y) {
    char* msg = "Score: ";
    char buffer[10];
    itoa(score, buffer, 10);
    write_string(BLACK, LIGHT_CYAN, x, y, msg);
    write_string(BLACK, LIGHT_CYAN, x, y+7, buffer);
}

/**
 * Replaces the display with all black.
 */
void clear_display() {
    unsigned i;
    for (i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        video[2*i] = '\0';
        video[2*i+1] = '\0';
    }
}

/**
 * Fills the display with the background.
 */
void display_background() {
    unsigned i;
    unsigned j;
    for (i = 0; i < SCREEN_HEIGHT; i++) {
        char bcolor;
        char fcolor;
        char todisplay = '\0';
        if (i < SKY_HEIGHT) {
            bcolor = LIGHT_CYAN;
        }
        else if (i == SKY_HEIGHT) {
            todisplay = 223;
            fcolor = BROWN;
            bcolor = LIGHT_GREEN;
        }
        else if (i < SKY_HEIGHT + 2) {
            todisplay = 221;
            fcolor = GREEN;
            bcolor = LIGHT_GREEN;
        } else {
            bcolor = YELLOW;
            todisplay = 176;
            fcolor = BROWN;
        }

        for (j = 0; j < SCREEN_WIDTH; j++) {
            write_char(fcolor, bcolor, i, j, todisplay);
        }
    }
}

/**
 * Draws the pipes using the specified two colors, which can be used to draw or erase the pipes.
 */
void pipedraw(char c1, char c2) {
    unsigned i;
    unsigned j;
    for (i = 0; i < NUM_PIPES; i++) {
        if (pipes[i].position == -1) continue;
        for (j = 0; j < SKY_HEIGHT; j++) {
            if (j > pipes[i].height && j < pipes[i].height + PIPE_GAP) continue;
            write_char(c2, c2, j, pipes[i].position, ' '); 
            if (pipes[i].position != SCREEN_WIDTH-1)
                write_char(c2, c2, j, pipes[i].position + 1, ' '); 
            if (pipes[i].position != 0) 
                write_char(c1, c1, j, pipes[i].position - 1, ' '); 
        }
    }
}

/**
 * Draw the bird at height h with a character as the eye.
 */
void draw_bird(int h, char eye) {
    write_char(BLACK, WHITE, h, BIRD_POS+3, eye);
    write_char(0, RED, h, BIRD_POS+2, 0);
    write_char(WHITE, RED, h, BIRD_POS+1, 220);
    write_char(LIGHT_CYAN, WHITE, h, BIRD_POS, 223);
    write_char(RED, LIGHT_RED, h+1, BIRD_POS+3, 220);
    write_char(LIGHT_CYAN, LIGHT_RED, h+1, BIRD_POS+4, 220);
    write_char(0, RED, h+1, BIRD_POS+2, 0);
    write_char(WHITE, RED, h+1, BIRD_POS+1, 223);
    write_char(LIGHT_CYAN, WHITE, h+1, BIRD_POS, 220);
}

/**
 * Erase the bird by covering it with cyan.
 */
void erase_bird(int h) {
    write_char(LIGHT_CYAN, LIGHT_CYAN, h, BIRD_POS+3, 0);
    write_char(LIGHT_CYAN, LIGHT_CYAN, h, BIRD_POS+2, 0);
    write_char(LIGHT_CYAN, LIGHT_CYAN, h, BIRD_POS+1, 0);
    write_char(LIGHT_CYAN, LIGHT_CYAN, h, BIRD_POS, 0);
    write_char(LIGHT_CYAN, LIGHT_CYAN, h+1, BIRD_POS+3, 0);
    write_char(LIGHT_CYAN, LIGHT_CYAN, h+1, BIRD_POS+4, 0);
    write_char(LIGHT_CYAN, LIGHT_CYAN, h+1, BIRD_POS+2, 0);
    write_char(LIGHT_CYAN, LIGHT_CYAN, h+1, BIRD_POS+1, 0);
    write_char(LIGHT_CYAN, LIGHT_CYAN, h+1, BIRD_POS, 0);
}

void display_over_msg() {
    char* died = "You Died.";
    char* max = "Max Score: ";
    char buffer[10];
    itoa(maxscore, buffer, 10);
    write_string(BLACK, LIGHT_CYAN, 5, 5, died);
    write_string(BLACK, LIGHT_CYAN, 6, 5, max);
    write_string(BLACK, LIGHT_CYAN, 6, 16, buffer);
}

void display_greet_msg() {
    char* greet = "FLOPPY BIRD";
    char* ins = "Press [Space] to move";
    write_string(BLACK, LIGHT_CYAN, 11, 35, greet);
    write_string(BLACK, LIGHT_CYAN, 12, 30, ins);
}

/**
 * Redraw all objects and messages on the game screen.
 */
void redraw() {
    if (!game_started) {
        display_greet_msg();
        return;
    }
    pipedraw(LIGHT_CYAN, LIGHT_CYAN);
    erase_bird(bird_height);
    game_step();
    pipedraw(LIGHT_GREEN, GREEN);

    draw_bird(bird_height, game_lost ? 'x' : 'o');
    print_score(score, 5, 5);

    if (game_lost) display_over_msg();
}

/** 
 * Writes a given string with given colors to the display at given location.
 */
void write_string(int fcolor, int bcolor, int x, int y, const char *string)
{
    while( *string != 0 )
    {
        write_char(fcolor, bcolor, x, y++, *string++);
        
    }
}

char * itoa( int value, char * str, int base )
{
    char * rc;
    char * ptr;
    char * low;
    // Check for supported base.
    if ( base < 2 || base > 36 )
    {
        *str = '\0';
        return str;
    }
    rc = ptr = str;
    // Set '-' for negative decimals.
    if ( value < 0 && base == 10 )
    {
        *ptr++ = '-';
    }
    // Remember where the numbers start.
    low = ptr;
    // The actual conversion.
    do
    {
        // Modulo is negative for negative value. This trick makes abs() unnecessary.
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz"[35 + value % base];
        value /= base;
    } while ( value );
    // Terminating the string.
    *ptr-- = '\0';
    // Invert the numbers.
    while ( low < ptr )
    {
        char tmp = *low;
        *low++ = *ptr;
        *ptr-- = tmp;
    }
    return rc;
}
