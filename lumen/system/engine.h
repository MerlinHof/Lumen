#ifndef LUMEN_ENGINE_H
#define LUMEN_ENGINE_H
#include <stddef.h>
#include <stdint.h>
typedef struct {
    char title[256];
    int page, depth;
} Chapter;
int engine_init(void);
int engine_note_pdf(const char *, const char *, int, const char *);
int engine_metadata(const char *path, char *title, size_t nt, char *author, size_t na);
int engine_open(const char *path, int size, int leading, int margin);
void engine_close(void);
void engine_shutdown(void);
int engine_count(void);
int engine_reflowable(void);
int engine_relayout(int size, int leading, int margin, int oldpage);
int engine_render(int page, unsigned char *pixels, int stride, int width, int height, float zoom,
                  float panx, float pany);
int engine_outline(Chapter *items, int max);
int engine_thumbnail(const char *path, const char *output);
int engine_cover_fill(const char *, unsigned char *, int, int);
int engine_thumbnail_hd(const char *, const char *);
const char *engine_error(void);
int engine_image_fit(const char *path, const char *output, int width, int height);
int engine_photo_thumbnail(const char *, const char *);
int engine_image_thumbnail(const char *path, const char *output);
void engine_options(int font, int align, int letterspace, int paragraph);
#include "contracts.h"
typedef struct {
    float x0, y0, x1, y1;
} TextRect;
int engine_image_rects(TextRect *out, int cap);
typedef struct {
    char text[2048];
    TextRect rects[64];
    int count;
} TextSelection;
int engine_select(int page, float x0, float y0, float x1, float y1, int width, int height,
                  float zoom, float panx, float pany, TextSelection *out);
int engine_find_text(int page, const char *text, int width, int height, float zoom, float panx,
                     float pany, TextSelection *out);
int engine_position(int page, ReaderPosition *position);
int engine_restore(const ReaderPosition *position);
#endif
