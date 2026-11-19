#include "engine.h"
#include <string.h>

static uint32_t random_next(uint32_t *r) {
    *r = *r * 1664525u + 1013904223u;
    return *r;
}
static int allowed(const unsigned char *b, int at, int v) {
    int r = at / 9, c = at % 9;
    for (int i = 0; i < 9; i++)
        if (b[r * 9 + i] == v || b[i * 9 + c] == v ||
            b[(r / 3 * 3 + i / 3) * 9 + c / 3 * 3 + i % 3] == v)
            return 0;
    return 1;
}
static int solve(unsigned char *b, int limit) {
    int at = -1, best = 10;
    for (int i = 0; i < 81; i++)
        if (!b[i]) {
            int n = 0;
            for (int v = 1; v <= 9; v++)
                n += allowed(b, i, v);
            if (!n)
                return 0;
            if (n < best) {
                best = n;
                at = i;
                if (n == 1)
                    break;
            }
        }
    if (at < 0)
        return 1;
    int count = 0;
    for (int v = 1; v <= 9 && count < limit; v++)
        if (allowed(b, at, v)) {
            b[at] = v;
            count += solve(b, limit - count);
            b[at] = 0;
        }
    return count;
}
int sudoku_count(const unsigned char *b, int limit) {
    unsigned char copy[81];
    memcpy(copy, b, 81);
    for (int i = 0; i < 81; i++)
        if (copy[i]) {
            int v = copy[i];
            copy[i] = 0;
            int okay = v <= 9 && allowed(copy, i, v);
            copy[i] = v;
            if (!okay)
                return 0;
        }
    return solve(copy, limit);
}
void sudoku_new(Sudoku *g, uint32_t seed, int level) {
    memset(g, 0, sizeof(*g));
    g->difficulty = level;
    int digits[9], rows[9], cols[9], bands[3] = {0, 1, 2}, stacks[3] = {0, 1, 2};
    for (int i = 0; i < 9; i++)
        digits[i] = i + 1;
    for (int i = 8; i > 0; i--) {
        int j = random_next(&seed) % (i + 1), v = digits[i];
        digits[i] = digits[j];
        digits[j] = v;
    }
    for (int i = 2; i > 0; i--) {
        int j = random_next(&seed) % (i + 1), v = bands[i];
        bands[i] = bands[j];
        bands[j] = v;
        j = random_next(&seed) % (i + 1);
        v = stacks[i];
        stacks[i] = stacks[j];
        stacks[j] = v;
    }
    for (int b = 0; b < 3; b++) {
        int a[3] = {0, 1, 2}, c[3] = {0, 1, 2};
        for (int i = 2; i > 0; i--) {
            int j = random_next(&seed) % (i + 1), v = a[i];
            a[i] = a[j];
            a[j] = v;
            j = random_next(&seed) % (i + 1);
            v = c[i];
            c[i] = c[j];
            c[j] = v;
        }
        for (int i = 0; i < 3; i++) {
            rows[b * 3 + i] = bands[b] * 3 + a[i];
            cols[b * 3 + i] = stacks[b] * 3 + c[i];
        }
    }
    for (int r = 0; r < 9; r++)
        for (int c = 0; c < 9; c++)
            g->solution[r * 9 + c] = digits[(rows[r] * 3 + rows[r] / 3 + cols[c]) % 9];
    memcpy(g->board.cell, g->solution, 81);
    int order[81];
    for (int i = 0; i < 81; i++)
        order[i] = i;
    for (int i = 80; i > 0; i--) {
        int j = random_next(&seed) % (i + 1), v = order[i];
        order[i] = order[j];
        order[j] = v;
    }
    int left = 81, target = level == 0 ? 39 : level == 1 ? 33 : 28;
    for (int k = 0; k < 81 && left > target; k++) {
        int at = order[k], v = g->board.cell[at];
        g->board.cell[at] = 0;
        if (sudoku_count(g->board.cell, 2) == 1)
            left--;
        else
            g->board.cell[at] = v;
    }
    memcpy(g->given, g->board.cell, 81);
}
static void sud_save(Sudoku *g) {
    if (g->undos == 24) {
        memmove(g->undo, g->undo + 1, 23 * sizeof(*g->undo));
        g->undos--;
    }
    g->undo[g->undos++] = g->board;
}
int sudoku_put(Sudoku *g, int at, int v, int pencil) {
    if (at < 0 || at >= 81 || g->given[at] || v < 0 || v > 9)
        return 0;
    sud_save(g);
    if (pencil && v) {
        if (!g->board.cell[at])
            g->board.notes[at] ^= 1u << (v - 1);
    } else {
        g->board.cell[at] = v;
        g->board.notes[at] = 0;
        if (v) {
            int r = at / 9, c = at % 9;
            for (int i = 0; i < 81; i++)
                if (i / 9 == r || i % 9 == c || (i / 27 == r / 3 && i % 9 / 3 == c / 3))
                    g->board.notes[i] &= ~(1u << (v - 1));
        }
    }
    return 1;
}
int sudoku_hint(Sudoku *g, int at) {
    if (at < 0 || at >= 81 || g->given[at] || g->board.cell[at] == g->solution[at]) {
        for (at = 0; at < 81; at++)
            if (g->board.cell[at] != g->solution[at])
                break;
    }
    if (at >= 81)
        return 0;
    int okay = sudoku_put(g, at, g->solution[at], 0);
    if (okay)
        g->hints++;
    return okay;
}
int sudoku_undo(Sudoku *g) {
    if (!g->undos)
        return 0;
    g->board = g->undo[--g->undos];
    return 1;
}
int sudoku_won(const Sudoku *g) { return !memcmp(g->board.cell, g->solution, 81); }
int sudoku_conflict(const Sudoku *g, int at) {
    if (at < 0 || at >= 81 || !g->board.cell[at])
        return 0;
    unsigned char b[81];
    memcpy(b, g->board.cell, 81);
    int v = b[at];
    b[at] = 0;
    return !allowed(b, at, v);
}
