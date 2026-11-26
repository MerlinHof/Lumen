#include "lumen_app.h"

typedef struct {
    int x, y, w, h, action;
    char text[8];
} KeyboardKey18;
enum {
    KEY_TEXT18,
    KEY_SHIFT18,
    KEY_DELETE18,
    KEY_SYMBOL18,
    KEY_LANGUAGE18,
    KEY_SPACE18,
    KEY_ENTER18
};

static KeyboardKey18 keyboard_keys18[56];
static int keyboard_count18,shift,symbols_page;

static unsigned first_rune(const char *p) {
    const unsigned char *s=(const unsigned char *)p;
    if(s[0]<128)return s[0];
    return ((s[0]&31)<<6)|(s[1]&63);
}
static void keyboard_case18(char *key) {
    if (!shift || symbols_page)
        return;
    const char *p = key;
    unsigned c = first_rune(p);
    if (c >= 'a' && c <= 'z')
        key[0] -= 32;
    else if (c >= 0xe0 && c <= 0xfe && c != 0xf7) {
        c -= 32;
        key[0] = (char)(0xc0 | (c >> 6));
        key[1] = (char)(0x80 | (c & 63));
        key[2] = 0;
    } else if (c == 0x153)
        snprintf(key, 8, "Œ");
    else if (c == 0xdf)
        snprintf(key, 8, "ẞ");
}
static void keyboard_add18(int x, int y, int w, int h, int action, const char *text) {
    KeyboardKey18 *k = &keyboard_keys18[keyboard_count18++];
    *k = (KeyboardKey18){.x = x, .y = y, .w = w, .h = h, .action = action};
    snprintf(k->text, sizeof(k->text), "%s", text);
    if (action == KEY_TEXT18)
        keyboard_case18(k->text);
}
static void keyboard_layout18(int language) {
    keyboard_count18 = 0;
    for (int i = 0; i < 10; i++) {
        char b[2] = {(char)('0' + (i + 1) % 10), 0};
        keyboard_add18(8 + i * 59, 506, 54, 44, KEY_TEXT18, b);
    }
    static const char *rows[3][3] = {{"qwertzuiop", "asdfghjkl", "yxcvbnm"},
                                     {"qwertyuiop", "asdfghjkl", "zxcvbnm"},
                                     {"azertyuiop", "qsdfghjklm", "wxcvbn"}};
    static const char *symbols[2][30] = {
        {"!", "@", "#", "$", "%",  "^", "&", "*", "(", ")", "-", "_",  "=", "+", "[",
         "]", "{", "}", "/", "\\", ".", ",", ":", ";", "?", "'", "\"", "<", ">", "|"},
        {"€", "£", "¥", "°", "±", "×", "÷", "√", "π", "∞", "~", "`",  "•", "…", "≤",
         "≥", "≠", "²", "³", "µ", ".", ",", ":", ";", "?", "'", "\"", "©", "®", "§"}};
    for (int row = 0; row < 3; row++) {
        int n = symbols_page ? 10 : (int)strlen(rows[language][row]);
        int step = 59;
        int x0 = (600 - (n * step - 5)) / 2;
        for (int i = 0; i < n; i++) {
            char b[8] = {0};
            if (symbols_page)
                snprintf(b, 8, "%s", symbols[symbols_page - 1][row * 10 + i]);
            else {
                b[0] = rows[language][row][i];
            }
            keyboard_add18(x0 + i * step, 558 + row * 60, 54, 52, KEY_TEXT18, b);
        }
    }
    if (!symbols_page) {
        keyboard_add18(8, 678, 54, 52, KEY_SHIFT18, "");
        keyboard_add18(538, 678, 54, 52, KEY_DELETE18, "");
    }
    keyboard_add18(8, 742, 64, 56, KEY_SYMBOL18,
                   symbols_page == 1   ? "#+="
                   : symbols_page == 2 ? "ABC"
                                           : "123");
    const char *langs[] = {"DE", "EN", "FR"};
    keyboard_add18(80, 742, 62, 56, KEY_LANGUAGE18, langs[language]);
    keyboard_add18(150, 742, 272, 56, KEY_SPACE18, "");
    keyboard_add18(430, 742, 62, 56, symbols_page ? KEY_DELETE18 : KEY_TEXT18,
                   symbols_page ? "" : ".");
    keyboard_add18(500, 742, 92, 56, KEY_ENTER18, "");
}

