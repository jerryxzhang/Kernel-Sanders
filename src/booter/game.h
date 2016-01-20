#define NUM_PIPES 10
#define PIPE_TIME 30
#define PIPE_GAP 7
#define SKY_HEIGHT 22
#define BIRD_POS 20

extern volatile int game_started;
extern volatile unsigned timer_tick;
extern volatile int pipespawn;
extern volatile int bird_height;
extern volatile int bird_direction;
extern volatile int bird_countdown;
extern volatile int game_lost;
extern volatile int score;
extern volatile int maxscore;

typedef struct PipeStruct {
    int position;
    int height;
} Pipe;

extern volatile Pipe pipes[];

void game_step();

