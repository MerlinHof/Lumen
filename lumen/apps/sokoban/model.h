/* App-owned module; assembled by apps/host/compose.h. */
/* Solvable reverse-generated warehouses and larger perfect mazes. */
typedef struct {
    int level, w, h, x, y, moves;
    unsigned char board[361];
} Warehouse17;
static Warehouse17 warehouse17, undo_warehouse17, initial_warehouse17;
static int warehouse_ready17, warehouse_undo17, warehouse_facing17 = 4;
static unsigned char maze17[33 * 35], maze_trace17[33 * 35];
static int maze_ready17, maze_x17 = 1, maze_y17 = 1, maze_moves17, maze_win17;
static int box17(int v) {
    return v == 3 || v == 5;
}
static int floor17(int v) {
    return v == 0 || v == 2;
}
static void warehouse_new17(int level) {
    Warehouse17 *s = &warehouse17;
    memset(s, 0, sizeof(*s));
    s->level = level < 1 ? 1 : level;
    int scale = (level - 1) / 3;
    if (scale > 4)
        scale = 4;
    s->w = 11 + scale * 2;
    s->h = 13 + scale * 2;
    if (s->h > 19)
        s->h = 19;
    int boxes = 3 + (level - 1) / 2;
    if (boxes > 8)
        boxes = 8;
    Warehouse17 best;
    int best_score=-1;
    for (int attempt = 0; attempt < 24; attempt++) {
        memset(s->board, 0, sizeof(s->board));
        for (int y = 0; y < s->h; y++)
            for (int x = 0; x < s->w; x++)
                if (!x || !y || x == s->w - 1 || y == s->h - 1)
                    s->board[y * s->w + x] = 1;
        for (int k = 0; k < (s->w * s->h) * (8 + scale * 2) / 100; k++) {
            int x = 1 + random16() % (s->w - 2), y = 1 + random16() % (s->h - 2);
            s->board[y * s->w + x] = 1;
        }
        for (int k = 0; k < boxes; k++) {
            int at;
            do {
                int x = 2 + random16() % (s->w - 4), y = 2 + random16() % (s->h - 4);
                at = y * s->w + x;
            } while (s->board[at]);
            s->board[at] = 5;
        }
        do {
            s->x = 1 + random16() % (s->w - 2);
            s->y = 1 + random16() % (s->h - 2);
        } while (s->board[s->y * s->w + s->x]);
        int pulls = 0;
        for (int k = 0; k < 5000 + scale * 2000; k++) {
            int d = random16() % 4,
                dx = d == 0   ? 1
                     : d == 1 ? -1
                              : 0,
                dy = d == 2   ? 1
                     : d == 3 ? -1
                              : 0,
                nx = s->x + dx, ny = s->y + dy;
            if (nx <= 0 || ny <= 0 || nx >= s->w - 1 || ny >= s->h - 1 ||
                !floor17(s->board[ny * s->w + nx]))
                continue;
            int behind = (s->y - dy) * s->w + s->x - dx;
            if (box17(s->board[behind]) && (random16() & 3)) {
                int here = s->y * s->w + s->x;
                s->board[here] = s->board[here] == 2 ? 5 : 3;
                s->board[behind] = s->board[behind] == 5 ? 2 : 0;
                pulls++;
            }
            s->x = nx;
            s->y = ny;
        }
        int off = 0;
        for (int i = 0; i < s->w * s->h; i++)
            if (s->board[i] == 3)
                off++;
        int distance=0;
        for (int at=0;at<s->w*s->h;at++) if (box17(s->board[at])) {
            int nearest=s->w+s->h;
            for (int goal=0;goal<s->w*s->h;goal++)
                if (s->board[goal]==2 || s->board[goal]==5) {
                    int d=abs(at%s->w-goal%s->w)+abs(at/s->w-goal/s->w);
                    if (d<nearest) nearest=d;
                }
            distance+=nearest;
        }
        int score=distance+off*4;
        if (pulls>8 && score>best_score) { best=*s;best_score=score; }
        if (off==boxes && distance>=boxes*(3+scale)) break;
    }
    if (best_score>=0) *s=best;
    s->moves = 0;
    initial_warehouse17 = *s;
    warehouse_ready17 = 1;
    warehouse_undo17 = 0;
}
static void warehouse_step17(int dx, int dy) {
    Warehouse17 *s = &warehouse17;
    warehouse_facing17 = dx < 0 ? 7 : dx > 0 ? 8 : dy < 0 ? 6 : 4;
    int nx = s->x + dx, ny = s->y + dy;
    if (nx < 0 || ny < 0 || nx >= s->w || ny >= s->h)
        return;
    int at = ny * s->w + nx, v = s->board[at];
    if (v == 1)
        return;
    int to = (ny + dy) * s->w + nx + dx;
    if (box17(v) && (nx + dx < 0 || nx + dx >= s->w || ny + dy < 0 || ny + dy >= s->h ||
                     !floor17(s->board[to])))
        return;
    undo_warehouse17 = *s;
    warehouse_undo17 = 1;
    if (box17(v)) {
        s->board[to] = s->board[to] == 2 ? 5 : 3;
        s->board[at] = v == 5 ? 2 : 0;
    }
    s->x = nx;
    s->y = ny;
    s->moves++;
    int off = 0;
    for (int i = 0; i < s->w * s->h; i++)
        if (s->board[i] == 3)
            off++;
    if (!off) {
        char b[120];
        snprintf(b, sizeof(b), tr("Level %d geschafft!"), s->level);
        int level = s->level;
        warehouse_new17(level + 1);
        ui_message(b);
    }
}
