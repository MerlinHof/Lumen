#ifndef LUMEN_UNICODE_H
#define LUMEN_UNICODE_H
/* Bounded cache for user text outside the pre-rendered UI catalogue. */
typedef struct {
    unsigned code;
    int size, serif, w, h, advance, top, bottom;
    unsigned char pixels[50 * 56];
} UiUnicodeGlyph;
const UiUnicodeGlyph *ui_unicode_glyph(unsigned code, int size, int serif, int w, int h);
void ui_unicode_close(void);
#endif
