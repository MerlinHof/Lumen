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
static void frog_draw(void) {
    if (!frog_ready) frog_reset();
    heading_actions("Frosch-Solitär", 2);
    icon_button(468, 74, IC_UNDO);
    icon_button(528, 74, IC_RESTART);
    int can_move = 0;
    for (int i = 0; i < 49; i++) {
        if (!frog_pad(i/7,i%7)) continue;
        int x = 68 + i%7*77, y = 174 + i/7*77;
        region(x-34,y-34,68,68);
        ring(x,y,29,2);
        if (i == 24) ring(x,y,24,2);
        if (frog_jump(frog_selected,i)) disc(x,y,5,0);
        if (!(frog_board & (1ULL<<i))) continue;
        if (frog_jump(i,i-14)||frog_jump(i,i+14)||frog_jump(i,i-2)||frog_jump(i,i+2)) can_move=1;
        if (i == frog_selected) ring(x,y,33,4);
        aa_disc(x,y+4,17,0);
        aa_disc(x-11,y-10,8,0); aa_disc(x+11,y-10,8,0);
        aa_disc(x-11,y-10,3,255); aa_disc(x+11,y-10,3,255);
        stroke_color(x-6,y+8,x+6,y+8,2,255);
    }
    type_centered(28, 715, 544, 48, frog_board == (1ULL<<24) ? "Gewonnen!" :
                  can_move ? "Springe über einen Frosch auf ein freies Feld." :
                  "Keine Sprünge mehr. Rückgängig oder neu starten.", UI_BODY);
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
