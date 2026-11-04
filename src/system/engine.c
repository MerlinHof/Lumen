#include "engine.h"
#include "fonts.h"
#include <math.h>
#include <mupdf/fitz.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static fz_context *ctx;
static int preferred_font, preferred_align, preferred_spacing, preferred_paragraph = 8;
static fz_document *doc;
static fz_stext_page *selection_text;
static int selection_number = -1;
static int count, reflow;
static char error[256];
static unsigned char render_cache[544 * 728];
static int cached_page = -1, cached_w, cached_h;
static float cached_zoom, cached_x, cached_y;
static void seterr(void) { snprintf(error, sizeof(error), "%s", fz_caught_message(ctx)); }
const char *engine_error(void) { return error; }
int engine_init(void) {
    ctx = fz_new_context(NULL, NULL, 12 * 1024 * 1024);
    if (!ctx)
        return 0;
    lumen_fonts_install(ctx);
    fz_register_document_handlers(ctx);
    return 1;
}
void engine_close(void) {
    fz_drop_stext_page(ctx, selection_text);
    selection_text = NULL;
    selection_number = -1;
    cached_page = -1;
    if (doc)
        fz_drop_document(ctx, doc);
    doc = NULL;
    count = 0;
    fz_empty_store(ctx);
}
void engine_shutdown(void) {
    engine_close();
    fz_drop_context(ctx);
    ctx = NULL;
}
int engine_count(void) { return count; }
int engine_reflowable(void) { return reflow; }
int engine_metadata(const char *path, char *title, size_t nt, char *author, size_t na) {
    fz_document *d = NULL;
    int ok = 1;
    fz_var(d);
    fz_try(ctx) {
        d = fz_open_document(ctx, path);
        fz_lookup_metadata(ctx, d, FZ_META_INFO_TITLE, title, nt);
        fz_lookup_metadata(ctx, d, FZ_META_INFO_AUTHOR, author, na);
    }
    fz_always(ctx) {
        fz_drop_document(ctx, d);
        fz_empty_store(ctx);
    }
    fz_catch(ctx) {
        seterr();
        ok = 0;
    }
    return ok;
}
static void style(int leading, int margin) {
    char css[2048];
    snprintf(css, sizeof(css),
             "@page { margin: %dpx !important; } "
             "html, body { margin:0 !important; padding:0 !important; } "
             "body, p, div, span, li { font-family: %s !important; font-size:1em !important; "
             "line-height:%.2f !important; text-align:%s !important; letter-spacing:%.2fpx "
             "!important; } "
             "p { orphans:2; widows:2; margin-top:0 !important; margin-bottom:%dpx !important; } "
             "img { max-width:100%%; height:auto; }",
             margin,
             preferred_font == 1   ? "sans-serif"
             : preferred_font == 2 ? "monospace"
             : preferred_font == 3 ? "Times"
             : preferred_font == 4 ? "\"Noto Serif\""
                                   : "serif",
             leading / 100.0, preferred_align ? "justify" : "left", preferred_spacing / 10.0,
             preferred_paragraph);
    fz_style_document(ctx, doc, 0, css);
}
int engine_open(const char *path, int size, int leading, int margin) {
    engine_close();
    error[0] = 0;
    int ok = 1;
    fz_try(ctx) {
        doc = fz_open_document(ctx, path);
        if (fz_needs_password(ctx, doc))
            fz_throw(ctx, FZ_ERROR_ARGUMENT,
                     "Dieses Buch benötigt ein Passwort oder ist geschützt.");
        reflow = fz_is_document_reflowable(ctx, doc);
        style(leading, margin);
        fz_layout_document(ctx, doc, 544, 728, (float)size);
        count = fz_count_pages(ctx, doc);
        if (count <= 0)
            fz_throw(ctx, FZ_ERROR_FORMAT, "Keine lesbaren Seiten gefunden.");
    }
    fz_catch(ctx) {
        seterr();
        engine_close();
        ok = 0;
    }
    return ok;
}
int engine_relayout(int size, int leading, int margin, int oldpage) {
    if (!doc || !reflow)
        return oldpage;
    ReaderPosition position;
    int anchored = engine_position(oldpage, &position);
    cached_page = -1;
    fz_drop_stext_page(ctx, selection_text);
    selection_text = NULL;
    selection_number = -1;
    int page = oldpage;
    fz_try(ctx) {
        style(leading, margin);
        fz_layout_document(ctx, doc, 544, 728, (float)size);
        count = fz_count_pages(ctx, doc);
        if (anchored)
            page = engine_restore(&position);
    }
    fz_catch(ctx) { seterr(); }
    if (page < 0)
        page = 0;
    if (page >= count)
        page = count - 1;
    return page;
}

