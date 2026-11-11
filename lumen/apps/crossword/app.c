#include "lumen_app.h"
#include <ctype.h>
#include <strings.h>

static char crossword_answer16[5][16],definitions[5][512];
static unsigned game_seed16;
static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static unsigned random16(void) {
    if(!game_seed16)game_seed16=(unsigned)time(NULL)^0xa6b47d13;
    game_seed16^=game_seed16<<13;
    game_seed16^=game_seed16>>17;
    game_seed16^=game_seed16<<5;
    return game_seed16;
}
static void notify(const char *text){lumen_request("notify","",text,0);}
#define rect(x,y,w,h,c) ui_paint_surface(x,y,w,h,0,c,c)
#define icon_button ui_paint_icon_button
#define key_outline(x,y,w,h) ui_paint_surface(x,y,w,h,3,255,115)
#define region ui_paint_hitbox
#define centered ui_paint_text
#define button_h lumen_button
static int crossword_pick16;
static const char *cross_bank16[][2] = {
    {"PLANETS", "Welten um einen Stern (EN)"},
    {"READING", "Lesen (EN)"},
    {"SCIENCE", "Wissenschaft (EN)"},
    {"HISTORY", "Geschichte (EN)"},
    {"PHYSICS", "Physik (EN)"},
    {"WEATHER", "Wetter (EN)"},
    {"NATURAL", "Naturgegeben (EN)"},
    {"MEMOIRS", "Lebenserinnerungen (EN)"},
    {"SPACE", "Weltraum (EN)"},
    {"EARTH", "Unser Heimatplanet (EN)"},
    {"TREE", "Große Pflanze mit Stamm (EN)"},
    {"STONE", "Stück Gestein (EN)"},
    {"MOON", "Natürlicher Erdbegleiter (EN)"},
    {"SUN", "Stern unseres Sonnensystems (EN)"},
    {"WATER", "H2O (EN)"},
    {"FIRE", "Flammen (EN)"},
    {"BOOK", "Gebundene Seiten zum Lesen (EN)"},
    {"PAGE", "Seite eines Buchs (EN)"},
    {"INK", "Flüssiger Schreibstoff (EN)"},
    {"LIGHT", "Helligkeit (EN)"},
    {"DARK", "Ohne Licht (EN)"},
    {"GREEN", "Farbe des Grases (EN)"},
    {"BLUE", "Farbe eines klaren Himmels (EN)"},
    {"RED", "Farbe einer reifen Tomate (EN)"},
    {"WHITE", "Farbe von Schnee (EN)"},
    {"BLACK", "Dunkelste Farbe (EN)"},
    {"BRAIN", "Denkorgan (EN)"},
    {"HEART", "Blutpumpe im Körper (EN)"},
    {"NERVE", "Leitung im Nervensystem (EN)"},
    {"ATOM", "Baustein eines Elements (EN)"},
    {"ENERGY", "Fähigkeit, Arbeit zu verrichten (EN)"},
    {"FORCE", "Masse mal Beschleunigung (EN)"},
    {"MASS", "Physikalische Größe in Kilogramm (EN)"},
    {"TIME", "Größe in Sekunden (EN)"},
    {"WAVE", "Schwingung, die sich ausbreitet (EN)"},
    {"STAR", "Leuchtende Gaskugel am Himmel (EN)"},
    {"ORBIT", "Umlaufbahn (EN)"},
    {"NORTH", "Gegenüber von Süden (EN)"},
    {"SOUTH", "Gegenüber von Norden (EN)"},
    {"EAST", "Richtung des Sonnenaufgangs (EN)"},
    {"WEST", "Gegenüber von Osten (EN)"},
    {"RAIN", "Flüssiger Niederschlag (EN)"},
    {"SNOW", "Gefrorener Niederschlag (EN)"},
    {"WIND", "Bewegte Luft (EN)"},
    {"CLOUD", "Wassertröpfchen am Himmel (EN)"},
    {"RIVER", "Fließendes Gewässer (EN)"},
    {"OCEAN", "Großes Meer (EN)"},
    {"FOREST", "Viele Bäume zusammen (EN)"},
    {"LEAF", "Grüner Teil einer Pflanze (EN)"},
    {"ROOT", "Verankert die Pflanze im Boden (EN)"},
    {"SEED", "Aus ihm wächst eine Pflanze (EN)"},
    {"BIRD", "Tier mit Federn (EN)"},
    {"FISH", "Tier mit Kiemen (EN)"},
    {"HORSE", "Tier, auf dem man reiten kann (EN)"},
    {"MOUSE", "Kleines Nagetier (EN)"},
    {"HOUSE", "Gebäude zum Wohnen (EN)"},
    {"CHAIR", "Möbel zum Sitzen (EN)"},
    {"TABLE", "Möbel mit Platte (EN)"},
    {"TRAIN", "Fährt auf Schienen (EN)"},
    {"ROAD", "Verkehrsweg (EN)"},
    {"BRIDGE", "Bauwerk über einen Fluss (EN)"},
    {"MUSIC", "Kunst mit Tönen (EN)"},
    {"PIANO", "Instrument mit Tasten (EN)"},
    {"VOICE", "Klang beim Sprechen (EN)"},
    {"DREAM", "Erlebnis im Schlaf (EN)"},
    {"IDEA", "Einfall (EN)"},
    {"LOGIC", "Lehre vom folgerichtigen Denken (EN)"},
    {"NUMBER", "Mathematischer Zählbegriff (EN)"},
    {"CIRCLE", "Runde geometrische Form (EN)"},
    {"SQUARE", "Viereck mit vier gleichen Seiten (EN)"},
    {"SKY", "Himmel (EN)"},
    {"DAY", "Tag (EN)"},
    {"YEAR", "Jahr (EN)"},
    {"KEY", "Schlüssel (EN)"},
};
static const char *cross_words16[5], *cross_clues16[5];
static int cross_x16[5], cross_y16[5] = {1, 1, 3, 5, 7}, cross_down16[5] = {1, 0, 0, 0, 0};
static unsigned cross_seed16;
static void crossword_new16(unsigned seed) {
    cross_seed16 = seed ? seed : random16();
    unsigned state = game_seed16;
    game_seed16 = cross_seed16;
    int central = random16() % 8;
    cross_words16[0] = cross_bank16[central][0];
    cross_clues16[0] = cross_bank16[central][1];
    cross_x16[0] = 4;
    for (int i = 1; i < 5; i++) {
        int choices[200], columns[200], count = 0;
        char match = cross_words16[0][(i - 1) * 2];
        for (unsigned j = 8; j < sizeof(cross_bank16) / sizeof(*cross_bank16); j++)
            for (int k = 0; cross_bank16[j][0][k]; k++)
                if (cross_bank16[j][0][k] == match && k <= 4 &&
                    4 - k + (int)strlen(cross_bank16[j][0]) <= 10 && count < 200) {
                    choices[count] = j;
                    columns[count++] = 4 - k;
                }
        int pick = count ? random16() % count : 0;
        if(!count){choices[0]=central;columns[0]=0;}
        cross_words16[i] = cross_bank16[choices[pick]][0];
        cross_clues16[i] = cross_bank16[choices[pick]][1];
        cross_x16[i] = columns[pick];
    }
    game_seed16 = state;
    memset(crossword_answer16, 0, sizeof(crossword_answer16));
}

