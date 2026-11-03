#include "../apps.h"
#include <string.h>
#include <stdlib.h>
static uint32_t random_state = 0x63b2a123;
void apps_seed(uint32_t n) {
    random_state = n ? n : 1;
}
static unsigned rnd(unsigned n) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return n ? random_state % n : 0;
}
int calendar_days(int y, int m) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m < 1 || m > 12)
        return 0;
    return days[m - 1] + (m == 2 && y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
}
void mines_new(Mines *m) {
    memset(m, 0, sizeof(*m));
}
int mines_adjacent(const Mines *m, int c) {
    int n = 0, x = c % 9, y = c / 9;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if ((dx || dy) && x + dx >= 0 && x + dx < 9 && y + dy >= 0 && y + dy < 10)
                n += m->bomb[(y + dy) * 9 + x + dx];
    return n;
}
void mines_flag(Mines *m, int c) {
    if (c < 0 || c >= 90 || m->over || m->open[c])
        return;
    m->flag[c] ^= 1;
    m->flags += m->flag[c] ? 1 : -1;
}
void mines_reveal(Mines *m, int c) {
    if (c < 0 || c >= 90 || m->over || m->flag[c])
        return;
    if (!m->started) {
        for (int n = 0; n < 12;) {
            int k = (int)rnd(90);
            if (abs(k % 9 - c % 9) <= 1 && abs(k / 9 - c / 9) <= 1)
                continue;
            if (!m->bomb[k]) {
                m->bomb[k] = 1;
                n++;
            }
        }
        m->started = 1;
    }
    if (m->open[c])
        return;
    if (m->bomb[c]) {
        m->open[c] = 1;
        m->over = 1;
        return;
    }
    int queue[90], head = 0, tail = 0;
    queue[tail++] = c;
    m->open[c] = 1;
    while (head < tail) {
        int k = queue[head++];
        if (mines_adjacent(m, k))
            continue;
        int x = k % 9, y = k / 9;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
                if (x + dx >= 0 && x + dx < 9 && y + dy >= 0 && y + dy < 10) {
                    int j = (y + dy) * 9 + x + dx;
                    if (!m->open[j] && !m->flag[j] && !m->bomb[j]) {
                        m->open[j] = 1;
                        queue[tail++] = j;
                    }
                }
    }
    int open = 0;
    for (int i = 0; i < 90; i++)
        open += m->open[i];
    if (open == 78)
        m->won = m->over = 1;
}
static const char *de[] = {"BIBLIOTHEK",    "KAPITEL",    "SCHMETTERLING", "WALD",
                           "GARTEN",        "LICHT",      "ABENTEUER",     "LESEZEICHEN",
                           "STERNENHIMMEL", "REGENBOGEN", "BUCHSTABE",     "PHANTASIE",
                           "INSEL",         "FERNWEH",    "GESCHICHTE",    "WISSEN",
                           "SCHIFF",        "NACHT",      "SONNENSCHEIN",  "PAPIER"};
static const char *en[] = {"LIBRARY",   "CHAPTER",     "BUTTERFLY", "FOREST",    "GARDEN",
                           "SUNLIGHT",  "ADVENTURE",   "BOOKMARK",  "STARLIGHT", "RAINBOW",
                           "LETTER",    "IMAGINATION", "ISLAND",    "JOURNEY",   "STORY",
                           "KNOWLEDGE", "OCEAN",       "NIGHT",     "PAPER",     "WONDER"};