int engine_render(int page, unsigned char *pixels, int stride, int width, int height, float zoom,
                  float panx, float pany) {
    if (!doc)
        return 0;
    if (cached_page == page && cached_w == width && cached_h == height && cached_zoom == zoom &&
        cached_x == panx && cached_y == pany) {
        for (int y = 0; y < height; y++)
            memcpy(pixels + y * stride, render_cache + y * width, (size_t)width);
        return 1;
    }
    fz_page *p = NULL;
    fz_pixmap *pix = NULL;
    fz_device *dev = NULL;
    int ok = 1;
    fz_var(p);
    fz_var(pix);
    fz_var(dev);
    fz_try(ctx) {
        p = fz_load_page(ctx, doc, page);
        fz_rect bounds = fz_bound_page(ctx, p);
        float pw = bounds.x1 - bounds.x0, ph = bounds.y1 - bounds.y0;
        if (pw <= 0 || ph <= 0)
            fz_throw(ctx, FZ_ERROR_FORMAT, "Ungültige Seitengröße.");
        float scale = fminf(width / pw, height / ph) * zoom;
        float tx = (width - pw * scale) / 2 - bounds.x0 * scale + panx;
        float ty = (height - ph * scale) / 2 - bounds.y0 * scale + pany;
        pix = fz_new_pixmap_with_data(ctx, fz_device_gray(ctx), width, height, NULL, 0, stride,
                                      pixels);
        fz_clear_pixmap_with_value(ctx, pix, 255);
        dev = fz_new_draw_device(ctx, fz_identity, pix);
        fz_matrix transform = {scale, 0, 0, scale, tx, ty};
        fz_run_page(ctx, p, dev, transform, NULL);
        fz_close_device(ctx, dev);
    }
    fz_always(ctx) {
        fz_drop_device(ctx, dev);
        fz_drop_pixmap(ctx, pix);
        fz_drop_page(ctx, p);
    }
    fz_catch(ctx) {
        seterr();
        ok = 0;
    }
    if (ok && (size_t)width * height <= sizeof(render_cache)) {
        for (int y = 0; y < height; y++)
            memcpy(render_cache + y * width, pixels + y * stride, (size_t)width);
        cached_page = page;
        cached_w = width;
        cached_h = height;
        cached_zoom = zoom;
        cached_x = panx;
        cached_y = pany;
    }
    return ok;
}
static int flatten(fz_outline *o, Chapter *items, int n, int max, int depth) {
    for (; o && n < max; o = o->next) {
        int page = fz_page_number_from_location(ctx, doc, o->page);
        if (page < 0 && o->uri) {
            fz_location loc = fz_resolve_link(ctx, doc, o->uri, NULL, NULL);
            page = fz_page_number_from_location(ctx, doc, loc);
        }
        if (page >= 0) {
            snprintf(items[n].title, sizeof(items[n].title), "%s", o->title ? o->title : "Kapitel");
            items[n].page = page;
            items[n].depth = depth;
            n++;
        }
        if (o->down)
            n = flatten(o->down, items, n, max, depth + 1);
    }
    return n;
}
int engine_outline(Chapter *items, int max) {
    fz_outline *o = NULL;
    int n = 0;
    fz_var(o);
    fz_try(ctx) {
        o = fz_load_outline(ctx, doc);
        n = flatten(o, items, 0, max, 0);
    }
    fz_always(ctx) { fz_drop_outline(ctx, o); }
    fz_catch(ctx) { seterr(); }
    return n;
}
int engine_thumbnail(const char *path, const char *output) {
    if (doc)
        return 0;
    if (!engine_open(path, 22, 145, 10))
        return 0;
    unsigned char pixels[120 * 164];
    int ok = engine_render(0, pixels, 120, 120, 164, 1, 0, 0);
    if (ok) {
        FILE *f = fopen(output, "wb");
        if (f) {
            fprintf(f, "P5\n120 164\n255\n");
            if (fwrite(pixels, 1, sizeof(pixels), f) != sizeof(pixels))
                ok = 0;
            if (fclose(f))
                ok = 0;
        } else
            ok = 0;
    }
    engine_close();
    return ok;
}
int engine_thumbnail_hd(const char *path, const char *output) {
    if (doc)
        return 0;
    if (!engine_open(path, 22, 145, 10))
        return 0;
    unsigned char pixels[360 * 492];
    int ok = engine_render(0, pixels, 360, 360, 492, 1, 0, 0);
    if (ok) {
        FILE *f = fopen(output, "wb");
        if (f) {
            fprintf(f, "P5\n360 492\n255\n");
            if (fwrite(pixels, 1, sizeof(pixels), f) != sizeof(pixels))
                ok = 0;
            if (fclose(f))
                ok = 0;
        } else
            ok = 0;
    }
    engine_close();
    return ok;
}
int engine_image_fit(const char *path, const char *output, int width, int height) {
    fz_image *image = NULL;
    fz_pixmap *source = NULL, *gray = NULL;
    fz_device *dev = NULL;
    fz_path *shape = NULL;
    int ok = 1;
    fz_var(image);
    fz_var(source);
    fz_var(gray);
    fz_var(dev);
    fz_var(shape);
    fz_try(ctx) {
        image = fz_new_image_from_file(ctx, path);
        image->interpolate = 1;
        if (image->w > 10000 || image->h > 10000 || (long long)image->w * image->h > 32000000)
            fz_throw(ctx, FZ_ERROR_LIMIT, "Image too large");
        fz_irect box = {0, 0, width, height};
        gray = fz_new_pixmap_with_bbox(ctx, fz_device_gray(ctx), box, NULL, 0);
        fz_clear_pixmap_with_value(ctx, gray, 255);
        dev = fz_new_draw_device(ctx, fz_identity, gray);
        int orientation = fz_image_orientation(ctx, image);
        int swap = orientation == 2 || orientation == 4 || orientation == 6 || orientation == 8;
        int iw = swap ? image->h : image->w, ih = swap ? image->w : image->h;
        float scale = fminf((float)width / iw, (float)height / ih), w = iw * scale, h = ih * scale;
        fz_matrix fit = {w, 0, 0, h, (width - w) / 2, (height - h) / 2};
        fz_matrix m = fz_concat(fz_image_orientation_matrix(ctx, image), fit);
        fz_fill_image(ctx, dev, image, m, 1, fz_default_color_params);
        fz_close_device(ctx, dev);
        FILE *f = fopen(output, "wb");
        if (!f)
            fz_throw(ctx, FZ_ERROR_SYSTEM, "Cover cache unavailable");
        fprintf(f, "P5\n%d %d\n255\n", width, height);
        int stride = fz_pixmap_stride(ctx, gray);
        unsigned char *samples = fz_pixmap_samples(ctx, gray);
        for (int y = 0; y < height; y++)
            if (fwrite(samples + y * stride, 1, (size_t)width, f) != (size_t)width)
                ok = 0;
        if (fclose(f))
            ok = 0;
    }
    fz_always(ctx) {
        fz_drop_path(ctx, shape);
        fz_drop_device(ctx, dev);
        fz_drop_pixmap(ctx, source);
        fz_drop_pixmap(ctx, gray);
        fz_drop_image(ctx, image);
    }
    fz_catch(ctx) {
        seterr();
        ok = 0;
    }
    return ok;
}