typedef struct {unsigned seed;char answers[5][16];} CrosswordSave;
static void save(void) {
    CrosswordSave record={.seed=cross_seed16};
    memcpy(record.answers,crossword_answer16,sizeof(record.answers));
    app_state_save("crossword.bin",&record,sizeof(record));
}
static void load(void) {
    CrosswordSave record={0};
    if(!app_state_load("crossword.bin",&record,sizeof(record))) {
        char seed[16]={0};
        size_t length=0;
        unsigned char legacy[8+sizeof(record.answers)];
        if(lumen_legacy_read("crossword-seed16.txt",seed,sizeof(seed)-1,&length) &&
           lumen_legacy_state("crossword16.bin",legacy,sizeof(legacy))) {
            uint32_t header[2];memcpy(header,legacy,8);
            if(header[0]==0x4c554d16 && header[1]==sizeof(record.answers)) {
                record.seed=(unsigned)strtoul(seed,NULL,10);
                memcpy(record.answers,legacy+8,sizeof(record.answers));
            }
        }
    }
    crossword_new16(record.seed);
    if(record.seed)for(int i=0;i<5;i++) {
        record.answers[i][15]=0;
        size_t n=strlen(record.answers[i]);
        if(n==strlen(cross_words16[i]) && strspn(record.answers[i],"ABCDEFGHIJKLMNOPQRSTUVWXYZ")==n)
            memcpy(crossword_answer16[i],record.answers[i],n+1);
    }
}
static void fetch_clues(void) {
    memset(definitions,0,sizeof(definitions));
    char terms[64];
    snprintf(terms,sizeof(terms),"%s %s %s %s %s",cross_words16[0],cross_words16[1],cross_words16[2],
             cross_words16[3],cross_words16[4]);
    lumen_service("dictionary","clues",terms,1);
}
static void tiny_number17(int x, int y, int n) {
    static const unsigned char glyphs[10][7] = {
        {14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14},  {14, 17, 1, 2, 4, 8, 31},
        {30, 1, 1, 14, 1, 1, 30},     {2, 6, 10, 18, 31, 2, 2}, {31, 16, 16, 30, 1, 1, 30},
        {14, 16, 16, 30, 17, 17, 14}, {31, 1, 2, 4, 8, 8, 8},   {14, 17, 17, 14, 17, 17, 14},
        {14, 17, 17, 15, 1, 1, 14}};
    for (int yy = 0; yy < 7; yy++)
        for (int xx = 0; xx < 5; xx++)
            if (glyphs[n % 10][yy] & (1 << (4 - xx)))
                rect(x + xx * 2, y + yy * 2, 2, 2, 0);
}
static int cross_selected17, cross_checked17;
static int cross_commit17(const char *text) {
    if (!*text) {
        crossword_answer16[crossword_pick16][0] = 0;
        save();
        notify(word("Antwort gelöscht.","Answer cleared.","Réponse effacée."));
        return 1;
    }
    char answer[16];
    int n = 0;
    for (; *text && n < 15; text++)
        if (isalpha((unsigned char)*text))
            answer[n++] = toupper((unsigned char)*text);
    answer[n] = 0;
    int entry = crossword_pick16;
    if (n != (int)strlen(cross_words16[entry])) {
        char b[100];
        snprintf(b, 100, word("Bitte %d Buchstaben eingeben.","Enter %d letters.","Saisissez %d lettres."), (int)strlen(cross_words16[entry]));
        notify(b);
        return 1;
    }
    for (int i = 0; i < 5; i++)
        if (i != entry)
            for (int k = 0; answer[k]; k++)
                for (int j = 0; crossword_answer16[i][j]; j++) {
                    int x = cross_x16[entry] + !cross_down16[entry] * k,
                        y = cross_y16[entry] + cross_down16[entry] * k,
                        xx = cross_x16[i] + !cross_down16[i] * j,
                        yy = cross_y16[i] + cross_down16[i] * j;
                    if (x == xx && y == yy && answer[k] != crossword_answer16[i][j]) {
                        notify(word("Der Buchstabe an der Kreuzung passt nicht.","The crossing letter does not match.","La lettre à l’intersection ne correspond pas."));
                        return 1;
                    }
                }
    snprintf(crossword_answer16[entry], 16, "%s", answer);
    cross_selected17 = entry;
    cross_checked17 = 0;
    save();
    return 1;
}
static void crossword_draw17(void) {
    ui_paint_heading(word("Kreuzworträtsel","Crossword","Mots croisés"),3);
    lumen_button(408,74,44,44,"?",0);
    icon_button(468, 74, IC_DONE);
    icon_button(528, 74, IC_RESTART);
    char grid[8][10] = {{0}}, mask[8][10] = {{0}};
    for (int i = 0; i < 5; i++)
        for (int k = 0; cross_words16[i][k]; k++) {
            int x = cross_x16[i] + !cross_down16[i] * k, y = cross_y16[i] + cross_down16[i] * k;
            if (x < 0 || x >= 10 || y < 0 || y >= 8)
                continue;
            mask[y][x] = 1;
            if (crossword_answer16[i][k])
                grid[y][x] = crossword_answer16[i][k];
        }
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 10; x++)
            if (mask[y][x]) {
                int xx = 30 + x * 54, yy = 132 + y * 46;
                key_outline(xx, yy, 54, 46);
                region(xx, yy, 54, 46);
                if (grid[y][x]) {
                    char b[2] = {grid[y][x], 0};
                    centered(xx, yy + 10, 54, 36, b, PAINT_BODY);
                }
            }
    for (int i = 0; i < 5; i++) {
        int offset = 0;
        for (int k = 0; k < i; k++)
            if (cross_x16[k] == cross_x16[i] && cross_y16[k] == cross_y16[i])
                offset += 12;
        tiny_number17(34 + cross_x16[i] * 54 + offset, 136 + cross_y16[i] * 46, i + 1);
        char b[300];
        snprintf(
            b, sizeof(b), "%d %s  %s%s", i + 1, cross_down16[i] ? "↓" : "→", *definitions[i] ? definitions[i] : cross_clues16[i],
            cross_checked17 ? (!strcasecmp(crossword_answer16[i], cross_words16[i]) ? " ✓" : " ×")
                            : "");
        button_h(28, 520 + i * 49, 544, 43, b, cross_selected17 == i);
    }
}

