#include "engine.h"
#include <string.h>

static uint32_t random_next(uint32_t *r) {
    *r = *r * 1664525u + 1013904223u;
    return *r;
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
