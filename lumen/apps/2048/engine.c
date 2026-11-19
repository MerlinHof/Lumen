#include "engine.h"
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
