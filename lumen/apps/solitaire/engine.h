#ifndef LUMEN_SOLITAIRE_ENGINE_H
#define LUMEN_SOLITAIRE_ENGINE_H
#include <stdint.h>

typedef struct {
    unsigned char column[7][52], count[7], hidden[7], stock[24], waste[24], ns, nw, foundation[4];
    unsigned moves;
    uint32_t rng;
} PatienceState;
typedef struct {
    PatienceState state, undo[24];
    int undos;
} Patience;
void patience_new(Patience *, uint32_t);
int patience_draw(Patience *);
int patience_move(Patience *, int, int, int);
int patience_auto(Patience *);
int patience_undo(Patience *);
int patience_won(const Patience *);
#endif
