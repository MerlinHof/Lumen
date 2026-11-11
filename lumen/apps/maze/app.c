#include "lumen_app.h"

enum { COLS=33, ROWS=35, LEFT=36, TOP=140, TILE=16 };
typedef struct {
    unsigned char wall[COLS*ROWS], trail[COLS*ROWS];
    int x,y,moves;
} Maze;
static Maze maze;
static int dirty;

static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void new_maze(void) {
    memset(maze.wall,1,sizeof(maze.wall));
    memset(maze.trail,0,sizeof(maze.trail));
    int stack[16*17],depth=1;
    stack[0]=COLS+1;
    maze.wall[COLS+1]=0;
    maze.trail[COLS+1]=1;
    const int dx[]={2,-2,0,0},dy[]={0,0,2,-2};
    while(depth) {
        int at=stack[depth-1],x=at%COLS,y=at/COLS,choices[4],count=0;
        for(int d=0;d<4;d++) {
            int xx=x+dx[d],yy=y+dy[d];
            if(xx>0 && xx<COLS-1 && yy>0 && yy<ROWS-1 && maze.wall[yy*COLS+xx])
                choices[count++]=yy*COLS+xx;
        }
        if(!count){depth--;continue;}
        int next=choices[rand()%count];
        maze.wall[(at+next)/2]=maze.wall[next]=0;
        stack[depth++]=next;
    }
    maze.x=maze.y=1;
    maze.moves=0;dirty=1;
}
static int won(void) {return maze.x==COLS-2 && maze.y==ROWS-2;}
static int valid_maze(void) {
    return maze.x>0 && maze.x<COLS-1 && maze.y>0 && maze.y<ROWS-1 &&
        !maze.wall[maze.y*COLS+maze.x] && !maze.wall[COLS*(ROWS-2)+COLS-2];
}
static void save(void) {
    if(dirty && app_state_save("maze.bin",&maze,sizeof(maze)))dirty=0;
}
static void load(void) {
    if(app_state_load("maze.bin",&maze,sizeof(maze)) && valid_maze())return;
    unsigned char old[8+COLS*ROWS+12],trail[8+COLS*ROWS];
    uint32_t header[2];int location[3];
    if(lumen_legacy_state("maze17.bin",old,sizeof(old))) {
        memcpy(header,old,8);
        if(header[0]==0x4c554d16 && header[1]==sizeof(old)-8) {
            memcpy(maze.wall,old+8,sizeof(maze.wall));
            memcpy(location,old+8+sizeof(maze.wall),sizeof(location));
            maze.x=location[0];maze.y=location[1];maze.moves=location[2];
            if(valid_maze()) {
                if(lumen_legacy_state("maze-trace.bin",trail,sizeof(trail))) {
                    memcpy(header,trail,8);
                    if(header[0]==0x4c554d16 && header[1]==sizeof(maze.trail))
                        memcpy(maze.trail,trail+8,sizeof(maze.trail));
                }
                dirty=1;save();return;
            }
        }
    }
    new_maze();
}
static void draw(void) {
    ui_paint_heading(word("Labyrinth","Maze","Labyrinthe"),2);
    lumen_button(468,74,44,44,"?",0);
    ui_paint_icon_button(528,74,IC_RESTART);
    for(int y=0;y<ROWS;y++) {
        for(int x=0;x<COLS;) {
            if(maze.wall[y*COLS+x]) {
                int begin=x;
                while(x<COLS && maze.wall[y*COLS+x])x++;
                ui_paint_surface(LEFT+begin*TILE,TOP+y*TILE,(x-begin)*TILE,TILE,0,38,38);
            } else {
                if(maze.trail[y*COLS+x])ui_paint_ellipse(LEFT+x*TILE+8,TOP+y*TILE+8,2,2,145);
                x++;
            }
        }
    }
    ui_paint_ring(LEFT+(COLS-2)*TILE+8,TOP+(ROWS-2)*TILE+8,6,2);
    ui_paint_ellipse(LEFT+maze.x*TILE+8,TOP+maze.y*TILE+8,6,6,0);
    const int icons[]={IC_PREV,IC_UP,IC_DOWN,IC_NEXT};
    for(int i=0;i<4;i++) {
        lumen_button(150+i*79,728,60,44,"",0);
        ui_paint_icon_button(158+i*79,728,icons[i]);
    }
}
static int input(const Input *in) {
    int x=in->x,y=in->y,dx=0,dy=0,steps=1;
    if(in->kind==INPUT_SWIPE && (in->dx || in->dy)) {
        if(abs(in->dx)>abs(in->dy))dx=in->dx>0 ? 1 : -1;
        else dy=in->dy>0 ? 1 : -1;
        steps=4;
    } else if(in->kind!=INPUT_TAP)return 0;
    else if(y>=74 && y<118 && x>=468 && x<512) {
        lumen_app_open_with("lumen.reader","open_document","",word("Labyrinth","Maze","Labyrinthe"),
            word("Finde den Ausgang unten rechts. Wische, tippe auf einen Gang oder nutze die Pfeile. Graue Punkte markieren deinen Weg.",
                 "Find the exit in the lower right. Swipe, tap a corridor or use the arrows. Gray dots mark your trail.",
                 "Trouvez la sortie en bas à droite. Balayez, touchez un couloir ou utilisez les flèches. Les points gris marquent votre trajet."));
        return 0;
    } else if(y>=74 && y<118 && x>=528 && x<572) {
        new_maze();return 1;
    } else if(y>=728 && y<772 && x>=150 && x<447) {
        int button=(x-150)/79;
        if((x-150)%79>=60)return 0;
        dx=button==0 ? -1 : button==3 ? 1 : 0;
        dy=button==1 ? -1 : button==2 ? 1 : 0;
    } else if(x>=LEFT && x<LEFT+COLS*TILE && y>=TOP && y<TOP+ROWS*TILE) {
        int delta_x=(x-LEFT)/TILE-maze.x,delta_y=(y-TOP)/TILE-maze.y;
        if(abs(delta_x)>abs(delta_y)){dx=delta_x>0 ? 1 : -1;steps=abs(delta_x);}
        else if(delta_y){dy=delta_y>0 ? 1 : -1;steps=abs(delta_y);}
    }
    if(!dx && !dy)return 0;
    int before=maze.moves;
    while(steps-- && !won()) {
        int xx=maze.x+dx,yy=maze.y+dy;
        if(xx<0 || yy<0 || xx>=COLS || yy>=ROWS || maze.wall[yy*COLS+xx])break;
        maze.x=xx;maze.y=yy;maze.trail[yy*COLS+xx]=1;maze.moves++;dirty=1;
    }
    if(won() && maze.moves!=before)
        lumen_request("notify","",word("Ausgang gefunden!","Exit found!","Sortie trouvée !"),0);
    return maze.moves!=before;
}
int main(void) {
    srand((unsigned)time(NULL));
    load();
    lumen_run(draw,input,save);
    return 0;
}
