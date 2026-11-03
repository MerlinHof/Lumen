#define _GNU_SOURCE
#include "../src/system/ui/ui.c"
#include <assert.h>
static void tap(int x,int y){Input i={.kind=INPUT_TAP,.x=x,.y=y};ui_input(&i);ui_draw();}
static int perft(ChessPosition p,int depth){if(!depth)return 1;int n=0;for(int a=0;a<64;a++)for(int b=0;b<64;b++)if(chess_legal(&p,a,b)){ChessPosition q=p;chess_apply(&q,a,b,5);n+=perft(q,depth-1);}return n;}
int main(int argc,char **argv){
    assert(argc==3);
    engine_init();data_root=argv[1];library_root=argv[2];mkdir(data_root,0700);mkdir(library_root,0700);ui_init();library_defaults();
    assert(options[OPT_AUTO_TIME]&&options[OPT_APPS]&&!options[OPT_TAB_LABELS]);
    chess_reset();assert(perft(chess,1)==20&&perft(chess,2)==400);
    assert(chess_legal(&chess,52,36));chess_apply(&chess,52,36,5);assert(chess.ep==44&&chess.turn==-1);
    memset(&chess,0,sizeof(chess));chess.b[60]=6;chess.b[63]=4;chess.b[4]=-6;chess.turn=1;chess.rights=1;chess.ep=-1;
    assert(chess_legal(&chess,60,62));chess.b[5]=-4;assert(!chess_legal(&chess,60,62));
    chess_reset();screen=48;assert(!has_tabs());screen=49;tic_result=0;tic_turn=1;memset(tic,0,sizeof(tic));
    tap(100,300);tap(100,450);tap(300,300);tap(300,450);tap(450,300);assert(tic_result==1);
    screen=30;note_selected=-1;open_keyboard(30,"Notiz","hello world");note_cursor=5;note_keyboard_insert(" little");assert(!strcmp(edit,"hello little world"));note_keyboard_delete();assert(!strcmp(edit,"hello littl world"));commit_keyboard();assert(strstr(note_text,"littl"));
    screen=27;strcpy(calc_expr,"sin(pi/3)^2+sqrt(5^2+3^2)");calc_cursor=strlen(calc_expr);calc_degrees=0;calc_run();assert(!*calc_error);ui_draw();assert(!has_tabs());
    screen=28;timer_panel=0;timer_minutes=1;tap(300,590);assert(timer_due);timer_due=extra_millis()-1;extra_tick();assert(alarm_ring==1);ui_draw();tap(200,645);assert(!alarm_ring&&screen==28);
    screen=31;drawing_count=1;drawing_slot=0;drawing_load();tap(220,80);assert(drawing_count==2&&drawing_slot==1);drawing_save();assert(!drawing_dirty);
    options[OPT_LANGUAGE]=3;screen=13;ui_draw();assert(!strcmp(tr("Einstellungen"),"设置"));options[OPT_LANGUAGE]=2;assert(!strcmp(tr("Einstellungen"),"Réglages"));
    puts("PASS: chess legal moves/castling, two-player tic-tac-toe, inline note editing/save, formula layout, timer interruption/resume, extensible drawing pages, language glyphs and defaults.");engine_shutdown();return 0;
}
