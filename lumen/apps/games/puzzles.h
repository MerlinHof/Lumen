#ifndef LUMEN_PUZZLES_H
#define LUMEN_PUZZLES_H
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
typedef struct {
    unsigned char cell[81];
    uint16_t notes[81];
} SudokuBoard;
typedef struct {
    SudokuBoard board, undo[24];
    unsigned char solution[81], given[81];
    int undos, hints, difficulty;
} Sudoku;
void sudoku_new(Sudoku *, uint32_t, int);
int sudoku_count(const unsigned char *, int);
int sudoku_put(Sudoku *, int, int, int);
int sudoku_hint(Sudoku *, int);
int sudoku_undo(Sudoku *);
int sudoku_won(const Sudoku *);
int sudoku_conflict(const Sudoku *, int);
#endif
