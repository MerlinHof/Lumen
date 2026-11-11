#include "lumen_app.h"

static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static unsigned warehouse_random(void) {return (unsigned)rand();}
#include "model.h"

static void save(void) {
    Warehouse17 states[]={warehouse17,initial_warehouse17};
    app_state_save("warehouse.bin",states,sizeof(states));
}
static int valid(const Warehouse17 *state) {
    if(state->w<5 || state->w>19 || state->h<5 || state->h>19 ||
       state->x<1 || state->x>=state->w-1 || state->y<1 || state->y>=state->h-1 ||
       state->level<1 || state->level>1000000)return 0;
    int goals=0,boxes=0;
    for(int i=0;i<state->w*state->h;i++) {
        int tile=state->board[i];
        if(tile>5 || tile==4)return 0;
        goals+=tile==2 || tile==5;boxes+=box17(tile);
    }
    return goals==boxes && boxes>0 && floor17(state->board[state->y*state->w+state->x]);
}
static void load(void) {
    Warehouse17 states[2];
    int loaded=app_state_load("warehouse.bin",states,sizeof(states));
    if(!loaded) {
        unsigned char legacy[8+sizeof(states)];
        uint32_t header[2];
        if(lumen_legacy_state("sokoban17.bin",legacy,sizeof(legacy))) {
            memcpy(header,legacy,8);
            if(header[0]==0x4c554d16 && header[1]==sizeof(states)) {
                memcpy(states,legacy+8,sizeof(states));loaded=1;
            }
        }
    }
    if(loaded && valid(states) && valid(states+1) && states[0].level==states[1].level) {
        warehouse17=states[0];initial_warehouse17=states[1];
    } else warehouse_new17(1);
}
static int cell_size(void) {
    int w=544/warehouse17.w,h=528/warehouse17.h;
    int cell=w<h ? w : h;
    return cell>40 ? 40 : cell;
}
static void draw(void) {
    ui_paint_heading("Sokoban",3);
    lumen_button(408,74,44,44,"?",0);
    ui_paint_icon_button(468,74,IC_UNDO);
    ui_paint_icon_button(528,74,IC_RESTART);
    char label[100];
    snprintf(label,sizeof(label),word("Level %d · %d Züge","Level %d · %d moves","Niveau %d · %d coups"),
             warehouse17.level,warehouse17.moves);
    ui_paint_text(28,124,544,40,label,PAINT_SECONDARY);
    int cell=cell_size(),left=300-warehouse17.w*cell/2;
    ui_paint_hitbox(left,170,warehouse17.w*cell,warehouse17.h*cell);
    for(int y=0;y<warehouse17.h;y++)
        for(int x=0;x<warehouse17.w;x++) {
            int tile=warehouse17.board[y*warehouse17.w+x];
            if(tile)lumen_sprite("tiles.pgm",left+x*cell,170+y*cell,cell,tile,0);
        }
    lumen_sprite("tiles.pgm",left+warehouse17.x*cell,170+warehouse17.y*cell,
                 cell,warehouse_facing17,1);
    const int icons[]={IC_PREV,IC_UP,IC_DOWN,IC_NEXT};
    for(int i=0;i<4;i++) {
        lumen_button(150+i*79,728,60,44,"",0);
        ui_paint_icon_button(158+i*79,728,icons[i]);
    }
}
static int input(const Input *in) {
    int x=in->x,y=in->y,dx=0,dy=0;
    if(in->kind==INPUT_SWIPE && (in->dx || in->dy)) {
        if(abs(in->dx)>abs(in->dy))dx=in->dx>0 ? 1 : -1;
        else dy=in->dy>0 ? 1 : -1;
    } else if(in->kind!=INPUT_TAP)return 0;
    else if(y>=74 && y<118) {
        if(x>=408 && x<452)
            lumen_app_open_with("lumen.reader","open_document","","Sokoban",
                word("Schiebe alle Kisten auf die Zielkreise. Du kannst Kisten weder ziehen noch zwei gleichzeitig schieben. Plane voraus und verwende Rückgängig, falls eine Kiste feststeckt.",
                "Push every crate onto a target. You cannot pull crates or push two at once. Plan ahead and use Undo if a crate gets stuck.",
                "Poussez chaque caisse sur une cible. Impossible de tirer une caisse ou d’en pousser deux à la fois. Utilisez Annuler si une caisse est coincée."));
        else if(x>=468 && x<512 && warehouse_undo17) {
            warehouse17=undo_warehouse17;warehouse_undo17=0;save();
        } else if(x>=528 && x<572) {
            warehouse17=initial_warehouse17;warehouse_undo17=0;save();
        }
        return 1;
    } else if(y>=728 && y<772 && x>=150 && x<447 && (x-150)%79<60) {
        int key=(x-150)/79;
        dx=key==0 ? -1 : key==3 ? 1 : 0;
        dy=key==1 ? -1 : key==2 ? 1 : 0;
    } else {
        int cell=cell_size(),left=300-warehouse17.w*cell/2;
        if(x<left || x>=left+warehouse17.w*cell || y<170 || y>=170+warehouse17.h*cell)return 0;
        int rx=(x-left)/cell-warehouse17.x,ry=(y-170)/cell-warehouse17.y;
        if(abs(rx)>abs(ry))dx=rx>0 ? 1 : -1;
        else if(ry)dy=ry>0 ? 1 : -1;
    }
    if(dx || dy) {
        int moves=warehouse17.moves,level=warehouse17.level;
        warehouse_step17(dx,dy);
        if(moves!=warehouse17.moves || level!=warehouse17.level)save();
    }
    return 1;
}
int main(void) {
    srand((unsigned)time(NULL));
    load();save();
    lumen_run(draw,input,NULL);
    return 0;
}
