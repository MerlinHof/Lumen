#include "puzzles.h"
#include <stdlib.h>
#include <string.h>
static uint32_t random_next(uint32_t *r) {
    *r = *r * 1664525u + 1013904223u;
    return *r;
}
static void spawn(TileState *s) {
    int empty[16], n = 0;
    for (int i = 0; i < 16; i++)
        if (!s->cell[i])
            empty[n++] = i;
    if (n)
        s->cell[empty[random_next(&s->rng) % n]] = random_next(&s->rng) % 10 ? 1 : 2;
}
void tiles_new(Tiles *g, uint32_t seed) {
    memset(g, 0, sizeof(*g));
    g->state.rng = seed;
    spawn(&g->state);
    spawn(&g->state);
}
int tiles_move(Tiles *g, int direction) {
    if (direction < 0 || direction > 3)
        return 0;
    TileState next = g->state;
    for (int line = 0; line < 4; line++) {
        int indices[4];
        unsigned char a[4] = {0}, b[4] = {0};
        int n = 0, k = 0;
        for (int i = 0; i < 4; i++) {
            indices[i] = direction == 0   ? line * 4 + i
                         : direction == 1 ? line * 4 + 3 - i
                         : direction == 2 ? i * 4 + line
                                          : (3 - i) * 4 + line;
            unsigned char v = next.cell[indices[i]];
            if (v)
                a[n++] = v;
        }
        for (int i = 0; i < n; i++) {
            unsigned v = a[i];
            if (i + 1 < n && a[i + 1] == v && v < 30) {
                v++;
                i++;
                next.score += (uint64_t)1 << v;
            }
            b[k++] = v;
            if (v >= 11)
                next.won = 1;
        }
        for (int i = 0; i < 4; i++)
            next.cell[indices[i]] = b[i];
    }
    if (!memcmp(next.cell, g->state.cell, 16))
        return 0;
    g->undo = g->state;
    g->can_undo = 1;
    spawn(&next);
    g->state = next;
    return 1;
}
int tiles_over(const Tiles *g) {
    for (int i = 0; i < 16; i++) {
        if (!g->state.cell[i])
            return 0;
        if (i % 4 < 3 && g->state.cell[i] == g->state.cell[i + 1])
            return 0;
        if (i / 4 < 3 && g->state.cell[i] == g->state.cell[i + 4])
            return 0;
    }
    return 1;
}
static int rank(int c) { return c % 13 + 1; }
static int suit(int c) { return c / 13; }
static int red(int c) { return suit(c) < 2; }
static void pat_save(Patience *g) {
    if (g->undos == 24) {
        memmove(g->undo, g->undo + 1, 23 * sizeof(*g->undo));
        g->undos--;
    }
    g->undo[g->undos++] = g->state;
}
void patience_new(Patience *g, uint32_t seed) {
    memset(g, 0, sizeof(*g));
    unsigned char deck[52];
    for (int i = 0; i < 52; i++)
        deck[i] = i;
    for (int i = 51; i > 0; i--) {
        int j = random_next(&seed) % (i + 1);
        unsigned char c = deck[i];
        deck[i] = deck[j];
        deck[j] = c;
    }
    int at = 0;
    for (int c = 0; c < 7; c++) {
        g->state.count[c] = c + 1;
        g->state.hidden[c] = c;
        for (int r = 0; r <= c; r++)
            g->state.column[c][r] = deck[at++];
    }
    for (int i = 0; i < 24; i++)
        g->state.stock[i] = deck[at++];
    g->state.ns = 24;
    g->state.rng = seed;
}
int patience_draw(Patience *g) {
    PatienceState *s = &g->state;
    if (!s->ns && !s->nw)
        return 0;
    pat_save(g);
    if (s->ns)
        s->waste[s->nw++] = s->stock[--s->ns];
    else {
        while (s->nw)
            s->stock[s->ns++] = s->waste[--s->nw];
    }
    s->moves++;
    return 1;
}
int patience_move(Patience *g, int from, int index, int to) {
    PatienceState *s = &g->state;
    int card, n = 1;
    if (from == -1) {
        if (!s->nw)
            return 0;
        card = s->waste[s->nw - 1];
    } else if (from >= 0 && from < 7) {
        if (index < s->hidden[from] || index >= s->count[from])
            return 0;
        card = s->column[from][index];
        n = s->count[from] - index;
        for (int i = index + 1; i < s->count[from]; i++)
            if (rank(s->column[from][i - 1]) != rank(s->column[from][i]) + 1 ||
                red(s->column[from][i - 1]) == red(s->column[from][i]))
                return 0;
    } else if (from >= 7 && from < 11) {
        int u = from - 7;
        if (!s->foundation[u])
            return 0;
        card = u * 13 + s->foundation[u] - 1;
    } else
        return 0;
    if (to == from)
        return 0;
    if (to >= 7 && to < 11) {
        int u = to - 7;
        if (n != 1 || suit(card) != u || rank(card) != s->foundation[u] + 1)
            return 0;
    } else if (to >= 0 && to < 7) {
        int count = s->count[to];
        if (count + n > 52)
            return 0;
        if (count) {
            int target = s->column[to][count - 1];
            if (red(target) == red(card) || rank(target) != rank(card) + 1)
                return 0;
        } else if (rank(card) != 13)
            return 0;
    } else
        return 0;
    pat_save(g);
    if (to >= 7)
        s->foundation[to - 7]++;
    else {
        if (from >= 0 && from < 7)
            memcpy(s->column[to] + s->count[to], s->column[from] + index, n);
        else
            s->column[to][s->count[to]] = card;
        s->count[to] += n;
    }
    if (from == -1)
        s->nw--;
    else if (from >= 7)
        s->foundation[from - 7]--;
    else {
        s->count[from] -= n;
        if (s->hidden[from] && s->hidden[from] >= s->count[from])
            s->hidden[from]--;
    }
    s->moves++;
    return 1;
}
int patience_auto(Patience *g) {
    int moved = 0, again;
    do {
        again = 0;
        PatienceState *s = &g->state;
        if (s->nw) {
            int c = s->waste[s->nw - 1];
            if (patience_move(g, -1, 0, 7 + suit(c))) {
                again = moved = 1;
                continue;
            }
        }
        for (int i = 0; i < 7; i++)
            if (s->count[i]) {
                int c = s->column[i][s->count[i] - 1];
                if (patience_move(g, i, s->count[i] - 1, 7 + suit(c))) {
                    again = moved = 1;
                    break;
                }
            }
    } while (again);
    return moved;
}
int patience_undo(Patience *g) {
    if (!g->undos)
        return 0;
    g->state = g->undo[--g->undos];
    return 1;
}
int patience_won(const Patience *g) {
    for (int i = 0; i < 4; i++)
        if (g->state.foundation[i] != 13)
            return 0;
    return 1;
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