int engine_image_thumbnail(const char *path, const char *output) {
    return engine_image_fit(path, output, 120, 164);
}

void engine_options(int font, int align, int letterspace, int paragraph) {
    preferred_font = font;
    preferred_align = align;
    preferred_spacing = letterspace;
    preferred_paragraph = paragraph;
}

int engine_position(int page, ReaderPosition *position) {
    memset(position, 0, sizeof(*position));
    if (!doc || page < 0 || page >= count)
        return 0;
    fz_buffer *text = NULL;
    int ok = 1;
    fz_var(text);
    fz_try(ctx) {
        fz_location loc = fz_location_from_page_number(ctx, doc, page);
        position->chapter = loc.chapter;
        position->page = loc.page;
        position->chapter_pages = fz_count_chapter_pages(ctx, doc, loc.chapter);
        if (reflow) {
            text = fz_new_buffer_from_page_number(ctx, doc, page, NULL);
            unsigned char *bytes;
            size_t n = fz_buffer_storage(ctx, text, &bytes), w = 0;
            int space = 0;
            for (size_t i = 0; i < n && w < 96; i++) {
                unsigned char c = bytes[i];
                if (c <= ' ') {
                    space = 1;
                    continue;
                }
                if (space && w)
                    position->quote[w++] = ' ';
                space = 0;
                if (w < 96)
                    position->quote[w++] = (char)c;
            }
            while (w && ((unsigned char)position->quote[w - 1] & 192) == 128)
                w--;
            if (w && (unsigned char)position->quote[w - 1] >= 192)
                w--;
            position->quote[w] = 0;
        }
    }
    fz_always(ctx) { fz_drop_buffer(ctx, text); }
    fz_catch(ctx) {
        seterr();
        ok = 0;
    }
    return ok;
}
int engine_restore(const ReaderPosition *position) {
    if (!doc)
        return 0;
    int page = 0;
    fz_try(ctx) {
        int chapter = position->chapter;
        if (chapter < 0 || chapter >= fz_count_chapters(ctx, doc))
            chapter = 0;
        int chapter_pages = fz_count_chapter_pages(ctx, doc, chapter);
        int guess = position->chapter_pages > 0
                        ? (int)((long long)position->page * chapter_pages / position->chapter_pages)
                        : position->page;
        if (guess >= chapter_pages)
            guess = chapter_pages - 1;
        if (guess < 0)
            guess = 0;
        int first = fz_page_number_from_location(ctx, doc, (fz_location){chapter, 0});
        page = first + guess;
        if (reflow && *position->quote) {
            for (int pass = 0; pass < 2; pass++) {
                char quote[128];
                snprintf(quote, sizeof(quote), "%s", position->quote);
                if (pass && strlen(quote) > 38) {
                    quote[38] = 0;
                    while (strlen(quote) && ((unsigned char)quote[strlen(quote) - 1] & 192) == 128)
                        quote[strlen(quote) - 1] = 0;
                }
                int found = 0;
                for (int delta = 0; delta <= 12 && !found; delta++) {
                    int offset = delta % 2 ? -(delta + 1) / 2 : delta / 2;
                    int candidate = guess + offset;
                    if (candidate < 0 || candidate >= chapter_pages)
                        continue;
                    fz_quad hit[1];
                    int marks[1];
                    if (fz_search_page_number(ctx, doc, first + candidate, quote, marks, hit, 1) >
                        0) {
                        page = first + candidate;
                        found = 1;
                    }
                }
                if (found)
                    break;
            }
        }
    }
    fz_catch(ctx) { seterr(); }
    if (page < 0)
        page = 0;
    if (page >= count)
        page = count - 1;
    return page;
}

