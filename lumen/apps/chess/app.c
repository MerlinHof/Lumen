#include "lumen_app.h"
#include "strings.h"

static const char *tr(const char *text) {
    int language=lumen_language==1 ? 1 : lumen_language==2 ? 2 : 0;
    for(size_t i=0;i<sizeof(strings)/sizeof(*strings);i++)
        if(!strcmp(strings[i].key,text))return strings[i].values[language];
    return text;
}
#define heading_actions ui_paint_heading
#define icon_button ui_paint_icon_button
#define UI_PRIMARY PAINT_HEADING
#define UI_SECONDARY PAINT_SECONDARY
#define ui_font(x) (x)
#define centered(x,y,w,h,s,r) ui_paint_text(x,y,w,h,tr(s),r)
#define rect(x,y,w,h,c) ui_paint_surface(x,y,w,h,0,c,c)
#define region ui_paint_hitbox
#define ring ui_paint_ring
#define disc(x,y,r,c) ui_paint_ellipse(x,y,r,r,c)
#define button_h(x,y,w,h,s,p) lumen_button(x,y,w,h,tr(s),p)

/* Local two-player board games. Rules and rendering kept separate from navigation. */
typedef struct {
    signed char b[64];
    int turn, rights, ep, half;
} ChessPosition;
static ChessPosition chess, chess_undo[256];
static int chess_history, chess_selected = -1, chess_flip, chess_promotion = -1,
                                       chess_promotion_from;
