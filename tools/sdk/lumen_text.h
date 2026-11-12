#ifndef LUMEN_TEXT_H
#define LUMEN_TEXT_H
#include <ctype.h>
#include <stddef.h>

static inline unsigned lumen_utf8_next(const char **s) {
    unsigned char c = (unsigned char)**s;
    if (!c)
        return 0;
    (*s)++;
    if (c < 128)
        return c;
    unsigned cp;
    int n;
    if ((c & 224) == 192) {
        cp = c & 31;
        n = 1;
    } else if ((c & 240) == 224) {
        cp = c & 15;
        n = 2;
    } else if ((c & 248) == 240) {
        cp = c & 7;
        n = 3;
    } else
        return 0xfffd;
    for (int i = 0; i < n; i++) {
        unsigned char d = (unsigned char)**s;
        if ((d & 192) != 128)
            return 0xfffd;
        (*s)++;
        cp = (cp << 6) | (d & 63);
    }
    if ((n == 1 && cp < 128) || (n == 2 && cp < 2048) || (n == 3 && cp < 65536) || cp > 0x10ffff ||
        (cp >= 0xd800 && cp <= 0xdfff))
        return 0xfffd;
    return cp;
}
static inline void lumen_utf8_fold(const char *in, char *out, size_t max) {
    if (!max)
        return;
    size_t n = 0;
    while (*in && n + 1 < max) {
        unsigned c = lumen_utf8_next(&in);
        if (c >= 0x300 && c <= 0x36f)
            continue;
        if (c >= 192 && c <= 222 && c != 215)
            c += 32;
        if (c == 228 || c == 224 || c == 225 || c == 226)
            c = 'a';
        if (c == 246 || c == 243 || c == 242 || c == 244)
            c = 'o';
        if (c == 252 || c == 250 || c == 249 || c == 251)
            c = 'u';
        if (c == 233 || c == 232 || c == 234)
            c = 'e';
        if (c == 223) {
            out[n++] = 's';
            if (n + 1 < max)
                out[n++] = 's';
            continue;
        }
        if (c < 128)
            out[n++] = (char)tolower(c);
        else
            out[n++] = '?';
    }
    out[n] = 0;
}
#endif
