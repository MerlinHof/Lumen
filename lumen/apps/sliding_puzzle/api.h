#ifndef LUMEN_SLIDING_PUZZLE_H
#define LUMEN_SLIDING_PUZZLE_H
typedef struct { int tiles[16], empty, steps; } SignalGame;
void signal_new(SignalGame *, unsigned seed, int moves);
int signal_slide(SignalGame *, int index);
int signal_won(const SignalGame *);
void sliding_puzzle_register(void);
#endif
