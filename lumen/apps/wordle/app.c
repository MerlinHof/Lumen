#include "../games/package_ui.h"
#include "../../vendor/wordle_words.h"
#include <ctype.h>
static struct {
    int date, row, over;
    char guesses[6][6];
    unsigned char result[6][5];
} wordle_game;
static char word_secret[6];
static const char *wordle_answers[] = {
    "PAPER", "LIGHT", "STONE", "EARTH", "WATER", "DREAM", "BRAIN", "BOOKS", "PLANT", "STARS",
    "NIGHT", "CLOUD", "OCEAN", "GRACE", "STUDY", "LEARN", "MUSIC", "SPACE", "GREEN", "HOUSE",
    "TRAIN", "FRAME", "CHESS", "CLOCK", "SOUND", "HEART", "CLEAR", "FRESH", "SWEET", "QUIET",
    "WORLD", "NORTH", "SOUTH", "RIVER", "FLAME", "SHORE", "BREAD", "PEACH", "APPLE", "TABLE",
    "CHAIR", "STORY", "COVER", "LAYER", "FIELD", "ORBIT", "SOLAR", "LUNAR", "SCALE", "LOGIC",
    "QUEST", "PRIDE", "BRAVE", "SMILE", "WHALE", "EAGLE", "HORSE", "TIGER", "ZEBRA", "BLOOM",
    "GLASS", "METAL", "AMBER", "CORAL"};
static int word_valid(const char *w) {
    int l = 0, r = sizeof(wordle_words) / 6;
    while (l < r) {
        int m = (l + r) / 2, c = strcmp(w, wordle_words[m]);
        if (!c)
            return 1;
        if (c > 0)
            l = m + 1;
        else
            r = m;
    }
    return 0;
}
static void wordle_score(const char *s, const char *g, unsigned char r[5]) {
    int used[5] = {0};
    memset(r, 0, 5);
    for (int i = 0; i < 5; i++)
        if (s[i] == g[i]) {
            r[i] = 2;
            used[i] = 1;
        }
    for (int i = 0; i < 5; i++)
        if (!r[i])
            for (int j = 0; j < 5; j++)
                if (!used[j] && s[j] == g[i]) {
                    r[i] = 1;
                    used[j] = 1;
                    break;
                }
}

static int dirty,offset;
static void daily(void) {
    time_t now=time(NULL);
    struct tm t;localtime_r(&now,&t);
    int y=t.tm_year+1900,m=t.tm_mon+1,d=t.tm_mday;
    int date=10000*y+100*m+d;
    if(wordle_game.date!=date) {
        memset(&wordle_game,0,sizeof(wordle_game));wordle_game.date=date;dirty=1;
    }
    y-=m<=2;
    int era=(y>=0 ? y : y-399)/400;
    unsigned yoe=y-era*400,doy=(153*(m+(m>2 ? -3 : 9))+2)/5+d-1,
             doe=yoe*365+yoe/4-yoe/100+doy;
    int total=sizeof(wordle_answers)/sizeof(*wordle_answers);
    int choice=(era*146097+(int)doe+offset)%total;
    snprintf(word_secret,sizeof(word_secret),"%s",wordle_answers[(choice+total)%total]);
}
static void draw(void) {
    daily();
    ui_paint_heading("Wordle",1);ui_paint_icon_button(528,74,IC_BOOK);
    char b[96];
    snprintf(b,sizeof(b),"%02d.%02d.%04d · %s",wordle_game.date%100,
             wordle_game.date/100%100,wordle_game.date/10000,
             game_text("Englisch · 6 Versuche","English · 6 guesses","Anglais · 6 essais"));
    ui_paint_text(28,130,544,35,b,PAINT_SECONDARY);
    for(int r=0;r<6;r++)for(int c=0;c<5;c++) {
        int x=90+c*86,y=187+r*72,v=wordle_game.result[r][c];
        char key[2]={wordle_game.guesses[r][c],0};
        ui_paint_surface(x,y,76,62,7,v==2 ? 0 : v==1 ? 175 : 255,1);
        lumen_text(x,y,76,62,key,PAINT_DISPLAY,v==2 ? 255 : 0,0);
    }
    if(wordle_game.over) {
        snprintf(b,sizeof(b),"%s · %s",wordle_game.over==1 ?
            game_text("Gewonnen!","Solved!","Gagné !") : game_text("Lösung","Answer","Solution"),word_secret);
        ui_paint_text(28,652,544,42,b,PAINT_HEADING);
        ui_paint_text(28,712,544,35,game_text("Morgen ein neues Wort","A new word tomorrow","Un nouveau mot demain"),PAINT_SECONDARY);
    } else lumen_button(90,663,420,60,game_text("Wort eingeben","Enter a word","Saisir un mot"),0);
}
static int input(const Input *in) {
    daily();
    if(in->kind==INPUT_TAP) {
        if(in->x>=528 && in->y>=74 && in->y<118)game_help("Wordle",
            "Errate das englische Wort in sechs Versuchen. Schwarz markiert die richtige Position, Grau einen Buchstaben an einer anderen Stelle.",
            "Guess the English word in six tries. Black means the right letter in the right place. Gray means the right letter elsewhere.",
            "Devinez le mot anglais en six essais. Noir indique la bonne place. Gris indique une lettre présente ailleurs.");
        else if(!wordle_game.over && in->x>=90 && in->x<510 && in->y>=663 && in->y<723)
            lumen_request("input","Wordle · English","",1);
        return 1;
    }
    if(in->kind!=INPUT_TEXT || in->x!=1 || wordle_game.over || wordle_game.row>=6)return 1;
    char guess[16];lumen_string("text",guess,sizeof(guess));
    int valid=strlen(guess)==5;
    for(int i=0;valid && i<5;i++) {
        unsigned char c=guess[i];
        if(c>127 || !isalpha(c))valid=0;
        else guess[i]=toupper(c);
    }
    if(!valid || !word_valid(guess)) {
        lumen_request("notify","Wordle",valid ?
            game_text("Wort nicht im Wörterbuch.","Word not in the dictionary.","Mot absent du dictionnaire.") :
            game_text("Fünf englische Buchstaben eingeben.","Enter five English letters.","Saisissez cinq lettres anglaises."),0);
        return 1;
    }
    int r=wordle_game.row++;
    memcpy(wordle_game.guesses[r],guess,6);
    wordle_score(word_secret,guess,wordle_game.result[r]);
    if(!strcmp(guess,word_secret))wordle_game.over=1;
    else if(wordle_game.row==6)wordle_game.over=2;
    dirty=1;return 1;
}
static void save(void){if(dirty && app_state_save("game.bin",&wordle_game,sizeof(wordle_game)))dirty=0;}
int main(void) {
    if(!app_state_load("game.bin",&wordle_game,sizeof(wordle_game))) {
        lumen_legacy_state("wordle-1.bin",&wordle_game,sizeof(wordle_game));
        dirty=1;
    }
    if(wordle_game.row<0 || wordle_game.row>6 || wordle_game.over<0 || wordle_game.over>2)
        memset(&wordle_game,0,sizeof(wordle_game));
    const char *root=getenv("LUMEN_APP_DATA");
    if(root) {
        char path[2048];snprintf(path,sizeof(path),"%s/../../wordle-calendar-offset-v11.txt",root);
        FILE *file=fopen(path,"r");
        if(file){if(fscanf(file,"%d",&offset)!=1)offset=0;fclose(file);}
    }
    if(offset<0 || offset>=(int)(sizeof(wordle_answers)/sizeof(*wordle_answers)))offset=0;
    lumen_run(draw,input,save);return 0;
}