static fz_stext_page *selection_get(int number, fz_page *p) {
    if (selection_number != number || !selection_text) {
        fz_drop_stext_page(ctx, selection_text);
        selection_text = NULL;
        selection_number = -1;
        selection_text = fz_new_stext_page_from_page(ctx, p, NULL);
        selection_number = number;
    }
    return selection_text;
}
static fz_matrix text_transform(fz_page *p, int width, int height, float zoom, float px, float py) {
    fz_rect b = fz_bound_page(ctx, p);
    float w = b.x1 - b.x0, h = b.y1 - b.y0;
    if (w <= 0 || h <= 0)
        fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid page bounds");
    float scale = fminf(width / w, height / h) * zoom;
    return (fz_matrix){scale,
                       0,
                       0,
                       scale,
                       (width - w * scale) / 2 - b.x0 * scale + px,
                       (height - h * scale) / 2 - b.y0 * scale + py};
}
static void selection_rects(TextSelection *out, fz_quad *q, int n, fz_matrix m) {
    out->count = n;
    for (int i = 0; i < n; i++) {
        fz_rect r = fz_rect_from_quad(fz_transform_quad(q[i], m));
        out->rects[i] = (TextRect){r.x0, r.y0, r.x1, r.y1};
    }
}
int engine_select(int number, float x0, float y0, float x1, float y1, int width, int height,
                  float zoom, float px, float py, TextSelection *out) {
    memset(out, 0, sizeof(*out));
    if (!doc)
        return 0;
    fz_page *p = NULL;
    fz_stext_page *text = NULL;
    char *copy = NULL;
    int ok = 0;
    fz_var(p);
    fz_var(text);
    fz_var(copy);
    fz_var(ok);
    fz_try(ctx) {
        p = fz_load_page(ctx, doc, number);
        fz_matrix m = text_transform(p, width, height, zoom, px, py), inverse = fz_invert_matrix(m);
        text = selection_get(number, p);
        fz_point a = fz_transform_point((fz_point){x0, y0}, inverse),
                 b = fz_transform_point((fz_point){x1, y1}, inverse);
        fz_snap_selection(ctx, text, &a, &b, FZ_SELECT_WORDS);
        fz_quad q[64];
        int n = fz_highlight_selection(ctx, text, a, b, q, 64);
        copy = fz_copy_selection(ctx, text, a, b, 0);
        if (copy && *copy && n > 0) {
            size_t len = strlen(copy);
            if (len >= sizeof(out->text))
                len = sizeof(out->text) - 1;
            while (len && ((unsigned char)copy[len] & 192) == 128)
                len--;
            memcpy(out->text, copy, len);
            out->text[len] = 0;
            selection_rects(out, q, n, m);
            ok = 1;
        }
    }
    fz_always(ctx) {
        fz_free(ctx, copy);
        fz_drop_page(ctx, p);
    }
    fz_catch(ctx) { seterr(); }
    return ok;
}
int engine_find_text(int number, const char *needle, int width, int height, float zoom, float px,
                     float py, TextSelection *out) {
    memset(out, 0, sizeof(*out));
    if (!doc || !*needle)
        return 0;
    fz_page *p = NULL;
    int ok = 0;
    fz_var(p);
    fz_var(ok);
    fz_try(ctx) {
        p = fz_load_page(ctx, doc, number);
        fz_matrix m = text_transform(p, width, height, zoom, px, py);
        fz_quad q[64];
        int marks[64];
        int n = fz_search_stext_page(ctx, selection_get(number, p), needle, marks, q, 64);
        if (n > 0) {
            selection_rects(out, q, n, m);
            ok = 1;
        }
    }
    fz_always(ctx) { fz_drop_page(ctx, p); }
    fz_catch(ctx) { seterr(); }
    return ok;
}

