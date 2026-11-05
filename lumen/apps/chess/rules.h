/* Local two-player board games. Rules and rendering kept separate from navigation. */
typedef struct {
    signed char b[64];
    int turn, rights, ep, half;
} ChessPosition;
static ChessPosition chess, chess_undo[256];
static int chess_ready, chess_history, chess_selected = -1, chess_flip, chess_promotion = -1,
                                       chess_promotion_from;
static int chess_computer;
static int tic[9], tic_turn = 1, tic_result, tic_score[3];
static int side(int p) {
    return p > 0 ? 1 : p < 0 ? -1 : 0;
}
static void chess_reset(void) {
    memset(&chess, 0, sizeof(chess));
    int row[] = {4, 2, 3, 5, 6, 3, 2, 4};
    for (int i = 0; i < 8; i++) {
        chess.b[i] = -row[i];
        chess.b[8 + i] = -1;
        chess.b[48 + i] = 1;
        chess.b[56 + i] = row[i];
    }
    chess.turn = 1;
    chess.rights = 15;
    chess.ep = -1;
    chess_selected = -1;
    chess_history = 0;
    chess_promotion = -1;
    chess_ready = 1;
}
static int chess_attacked(const ChessPosition *p, int sq, int by) {
    int x = sq % 8, y = sq / 8;
    for (int from = 0; from < 64; from++) {
        int v = p->b[from];
        if (side(v) != by)
            continue;
        int dx = x - from % 8, dy = y - from / 8, ax = abs(dx), ay = abs(dy), kind = abs(v);
        if (kind == 1 && ax == 1 && dy == -by)
            return 1;
        if (kind == 2 && ax * ay == 2)
            return 1;
        if (kind == 6 && ax <= 1 && ay <= 1 && (ax || ay))
            return 1;
        if ((kind == 3 && ax == ay && ax) || (kind == 4 && ((!dx && dy) || (!dy && dx))) ||
            (kind == 5 && ((ax == ay && ax) || (!dx && dy) || (!dy && dx)))) {
            int sx = side(dx), sy = side(dy), i = from % 8 + sx, j = from / 8 + sy;
            while (i != x || j != y) {
                if (p->b[j * 8 + i])
                    break;
                i += sx;
                j += sy;
            }
            if (i == x && j == y)
                return 1;
        }
    }
    return 0;
}
static int chess_check(const ChessPosition *p, int who) {
    for (int i = 0; i < 64; i++)
        if (p->b[i] == who * 6)
            return chess_attacked(p, i, -who);
    return 1;
}
static int chess_pseudo(const ChessPosition *p, int from, int to) {
    if (from < 0 || from >= 64 || to < 0 || to >= 64 || from == to)
        return 0;
    int v = p->b[from], who = side(v), target = p->b[to], kind = abs(v), dx = to % 8 - from % 8,
        dy = to / 8 - from / 8, ax = abs(dx), ay = abs(dy);
    if (who != p->turn || side(target) == who || abs(target) == 6)
        return 0;
    if (kind == 1) {
        if (!dx && !target && dy == -who)
            return 1;
        if (!dx && !target && dy == -2 * who && from / 8 == (who == 1 ? 6 : 1) &&
            !p->b[from - who * 8])
            return 1;
        if (ax == 1 && dy == -who && (target || (to == p->ep && p->b[to + who * 8] == -who)))
            return 1;
        return 0;
    }
    if (kind == 2)
        return ax * ay == 2;
    if (kind == 6) {
        if (ax <= 1 && ay <= 1)
            return 1;
        int base = who == 1 ? 56 : 0, mask = who == 1 ? 1 : 4;
        if (from != base + 4 || dy || ax != 2 || chess_check(p, who))
            return 0;
        if (dx == 2 && (p->rights & mask) && p->b[base + 7] == who * 4 && !p->b[base + 5] &&
            !p->b[base + 6] && !chess_attacked(p, base + 5, -who) &&
            !chess_attacked(p, base + 6, -who))
            return 1;
        if (dx == -2 && (p->rights & (mask * 2)) && p->b[base] == who * 4 && !p->b[base + 1] &&
            !p->b[base + 2] && !p->b[base + 3] && !chess_attacked(p, base + 3, -who) &&
            !chess_attacked(p, base + 2, -who))
            return 1;
        return 0;
    }
    if (!((kind == 3 && ax == ay && ax) || (kind == 4 && ((!dx && dy) || (!dy && dx))) ||
          (kind == 5 && ((ax == ay && ax) || (!dx && dy) || (!dy && dx)))))
        return 0;
    int sx = side(dx), sy = side(dy), x = from % 8 + sx, y = from / 8 + sy;
    while (x != to % 8 || y != to / 8) {
        if (p->b[y * 8 + x])
            return 0;
        x += sx;
        y += sy;
    }
    return 1;
}
static void chess_apply(ChessPosition *p, int from, int to, int promote) {
    int v = p->b[from], target = p->b[to], who = side(v), base = who == 1 ? 56 : 0;
    if (abs(v) == 1 && to == p->ep && !target)
        p->b[to + who * 8] = 0;
    p->b[to] = v;
    p->b[from] = 0;
    if (abs(v) == 6) {
        p->rights &= who == 1 ? ~3 : ~12;
        if (to - from == 2) {
            p->b[base + 5] = p->b[base + 7];
            p->b[base + 7] = 0;
        }
        if (to - from == -2) {
            p->b[base + 3] = p->b[base];
            p->b[base] = 0;
        }
    }
    if (from == 63 || to == 63)
        p->rights &= ~1;
    if (from == 56 || to == 56)
        p->rights &= ~2;
    if (from == 7 || to == 7)
        p->rights &= ~4;
    if (from == 0 || to == 0)
        p->rights &= ~8;
    p->ep = abs(v) == 1 && abs(to - from) == 16 ? (to + from) / 2 : -1;
    if (abs(v) == 1 && (to / 8 == 0 || to / 8 == 7))
        p->b[to] = who * promote;
    p->half = abs(v) == 1 || target ? 0 : p->half + 1;
    p->turn = -who;
}
static int chess_legal(const ChessPosition *p, int from, int to) {
    if (!chess_pseudo(p, from, to))
        return 0;
    ChessPosition q = *p;
    chess_apply(&q, from, to, 5);
    return !chess_check(&q, p->turn);
}
static int chess_moves(void) {
    for (int i = 0; i < 64; i++)
        for (int j = 0; j < 64; j++)
            if (chess_legal(&chess, i, j))
                return 1;
    return 0;
}
static int chess_drawn(void) {
    if (chess.half >= 100)
        return 1;
    int minor = 0, other = 0;
    for (int i = 0; i < 64; i++) {
        int k = abs(chess.b[i]);
        if (k == 2 || k == 3)
            minor++;
        else if (k && k != 6)
            other++;
    }
    if (!other && minor <= 1)
        return 1;
    int repeat = 1;
    for (int i = 0; i < chess_history; i++)
        if (chess_undo[i].turn == chess.turn && chess_undo[i].rights == chess.rights &&
            chess_undo[i].ep == chess.ep && !memcmp(chess_undo[i].b, chess.b, 64))
            repeat++;
    return repeat >= 3;
}
static void chess_store(void) {
    char p[2048];
    local_path(p, sizeof(p), "chess-v1.bin");
    atomic_write(p, &chess, sizeof(chess));
}
static void games_init(int target) {
    if (target == 48 && !chess_ready) {
        chess_reset();
        char p[2048];
        local_path(p, sizeof(p), "chess-v1.bin");
        FILE *f = fopen(p, "rb");
        if (f) {
            ChessPosition q;
            if (fread(&q, sizeof(q), 1, f) == 1 && (q.turn == 1 || q.turn == -1) && q.ep >= -1 &&
                q.ep < 64) {
                int good = 1, k1 = 0, k2 = 0;
                for (int i = 0; i < 64; i++) {
                    if (abs(q.b[i]) > 6)
                        good = 0;
                    k1 += q.b[i] == 6;
                    k2 += q.b[i] == -6;
                }
                if (good && k1 == 1 && k2 == 1)
                    chess = q;
            }
            fclose(f);
        }
    }
}
#include "../../system/ui/chess_icons.h"
static void app_chess(void) {
    heading_actions("Schach", 1);
    icon_button(528, 74, IC_RESTART);
    const char *modes[] = {"Computer", "Multiplayer"};
    segments(28, 125, 544, 36, modes, 2, !chess_computer);
    char b[80];
    int moves = chess_moves(), check = chess_check(&chess, chess.turn), draw = chess_drawn();
    const char *label = !moves            ? (check ? "Schachmatt" : "Patt")
                        : draw            ? "Remis"
                        : chess.turn == 1 ? "Weiß am Zug"
                                          : "Schwarz am Zug";
    snprintf(b, sizeof(b), "%s%s", tr(label), moves && !draw && check ? tr(" · Check") : "");
    centered(28, 163, 544, 26, chess_computer && chess.turn < 0 ? tr("Computer denkt …") : b,
             ui_font(UI_SECONDARY));
    for (int v = 0; v < 64; v++) {
        int sq = v, x = 36 + (v % 8) * 66, y = 190 + (v / 8) * 66;
        rect(x, y, 66, 66, (v / 8 + v % 8) % 2 ? 180 : 242);
        region(x, y, 66, 66);
        if (chess_selected == sq) {
            rect(x, y, 66, 66, 120);
        }
        if (chess.b[sq]) {
            chess_piece(x, y, chess.b[sq]);
            if (chess_flip) {
                for (int yy = 5; yy < 58; yy++)
                    for (int xx = 5; xx < 61; xx++) {
                        int ox = 66 - xx, oy = 64 - yy;
                        if (yy * 66 + xx >= oy * 66 + ox)
                            continue;
                        int a = (y + yy) * 600 + x + xx, b = (y + oy) * 600 + x + ox;
                        unsigned char v = ui_pixels[a];
                        ui_pixels[a] = ui_pixels[b];
                        ui_pixels[b] = v;
                    }
            }
        }
        if (chess_selected >= 0 && chess_legal(&chess, chess_selected, sq)) {
            if (chess.b[sq])
                ring(x + 33, y + 33, 29, 3);
            else
                disc(x + 33, y + 33, 5, 0);
        }
    }
    button_h(28, 740, 262, 48, "Zug zurück", 0);
    button_h(310, 740, 262, 48, "Figuren drehen", chess_flip);
    if (chess_promotion >= 0) {
        rect(28, 298, 544, 182, 255);
        centered(28, 306, 544, 44, "Umwandlung", ui_font(UI_PRIMARY));
        int pieces[] = {5, 4, 3, 2};
        for (int i = 0; i < 4; i++) {
            button_h(48 + i * 132, 373, 108, 80, "", 0);
            chess_piece(70 + i * 132, 379, chess.turn * pieces[i]);
        }
    }
}
static void app_tic(void) {
    heading_actions("Tic Tac Toe", 1);
    icon_button(528, 74, IC_RESTART);
    centered(28, 145, 544, 48,
             tic_result == 3 ? "Unentschieden"
             : tic_result    ? "Gewonnen!"
             : tic_turn == 1 ? "X am Zug"
                             : "O am Zug",
             ui_font(UI_PRIMARY));
    for (int i = 0; i < 9; i++) {
        int x = 63 + (i % 3) * 160, y = 241 + (i / 3) * 160;
        region(x, y, 154, 154);
        soft_rect(x, y, 154, 154, 12, 218);
        if (tic[i] == 1) {
            stroke(x + 40, y + 40, x + 114, y + 114, 6);
            stroke(x + 114, y + 40, x + 40, y + 114, 6);
        }
        if (tic[i] == 2)
            ring(x + 77, y + 77, 42, 6);
    }
    char b[100];
    snprintf(b, sizeof(b), "X  %d     ·     O  %d", tic_score[1], tic_score[2]);
    centered(28, 739, 544, 44, b, ui_font(UI_BODY));
}
static int games_input(const Input *in) {
    if (in->kind != INPUT_TAP)
        return 2;
    int x = in->x, y = in->y;
    if (screen == 48) {
        if (y >= 125 && y < 161) {
            chess_computer = x < 300;
            chess_selected = -1;
            return 1;
        }
        if (chess_promotion >= 0) {
            if (y >= 373 && y < 453 && x >= 48 && x < 576) {
                int i = (x - 48) / 132;
                if (i < 4) {
                    int pieces[] = {5, 4, 3, 2};
                    if (chess_history < 256)
                        chess_undo[chess_history++] = chess;
                    chess_apply(&chess, chess_promotion_from, chess_promotion, pieces[i]);
                    chess_promotion = -1;
                    chess_selected = -1;
                    chess_store();
                }
            }
            return 1;
        }
        if (y >= 74 && y < 118 && x >= 516) {
            if (history_count < 64)
                history[history_count++] = 48;
            route_to(50);
            return 1;
        }
        if (y >= 740) {
            if (x < 300 && chess_history) {
                chess = chess_undo[--chess_history];
                if (chess_computer && chess.turn < 0 && chess_history)
                    chess = chess_undo[--chess_history];
                chess_selected = -1;
                chess_store();
            } else if (x >= 300)
                chess_flip ^= 1;
            return 1;
        }
        if (chess_computer && chess.turn < 0)
            return 1;
        if (x < 36 || x >= 564 || y < 190 || y >= 718 || !chess_moves() || chess_drawn())
            return 1;
        int sq = ((y - 190) / 66) * 8 + (x - 36) / 66;

        if (chess_selected >= 0 && chess_legal(&chess, chess_selected, sq)) {
            if (abs(chess.b[chess_selected]) == 1 && (sq / 8 == 0 || sq / 8 == 7)) {
                chess_promotion = sq;
                chess_promotion_from = chess_selected;
                return 1;
            }
            if (chess_history < 256)
                chess_undo[chess_history++] = chess;
            chess_apply(&chess, chess_selected, sq, 5);
            chess_selected = -1;
            chess_store();
        } else
            chess_selected = side(chess.b[sq]) == chess.turn ? sq : -1;
        return 1;
    }
    if (screen == 50) {
        if (y >= 455 && y < 515) {
            chess_reset();
            chess_store();
            extra_return(48);
        } else if (y >= 540 && y < 600)
            extra_return(48);
        return 1;
    }
    if (y >= 74 && y < 118 && x >= 516) {
        memset(tic, 0, sizeof(tic));
        tic_turn = 1;
        tic_result = 0;
        return 1;
    }
    if (tic_result || x < 63 || x >= 537 || y < 241 || y >= 715)
        return 1;
    int c = (x - 63) / 160, r = (y - 241) / 160, i = r * 3 + c;
    if (c > 2 || r > 2 || tic[i])
        return 1;
    tic[i] = tic_turn;
    const int wins[8][3] = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {0, 3, 6},
                            {1, 4, 7}, {2, 5, 8}, {0, 4, 8}, {2, 4, 6}};
    for (int w = 0; w < 8; w++)
        if (tic[wins[w][0]] && tic[wins[w][0]] == tic[wins[w][1]] &&
            tic[wins[w][1]] == tic[wins[w][2]])
            tic_result = tic_turn;
    if (tic_result)
        tic_score[tic_turn]++;
    else {
        int empty = 0;
        for (int j = 0; j < 9; j++)
            empty += !tic[j];
        if (!empty)
            tic_result = 3;
    }
    tic_turn = 3 - tic_turn;
    return 1;
}
