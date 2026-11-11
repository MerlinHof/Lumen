#include "lumen_app.h"

static const char *local(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void help(const char *name,const char *text) {
    lumen_app_open_with("lumen.reader","open_document","",name,text);
}

typedef struct {
    unsigned char board[16][10];
    int piece, next, rotation, x, y, score, lines, over, paused;
} Blocks;
static Blocks block_game;
static unsigned random_below(unsigned limit){return limit ? (unsigned)rand()%limit : 0;}
static int blocks_cell(int p, int r, int x, int y);
static void blocks_new(Blocks *b);
static void blocks_move(Blocks *b, int dx);
static void blocks_rotate(Blocks *b);
static void blocks_tick(Blocks *b);
static void blocks_drop(Blocks *b);
static int blocks_landing_y(const Blocks *b);
/* Four by four masks; rotation is geometric and independent of rendering. */
static const uint16_t shapes[] = {0x00f0, 0x0066, 0x0072, 0x0036, 0x0063, 0x0071, 0x0074};
static int blocks_cell(int p, int r, int x, int y) {
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
    b->next = (int)random_below(7);
    b->x = 3;
    b->y = 0;
    b->rotation = 0;
    if (!fits(b, b->x, b->y, 0))
        b->over = 1;
}
static void blocks_new(Blocks *b) {
    memset(b, 0, sizeof(*b));
    b->next = (int)random_below(7);
    spawn(b);
    b->paused = 1;
}
static void blocks_move(Blocks *b, int dx) {
    if (!b->over && !b->paused && fits(b, b->x + dx, b->y, b->rotation))
        b->x += dx;
}
static void blocks_rotate(Blocks *b) {
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
static void blocks_tick(Blocks *b) {
    if (b->over || b->paused)
        return;
    if (fits(b, b->x, b->y + 1, b->rotation))
        b->y++;
    else
        lock_piece(b);
}
static void blocks_drop(Blocks *b) {
    if (b->over || b->paused)
        return;
    while (fits(b, b->x, b->y + 1, b->rotation)) {
        b->y++;
        b->score++;
    }
    lock_piece(b);
}
static int blocks_landing_y(const Blocks *b) {
    int y = b->y;
    while (fits(b, b->x, y + 1, b->rotation))
        y++;
    return y;
}

static int64_t next_tick;
static const int tick_ms=650;
static int input(const Input *in) {
    if(in->kind==INPUT_PAUSE || in->kind==INPUT_RESUME){block_game.paused=1;return 1;}
    if(in->kind==INPUT_TICK){if(!block_game.paused && !block_game.over)blocks_tick(&block_game);next_tick=ui_clock_millis()+tick_ms;return 1;}
    if(in->kind!=INPUT_TAP)return 0;
    int x=in->x,y=in->y;
    if(y>=74 && y<118) {
        if(x>=528)blocks_new(&block_game);
        else if(x>=468)help("Tetris",local("Drehe und verschiebe die Steine. Fülle Reihen, um sie zu entfernen.",
            "Rotate and move falling blocks. Complete horizontal lines to clear them.",
            "Tournez et déplacez les blocs. Complétez des lignes pour les effacer."));
    } else if(x>=414 && y>=124 && y<164) {
        if(block_game.over)blocks_new(&block_game);
        block_game.paused=!block_game.paused;
    } else if(y>=735 && y<783 && x>=49 && (x-49)/136<4 && (x-49)%136<94) {
        int key=(x-49)/136;
        if(key==0)blocks_move(&block_game,-1);
        else if(key==1)blocks_rotate(&block_game);
        else if(key==2)blocks_move(&block_game,1);
        else blocks_drop(&block_game);
    }
    return 1;
}
/* Adapters to shared SDK drawing primitives. */
#define RADIUS_SMALL 6
#define RADIUS_MEDIUM 10
#define UI_PRIMARY PAINT_HEADING
#define UI_SECONDARY PAINT_SECONDARY
#define ui_font(x) (x)
#define centered(x,y,w,h,s,r) ui_paint_text(x,y,w,h,s,r)
#define drawtext(x,y,s,r,w) lumen_text(x,y,w,34,s,r,-1,1)
#define region ui_paint_hitbox
#define rect(x,y,w,h,c) ui_paint_surface(x,y,w,h,0,c,c)
#define control_surface(x,y,w,h,r,c) ui_paint_surface(x,y,w,h,r,c,c)
#define outline_surface(x,y,w,h,r,c) ui_paint_surface(x,y,w,h,r,255,c)
#define key_outline(x,y,w,h) outline_surface(x,y,w,h,8,115)
#define button_h lumen_button
#define icon ui_paint_icon_button
static void board_tile(int x, int y, int size, int color) {
    control_surface(x + 1, y + 1, size - 2, size - 2, RADIUS_SMALL, color);
}
static void draw(void) {
    ui_paint_heading("Tetris",2);
    lumen_button(468,74,44,44,"?",0);
    ui_paint_icon_button(528,74,IC_RESTART);
    button_h(414, 124, 158, 40,
             block_game.over     ? local("Neu starten","Restart","Rejouer")
             : block_game.paused ? local("Start","Start","Démarrer")
                                 : local("Pause","Pause","Pause"),
             0);
    int x0 = 28, y0 = 132, cell = 36;
    outline_surface(x0 - 2, y0 - 2, 364, 580, RADIUS_MEDIUM, 128);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 10; x++)
            if (block_game.board[y][x])
                board_tile(x0 + x * cell, y0 + y * cell, cell, 65);
    if (!block_game.over) {
        int ghost = blocks_landing_y(&block_game);
        for (int y = 0; y < 4; y++)
            for (int x = 0; x < 4; x++)
                if (blocks_cell(block_game.piece, block_game.rotation, x, y)) {
                    int xx = x0 + (block_game.x + x) * cell, yy = y0 + (ghost + y) * cell;
                    if (ghost != block_game.y)
                        key_outline(xx + 2, yy + 2, cell - 4, cell - 4);
                    board_tile(xx, y0 + (block_game.y + y) * cell, cell, 0);
                }
    }
    char b[40];
    drawtext(410, 211, local("Punkte","Score","Score"), ui_font(UI_SECONDARY), 162);
    snprintf(b, sizeof(b), "%d", block_game.score);
    drawtext(410, 246, b, ui_font(UI_PRIMARY), 162);
    drawtext(410, 309, local("Linien","Lines","Lignes"), ui_font(UI_SECONDARY), 162);
    snprintf(b, sizeof(b), "%d", block_game.lines);
    drawtext(410, 344, b, ui_font(UI_PRIMARY), 162);
    drawtext(410, 407, local("Als Nächstes","Next","Suivant"), ui_font(UI_SECONDARY), 162);
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            if (blocks_cell(block_game.next, 0, x, y))
                board_tile(408 + x * 22, 452 + y * 22, 22, 0);
    if (block_game.paused || block_game.over) {
        rect(75, 334, 238, 100, 255);
        centered(75, 345, 238, 50, block_game.over ? local("Spiel beendet","Game over","Partie terminée") : local("Bereit?","Ready?","Prêt ?"), ui_font(UI_PRIMARY));
        centered(75, 395, 238, 30, local("Start oben antippen","Tap Start above","Touchez Démarrer"), ui_font(UI_SECONDARY));
    }
    int kinds[] = {IC_PREV, IC_ROTATE, IC_NEXT, IC_DOWN};
    for (int i = 0; i < 4; i++) {
        int x = 49 + i * 136;
        region(x, 735, 94, 48);
        key_outline(x, 735, 94, 48);
        icon(x + 25, 737, kinds[i]);
    }
    if(!block_game.paused && !block_game.over) {
        int64_t now=ui_clock_millis();
        if(!next_tick)next_tick=now+tick_ms;
        lumen_schedule(next_tick>now ? (int)(next_tick-now) : 20);
    } else next_tick=0;
}

int main(void) {
    srand((unsigned)time(NULL));
    blocks_new(&block_game);
    lumen_run(draw,input,NULL);
    return 0;
}