static const char *fr[]={"BIBLIOTHEQUE","CHAPITRE","PAPILLON","FORET","JARDIN","LUMIERE","AVENTURE","SIGNET","ETOILE","ARCENCIEL","LETTRE","IMAGINATION","ILE","VOYAGE","HISTOIRE","SAVOIR","OCEAN","NUIT","PAPIER","REVE"};
static const char *pinyin[]={"TUSHU","ZHANGJIE","HUDIE","SENLIN","HUAYUAN","YANGGUANG","MAOXIAN","SHUQIAN","XINGKONG","CAIHONG","ZIMU","XIANGXIANG","DAO","LUXING","GUSHI","ZHISHI","HAIYANG","YEWAN","ZHIZHANG","MENGXIANG"};
void hangman_new(Hangman *h,int language){memset(h,0,sizeof(*h));const char **words=language==2?fr:language==3?pinyin:language==1?en:de;strcpy(h->word,words[rnd(20)]);}
void hangman_guess(Hangman *h, int letter) {
    if (letter < 0 || letter >= 26 || h->over || (h->guesses & (1u << letter)))
        return;
    h->guesses |= 1u << letter;
    int found = 0, missing = 0;
    for (const char *p = h->word; *p; p++) {
        if (*p == 'A' + letter)
            found = 1;
        if (!(h->guesses & (1u << (*p - 'A'))))
            missing++;
    }
    if (!found)
        h->misses++;
    if (!missing)
        h->won = h->over = 1;
    else if (h->misses >= 6)
        h->over = 1;
}
/* Four by four masks; rotation is geometric and independent of rendering. */
static const uint16_t shapes[] = {0x00f0, 0x0066, 0x0072, 0x0036, 0x0063, 0x0071, 0x0074};
int blocks_cell(int p, int r, int x, int y) {
    if (p < 0 || p > 6 || x < 0 || x > 3 || y < 0 || y > 3)
        return 0;
    if (p == 1)
        r = 0;
    for (int turn = 0; turn < (r & 3); turn++) {
        int z = x;
        x = y;
        y = 3 - z;
    }
    return (shapes[p] >> (y * 4 + x)) & 1;
}
static int fits(const Blocks *b, int x, int y, int r) {
    for (int j = 0; j < 4; j++)
        for (int i = 0; i < 4; i++)
            if (blocks_cell(b->piece, r, i, j)) {
                int xx = x + i, yy = y + j;
                if (xx < 0 || xx >= 10 || yy >= 16 || (yy >= 0 && b->board[yy][xx]))
                    return 0;
            }
    return 1;
}
static void spawn(Blocks *b) {
    b->piece = b->next;
    b->next = (int)rnd(7);
    b->x = 3;
    b->y = 0;
    b->rotation = 0;
    if (!fits(b, b->x, b->y, 0))
        b->over = 1;
}
void blocks_new(Blocks *b) {
    memset(b, 0, sizeof(*b));
    b->next = (int)rnd(7);
    spawn(b);
    b->paused = 1;
}
void blocks_move(Blocks *b, int dx) {
    if (!b->over && !b->paused && fits(b, b->x + dx, b->y, b->rotation))
        b->x += dx;
}
void blocks_rotate(Blocks *b) {
    if (b->over || b->paused)
        return;
    int r = (b->rotation + 1) % 4;
    int kicks[] = {0, -1, 1, -2, 2};
    for (int k = 0; k < 5; k++)
        if (fits(b, b->x + kicks[k], b->y, r)) {
            b->x += kicks[k];
            b->rotation = r;
            return;
        }
}
static void lock_piece(Blocks *b) {
    for (int j = 0; j < 4; j++)
        for (int i = 0; i < 4; i++)
            if (blocks_cell(b->piece, b->rotation, i, j)) {
                int x = b->x + i, y = b->y + j;
                if (y >= 0 && y < 16 && x >= 0 && x < 10)
                    b->board[y][x] = 1;
                else
                    b->over = 1;
            }
    int cleared = 0;
    for (int y = 15; y >= 0; y--) {
        int full = 1;
        for (int x = 0; x < 10; x++)
            if (!b->board[y][x])
                full = 0;
        if (full) {
            for (int j = y; j > 0; j--)
                memcpy(b->board[j], b->board[j - 1], 10);
            memset(b->board[0], 0, 10);
            cleared++;
            y++;
        }
    }
    b->lines += cleared;
    b->score += cleared == 4   ? 800
                : cleared == 3 ? 500
                : cleared == 2 ? 300
                : cleared == 1 ? 100
                               : 0;
    if (!b->over)
        spawn(b);
}
void blocks_tick(Blocks *b) {
    if (b->over || b->paused)
        return;
    if (fits(b, b->x, b->y + 1, b->rotation))
        b->y++;
    else
        lock_piece(b);
}
void blocks_drop(Blocks *b) {
    if (b->over || b->paused)
        return;
    while (fits(b, b->x, b->y + 1, b->rotation)) {
        b->y++;
        b->score++;
    }
    lock_piece(b);
}
static void food(Snake *s) {
    int free_cells[288], n = 0;
    for (int c = 0; c < 288; c++) {
        int occupied = 0;
        for (int i = 0; i < s->length; i++)
            if (s->cells[i] == c) {
                occupied = 1;
                break;
            }
        if (!occupied)
            free_cells[n++] = c;
    }
    if (!n) {
        s->over = 1;
        s->food = -1;
    } else
        s->food = free_cells[rnd((unsigned)n)];
}
void snake_new(Snake *s) {
    memset(s, 0, sizeof(*s));
    s->length = 4;
    s->dx = s->next_dx = 1;
    for (int i = 0; i < 4; i++)
        s->cells[i] = 8 * 18 + 8 - i;
    s->paused = 1;
    food(s);
}
void snake_direction(Snake *s, int dx, int dy) {
    if (abs(dx) + abs(dy) != 1 || (dx == -s->dx && dy == -s->dy))
        return;
    s->next_dx = dx;
    s->next_dy = dy;
}
void snake_tick(Snake *s) {
    if (s->over || s->paused)
        return;
    s->dx = s->next_dx;
    s->dy = s->next_dy;
    int x = s->cells[0] % 18 + s->dx, y = s->cells[0] / 18 + s->dy;
    if (x < 0 || x >= 18 || y < 0 || y >= 16) {
        s->over = 1;
        return;
    }
    int c = y * 18 + x, grow = c == s->food;
    for (int i = 0; i < s->length - (grow ? 0 : 1); i++)
        if (s->cells[i] == c) {
            s->over = 1;
            return;
        }
    if (grow && s->length < 288)
        s->length++;
    for (int i = s->length - 1; i > 0; i--)
        s->cells[i] = s->cells[i - 1];
    s->cells[0] = c;
    if (grow) {
        s->score++;
        food(s);
    }
}
int blocks_landing_y(const Blocks *b) {
    int y = b->y;
    while (fits(b, b->x, y + 1, b->rotation))
        y++;
    return y;
}
