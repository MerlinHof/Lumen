#ifdef LUMEN_PACKAGED
#include "lumen_app.h"
#else
#include "app.h"
#include "../../system/app_runtime.h"
#include "../../system/ui/paint.h"
#include <stdlib.h>

#endif

/* English-style peg solitaire, using a cross of lily pads. */
static unsigned long long frog_board, frog_undo[32];
static int frog_selected = -1, frog_moves, frog_ready;

static int frog_pad(int row, int col) {
    return row >= 0 && row < 7 && col >= 0 && col < 7 &&
           ((row >= 2 && row <= 4) || (col >= 2 && col <= 4));
}
static void frog_reset(void) {
    frog_board = 0;
    for (int i = 0; i < 49; i++)
        if (i != 24 && frog_pad(i / 7, i % 7)) frog_board |= 1ULL << i;
    frog_moves = 0;
    frog_selected = -1;
    frog_ready = 1;
}
static int frog_jump(int from, int to) {
    if (from < 0 || to < 0 || from >= 49 || to >= 49 || !frog_pad(to / 7, to % 7))
        return 0;
    int dr = abs(from / 7 - to / 7), dc = abs(from % 7 - to % 7);
    return ((dr == 2 && !dc) || (dc == 2 && !dr)) &&
           (frog_board & (1ULL << from)) && (frog_board & (1ULL << ((from + to)/2))) &&
           !(frog_board & (1ULL << to));
}
/* A crouching frog with a shaded belly, folded feet and reflective eyes. */
static void frog_piece(int x, int y, int selected, int look) {
    ui_paint_ellipse(x, y+17, 24, 6, 175);
    ui_paint_ellipse(x-15, y+9, 10, 12, 38);
    ui_paint_ellipse(x+15, y+9, 10, 12, 38);
    ui_paint_ellipse(x-15, y+8, 8, 10, 118);
    ui_paint_ellipse(x+15, y+8, 8, 10, 118);
    ui_paint_ellipse(x, y+6, 19, 15, 36);
    ui_paint_ellipse(x, y+5, 17, 13, 102);
    ui_paint_ellipse(x, y+11, 11, 7, 201);
    for (int side = -1; side <= 1; side += 2) {
        ui_paint_ellipse(x+side*11, y-11, 8, 8, 37);
        ui_paint_ellipse(x+side*11, y-11, 6, 6, 222);
        ui_paint_ellipse(x+side*11+look, y-10, 3, 4, 16);
        ui_paint_ellipse(x+side*11+look-1, y-12, 1, 1, 255);
        ui_paint_line(x+side*8, y+16, x+side*17, y+19, 3, 58);
    }
    ui_paint_ellipse(x, y-3, 17, 8, selected ? 163 : 138);
    ui_paint_line(x-8, y-1, x-4, y+2, 2, 42);
    ui_paint_line(x-4, y+2, x+4, y+2, 2, 42);
    ui_paint_line(x+4, y+2, x+8, y-1, 2, 42);
    ui_paint_ellipse(x-4, y-5, 1, 1, 73);
    ui_paint_ellipse(x+4, y-5, 1, 1, 73);
}

static void frog_draw(void) {
    if (!frog_ready) frog_reset();
    ui_paint_heading("Frosch-Solitär", 2);
    ui_paint_icon_button(468, 74, IC_UNDO);
    ui_paint_icon_button(528, 74, IC_RESTART);
    int can_move = 0;
    for (int i = 0; i < 49; i++) {
        if (!frog_pad(i/7,i%7)) continue;
        int x = 68 + i%7*77, y = 174 + i/7*77;
        ui_paint_hitbox(x-34,y-34,68,68);
        int target = frog_jump(frog_selected, i);
        ui_paint_ellipse(x, y+4, 28, 25, 198);
        ui_paint_ellipse(x, y+1, 28, 25, target ? 94 : 139);
        ui_paint_ellipse(x, y, 26, 23, target ? 211 : 239);
        ui_paint_line(x+7, y+5, x+27, y+16, 4, 255);
        if (i == 24) ui_paint_ring(x, y+3, 10, 2);
        if (target) ui_paint_ellipse(x, y+2, 6, 6, 43);
        if (!(frog_board & (1ULL << i))) continue;
        if (frog_jump(i,i-14)||frog_jump(i,i+14)||frog_jump(i,i-2)||frog_jump(i,i+2)) can_move=1;
        if (i == frog_selected) ui_paint_ring(x, y+1, 32, 3);
        frog_piece(x, y-3, i == frog_selected, i == frog_selected ? 0 : i%3-1);
    }
    ui_paint_text(28, 715, 544, 48, frog_board == (1ULL<<24) ? "Gewonnen!" :
                  can_move ? "Springe über einen Frosch auf ein freies Feld." :
                  "Keine Sprünge mehr. Rückgängig oder neu starten.", PAINT_BODY);
}
static int frog_input(const Input *in) {
    if (in->kind != INPUT_TAP) return 1;
    if (!frog_ready) frog_reset();
    if (in->y >= 74 && in->y < 118) {
        if (in->x >= 528) frog_reset();
        else if (in->x >= 468 && frog_moves) {
            frog_board = frog_undo[--frog_moves];
            frog_selected = -1;
        }
    } else if (in->x >= 30 && in->x < 569 && in->y >= 136 && in->y < 675) {
        int col = (in->x-30)/77, row = (in->y-136)/77, to = row*7+col;
        if (frog_pad(row,col)) {
            if (frog_board & (1ULL<<to)) frog_selected = frog_selected == to ? -1 : to;
            else if (frog_jump(frog_selected,to) && frog_moves < 32) {
                frog_undo[frog_moves++] = frog_board;
                frog_board ^= (1ULL<<frog_selected) | (1ULL<<((frog_selected+to)/2)) | (1ULL<<to);
                frog_selected = -1;
            }
        }
    }
    return 1;
}

#ifdef LUMEN_PACKAGED
static void frog_save(void){struct{unsigned long long board,undo[32];int selected,moves;}s={frog_board,{0},frog_selected,frog_moves};memcpy(s.undo,frog_undo,sizeof(frog_undo));if(frog_ready)app_state_save("game.bin",&s,sizeof(s));}
int main(void){struct{unsigned long long board,undo[32];int selected,moves;}s;
 if(app_state_load("game.bin",&s,sizeof(s))&&s.moves>=0&&s.moves<=32&&s.selected>=-1&&s.selected<49){frog_board=s.board;memcpy(frog_undo,s.undo,sizeof(frog_undo));frog_selected=s.selected;frog_moves=s.moves;frog_ready=1;}
 lumen_run(frog_draw,frog_input,frog_save);return 0;}
#else
void frog_register(void) {
    app_page_register(APP_FROG, frog_draw, frog_input, NULL);
}

#endif
