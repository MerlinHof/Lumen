#include "fonts.h"
#include "library.h"
#include <mupdf/ucdn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
/* The small MuPDF build omits its large built-in CJK fonts. Use the licensed
 * companion font on demand for both reader and UI; Latin fonts stay unchanged. */
static fz_font *load_cjk(fz_context *ctx, const char *name, int ordering, int serif) {
    (void)name;
    (void)ordering;
    (void)serif;
    char path[2048];
    const char *override = getenv("LUMEN_UI_FONT");
    if (override)
        snprintf(path, sizeof(path), "%s", override);
    else
        snprintf(path, sizeof(path), "%s/../fonts/DroidSansFallbackFull.ttf", data_root);
    if (access(path, R_OK))
        return NULL;
    fz_font *font = NULL;
    fz_var(font);
    fz_try(ctx) {
        font = fz_new_font_from_file(ctx, "LUMEN Unicode", path, 0, 0);
    }
    fz_catch(ctx) {
        font = NULL;
    }
    return font;
}
static fz_font *load_fallback(fz_context *ctx, int script, int language, int serif, int bold,
                              int italic) {
    (void)language;
    (void)bold;
    (void)italic;
    if (script != UCDN_SCRIPT_HAN && script != UCDN_SCRIPT_HANGUL &&
        script != UCDN_SCRIPT_HIRAGANA && script != UCDN_SCRIPT_KATAKANA &&
        script != UCDN_SCRIPT_BOPOMOFO)
        return NULL;
    return load_cjk(ctx, NULL, 0, serif);
}
void lumen_fonts_install(fz_context *ctx) {
    fz_install_load_system_font_funcs(ctx, NULL, load_cjk, load_fallback);
}
