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

static void layouts(void) {
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

int main(void) {
    while(lumen_next()) {
        if(!lumen_is("open"))continue;
        char role[24];
        lumen_string("role",role,sizeof(role));
        if(!strcmp(role,"overlay"))layouts();
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
