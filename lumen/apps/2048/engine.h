#ifndef LUMEN_2048_ENGINE_H
#define LUMEN_2048_ENGINE_H
#include <stdint.h>

typedef struct {
    uint32_t rng;
    uint64_t score;
    unsigned char cell[16];
    int won;
} TileState;
typedef struct {
    TileState state, undo;
    int can_undo;
} Tiles;
void tiles_new(Tiles *, uint32_t);
int tiles_move(Tiles *, int);
int tiles_over(const Tiles *);
#endif