/* Persistent square thumbnails shared by native Photos and Connect. */
int engine_photo_thumbnail(const char *source, const char *output) {
    FILE *f = fopen(source, "rb");
    if (!f)
        return 0;
    char magic[3];
    int w, h, max;
    int ok = fscanf(f, "%2s %d %d %d", magic, &w, &h, &max) == 4 && !strcmp(magic, "P5") &&
             w == 600 && h == 800 && max == 255;
    unsigned char *raw = NULL, *tiny = NULL;
    if (ok) {
        fgetc(f);
        raw = malloc(480000);
        tiny = malloc(18496);
        ok = raw && tiny && fread(raw, 1, 480000, f) == 480000;
    }
    fclose(f);
    if (!ok) {
        free(raw);
        free(tiny);
        return 0;
    }
    int top = 0, bot = 799, left = 0, right = 599;
    for (; top < 799; top++) {
        int nonwhite = 0;
        for (int x = 0; x < 600; x++)
            if (raw[top * 600 + x] < 247) {
                nonwhite = 1;
                break;
            }
        if (nonwhite)
            break;
    }
    for (; bot > top; bot--) {
        int nonwhite = 0;
        for (int x = 0; x < 600; x++)
            if (raw[bot * 600 + x] < 247) {
                nonwhite = 1;
                break;
            }
        if (nonwhite)
            break;
    }
    for (; left < 599; left++) {
        int nonwhite = 0;
        for (int y = top; y <= bot; y++)
            if (raw[y * 600 + left] < 247) {
                nonwhite = 1;
                break;
            }
        if (nonwhite)
            break;
    }
    for (; right > left; right--) {
        int nonwhite = 0;
        for (int y = top; y <= bot; y++)
            if (raw[y * 600 + right] < 247) {
                nonwhite = 1;
                break;
            }
        if (nonwhite)
            break;
    }
    if (right - left < 10 || bot - top < 10) {
        top = left = 0;
        bot = 799;
        right = 599;
    }
    int side = right - left + 1;
    if (bot - top + 1 < side)
        side = bot - top + 1;
    left += (right - left + 1 - side) / 2;
    top += (bot - top + 1 - side) / 2;
    for (int y = 0; y < 136; y++)
        for (int x = 0; x < 136; x++)
            tiny[y * 136 + x] = raw[(top + y * side / 136) * 600 + left + x * side / 136];
    char temp[4096];
    snprintf(temp, sizeof(temp), "%s.tmp-XXXXXX", output);
    int fd = mkstemp(temp);
    ok = fd >= 0;
    if (ok) {
        FILE *g = fdopen(fd, "wb");
        if (g) {
            ok = fprintf(g, "P5\n136 136\n255\n") > 0 && fwrite(tiny, 1, 18496, g) == 18496;
            if (fflush(g) || fsync(fd))
                ok = 0;
            if (fclose(g))
                ok = 0;
        } else {
            close(fd);
            ok = 0;
        }
        if (ok)
            ok = rename(temp, output) == 0;
        if (!ok)
            unlink(temp);
    }
    free(raw);
    free(tiny);
    return ok;
}