static int chess_computer;
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
typedef struct {
    ChessPosition current,history[256];
    int count,computer,rotated;
} ChessSave;
static void chess_store(void) {
    static ChessSave saved;
    saved.current=chess;saved.count=chess_history;saved.computer=chess_computer;saved.rotated=chess_flip;
    memset(saved.history,0,sizeof(saved.history));
    memcpy(saved.history,chess_undo,(size_t)chess_history*sizeof(*chess_undo));
    app_state_save("game.bin",&saved,sizeof(saved));
}
static int valid(const ChessPosition *saved) {
    if((saved->turn!=1 && saved->turn!=-1) || saved->ep< -1 || saved->ep>=64 ||
       saved->rights<0 || saved->rights>15 || saved->half<0)return 0;
    int white=0,black=0;
    for(int i=0;i<64;i++) {
        if(abs(saved->b[i])>6)return 0;
        white+=saved->b[i]==6;black+=saved->b[i]==-6;
    }
    return white==1 && black==1;
}
static void load(void) {
    chess_reset();
    static ChessSave saved;
    if(app_state_load("game.bin",&saved,sizeof(saved)) && valid(&saved.current)) {
        chess=saved.current;chess_computer=!!saved.computer;chess_flip=!!saved.rotated;
        if(saved.count>=0 && saved.count<=256) {
            for(int i=0;i<saved.count;i++)if(!valid(saved.history+i))return;
            memcpy(chess_undo,saved.history,(size_t)saved.count*sizeof(*chess_undo));
            chess_history=saved.count;
        }
    } else {
        ChessPosition old;
        if(lumen_legacy_state("chess-v1.bin",&old,sizeof(old)) && valid(&old))chess=old;
    }
}
static void draw(void) {
    heading_actions(tr("Schach"),2);
    lumen_button(468,74,44,44,"?",0);
    icon_button(528, 74, IC_RESTART);
    const char *modes[] = {tr("Computer"),tr("Multiplayer")};
    lumen_segments("opponent",28,125,544,36,modes,2,!chess_computer);
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
        if(chess.b[sq])lumen_chess_piece(x,y,chess.b[sq],chess_flip);
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
            lumen_chess_piece(70+i*132,379,chess.turn*pieces[i],0);
        }
    }
    if(chess_computer && chess.turn<0 && moves && !draw)lumen_schedule(80);
}
static void computer_move(void);
static int input(const Input *in) {
    if(in->kind==INPUT_TICK){computer_move();return 1;}
    if(in->kind==INPUT_COMPONENT) {
        char id[24],value[8],gesture[16];
        lumen_string("id",id,sizeof(id));lumen_string("text",value,sizeof(value));
        lumen_string("gesture",gesture,sizeof(gesture));
        if(!strcmp(id,"opponent") && !strcmp(gesture,"selected") && chess_promotion<0) {
            chess_computer=!strcmp(value,"0");chess_selected=-1;chess_store();
        }
        return 1;
    }
    if(in->kind==INPUT_SELECT && in->x==1 && in->y==1){chess_reset();chess_store();return 1;}
    if(in->kind!=INPUT_TAP)return 0;
    int x=in->x,y=in->y;
    {
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
        if(y>=74 && y<118 && x>=468 && x<512) {
            lumen_app_open_with("lumen.reader","open_document","",tr("Schach"),
                lumen_language==1 ? "Checkmate the opposing king. Tap a piece, then a legal square. Play the computer or take turns on this Kindle." :
                lumen_language==2 ? "Mettez le roi adverse échec et mat. Touchez une pièce, puis une case autorisée." :
                "Setze den gegnerischen König matt. Tippe eine Figur und dann ein erlaubtes Zielfeld an.");
            return 1;
        }
        if(y>=74 && y<118 && x>=528 && x<572) {
            const char *choices[]={tr("Abbrechen"),tr("Neue Partie")};
            lumen_select(choices,2,-1,1);return 1;
        }
        if (y >= 740) {
            if (x < 300 && chess_history) {
                chess = chess_undo[--chess_history];
                if (chess_computer && chess.turn < 0 && chess_history)
                    chess = chess_undo[--chess_history];
                chess_selected = -1;
                chess_store();
            } else if (x >= 300) {
                chess_flip ^= 1;
                chess_store();
            }
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
}

static unsigned chess_nodes;
static int chess_eval(const ChessPosition *p) {
    static const int val[] = {0, 100, 320, 330, 500, 900, 20000};
    int n = 0;
    for (int i = 0; i < 64; i++) {
        int v = p->b[i];
        if (v) {
            int center = 7 - abs(i % 8 * 2 - 7) / 2 - abs(i / 8 * 2 - 7) / 2;
            n += side(v) * (val[abs(v)] + (abs(v) == 1  ? (v > 0 ? 6 - i / 8 : i / 8 - 1) * 5
                                           : abs(v) < 4 ? center * 5
                                                        : 0));
        }
    }
    return n * p->turn;
}
static int chess_search(const ChessPosition *p, int depth, int alpha, int beta) {
    if (!depth || ++chess_nodes > 3500)
        return chess_eval(p);
    int best = -30000, moves = 0;
    for (int captures = 1; captures >= 0; captures--)
        for (int f = 0; f < 64; f++)
            if (side(p->b[f]) == p->turn)
                for (int t = 0; t < 64; t++) {
                    if ((p->b[t] != 0) != captures || !chess_pseudo(p, f, t) ||
                        !chess_legal(p, f, t))
                        continue;
                    moves++;
                    ChessPosition q = *p;
                    chess_apply(&q, f, t, 5);
                    int score = -chess_search(&q, depth - 1, -beta, -alpha);
                    if (score > best)
                        best = score;
                    if (score > alpha)
                        alpha = score;
                    if (alpha >= beta)
                        return alpha;
                }
    return moves ? best : chess_check(p, p->turn) ? -29000 - depth : 0;
}
static void computer_move(void) {
    if(!chess_computer || chess.turn!=-1 || chess_drawn())return;
    chess_nodes=0;
    int move[2]={-1,-1},best=-40000;
    for(int from=0;from<64;from++) {
        if(side(chess.b[from])!=chess.turn)continue;
        for(int to=0;to<64;to++) {
            if(!chess_legal(&chess,from,to))continue;
            ChessPosition next=chess;
            chess_apply(&next,from,to,5);
            int score=-chess_search(&next,2,-30000,30000);
            if(score>best){best=score;move[0]=from;move[1]=to;}
        }
    }
    if(move[0]<0)return;
    if(chess_history<256)chess_undo[chess_history++]=chess;
    chess_apply(&chess,move[0],move[1],5);
    chess_store();
}
int main(void){load();lumen_run(draw,input,NULL);return 0;}
