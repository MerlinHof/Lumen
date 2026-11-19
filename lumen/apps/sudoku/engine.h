#ifndef LUMEN_SUDOKU_ENGINE_H
#define LUMEN_SUDOKU_ENGINE_H
#include <stdint.h>

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