/* Export a Markdown note with page-bound ink using an independent document.
 * The reader's current document and selection are never modified. */
int engine_note_pdf(const char *html, const char *inkdir, int id, const char *output) {
    fz_context *c = fz_new_context(NULL, NULL, 16 * 1024 * 1024);
    if (!c)
        return 0;
    lumen_fonts_install(c);
    fz_register_document_handlers(c);
    fz_document *d = NULL;
    fz_document_writer *w = NULL;
    fz_page *p = NULL;
    fz_buffer *b = NULL;
    fz_image *image = NULL;
    int ok = 0;
    fz_var(d);
    fz_var(w);
    fz_var(p);
    fz_var(b);
    fz_var(image);
    fz_var(ok);
    fz_try(c) {
        d = fz_open_document(c, html);
        fz_layout_document(c, d, 600, 800, 20);
        w = fz_new_document_writer(c, output, "pdf", NULL);
        int pages = fz_count_pages(c, d), inkpages = pages;
        for (int i = 0; i < 512; i++) {
            char path[4096];
            snprintf(path, sizeof(path), "%s/%d-page-%d.ink", inkdir, id, i);
            if (!access(path, R_OK))
                inkpages = i + 1 > inkpages ? i + 1 : inkpages;
        }
        for (int i = 0; i < inkpages; i++) {
            fz_device *dev = fz_begin_page(c, w, fz_make_rect(0, 0, 600, 800));
            if (i < pages) {
                p = fz_load_page(c, d, i);
                fz_run_page(c, p, dev, fz_identity, NULL);
                fz_drop_page(c, p);
                p = NULL;
            }
            char path[4096];
            snprintf(path, sizeof(path), "%s/%d-page-%d.ink", inkdir, id, i);
            FILE *f = fopen(path, "rb");
            if (f) {
                unsigned char *raw = malloc(600 * 688);
                size_t n = raw ? fread(raw, 1, 600 * 688, f) : 0;
                fclose(f);
                if (n == 600 * 688) {
                    /* White pixels are transparent so pen marks overlay text. */
                    fz_pixmap *pix = fz_new_pixmap(c, fz_device_gray(c), 600, 688, NULL, 1);
                    unsigned char *s = fz_pixmap_samples(c, pix);
                    for (int k = 0; k < 600 * 688; k++) {
                        s[k * 2] = 0;
                        s[k * 2 + 1] = 255 - raw[k];
                    }
                    image = fz_new_image_from_pixmap(c, pix, NULL);
                    fz_drop_pixmap(c, pix);
                    fz_fill_image(c, dev, image, fz_make_matrix(600, 0, 0, 688, 0, 54), 1,
                                  fz_default_color_params);
                    fz_drop_image(c, image);
                    image = NULL;
                }
                free(raw);
            }
            fz_end_page(c, w);
        }
        fz_close_document_writer(c, w);
        ok = 1;
    }
    fz_always(c) {
        fz_drop_image(c, image);
        fz_drop_buffer(c, b);
        fz_drop_page(c, p);
        fz_drop_document_writer(c, w);
        fz_drop_document(c, d);
    }
    fz_catch(c) { ok = 0; }
    fz_drop_context(c);
    if (!ok)
        unlink(output);
    return ok;
}
