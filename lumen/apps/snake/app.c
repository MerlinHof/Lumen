#include "lumen_app.h"

static const char *local(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void help(const char *name,const char *text) {
    lumen_app_open_with("lumen.reader","open_document","",name,text);
}

typedef struct {
    int cells[288], length, dx, dy, next_dx, next_dy, food, over, paused, score;
} Snake;
static Snake snake_game;
static unsigned random_below(unsigned limit){return limit ? (unsigned)rand()%limit : 0;}
static void snake_new(Snake *s);
static void snake_direction(Snake *s, int dx, int dy);
static void snake_tick(Snake *s);
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
        s->food = free_cells[random_below((unsigned)n)];
}
static void snake_new(Snake *s) {
    memset(s, 0, sizeof(*s));
    s->length = 4;
    s->dx = s->next_dx = 1;
    for (int i = 0; i < 4; i++)
        s->cells[i] = 8 * 18 + 8 - i;
    s->paused = 1;
    food(s);
}
static void snake_direction(Snake *s, int dx, int dy) {
    if (abs(dx) + abs(dy) != 1 || (dx == -s->dx && dy == -s->dy))
        return;
    s->next_dx = dx;
    s->next_dy = dy;
}
static void snake_tick(Snake *s) {
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

static unsigned best_score;
static int64_t next_tick;
static const int tick_ms=550;
static int input(const Input *in) {
    if(in->kind==INPUT_PAUSE || in->kind==INPUT_RESUME){snake_game.paused=1;return 1;}
    if(in->kind==INPUT_TICK) {
        if(!snake_game.paused && !snake_game.over)snake_tick(&snake_game);
        next_tick=ui_clock_millis()+tick_ms;
        if((unsigned)snake_game.score>best_score){
            best_score=snake_game.score;
            app_state_save("score.bin",&best_score,sizeof(best_score));
        }
        return 1;
    }
    if(in->kind==INPUT_SWIPE && (in->dx || in->dy)) {
        if(abs(in->dx)>abs(in->dy))snake_direction(&snake_game,in->dx>0 ? 1 : -1,0);
        else snake_direction(&snake_game,0,in->dy>0 ? 1 : -1);
        return 1;
    }
    if(in->kind!=INPUT_TAP)return 0;
    int x=in->x,y=in->y;
    if(y>=74 && y<118) {
        if(x>=528)snake_new(&snake_game);
        else if(x>=468)help("Snake",local("Sammle Äpfel und vermeide Wände und deinen eigenen Körper.",
            "Collect apples and avoid the walls and your own body.",
            "Ramassez les pommes. Évitez les murs et le corps du serpent."));
    } else if(x>=414 && y>=124 && y<164) {
        if(snake_game.over)snake_new(&snake_game);
        snake_game.paused=!snake_game.paused;
    } else if(y>=677 && y<725 && x>=258 && x<342)snake_direction(&snake_game,0,-1);
    else if(y>=735 && y<783) {
        if(x>=153 && x<237)snake_direction(&snake_game,-1,0);
        else if(x>=258 && x<342)snake_direction(&snake_game,0,1);
        else if(x>=363 && x<447)snake_direction(&snake_game,1,0);
    }
    return 1;
}
/* Adapters to shared SDK drawing primitives. */
#define RADIUS_MEDIUM 10
#define UI_BODY PAINT_BODY
#define UI_PRIMARY PAINT_HEADING
#define UI_SECONDARY PAINT_SECONDARY
#define ui_font(x) (x)
#define centered(x,y,w,h,s,r) ui_paint_text(x,y,w,h,s,r)
#define drawtext(x,y,s,r,w) lumen_text(x,y,w,34,s,r,-1,1)
#define region ui_paint_hitbox
#define stroke_color ui_paint_line
#define stroke(x,y,xx,yy,w) ui_paint_line(x,y,xx,yy,w,0)
#define aa_disc(x,y,r,c) ui_paint_ellipse(x,y,r,r,c)
#define control_surface(x,y,w,h,r,c) ui_paint_surface(x,y,w,h,r,c,c)
#define outline_surface(x,y,w,h,r,c) ui_paint_surface(x,y,w,h,r,255,c)
#define key_outline(x,y,w,h) outline_surface(x,y,w,h,8,115)
#define button_h lumen_button
#define icon ui_paint_icon_button
static void draw(void) {
    ui_paint_heading("Snake",2);
    lumen_button(468,74,44,44,"?",0);
    ui_paint_icon_button(528,74,IC_RESTART);
    char b[60];
    snprintf(b, sizeof(b), "%d  ·  %s %u", snake_game.score, (local("Rekord","Best","Record")), best_score);
    drawtext(30, 132, b, ui_font(UI_BODY), 360);
    button_h(414, 124, 158, 40,
             snake_game.over     ? local("Neu starten","Restart","Rejouer")
             : snake_game.paused ? local("Start","Start","Démarrer")
                                 : local("Pause","Pause","Pause"),
             0);
    int x0 = 30, y0 = 180, cell = 30;
    key_outline(x0 - 1, y0 - 1, 542, 482);
    if (snake_game.food >= 0) {
        int fx=x0+(snake_game.food%18)*cell+15, fy=y0+(snake_game.food/18)*cell+15;
        ui_paint_ellipse(fx-4,fy+1,7,9,16);
        ui_paint_ellipse(fx+4,fy+1,7,9,16);
        stroke(fx,fy-8,fx+1,fy-14,2);
        ui_paint_ellipse(fx+5,fy-12,5,2,104);
        stroke(fx-5,fy-3,fx-6,fy+2,2);
    }
    for (int pass=0;pass<2;pass++) {
        for (int i=snake_game.length-1;i>0;i--) {
            int at=snake_game.cells[i], next=snake_game.cells[i-1];
            int cx=x0+(at%18)*cell+15,cy=y0+(at/18)*cell+15;
            int nx=x0+(next%18)*cell+15,ny=y0+(next/18)*cell+15;
            stroke_color(cx,cy,nx,ny,pass ? 16 : 22,pass ? 112 : 24);
        }
    }
    if (snake_game.length) {
        int at=snake_game.cells[0],cx=x0+(at%18)*cell+15,cy=y0+(at/18)*cell+15;
        int dx=snake_game.dx,dy=snake_game.dy;
        aa_disc(cx,cy,11,24);
        aa_disc(cx,cy,8,112);
        for (int side=-1;side<=1;side+=2) {
            aa_disc(cx+dx*4-dy*side*5,cy+dy*4+dx*side*5,3,255);
            aa_disc(cx+dx*5-dy*side*5,cy+dy*5+dx*side*5,1,0);
        }
    }
    if (snake_game.paused || snake_game.over) {
        control_surface(166, 229, 268, 98, RADIUS_MEDIUM, 255);
        centered(166, 237, 268, 44, snake_game.over ? local("Spiel beendet","Game over","Partie terminée") : local("Bereit?","Ready?","Prêt ?"), ui_font(UI_PRIMARY));
        centered(166, 283, 268, 36, local("Wischen oder Pfeile","Swipe or use arrows","Balayez ou utilisez les flèches"), ui_font(UI_SECONDARY));
    }
    int x[] = {258, 153, 258, 363}, y[] = {677, 735, 735, 735},
        kind[] = {IC_UP, IC_PREV, IC_DOWN, IC_NEXT};
    for (int i = 0; i < 4; i++) {
        region(x[i], y[i], 84, 48);
        key_outline(x[i], y[i], 84, 48);
        icon(x[i] + 20, y[i] + 2, kind[i]);
    }
    if(!snake_game.paused && !snake_game.over) {
        int64_t now=ui_clock_millis();
        if(!next_tick)next_tick=now+tick_ms;
        lumen_schedule(next_tick>now ? (int)(next_tick-now) : 20);
    } else next_tick=0;
}

int main(void) {
    srand((unsigned)time(NULL));
    snake_new(&snake_game);
    if(!app_state_load("score.bin",&best_score,sizeof(best_score))) {
        unsigned previous[101];
        if(lumen_legacy_state("apps-v1.bin",previous,sizeof(previous)) &&
           previous[0]==0x4c415050 && previous[1]==1)best_score=previous[4];
    }
    lumen_run(draw,input,NULL);
    return 0;
}