static int input(const Input *in) {
    if(in->kind==INPUT_OPEN){fetch_clues();return 1;}
    if(in->kind==INPUT_SERVICE && in->x==1 && in->y) {
        for(int i=0;i<5;i++){char key[16];snprintf(key,sizeof(key),"clue%d",i);lumen_string(key,definitions[i],sizeof(definitions[i]));}
        return 1;
    }
    if(in->kind==INPUT_TEXT && in->x>=10 && in->x<15) {
        char text[128];
        if(lumen_string("text",text,sizeof(text))){crossword_pick16=in->x-10;cross_commit17(text);}
        return 1;
    }
    if(in->kind!=INPUT_TAP)return 0;
    int x=in->x,y=in->y;
    if(y>=74 && y<118) {
        if(x>=408 && x<452)lumen_app_open_with("lumen.reader","open_document","",
            word("Kreuzworträtsel","Crossword","Mots croisés"),
            word("Tippe auf einen Hinweis, um die Lösung einzugeben. Die Lösungen sind englische Wörter.",
                 "Tap a clue to enter its answer. All answers are English words.",
                 "Touchez un indice pour saisir sa réponse. Les réponses sont en anglais."));
        else if(x>=528 && x<572){crossword_new16(random16());cross_checked17=0;save();fetch_clues();}
        else if(x>=468 && x<512) {
            cross_checked17=1;
            int solved=1;
            for(int i=0;i<5;i++)solved&=!strcasecmp(crossword_answer16[i],cross_words16[i]);
            notify(solved ? word("Alle Wörter stimmen!","All words are correct!","Tout est correct !") :
                            word("Lösungen geprüft.","Answers checked.","Réponses vérifiées."));
        }
        return 1;
    }
    int pick=-1;
    if(x>=28 && x<572 && y>=520 && y<765)pick=(y-520)/49;
    else if(x>=30 && x<570 && y>=132 && y<500) {
        int col=(x-30)/54,row=(y-132)/46;
        for(int i=0;i<5;i++)for(int k=0;cross_words16[i][k];k++)
            if(cross_x16[i]+!cross_down16[i]*k==col && cross_y16[i]+cross_down16[i]*k==row)pick=i;
    }
    if(pick>=0) {
        cross_selected17=pick;
        lumen_request("input",*definitions[pick] ? definitions[pick] : cross_clues16[pick],
                      crossword_answer16[pick],10+pick);
    }
    return 1;
}
int main(void) {
    load();save();
    lumen_run(crossword_draw17,input,NULL);
    return 0;
}
