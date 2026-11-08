#include "unicode.h"
#include "fonts.h"
#include <mupdf/fitz.h>
#include <mupdf/ucdn.h>
#include <string.h>
#include <math.h>
static fz_context *unicode_ctx;
static fz_font *base_font[2];
static UiUnicodeGlyph glyphs[96];
static unsigned slot;
const UiUnicodeGlyph *ui_unicode_glyph(unsigned code, int size, int serif, int w, int h) {
    serif = !!serif;
    if (w > 50)
        w = 50;
    if (h > 56)
        h = 56;
    for (unsigned i = 0; i < 96; i++)
        if (glyphs[i].code == code && glyphs[i].size == size && glyphs[i].serif == serif)
            return &glyphs[i];
    UiUnicodeGlyph *g = &glyphs[slot++ % 96];
    memset(g, 0, sizeof(*g));
    g->code = code;
    g->size = size;
    g->serif = serif;
    g->w = w;
    g->h = h;
    g->advance = size;
    g->top = h;
    memset(g->pixels, 255, sizeof(g->pixels));
    if (!unicode_ctx) {
        unicode_ctx = fz_new_context(NULL, NULL, 1024 * 1024);
        if (unicode_ctx)
            lumen_fonts_install(unicode_ctx);
    }
    if (!unicode_ctx)
        return g;
    fz_context *ctx = unicode_ctx;
    fz_pixmap *pix = NULL;
    fz_device *dev = NULL;
    fz_text *text = NULL;
    fz_var(pix);
    fz_var(dev);
    fz_var(text);
    fz_try(ctx) {
        if (!base_font[serif])
            base_font[serif] = fz_new_base14_font(ctx, serif ? "Times-Roman" : "Helvetica");
        fz_font *font = base_font[serif];
        int gid = fz_encode_character_with_fallback(ctx, font, code, ucdn_get_script(code),
                                                    FZ_LANG_zh_Hans, &font);
        g->advance = (int)ceilf(size * fz_advance_glyph(ctx, font, gid, 0));
        if (g->advance > w)
            g->advance = w;
        pix = fz_new_pixmap_with_data(ctx, fz_device_gray(ctx), w, h, NULL, 0, w, g->pixels);
        dev = fz_new_draw_device(ctx, fz_identity, pix);
        text = fz_new_text(ctx);
        fz_matrix matrix = {size, 0, 0, -size, 1, size + 2};
        fz_show_glyph(ctx, text, font, matrix, gid, code, 0, 0, FZ_BIDI_NEUTRAL, FZ_LANG_zh_Hans);
        float black = 0;
        fz_fill_text(ctx, dev, text, fz_identity, fz_device_gray(ctx), &black, 1,
                     fz_default_color_params);
        fz_close_device(ctx, dev);
    }
    fz_always(ctx) {
        fz_drop_text(ctx, text);
        fz_drop_device(ctx, dev);
        fz_drop_pixmap(ctx, pix);
    }
    fz_catch(ctx) { /* A missing external glyph must never abort the UI. */ }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (g->pixels[y * w + x] < 160) {
                if (y < g->top)
                    g->top = y;
                if (y > g->bottom)
                    g->bottom = y;
            }
    return g;
}
void ui_unicode_close(void) {
    if (unicode_ctx) {
        fz_drop_font(unicode_ctx, base_font[0]);
        fz_drop_font(unicode_ctx, base_font[1]);
        fz_drop_context(unicode_ctx);
    }
    unicode_ctx = NULL;
    base_font[0] = base_font[1] = NULL;
    memset(glyphs, 0, sizeof(glyphs));
    slot = 0;
}

/* Shared display typography: render outlines at their final size, never enlarge
 * the small UI bitmap font. The caller may cache the returned tile. */
int ui_display_text(const char *string,int family,int size,int width,int height,unsigned char *pixels) {
    if(width<1 || width>600 || height<1 || height>180 || size<12 || size>160)return 0;
    if(!unicode_ctx){unicode_ctx=fz_new_context(NULL,NULL,1024*1024);if(unicode_ctx)lumen_fonts_install(unicode_ctx);}
    if(!unicode_ctx)return 0;
    fz_context *ctx=unicode_ctx;
    fz_font *font=NULL;fz_text *text=NULL;fz_pixmap *pix=NULL;fz_device *dev=NULL;
    int ok=0;
    static const char *names[]={"Helvetica","Times-Roman","Times-Italic","Helvetica-Bold","Times-Bold"};
    if(family<0 || family>4)family=1;
    memset(pixels,255,(size_t)width*height);
    fz_var(font);fz_var(text);fz_var(pix);fz_var(dev);fz_var(ok);
    fz_try(ctx) {
        font=fz_new_base14_font(ctx,names[family]);
        text=fz_new_text(ctx);
        fz_show_string(ctx,text,font,(fz_matrix){size,0,0,-size,0,0},string,0,0,FZ_BIDI_NEUTRAL,FZ_LANG_UNSET);
        fz_rect bounds=fz_bound_text(ctx,text,NULL,fz_identity);
        fz_matrix place=fz_translate((width-bounds.x1-bounds.x0)/2,(height-bounds.y1-bounds.y0)/2);
        pix=fz_new_pixmap_with_data(ctx,fz_device_gray(ctx),width,height,NULL,0,width,pixels);
        dev=fz_new_draw_device(ctx,fz_identity,pix);
        float black=0;
        fz_fill_text(ctx,dev,text,place,fz_device_gray(ctx),&black,1,fz_default_color_params);
        fz_close_device(ctx,dev);ok=1;
    }
    fz_always(ctx){fz_drop_device(ctx,dev);fz_drop_pixmap(ctx,pix);fz_drop_text(ctx,text);fz_drop_font(ctx,font);}
    fz_catch(ctx){}
    return ok;
}
