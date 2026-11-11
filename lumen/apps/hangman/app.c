#include "lumen_app.h"

static const char *local(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void help(const char *name,const char *text) {
    lumen_app_open_with("lumen.reader","open_document","",name,text);
}

typedef struct {
    char word[24];
    uint32_t guesses;
    int misses, over, won;
} Hangman;
static Hangman word_game;
static void hangman_guess(Hangman *h, int letter);
static void hangman_guess(Hangman *h, int letter) {
    if (letter < 0 || letter >= 26 || h->over || (h->guesses & (1u << letter)))
        return;
    h->guesses |= 1u << letter;
    int found = 0, missing = 0;
    for (const char *p = h->word; *p; p++) {
        if (*p == 'A' + letter)
            found = 1;
        if (!(h->guesses & (1u << (*p - 'A'))))
            missing++;
    }
    if (!found)
        h->misses++;
    if (!missing)
        h->won = h->over = 1;
    else if (h->misses >= 6)
        h->over = 1;
}

static void restart(void) {
    memset(&word_game,0,sizeof(word_game));
    const char *fallback[]={ "WALD", "PAPER", "LIVRE" };
    snprintf(word_game.word,sizeof(word_game.word),"%s",fallback[lumen_language>=0 && lumen_language<3 ? lumen_language : 1]);
    char seed[32];snprintf(seed,sizeof(seed),"%u",(unsigned)rand());
    lumen_service("dictionary","game_word",seed,1);
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN)restart();
    if(in->kind==INPUT_SERVICE && in->x==1 && in->y && !word_game.guesses) {
        char word[24];lumen_string("word",word,sizeof(word));
        if(*word && strspn(word,"ABCDEFGHIJKLMNOPQRSTUVWXYZ")==strlen(word))
            snprintf(word_game.word,sizeof(word_game.word),"%s",word);
    }
    if(in->kind!=INPUT_TAP)return 0;
    if(in->y>=74 && in->y<118) {
        if(in->x>=528)restart();
        else if(in->x>=468)help(local("Wortraten","Hangman","Pendu"),
            local("Errate das Wort Buchstabe für Buchstabe. Sechs falsche Buchstaben beenden das Spiel.",
            "Guess the hidden word one letter at a time. Six wrong guesses end the round.",
            "Devinez le mot lettre par lettre. Six erreurs terminent la partie."));
    } else if(in->y>=455 && in->y<629) {
        int row=(in->y-455)/58,cols=row==2 ? 6 : 10,start=(600-cols*54)/2;
        int dx=in->x-start;
        if(dx>=0 && dx/54<cols && dx%54<48 && (in->y-455)%58<48)
            hangman_guess(&word_game,row*10+dx/54);
    }
    return 1;
}
/* Adapters to shared SDK drawing primitives. */
#define RADIUS_MEDIUM 10
#define UI_BODY PAINT_BODY
#define UI_PRIMARY PAINT_HEADING
#define ui_font(x) (x)
#define centered(x,y,w,h,s,r) ui_paint_text(x,y,w,h,s,r)
#define rect(x,y,w,h,c) ui_paint_surface(x,y,w,h,0,c,c)
#define stroke(x,y,xx,yy,w) ui_paint_line(x,y,xx,yy,w,0)
#define control_surface(x,y,w,h,r,c) ui_paint_surface(x,y,w,h,r,c,c)
#define outline_surface(x,y,w,h,r,c) ui_paint_surface(x,y,w,h,r,255,c)
#define button_h lumen_button
#define ring ui_paint_ring
static void draw(void) {
    ui_paint_heading(local("Wortraten","Hangman","Pendu"),2);
    lumen_button(468,74,44,44,"?",0);
    ui_paint_icon_button(528,74,IC_RESTART);
    stroke(118, 320, 300, 320, 2);
    stroke(154, 320, 154, 151, 2);
    stroke(154, 151, 260, 151, 2);
    stroke(260, 151, 260, 175, 2);
    int n = word_game.misses;
    if (n > 0)
        ring(260, 193, 17, 2);
    if (n > 1)
        stroke(260, 210, 260, 264, 2);
    if (n > 2)
        stroke(260, 224, 230, 245, 2);
    if (n > 3)
        stroke(260, 224, 290, 245, 2);
    if (n > 4)
        stroke(260, 264, 235, 300, 2);
    if (n > 5)
        stroke(260, 264, 285, 300, 2);
    int len = (int)strlen(word_game.word), w = 34, x = (600 - w * len) / 2;
    for (int i = 0; i < len; i++) {
        char ch[2] = {word_game.word[i], 0};
        if (word_game.over || (word_game.guesses & (1u << (ch[0] - 'A'))))
            centered(x + i * w, 365, w, 44, ch, ui_font(UI_PRIMARY));
        rect(x + i * w + 4, 414, w - 8, 2, 0);
    }
    for (int i = 0; i < 26; i++) {
        int row = i / 10, col = i % 10, cols = row == 2 ? 6 : 10,
            xx = (600 - cols * 54) / 2 + col * 54, y = 455 + row * 58;
        char key[2] = {(char)('A' + i), 0};
        if (word_game.guesses & (1u << i)) {
            control_surface(xx, y, 48, 48, RADIUS_MEDIUM, 220);
            centered(xx, y, 48, 48, key, ui_font(UI_BODY));
        } else
            button_h(xx, y, 48, 48, key, 0);
    }
    if (word_game.over && !word_game.won) {
        char solution[80];
        snprintf(solution, sizeof(solution), "%s: %s", (local("Das Wort war","The word was","Le mot était")), word_game.word);
        centered(28, 626, 544, 40, solution, ui_font(UI_PRIMARY));
    }
    centered(28, 690, 544, 48,
             word_game.over
                 ? (word_game.won ? local("Gewonnen!","You won!","Gagné !") : local("Neues Spiel über das Neustart-Symbol.","Tap restart for a new game.","Touchez Redémarrer pour rejouer."))
             : local("Ein Wort, sechs Versuche","One word, six misses","Un mot, six erreurs"),
             ui_font(UI_BODY));
}

int main(void) {
    srand((unsigned)time(NULL));

    lumen_run(draw,input,NULL);
    return 0;
}