enum { EDIT_INSERT, EDIT_ERASE, EDIT_SUBMIT, EDIT_LAYOUT };
enum { CAP_NUMBER=1, CAP_LETTER=2, CAP_SELECTED=4 };
static void layout_key(const KeyboardKey18 *key,int language) {
    int action=EDIT_LAYOUT, icon=-1, style=0;
    int next=language*6+symbols_page*2+shift;
    const char *text=key->text;
    switch(key->action) {
    case KEY_TEXT18:
        action=EDIT_INSERT;
        if(!symbols_page)next-=shift;
        if(key->y>550)style=CAP_LETTER;
        break;
    case KEY_SHIFT18: icon=IC_SHIFT;next^=1;style=shift ? CAP_SELECTED : 0;break;
    case KEY_DELETE18: action=EDIT_ERASE;icon=IC_DELETE;break;
    case KEY_SYMBOL18: next=language*6+((symbols_page+1)%3)*2+shift;break;
    case KEY_LANGUAGE18: next=((language+1)%3)*6+symbols_page*2+shift;break;
    case KEY_SPACE18: action=EDIT_INSERT;icon=IC_SPACE;text=" ";break;
    case KEY_ENTER18: action=EDIT_SUBMIT;icon=IC_ENTER;break;
    }
    if(key->y==506)style=CAP_NUMBER;
    printf("[%d,%d,%d,%d,%d,%d,%d,%d,",lumen_position(key->x,WIDTH),
           lumen_position(key->y,HEIGHT),lumen_position(key->w,WIDTH),
           lumen_position(key->h,HEIGHT),action,next,icon,style);
    lumen_json(text);
    putchar(']');
}
static void legacy_layouts(void) {
    fputs("{\"ui\":[],\"layouts\":[",stdout);
    for(int language=0;language<3;language++) {
        for(symbols_page=0;symbols_page<3;symbols_page++) {
            for(shift=0;shift<2;shift++) {
                if(language || symbols_page || shift)putchar(',');
                keyboard_layout18(language);
                putchar('[');
                for(int i=0;i<keyboard_count18;i++) {
                    KeyboardKey18 *k=&keyboard_keys18[i];
                    if(i)putchar(',');
                    printf("[%d,%d,%d,%d,%d,",k->x,k->y,k->w,k->h,k->action);
                    lumen_json(k->text);
                    putchar(']');
                }
                putchar(']');
            }
        }
    }
    fputs("],\"accents\":[",stdout);
    for(char c='a';c<='z';c++) {
    const char *variants = c == 'a'   ? "aäàáâãåæ"
                           : c == 'e' ? "eéèêë"
                           : c == 'i' ? "iïîíì"
                           : c == 'o' ? "oöœôóòõø"
                           : c == 'u' ? "uüûùú"
                           : c == 'c' ? "cçćč"
                           : c == 's' ? "sßśš"
                           : c == 'n' ? "nñń"
                           : c == 'y' ? "yÿý"
                           : c == 'z' ? "zžźż"
                                      : NULL;
        if(c!='a')putchar(',');
        lumen_json(variants ? variants : "");
    }
    fputs("]}\n",stdout);
    fflush(stdout);
}


static void layouts(void) {
    printf("{\"ui\":[],\"geometry\":%d,\"keymaps\":[",lumen_relative);
    for(int language=0;language<3;language++) {
        for(symbols_page=0;symbols_page<3;symbols_page++) {
            for(shift=0;shift<2;shift++) {
                if(language || symbols_page || shift)putchar(',');
                keyboard_layout18(language);
                putchar('[');
                for(int i=0;i<keyboard_count18;i++) {
                    if(i)putchar(',');
                    layout_key(&keyboard_keys18[i],language);
                }
                putchar(']');
            }
        }
    }
    fputs("],\"accent_maps\":[",stdout);
    symbols_page=0;
    for(shift=0;shift<2;shift++) {
        if(shift)putchar(',');
        putchar('[');
        for(char c='a';c<='z';c++) {
    const char *variants = c == 'a'   ? "aäàáâãåæ"
                           : c == 'e' ? "eéèêë"
                           : c == 'i' ? "iïîíì"
                           : c == 'o' ? "oöœôóòõø"
                           : c == 'u' ? "uüûùú"
                           : c == 'c' ? "cçćč"
                           : c == 's' ? "sßśš"
                           : c == 'n' ? "nñń"
                           : c == 'y' ? "yÿý"
                           : c == 'z' ? "zžźż"
                                      : NULL;
            if(c!='a')putchar(',');
            char mapped[64]="";
            size_t used=0;
            for(const char *p=variants; p && *p;) {
                size_t width=(unsigned char)*p<128 ? 1 : 2;
                char rune[8];
                memcpy(rune,p,width);rune[width]=0;
                p+=width;
                keyboard_case18(rune);
                size_t n=strlen(rune);
                if(n>=sizeof(mapped)-used)break;
                memcpy(mapped+used,rune,n+1);
                used+=n;
            }
            lumen_json(mapped);
        }
        putchar(']');
    }
    fputs("]}\n",stdout);
    fflush(stdout);
}

int main(void) {
    while(lumen_next()) {
        if(!lumen_is("open"))continue;
        char role[24];
        lumen_string("role",role,sizeof(role));
        if(!strcmp(role,"overlay")) {
            if(lumen_number("surface_api")>=2)layouts();
            else legacy_layouts();
        }
        else {
            lumen_begin();
            lumen_page(lumen_number("language")==0 ? "Tastatur" :
                       lumen_number("language")==2 ? "Clavier" : "Keyboard");
            lumen_component_button("settings",lumen_number("language")==0 ? "Einstellungen" :
                                   lumen_number("language")==2 ? "Réglages" : "Settings",0);
            lumen_end();
            while(lumen_next())if(lumen_is("component"))lumen_open("lumen.settings");
        }
    }
    return 0;
}
