#include "../games/package_ui.h"
#include "api.h"


static SignalGame signal_game;
static int signal_ready, signal_dirty;

static void signal_save(void) {
    if (signal_dirty && app_state_save("slide3d-v1.bin", &signal_game, sizeof(signal_game)))
        signal_dirty = 0;
}
static void signal_open(void) {
    if (signal_ready) return;
    signal_ready = 1;
    int loaded=app_state_load("slide3d-v1.bin", &signal_game, sizeof(signal_game));
    if (!loaded) {
        struct { uint32_t magic,size; SignalGame game; } legacy;
        if (lumen_legacy_state("slide3d-v1.bin",&legacy,sizeof(legacy)) &&
            legacy.magic==0x4c554d16 && legacy.size==sizeof(legacy.game)) {
            signal_game=legacy.game;
            loaded=1;
        }
    }
    if (loaded) {
        unsigned seen = 0;
        for (int i = 0; i < 16; i++)
            if (signal_game.tiles[i] >= 0 && signal_game.tiles[i] < 16)
                seen |= 1u << signal_game.tiles[i];
        if (seen == 65535 && signal_game.empty >= 0 && signal_game.empty < 16 &&
            !signal_game.tiles[signal_game.empty] && signal_game.steps >= 0)
            return;
    }
    signal_new(&signal_game, (unsigned)time(NULL), 20);
}
static void signal_draw(void) {
    signal_open();
    ui_paint_heading(game_text("Schiebepuzzle","Sliding Puzzle","Taquin"),2);
    lumen_icon_action("help",468,74,IC_BOOK);
    lumen_icon_action("restart",528,74,IC_RESTART);
    for (int i=0;i<16;i++) {
        int x=28+(i%4)*138, y=151+(i/4)*138;
        int value=signal_game.tiles[i];
        if (!value) continue;
        ui_paint_hitbox(x,y,130,130);
        ui_paint_surface(x,y,130,130,18,255,2);
        char label[8];
        snprintf(label,sizeof(label),"%d",value);
        ui_paint_text(x,y,130,130,label,PAINT_DISPLAY);
    }
    char steps[32];
    snprintf(steps, sizeof(steps), "%d", signal_game.steps);
    ui_paint_text(28, 716, 544, 48, steps, PAINT_DISPLAY);
    ui_paint_text(28, 758, 544, 24, "Züge", PAINT_SECONDARY);
    ui_paint_text(28, 121, 544, 24, signal_won(&signal_game) ? "Gewonnen!" :
                  "Ordne die Zahlen von 1 bis 15.", PAINT_BODY);
}
static int signal_input(const Input *in) {
    signal_open();
    if(in->kind==INPUT_COMPONENT) {
        char action[24];
        if(!game_action(in,action,sizeof(action)))return 0;
        if(!strcmp(action,"help")) {
            game_help(game_text("Schiebepuzzle","Sliding Puzzle","Taquin"),
                "Tippe auf einen Stein neben der Lücke, um ihn zu verschieben. Ordne die Zahlen von 1 bis 15 und lass die Lücke unten rechts.",
                "Tap a tile next to the gap to slide it. Put the numbers in order from 1 to 15, with the gap at the lower right.",
                "Touchez une tuile voisine de la case vide pour la déplacer. Rangez les nombres de 1 à 15, avec la case vide en bas à droite.");
            return 0;
        }
        if(strcmp(action,"restart"))return 0;
        signal_new(&signal_game,(unsigned)ui_clock_millis(),40);
    } else if(in->kind==INPUT_TAP) {
        int col=(in->x-28)/138, row=(in->y-151)/138;
        if (in->x<28 || in->y<151 || col>=4 || row>=4) return 1;
        int closest=row*4+col;
        if (signal_won(&signal_game) || !signal_slide(&signal_game, closest)) return 1;
    } else return 0;
    signal_dirty = 1;
    return 1;
}

int main(void){lumen_run(signal_draw,signal_input,signal_save);return 0;}
