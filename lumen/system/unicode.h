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
/* Native-resolution display type, centered by glyph bounds. family: sans, serif,
 * italic serif, bold sans, bold serif. Caller supplies a bounded grayscale tile. */
int ui_display_text(const char *text,int family,int size,int width,int height,unsigned char *pixels);

#endif
