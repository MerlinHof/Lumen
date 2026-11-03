#ifndef LUMEN_APPS_H
#define LUMEN_APPS_H
#include <stdint.h>
typedef struct {
    unsigned char bomb[90], open[90], flag[90];
    int started, over, won, flags;
} Mines;
typedef struct {
    char word[24];
    uint32_t guesses;
    int misses, over, won;
} Hangman;
typedef struct {
    unsigned char board[16][10];
    int piece, next, rotation, x, y, score, lines, over, paused;
} Blocks;
typedef struct {
    int cells[288], length, dx, dy, next_dx, next_dy, food, over, paused, score;
} Snake;
void apps_seed(uint32_t seed);
int calendar_days(int year, int month);
void mines_new(Mines *m);
int mines_adjacent(const Mines *m, int cell);
void mines_reveal(Mines *m, int cell);
void mines_flag(Mines *m, int cell);
void hangman_new(Hangman *h, int english);
void hangman_guess(Hangman *h, int letter);
int blocks_cell(int piece, int rotation, int x, int y);
void blocks_new(Blocks *b);
void blocks_move(Blocks *b, int dx);
void blocks_rotate(Blocks *b);
void blocks_tick(Blocks *b);
void blocks_drop(Blocks *b);
int blocks_landing_y(const Blocks *b);
void snake_new(Snake *s);
void snake_direction(Snake *s, int dx, int dy);
void snake_tick(Snake *s);
#endif
