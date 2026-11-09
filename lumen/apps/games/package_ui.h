#ifndef LUMEN_PACKAGED_GAME_UI_H
#define LUMEN_PACKAGED_GAME_UI_H
#include "lumen_app.h"

static const char *game_text(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void game_help(const char *title,const char *de,const char *en,const char *fr) {
    lumen_request("reader",title,game_text(de,en,fr),0);
}
static unsigned game_random(unsigned *seed) {
    *seed=*seed*1664525u+1013904223u;
    return *seed>>8;
}
#endif
