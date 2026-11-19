#include "../dictionary.h"
#define _GNU_SOURCE
#include "api.h"
#include "paint.h"
#include "notification_request.h"
#include "../amazon.h"
#include "../connect.h"
#include "../photo_image.h"
#include "../version.h"
#include "../wireless_update.h"
#include "../release_update.h"
#include "../app_packages.h"
#include "../system_update.h"
#include "../engine.h"
#include "font_data.h"
#include "../i18n.h"
#include "../library.h"
#include "../network.h"
#include "../qrcodegen.h"
#include "../services.h"
#include "../app_runtime.h"
#include "../app_storage.h"
#include "../paper_font.h"
#include "../unicode.h"
#include "../text_edit.h"
#include "../text_input.h"
#include "../preferences.h"
#include <ctype.h>
#include <math.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>
#ifndef LUMEN_HOST
#include <sys/prctl.h>
#endif
#include "theme.h"
unsigned char ui_pixels[WIDTH * HEIGHT];
static int ink_color, ui_y_offset;
static int power_menu,power_hold,power_request;
static int64_t power_menu_wake_until;

#include "editor_state.h"
static struct {
    char battery[24], wifi[32];
    time_t status_time, charging_checked;
    int battery_charging;
} status_ui;
#include "document_state.h"
static AppRuntime runtime;
static AppId document_provider(void) {
    return app_package_find(app_package_provider(PROVIDER_DOCUMENT));
}
static AppId books_provider(void) {
    return app_package_find(app_package_provider(PROVIDER_BOOKS));
}
static AppId active_app(void) { return runtime.active; }
static void app_leave(AppId app, void *ctx);
static int activate_app(AppId app);
static void show_library_detail(int index);
static int app_send(const AppIntent *intent);
static int package_ui_poll(void);
static int package_exit_requested;
static void package_ui_draw(void),package_clock_tick(void);
static int package_clock_wait(void);
static int package_fullscreen,package_document_visible,package_caret_visible;
static char package_view_title[128];
static AppId package_fullscreen_owner=-1;
static int package_ui_input(const Input *);
static int package_pointer_input(const Input *);
static int package_ui_commit(int target,const char *text);
static void package_input_checkpoint(void);
static void package_ui_choice(int choice);
static int app_read_book(int index);
static int app_handle_input(const Input *in);
static void app_render(void);
static int selected = -1, current = -1;
static int battery_popup;

static int text_editable_hit(const Input *);
static void small_number17(int, int, const char *, int);
static void reader_markdown17(const char *, const char *, int);
static void checkbox(int, int, int),
    select_row17(int, const char *, const char *);
static int keyboard_multiline(void);
static void editor_reset18(void);
static int keyboard_intercept18(const Input *in);
static void u16_tick(void);
static void package_pointer_reset(void);

static int page, full = 1, sleeping, updating;
static float zoom = 1, panx, pany;
static int selection_release;
static int64_t activity16;
static int annotations_write(void);
static void annotations_load(void);
static void annotations_draw(void);
static void json_escape16(FILE *,const char *);
static void document_excerpt_close(void),document_excerpt_poll(void);
static int document_excerpt_wait(void);


typedef struct {
    int x, y, w, h, system, radius, sides, quiet;
    int paint_x,paint_y,paint_w,paint_h;
} Widget;
static Widget widgets[1024], press_bounds;
static int widget_count, pressed = -1, press_canceled, down_y, history[64], history_count;
static int dropdown, drop_target,
    drop_count, drop_selected, drop_first, drop_visible;
static unsigned drop_generation;
static int drop_x, drop_y, drop_w;
static const char **drop_labels;
static size_t keyboard_capacity(void);
static int hit(int x, int y) {
    for (int i = widget_count - 1; i >= 0; i--)
        if (x >= widgets[i].x && x < widgets[i].x + widgets[i].w && y >= widgets[i].y &&
            y < widgets[i].y + widgets[i].h)
            return i;
    return -1;
}
static void region(int x, int y, int w, int h) {
    if (widget_count < 1000)
        widgets[widget_count++] = (Widget){.x=x, .y=y+ui_y_offset, .w=w, .h=h,
                                           .radius=RADIUS_SMALL, .sides=3};
}
static void widget_shape(int x, int y, int w, int h, int radius, int sides, int quiet) {
    if (!widget_count) return;
    Widget *widget = &widgets[widget_count - 1];
    if (widget->x != x || widget->y != y + ui_y_offset || widget->w != w || widget->h != h)
        return;
    widget->radius = radius;
    widget->sides = sides;
    widget->quiet = quiet;
}
static void system_region(int x, int y, int w, int h) {
    int count = widget_count;
    region(x, y, w, h);
    if (widget_count > count)
        widgets[count].system = 1;
}
static int textwidth(const char *s, const Font *f);
static int textwidth_raw(const char *s,const Font *f);
static void notification_draw(void),notification_dismiss(void);
static int notification_visible(void),notification_poll(void),notification_input(const Input*,int);
static int package_overlays_allowed(void);
static void package_overlays_draw(void),package_overlays_leave(int owner);
static int package_overlays_input(const Input *input),package_overlay_poll(int slot);
static int receive_notification(const void*,size_t,int64_t);
static int notification_visible(void);
static void overlays(void);
static void sleep_surface_draw(void);
void ui_feedback_draw(void);
static void modal_input(const Input *in);
static int library_dirty;
static void refresh_library(void);
static void wifi_open(void);
static void apps_init(void);
static void apps_pause(void);
static void apps_checkpoint(void);
static void apps_save(void);
static void apps_page_turned(void);
static void turn(int);
static void system_loading(void);
static void extra_init(void);
static int extra_poll(void);
static int extra_tick(void);
static void extra_save(void);
static void extra_timer_label(char *out, int cap);
static void extra_alarm_label(char *out, int cap);
static int extra_ringing(void);
static int64_t extra_millis(void);
static void extra_overlay(void);
static int extra_intercept(const Input *in);
static void extra_track(unsigned seconds, unsigned pages, uint64_t id);
static void extra_zone(void);
static void key_outline(int x, int y, int w, int h);
enum { READER_TOOLBAR_END = 126, READER_CONTENTS_X = 468, READER_SETTINGS_X = 528 };
static void control_surface(int x, int y, int w, int h, int r, int color);
static void outline_surface(int x, int y, int w, int h, int r, int color);
static void segment_fill(int x, int y, int w, int h, int r, int color, int sides);
static void stroke(int x0, int y0, int x1, int y1, int thickness);
static void commit_keyboard(void);

static void keyboard_insert(const char *s);
static void keyboard_backspace(void);

static void v1_init(void), v1_save(void);
static int v1_tick(void), v1_poll(void);
static int clipboard_input(const Input *);
static void clipboard_draw(void), v1_effect(void), v1_close(void);
static int v1_sleep_photo(void);
static void v1_timer_started(void);
static int v1_sleep_poll(void), v1_sleep_tick(void);
static void header(void);
static int status_overlay_poll(void);
static int status_action(const Input *input);
static int keyboard_package_poll(void);
static void package_back_results(void);

static uint64_t stable_text_id(const char *s) {
    uint64_t hash=1469598103934665603ull;
    for(;*s;s++)hash=(hash^(unsigned char)*s)*1099511628211ull;
    return hash;
}
static void polish_draw(void);
static int polish_power(const Input *);
#include "keyboard_request.h"
static void open_keyboard(int target, const char *title, const char *value) {
    const KeyboardRequest request = {target, title, value};
    keyboard_receive(&request, sizeof(request), extra_millis());
}
static void open_dropdown(int target, const char **labels, int count, int selected, int x, int y,
                          int w) {
    if (count <= 0) return;
    dropdown = 1;
    drop_generation++;
    editor_ui.keyboard = 0;
    drop_target = target;
    drop_labels = labels;
    drop_count = count;
    int max_rows = (790-STATUS_HEIGHT-10)/48;
    if (count > max_rows) max_rows = (790-STATUS_HEIGHT-10-44)/48;
    drop_visible = count < max_rows ? count : max_rows;
    drop_first = selected >= 0 ? selected/drop_visible*drop_visible : 0;
    drop_selected = selected;
    if (w > 400)
        w = 400;
    if (w < 220)
        w = 220;
    if (x + w > WIDTH - 12)
        x = WIDTH - w - 12;
    drop_x = x;
    drop_y = y;
    drop_w = w;
    pressed = -1;
}
static int theme_gray(int c) {
    return c >= 210 && c < 255 ? (c >= 231 ? 220 : 200) : c;
}
static void rect(int x, int y, int w, int h, int color) {
    y += ui_y_offset;
    color = theme_gray(color);
    if (w <= 0 || h <= 0 || x >= WIDTH || y >= HEIGHT)
        return;
    int left = x < 0 ? 0 : x, right = x + w > WIDTH ? WIDTH : x + w;
    int top = y < 0 ? 0 : y, bottom = y + h > HEIGHT ? HEIGHT : y + h;
    if (left >= right || top >= bottom)
        return;
    for (int j = top; j < bottom; j++)
        memset(ui_pixels + j * WIDTH + left, color, (size_t)(right - left));
}
static unsigned glyph(const char **s) {
    unsigned c = utf8_next(s);
    if (**s) {
        const char *next = *s;
        unsigned mark = utf8_next(&next);
        if (mark == 0x308) {
            unsigned composed = c == 'a'   ? 228
                                : c == 'o' ? 246
                                : c == 'u' ? 252
                                : c == 'A' ? 196
                                : c == 'O' ? 214
                                : c == 'U' ? 220
                                           : 0;
            if (composed) {
                *s = next;
                c = composed;
            }
        }
    }
    if (c == 0x2013 || c == 0x2014)
        c = '-';
    if (c == 0x2018 || c == 0x2019)
        c = '\'';
    if (c == 0x201c || c == 0x201d || c == 0x201e)
        c = '"';
    if (c == 0x2026)
        c = '.';
    if (c == 0x2022)
        c = '*';
    if (c < 256)
        return c;
    for (unsigned i = 0; i < 12; i++)
        if (c == ui_codepoints[i])
            return 256 + i;
    unsigned lo = 12, hi = sizeof(ui_codepoints) / sizeof(*ui_codepoints);
    while (lo < hi) {
        unsigned mid = lo + (hi - lo) / 2;
        if (ui_codepoints[mid] < c)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo < sizeof(ui_codepoints) / sizeof(*ui_codepoints) && ui_codepoints[lo] == c
               ? 256 + lo
               : 0x200000 + c;
}
static int glyph_advance(const Font *f, unsigned c) {
    return c < 0x200000 ? f->advance[c]
                        : ui_unicode_glyph(c - 0x200000, f->h - 12, f == &editorial || f == &title,
                                           f->w, f->h)
                              ->advance;
}
static const unsigned char *glyph_pixels(const Font *f, unsigned c) {
    return c < 0x200000 ? f->pixels + c * f->w * f->h
                        : ui_unicode_glyph(c - 0x200000, f->h - 12, f == &editorial || f == &title,
                                           f->w, f->h)
                              ->pixels;
}
static int glyph_top(const Font *f, unsigned c) {
    return c < 0x200000 ? f->top[c]
                        : ui_unicode_glyph(c - 0x200000, f->h - 12, f == &editorial || f == &title,
                                           f->w, f->h)
                              ->top;
}
static int glyph_bottom(const Font *f, unsigned c) {
    return c < 0x200000 ? f->bottom[c]
                        : ui_unicode_glyph(c - 0x200000, f->h - 12, f == &editorial || f == &title,
                                           f->w, f->h)
                              ->bottom;
}
/* Exact non-white bounds, cached once per built-in glyph. Font metrics used
 * for layout are unchanged; antialiasing fringes are included in these bounds. */
typedef struct {
    unsigned char ready, left, right, top, bottom;
} GlyphBounds;
static GlyphBounds glyph_bounds[5][sizeof(small_advance)];
static const GlyphBounds *text_bounds(const Font *f, unsigned c) {
    int face = f == &small       ? 0
               : f == &body      ? 1
               : f == &title     ? 2
               : f == &editorial ? 3
               : f == &caption   ? 4
                                 : -1;
    if (face < 0 || c >= sizeof(small_advance))
        return NULL;
    GlyphBounds *b = &glyph_bounds[face][c];
    if (!b->ready) {
        b->left = f->w;
        b->right = 0;
        b->top = f->h;
        b->bottom = 0;
        const unsigned char *pixels = f->pixels + c * f->w * f->h;
        for (int y = 0; y < f->h; y++)
            for (int x = 0; x < f->w; x++)
                if (pixels[y * f->w + x] < 255) {
                    if (x < b->left)
                        b->left = x;
                    if (x + 1 > b->right)
                        b->right = x + 1;
                    if (y < b->top)
                        b->top = y;
                    if (y + 1 > b->bottom)
                        b->bottom = y + 1;
                }
        b->ready = 1;
    }
    return b;
}
static int drawtext_raw(int x, int y, const char *s, const Font *f, int maxw) {
    y += ui_y_offset;
    int start = x;
    while (*s) {
        unsigned c = glyph(&s);
        if (c == '\n')
            break;
        int a = glyph_advance(f, c);
        if (x + a > start + maxw)
            break;
        const unsigned char *p = glyph_pixels(f, c);
        const GlyphBounds *bounds = text_bounds(f, c);
        int top = bounds ? bounds->top : 0, bottom = bounds ? bounds->bottom : f->h;
        int left = bounds ? bounds->left : 0, right = bounds ? bounds->right : f->w;
        if (top < -y)
            top = -y;
        if (bottom > HEIGHT - y)
            bottom = HEIGHT - y;
        if (left < -x)
            left = -x;
        if (right > WIDTH - x)
            right = WIDTH - x;
        int color = ink_color == 0 && f == &caption ? UI_SECONDARY_INK : ink_color < 0 ? 0 : ink_color;
        for (int j = top; j < bottom && left < right; j++) {
            const unsigned char *source = p + j * f->w;
            unsigned char *dest = ui_pixels + (y + j) * WIDTH;
            for (int i = left; i < right; i++) {
                int v = source[i];
                if (v < 255) {
                    int old = dest[x + i];
                    dest[x + i] = old + (color - old) * (255 - v) / 255;
                }
            }
        }
        x += a;
    }
    return x;
}
static int drawtext(int x, int y, const char *s, const Font *f, int maxw) {
    return drawtext_raw(x, y, tr(s), f, maxw);
}
static int wrapped_emphasis(int x,int y,const char *s,const Font *f,int width,int lines,
                            int offset,int length,const char **remaining) {
    const char *text=s;
    const char *first=text+offset,*last=first+length;
    int lh = f->h - 4;
    int row = 0;
    for (; row < lines && *s; row++) {
        const char *start = s, *p = s, *space = NULL;
        int w = 0;
        while (*p && *p != '\n') {
            const char *before = p;
            unsigned c = glyph(&p);
            if (w + glyph_advance(f, c) > width) {
                p = space && space > start ? space : before;
                break;
            }
            w += glyph_advance(f, c);
            if (c == ' ')
                space = p;
        }
        if (p == start) {
            glyph(&p);
        }
        char buf[1024];
        size_t n = (size_t)(p - start);
        if (n >= sizeof(buf))
            n = sizeof(buf) - 1;
        memcpy(buf, start, n);
        buf[n] = 0;
        if(x>=0) {
            if(length>0 && start<last && start+n>first) {
                int lo=first>start ? (int)(first-start) : 0;
                int hi=last<start+n ? (int)(last-start) : (int)n;
                char saved=buf[lo];
                buf[lo]=0;int left=textwidth_raw(buf,f);buf[lo]=saved;
                saved=buf[hi];buf[hi]=0;int right=textwidth_raw(buf,f);buf[hi]=saved;
                rect(x+left,y+row*lh+4,right-left,lh-3,208);
            }
            drawtext_raw(x,y+row*lh,buf,f,width);
        }
        s = p;
        while (*s == ' ' || *s == '\n')
            s++;
    }
    if(remaining)*remaining=s;
    return row;
}
static int wrapped_raw(int x,int y,const char *s,const Font *f,int width,int lines) {
    return wrapped_emphasis(x,y,s,f,width,lines,0,0,NULL);
}
static void wrapped(int x, int y, const char *s, const Font *f, int width, int lines) {
    wrapped_raw(x, y, tr(s), f, width, lines);
}
static int textwidth_raw(const char *s, const Font *f) {
    int w = 0;
    while (*s)
        w += glyph_advance(f, glyph(&s));
    return w;
}
static int textwidth(const char *s, const Font *f) {
    return textwidth_raw(tr(s), f);
}
static int ink_top(const char *label, const Font *f, int *height) {
    int top = f->h, bottom = -1;
    while (*label) {
        unsigned c = glyph(&label);
        if (glyph_bottom(f, c) >= glyph_top(f, c)) {
            if (glyph_top(f, c) < top)
                top = glyph_top(f, c);
            if (glyph_bottom(f, c) > bottom)
                bottom = glyph_bottom(f, c);
        }
    }
    if (bottom < top) {
        *height = 0;
        return 0;
    }
    *height = bottom - top + 1;
    return top;
}
static void text_caret(int x, int y, const Font *f) {
    int cap, top = ink_top("H", f, &cap), height = f->h - 8;
    rect(x, y + top + (cap - height) / 2, 2, height, 0);
}
/* Short blink bursts after editing keep an idle E-Ink panel quiet. */
enum { CARET_BLINK_MS=650, CARET_PHASES=6 };
static void caret_restart(void) {
    editor_ui.cursor_epoch=extra_millis();
    editor_ui.cursor_visible=1;
}
static int caret_active(void) {
    return !sleeping && !updating && !dropdown && !editor_ui.clip_active &&
        (editor_ui.keyboard || package_caret_visible);
}
static int caret_wait(void) {
    if(!caret_active() || !editor_ui.cursor_epoch)return -1;
    int64_t elapsed=extra_millis()-editor_ui.cursor_epoch;
    if(elapsed>=CARET_BLINK_MS*CARET_PHASES)return editor_ui.cursor_visible ? -1 : 0;
    return CARET_BLINK_MS-(int)(elapsed%CARET_BLINK_MS);
}
static int caret_tick(void) {
    if(!caret_active() || !editor_ui.cursor_epoch)return 0;
    int64_t phase=(extra_millis()-editor_ui.cursor_epoch)/CARET_BLINK_MS;
    int visible=phase>=CARET_PHASES || !(phase&1);
    if(visible==editor_ui.cursor_visible)return 0;
    editor_ui.cursor_visible=visible;
    return 1;
}
static void centered(int x, int y, int w, int h, const char *label, const Font *font) {
    label = tr(label);
    int width = textwidth(label, font);
    if (width > w - 16 && font == &small) {
        font = &caption;
        width = textwidth(label, font);
    }
    int ink, top = ink_top(label, font, &ink);
    drawtext(width > w - 16 ? x + 8 : x + (w - width) / 2, y + (h - ink) / 2 - top, label, font,
             w - 16);
}
static void button_h(int x, int y, int w, int h, const char *label, int chosen) {
    region(x, y, w, h);
    widget_shape(x, y, w, h, RADIUS_MEDIUM, 3, 0);
    control_surface(x, y, w, h, RADIUS_MEDIUM, 0);
    control_surface(x + 2, y + 2, w - 4, h - 4, 10, 255);
    if (chosen)
        rect(x + 8, y + h - 5, w - 16, 3, 0);
    centered(x, y, w, h, label, &small);
}
static void dropdown_button(int x, int y, int w, int h, const char *label) {
    region(x, y, w, h);
    widget_shape(x, y, w, h, RADIUS_MEDIUM, 3, 0);
    control_surface(x, y, w, h, 12, 0);
    control_surface(x + 2, y + 2, w - 4, h - 4, 10, 255);
    centered(x + 2, y, w - 34, h, label, &small);
    int tip=x+w-21, mid=y+h/2;
    stroke(tip-7,mid-4,tip,mid+3,3);
    stroke(tip,mid+3,tip+7,mid-4,3);
}
static void segments(int x, int y, int w, int h, const char *const *labels, int count,
                     int selected) {
    if (count < 1) return;
    control_surface(x, y, w, h, 13, UI_INK);
    for (int i = 0; i < count; i++) {
        int left = x + w * i / count;
        int right = x + w * (i + 1) / count;
        region(left, y, right - left, h);
        widget_shape(left,y,right-left,h,RADIUS_MEDIUM,(i==0 ? 1 : 0)|(i==count-1 ? 2 : 0),i==selected);
        if (i != selected) {
            int edge_left = i == 0 ? 2 : 0;
            int edge_right = i == count - 1 ? 2 : 0;
            int sides = (i == 0 ? 1 : 0) | (i == count - 1 ? 2 : 0);
            segment_fill(left + edge_left, y + 2, right - left - edge_left - edge_right,
                         h - 4, 11, UI_PAPER, sides);
            if (i > 0 && i - 1 != selected)
                rect(left, y + 10, 1, h - 20, UI_RULE);
        }
        int saved_ink = ink_color;
        ink_color = i == selected ? UI_PAPER : UI_INK;
        centered(left, y, right - left, h, labels[i], &small);
        if (i == selected)
            centered(left + 1, y, right - left, h, labels[i], &small);
        ink_color = saved_ink;
    }
}
static void disc(int x, int y, int r, int color);
static void aa_arc(int x, int y, int r, int from, int to, int thickness);
static void aa_disc(int x, int y, int r, int color);
static void wifi_symbol(int x, int y) {
    /* Three equally spaced bands radiating from the same center. */
    for (int radius=6; radius<=16; radius+=5)
        aa_arc(x+15,y+28,radius,217,323,2);
    aa_disc(x+15,y+28,2,0);
}
static void button(int x, int y, int w, const char *label, int chosen) {
    button_h(x, y, w, 48, label, chosen);
}
static void blend_pixel(int x, int y, float alpha, int color) {
    y += ui_y_offset;
    color = theme_gray(color);
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || alpha <= 0)
        return;
    if (alpha > 1)
        alpha = 1;
    int old = ui_pixels[y * WIDTH + x];
    ui_pixels[y * WIDTH + x] = (unsigned char)(old + (color - old) * alpha);
}
static void stroke_color(int x0, int y0, int x1, int y1, int thickness, int color) {
    if (thickness < 2)
        thickness = 2;
    float dx = x1 - x0, dy = y1 - y0, len = dx * dx + dy * dy;
    int pad = thickness / 2 + 2;
    int left = (x0 < x1 ? x0 : x1) - pad, right = (x0 > x1 ? x0 : x1) + pad,
        top = (y0 < y1 ? y0 : y1) - pad, bottom = (y0 > y1 ? y0 : y1) + pad;
    if (left < 0)
        left = 0;
    if (right >= WIDTH)
        right = WIDTH - 1;
    if (top < 0)
        top = 0;
    if (bottom >= HEIGHT)
        bottom = HEIGHT - 1;
    for (int y = top; y <= bottom; y++)
        for (int x = left; x <= right; x++) {
            float t = len ? ((x - x0) * dx + (y - y0) * dy) / len : 0;
            if (t < 0)
                t = 0;
            if (t > 1)
                t = 1;
            float xx = x - x0 - t * dx, yy = y - y0 - t * dy;
            blend_pixel(x, y, thickness / 2.f + 0.5f - sqrtf(xx * xx + yy * yy), color);
        }
}
static void aa_stroke(int x0,int y0,int x1,int y1,int thickness){stroke_color(x0,y0,x1,y1,thickness,0);}
static void aa_arc_color(int cx, int cy, int r, int start, int end, int thickness, int color) {
    if (thickness < 2)
        thickness = 2;
    int extent = r + (thickness + 1) / 2 + 1;
    for (int dy = -extent; dy <= extent; dy++)
        for (int dx = -extent; dx <= extent; dx++) {
            float squared = dx * dx + dy * dy;
            float half = thickness / 2.f + .5f;
            if (squared > (r + half) * (r + half) ||
                (r > half && squared < (r - half) * (r - half)))
                continue;
            if (start > 0 || end < 360) {
                float angle = atan2f(dy, dx) * 180 / 3.14159265f;
                if (angle < 0)
                    angle += 360;
                if (angle < start || angle > end)
                    continue;
            }
            float distance = sqrtf(dx * dx + dy * dy);
            float alpha = thickness / 2.f + .5f - fabsf(distance - r);
            blend_pixel(cx + dx, cy + dy, alpha, color);
        }
}
static void aa_arc(int cx, int cy, int r, int start, int end, int thickness) {
    aa_arc_color(cx, cy, r, start, end, thickness, 0);
}
static void stroke(int x0, int y0, int x1, int y1, int thickness) {
    aa_stroke(x0, y0, x1, y1, thickness);
}
static void ring(int x, int y, int r, int thickness) {
    aa_arc(x, y, r, 0, 360, thickness);
}
static int rounding_profile(void);
#include "icons.h"
#include "dropdown_icons.h"

static void aa_disc(int cx, int cy, int r, int color) {
    for (int y = -r - 1; y <= r + 1; y++)
        for (int x = -r - 1; x <= r + 1; x++)
            blend_pixel(cx + x, cy + y, r + 0.5f - sqrtf(x * x + y * y), color);
}
static void disc(int x, int y, int r, int color) {
    aa_disc(x, y, r, color);
}
#include "corners.h"
static void toggle(int x, int y, int on) {
    region(x - 10, y - 8, 90, 48);
    widget_shape(x-10,y-8,90,48,RADIUS_MEDIUM,3,1);
    control_surface(x + 2, y + 1, 64, 30, 15, 0);
    if (!on)
        control_surface(x + 4, y + 3, 60, 26, 13, 255);
    control_surface(x + (on ? 40 : 8), y + 6, 20, 20, 10, on ? 255 : 0);
}
static void key_outline(int x, int y, int w, int h) {
    if (w < 4 || h < 4) return;
    widget_shape(x, y, w, h, RADIUS_MEDIUM, 3, 0);
    outline_surface(x, y, w, h, RADIUS_MEDIUM, 48);
}
static AppId startup_app(void) {
    int id=app_package_find(app_package_provider(PROVIDER_STARTUP));
    return id;
}
static int can_back(void) {
    return (app_package_installed(active_app()) && app_process_frame_back()) || editor_ui.keyboard || dropdown || battery_popup ||
           history_count || active_app() != startup_app();
}
#include "image_sampling.h"
#include "cover_cache.h"
static void cover_bounds(const unsigned char *p, int cw, int ch, int *x0, int *y0, int *x1,
                         int *y1, int force) {
    *x0 = 0;
    *y0 = 0;
    *x1 = cw;
    *y1 = ch;
    if (!force && !options[OPT_COVER_CROP])
        return;
    int top = ch, left = cw, bottom = 0, right = 0;
    for (int y = 0; y < ch; y++) {
        int ink = 0;
        for (int x = 0; x < cw; x++)
            if (p[y * cw + x] < 235)
                ink++;
        if (ink >= 5) {
            if (y < top)
                top = y;
            bottom = y + 1;
        }
    }
    for (int x = 0; x < cw; x++) {
        int ink = 0;
        for (int y = 0; y < ch; y++)
            if (p[y * cw + x] < 235)
                ink++;
        if (ink >= 5) {
            if (x < left)
                left = x;
            right = x + 1;
        }
    }
    /* Avoid cropping almost-empty typographic covers down to just their title. */
    if (right - left >= cw / 2 && bottom - top >= ch / 2) {
        *x0 = left;
        *y0 = top;
        *x1 = right;
        *y1 = bottom;
    }
}
static void cover_draw_file(Book *b,int x,int y,int w,int h,int fill,FILE *f) {
    if(!fill)rounded_clip_begin(x,y,w,h);
    if (f) {
        struct stat source;
        int cacheable = !fill && !fstat(fileno(f), &source);
        if (cacheable && cover_tile_get(&source, x, y, w, h)) {
            fclose(f);
            if (!fill) rounded_clip_end(x, y, w, h);
            return;
        }
        char magic[3];
        int cw, ch, m;
        if (fscanf(f, "%2s %d %d %d", magic, &cw, &ch, &m) == 4 && !strcmp(magic, "P5") && cw > 0 &&
            cw <= 360 && ch > 0 && ch <= 492 && m == 255) {
            fgetc(f);
            unsigned char pix[360 * 492];
            size_t pixels = (size_t)cw * ch;
            if (fread(pix, 1, pixels, f) == pixels) {
                int x0, y0, x1, y1;
                cover_bounds(pix, cw, ch, &x0, &y0, &x1, &y1, fill);
                int sw = x1 - x0, sh = y1 - y0;
                if (fill || options[OPT_COVER_CROP]) {
                    if (sw * h > sh * w) {
                        int nw = sh * w / h;
                        x0 += (sw - nw) / 2;
                        sw = nw;
                    } else {
                        int nh = sw * h / w;
                        y0 += (sh - nh) / 2;
                        sh = nh;
                    }
                }
                for (int j = 0; j < h; j++)
                    for (int i = 0; i < w; i++) {
                        int u = ((2 * i + 1) * sw * 128 / w) - 128,
                            v = ((2 * j + 1) * sh * 128 / h) - 128;
                        int value = image_bilinear(pix + y0 * cw + x0, cw, sw, sh, u, v);
                        int px = x + i, py = y + j + ui_y_offset;
                        if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT)
                            ui_pixels[py * WIDTH + px] =
                                theme_gray(image_tone(value, x + i, y + j));
                    }
                if (cacheable)
                    cover_tile_put(&source, x, y, w, h);
                if (!fill) rounded_clip_end(x, y, w, h);
                fclose(f);
                return;
            }
        }
        fclose(f);
    }
    int tone = 238 + (int)(b->id % 3) * 5;
    rect(x, y, w, h, tone);
    rect(x + 8, y + 12, 2, h - 24, tone - 18);
    const char *q = b->title;
    unsigned letter = glyph(&q);
    char initial[8] = "?";
    if (letter < 128 && letter > 32) {
        initial[0] = (char)(letter >= 'a' && letter <= 'z' ? letter - 32 : letter);
        initial[1] = 0;
    } else {
        size_t n = (size_t)(q - b->title);
        if (n < sizeof(initial)) {
            memcpy(initial, b->title, n);
            initial[n] = 0;
        }
    }
    const Font *face = w > 90 ? &title : w > 70 ? &body : &small;
    centered(x + 8, y + h / 4, w - 16, h / 2, initial, face);
    if (w > 100)
        centered(x + 12, y + h - 42, w - 24, 26, b->format, &caption);
    if (!fill) rounded_clip_end(x, y, w, h);
}
static void cover_draw(Book *b, int x, int y, int w, int h, int fill) {
    char path[2048];
    library_cover_path(b, path, sizeof(path));
    char hd[2048];
    snprintf(hd, sizeof(hd), "%s/covers/%016llx.hi.pgm", data_root, (unsigned long long)b->id);
    FILE *f = fopen(hd, "rb");
    if (!f)
        f = fopen(path, "rb");
    cover_draw_file(b,x,y,w,h,fill,f);
}
static void cover_size(Book *b, int x, int y, int w, int h) {
    cover_draw(b,x,y,w,h,0);

}
static void page_arrow(int x, int y, int kind, int enabled) {
    if (enabled) {
        icon_button(x, y, kind);
        return;
    }
    unsigned char saved[44 * 44];
    int yy = y + ui_y_offset;
    for (int j = 0; j < 44; j++)
        for (int i = 0; i < 44; i++)
            saved[j * 44 + i] = ui_pixels[(yy + j) * WIDTH + x + i];
    icon(x, y, kind);
    for (int j = 0; j < 44; j++)
        for (int i = 0; i < 44; i++) {
            int at = (yy + j) * WIDTH + x + i, old = saved[j * 44 + i];
            ui_pixels[at] = old + (ui_pixels[at] - old) * 35 / 100;
        }
}
typedef struct {
    int *state, count, y, visible;
    void (*move)(int);
} Pagination;
static Pagination pagination;
static void pagination_move(int delta) {
    device_activity();
    if (!pagination.state)
        return;
    int next = *pagination.state + delta;
    if (next < 0 || next >= pagination.count)
        return;
    if (pagination.move)
        pagination.move(delta);
    else
        *pagination.state = next;
}
enum { PAGER_X = 205, PAGER_W = 189, PAGER_DOT_STEP = 24 };
static int pager_dot_start(int x, int w, int pages) {
    return x + (w-pages*PAGER_DOT_STEP)/2;
}
static int pager_target(int x, int left, int width, int page, int pages) {
    if (x >= left && x < left+44) return page > 0 ? page-1 : page;
    if (x >= left+width-44 && x < left+width) return page+1 < pages ? page+1 : page;
    if (pages <= 4) {
        int offset = x - pager_dot_start(left,width,pages);
        return offset >= 0 && offset < pages*PAGER_DOT_STEP ? offset/PAGER_DOT_STEP : -1;
    }
    return -1;
}
static int pagination_hit(const Input *in) {
    if (in->kind != INPUT_TAP || !pagination.state || !pagination.visible ||
        pagination.count <= 1 || in->y < pagination.y || in->y >= pagination.y+44)
        return 0;
    return pager_target(in->x,PAGER_X,PAGER_W,*pagination.state,pagination.count) >= 0;
}
static void pager_draw(int x, int y, int w, int page, int pages) {
    page_arrow(x,y,IC_PREV,page>0);
    page_arrow(x+w-44,y,IC_NEXT,page+1<pages);
    if (pages <= 4) {
        int start = pager_dot_start(x,w,pages);
        for (int i=0; i<pages; i++) {
            int left = start+i*PAGER_DOT_STEP;
            region(left,y,PAGER_DOT_STEP,44);
            widget_shape(left,y,PAGER_DOT_STEP,44,RADIUS_SMALL,3,1);
            int center = left+PAGER_DOT_STEP/2;
            aa_disc(center,y+22,5,i == page ? 0 : 172);
        }
        return;
    }
    char text[32];
    snprintf(text,sizeof(text),"%d / %d",page+1,pages);
    centered(x+44,y,w-88,44,text,&caption);
}
static void system_loading(void) {
#ifndef LUMEN_HOST
    /* Keep the previous page intact while a synchronous load runs. Only the
       small busy indicator reaches the panel until content is ready. */
    rect(258,336,84,68,255);
    aa_arc(300,370,15,35,310,3);
    if(options[OPT_INVERT])
        for(int y=336;y<404;y++)for(int x=258;x<342;x++)
            ui_pixels[y*WIDTH+x]=255-ui_pixels[y*WIDTH+x];
    device_present_async(ui_pixels);
#endif
}
#include "widgets.h"
#include "typography.h"
#include "selection_menu.h"
#include "text_flow.h"
#include "list.h"
#include "titlebar.h"
void ui_message(const char *s) {
    NotificationRequest request={.duration_ms=6000};
    snprintf(request.text,sizeof(request.text),"%s",tr(s));
    receive_notification(&request,sizeof(request),extra_millis());
}
int ui_select_app(const char *id) {
    AppId app=app_package_find(id);
    if(!app_package_foreground(app))return 0;
    full=1;
    return activate_app(app);
}
void ui_init(void) {
    app_packages_scan();
    if(!system_update_trial() || !app_package_foreground(startup_app()))app_packages_seed();
    app_provider_overlay_send(PROVIDER_TEXT_INPUT,"{\"event\":\"prepare\"}");
    app_runtime_init(&runtime, app_leave, NULL);
    library_settings_load();
    apps_init();
    extra_init();
    v1_init();
    if (options[OPT_LANGUAGE] < 0 || options[OPT_LANGUAGE] > 2)
        options[OPT_LANGUAGE] = 0;
    options[OPT_CONFIRM] = 1;
    settings.grid = 1;
    settings.filter = 0;

    activate_app(startup_app());
    device_status(status_ui.battery, sizeof(status_ui.battery), status_ui.wifi, sizeof(status_ui.wifi));
    status_ui.battery_charging = device_charging();
    status_ui.status_time = status_ui.charging_checked = time(NULL);
}
static void position_signature(int *v) {
    v[0] = settings.size;
    v[1] = settings.leading;
    v[2] = settings.margin;
    v[3] = options[OPT_FONT];
    v[4] = options[OPT_ALIGN];
    v[5] = options[OPT_LETTERSPACE];
    v[6] = options[OPT_PARAGRAPH] + 2000;
}
static void save_anchor(const Book *book, int page, int bookmark) {
    ReaderPosition p;
    if (!engine_position(page, &p))
        return;
    char path[2048], buf[1024];
    if (bookmark < 0)
        snprintf(path, sizeof(path), tr("%s/books/%016llx.position"), data_root,
                 (unsigned long long)book->id);
    else
        snprintf(path, sizeof(path), tr("%s/books/%016llx.mark%d"), data_root,
                 (unsigned long long)book->id, bookmark);
    int v[7];
    position_signature(v);
    int n = snprintf(buf, sizeof(buf), tr("LUMENPOS3 %d %d %d %d %d %d %d %d %d %d %d\n%s\n"), v[0],
                     v[1], v[2], v[3], v[4], v[5], v[6], page, p.chapter, p.page, p.chapter_pages,
                     p.quote);
    atomic_write(path, buf, (size_t)n);
}
static int restore_anchor(const Book *book, int fallback, int bookmark) {
    char path[2048];
    if (bookmark < 0)
        snprintf(path, sizeof(path), tr("%s/books/%016llx.position"), data_root,
                 (unsigned long long)book->id);
    else
        snprintf(path, sizeof(path), tr("%s/books/%016llx.mark%d"), data_root,
                 (unsigned long long)book->id, bookmark);
    FILE *f = fopen(path, "r");
    if (!f)
        return fallback;
    int v[7], current_signature[7], saved_page;
    ReaderPosition p = {0};
    int ok = fscanf(f, "LUMENPOS3 %d %d %d %d %d %d %d %d %d %d %d\n", v, v + 1, v + 2, v + 3,
                    v + 4, v + 5, v + 6, &saved_page, &p.chapter, &p.page, &p.chapter_pages) == 11;
    if (ok && fgets(p.quote, sizeof(p.quote), f)) {
        p.quote[strcspn(p.quote, "\r\n")] = 0;
        position_signature(current_signature);
        fallback = !memcmp(v, current_signature, sizeof(v)) ? saved_page : engine_restore(&p);
    }
    fclose(f);
    return fallback;
}

void ui_save(void) {
    package_input_checkpoint();
    apps_checkpoint();
    apps_save();
    extra_save();
    v1_save();
    if (current >= 0) {
        library_progress(&library[current], page, engine_count());
        save_anchor(&library[current], page, -1);
    }
    library_settings_save();
}
static int reader_layout[7], reader_layout_valid;
static void current_layout(int values[7]) {
    ReaderPreferences p = preferences_reader();
    int v[7] = {p.size, p.leading, p.margin, p.font, p.alignment, p.letterspace, p.paragraph};
    memcpy(values, v, sizeof(v));
}
static void apply_reading_settings(void) {
    library_settings_save();
    if (current < 0 || !engine_count())
        return;
    int next[7];
    current_layout(next);
    if (reader_layout_valid && !memcmp(reader_layout, next, sizeof(next)))
        return;
    reader_ui.layout_epoch++;
    ReaderPreferences p = preferences_reader();
    engine_options(p.font, p.alignment, p.letterspace, p.paragraph);
    page = engine_relayout(p.size, p.leading, p.margin, page);
    memcpy(reader_layout, next, sizeof(next));
    reader_layout_valid = 1;
    reader_ui.chapter_count = engine_outline(reader_ui.chapters, 256);
    library_progress(&library[current], page, engine_count());
}
int ui_open(int index) {
    reader_ui.layout_epoch++;
    if (index < 0 || (index >= library_count && !(index == BOOK_LIMIT && reader_ui.reader_document_active)))
        return 0;
    document_excerpt_close();
    reader_ui.selecting = selection_release = 0;
    notification_dismiss();
    Book *b = &library[index];
    if (!library_supported(b)) {
        show_library_detail(index);
        full = 1;
        ui_message("Dieses Amazon-Format bitte im Original-Reader öffnen.");
        return 0;
    }
    if (current >= 0) {
        library_progress(&library[current], page, engine_count());
        save_anchor(&library[current], page, -1);
    }
    if (b->size > 128*1024 || (index != BOOK_LIMIT && strcmp(b->format,"txt") && strcmp(b->format,"html"))) system_loading();
    engine_close();
    if (index != BOOK_LIMIT)
        reader_ui.reader_document_active = 0;
    current = -1;
    engine_options(options[OPT_FONT], options[OPT_ALIGN], options[OPT_LETTERSPACE],
                   options[OPT_PARAGRAPH]);
    char converted[2048];
    const char *readerpath = b->path;
    if (library_import_path(b, converted, sizeof(converted)))
        readerpath = converted;
    else if (amazon_can_prepare(b)) {
        if (!amazon_prepare(b, converted, sizeof(converted))) {
            b->protected_format = 1;
            show_library_detail(index);
            ui_message("Dieses Buch im Kindle-Reader öffnen.");
            return 0;
        }
        readerpath = converted;
    }
    if(readerpath==converted) {
        char title[384]="",author[192]="",meta[2048];
        snprintf(meta,sizeof(meta),"%s/books/%016llx.meta",data_root,(unsigned long long)b->id);
        if(access(meta,F_OK) && engine_metadata(readerpath,title,sizeof(title),author,sizeof(author))) {
            if(*title)snprintf(b->title,sizeof(b->title),"%s",title);
            if(*author)snprintf(b->author,sizeof(b->author),"%s",author);
            char text[640];int n=snprintf(text,sizeof(text),"%s\n%s\n",b->title,b->author);atomic_write(meta,text,n);
        }
        char cover[2048];snprintf(cover,sizeof(cover),"%s/covers/%016llx.hi.pgm",data_root,(unsigned long long)b->id);
        if(access(cover,R_OK))engine_thumbnail_hd(readerpath,cover);
        b->protected_format=0;
    }
    if (!engine_open(readerpath, settings.size, settings.leading, settings.margin)) {
        selected = -1;
        ui_message("Dieses Buch konnte nicht geöffnet oder dargestellt werden.");
        return 0;
    }
    current_layout(reader_layout);
    reader_layout_valid = 1;
    selected = current = index;
    page = restore_anchor(b, b->page, -1);
    for (int i = 0; i < b->bookmark_count; i++)
        b->bookmarks[i] = restore_anchor(b, b->bookmarks[i], i);
    if (page >= engine_count())
        page = engine_count() - 1;
    if (page < 0)
        page = 0;
    reader_ui.chapter_count = engine_outline(reader_ui.chapters, 256);
    zoom = 1;
    panx = pany = 0;
    activate_app(document_provider());
    reader_ui.reader_chrome = 0;
    full = 1;
    library_progress(b, page, engine_count());
    annotations_load();
    reader_ui.selecting = 0;
    if(app_process_open(document_provider()))
        app_process_send("{\"event\":\"intent\",\"operation\":\"document-opened\"}");
    return 1;
}
static int refresh_status(int force) {
    time_t now = time(NULL);
    if (force || !status_ui.status_time || now < status_ui.status_time || now - status_ui.status_time >= 30) {
        char previous_wifi[sizeof(status_ui.wifi)], previous_battery[sizeof(status_ui.battery)];
        memcpy(previous_wifi,status_ui.wifi,sizeof(previous_wifi));
        memcpy(previous_battery,status_ui.battery,sizeof(previous_battery));
        device_status(status_ui.battery, sizeof(status_ui.battery), status_ui.wifi, sizeof(status_ui.wifi));
        int before = status_ui.battery_charging;
        status_ui.battery_charging = device_charging();
        status_ui.status_time = status_ui.charging_checked = now;
        return before != status_ui.battery_charging || strcmp(previous_wifi,status_ui.wifi) || strcmp(previous_battery,status_ui.battery);
    }
    if (now < status_ui.charging_checked || now - status_ui.charging_checked >= (status_ui.battery_charging ? 5 : 30)) {
        int before = status_ui.battery_charging;
        status_ui.battery_charging = device_charging();
        status_ui.charging_checked = now;
        return before != status_ui.battery_charging;
    }
    return 0;
}
#include "author_label.h"
#include "document_surface.h"
static int centered_lines(int x, int y, int width, const char *text, const Font *font, int limit) {
    int row = 0;
    while (*text && row < limit) {
        const char *start = text, *end = text, *space = NULL;
        int w = 0;
        while (*end && *end != '\n') {
            const char *prev = end;
            unsigned c = glyph(&end);
            if (w + glyph_advance(font, c) > width) {
                end = space && space > start ? space : prev;
                break;
            }
            w += glyph_advance(font, c);
            if (c == ' ')
                space = end;
        }
        if (end == start)
            glyph(&end);
        size_t n = end - start;
        while (n && start[n - 1] == ' ')
            n--;
        char line[1024];
        if (n >= sizeof(line))
            n = sizeof(line) - 1;
        memcpy(line, start, n);
        line[n] = 0;
        int actual = 0;
        const char *measure = line;
        while (*measure)
            actual += glyph_advance(font, glyph(&measure));
        drawtext_raw(x + (width - actual) / 2, y + row * (font->h - 4), line, font, width);
        row++;
        text = end;
        while (*text == ' ' || *text == '\n')
            text++;
    }
    return row;
}

static const char *update_error;
void ui_update_progress(int step,int percent,const char *version,const char *error) {
    memset(ui_pixels,255,sizeof(ui_pixels));
    widget_count=0;ui_y_offset=0;ink_color=0;
    type_label(40,104,520,62,"LUMEN Update",UI_TITLE);
    if(version && *version)type_label(40,175,520,36,version,UI_SECONDARY);
    static const char *steps[]={"Download vorbereiten","Update herunterladen",
        "Prüfsumme prüfen","Installation vorbereiten","Update installieren","LUMEN starten"};
    for(int i=0;i<6;i++) {
        char label[128];
        snprintf(label,sizeof(label),"%02d   %s",i+1,tr(steps[i]));
        const Font *font=ui_font(i==step ? UI_HEADING : UI_BODY);
        ink_color=i==step && !error ? 0 : 145;
        label_ellipsis(40,242+i*58,i==1 && step==1 ? 425 : 520,52,label,font,0);
        if(i==step && !error)label_ellipsis(41,242+i*58,i==1 ? 424 : 519,52,label,font,0);
        if(i==1 && step==1 && !error) {
            snprintf(label,sizeof(label),"%d%%",percent);
            label_ellipsis(480,242+i*58,80,52,label,font,0);
        }
    }
    ink_color=0;
    if(error) {
        wrapped(40,620,error,ui_font(UI_BODY),520,2);
        button_h(40,706,520,50,"Zurück",0);
    }
}
int ui_defer_app_frame(void) {
    enum { LOADING_HINT_MS=120 };
    static AppId waiting=-1;
    static int64_t until;
    AppId owner=active_app();
    if(updating || sleeping || editor_ui.keyboard || dropdown || !app_package_installed(owner) ||
       (app_process_owner()==(int)owner && *app_process_error()) || !app_process_open(owner) || !app_process_frame_loading()) {
        waiting=-1;
        return 0;
    }
    if(waiting!=owner) {
        waiting=owner;
        until=extra_millis()+LOADING_HINT_MS;
        widget_count=0;
        package_caret_visible=0;
        pressed=-1;
    }
    /* A ready frame wakes the event loop immediately. Only the fallback
       loading screen has a threshold, so Back does not clear the old page. */
    return extra_millis()<until;
}
void ui_draw(void) {
    package_caret_visible=0;
    if(updating) {
        int percent=0,step=release_progress(&percent);
        ui_update_progress(step,percent,release_version(),update_error);
        return;
    }
    widget_count = 0;
    pagination = (Pagination){0};
    if (editor_ui.keyboard && editor_ui.keyboard_background_valid18 && !sleeping) {
        memcpy(ui_pixels, editor_ui.keyboard_background18, sizeof(ui_pixels));
        if (active_app()!=document_provider())
            header();
        goto input_overlays18;
    }
    memset(ui_pixels, 255, sizeof(ui_pixels));
    if (sleeping) {
        sleep_surface_draw();
        return;
    }
    if(app_package_installed(active_app())) {
        package_ui_draw();
        if(!package_fullscreen)header();
        if(editor_ui.keyboard){memcpy(editor_ui.keyboard_background18,ui_pixels,sizeof(ui_pixels));editor_ui.keyboard_background_valid18=1;}
        goto input_overlays18;
    }
    if(active_app()==startup_app()) {
        heading("Startseite");
        wrapped(28,175,"Startseiten-App fehlt.",ui_font(UI_BODY),544,2);
        button_h(28,280,544,48,"Einstellungen",0);
        button_h(28,345,544,48,"Dateiübertragung",0);
        return;
    }
    if (active_app()!=document_provider())
        header();
    app_render();
    if (active_app()==document_provider() && reader_ui.reader_chrome)
        header();
    if (editor_ui.keyboard) {
        memcpy(editor_ui.keyboard_background18, ui_pixels, sizeof(ui_pixels));
        editor_ui.keyboard_background_valid18 = 1;
    }
input_overlays18:
    if(package_overlays_allowed())package_overlays_draw();
    overlays();
    extra_overlay();
    clipboard_draw();
    polish_draw();
    v1_effect();
    notification_draw();

    if (options[OPT_INVERT])
        for (size_t i = 0; i < sizeof(ui_pixels); i++)
            ui_pixels[i] = 255 - ui_pixels[i];
    ui_feedback_draw();
}
int ui_full_refresh(void) {
    int n = full;
    full = 0;
    return n;
}
static void turn(int delta) {
    if (selected < 0)
        return;
    int next = page + delta;
    if (next < 0 || next >= engine_count())
        return;
    if (delta > 0)
        apps_page_turned();
    page = next;
    panx = pany = 0;
    library_progress(&library[selected], page, engine_count());
    save_anchor(&library[selected], page, -1);
}
static void toggle_bookmark(void) {
    Book *b = &library[selected];
    for (int i = 0; i < b->bookmark_count; i++)
        if (b->bookmarks[i] == page) {
            memmove(&b->bookmarks[i], &b->bookmarks[i + 1],
                    (size_t)(b->bookmark_count - i - 1) * sizeof(int));
            for (int j = i; j < b->bookmark_count; j++) {
                char from[2048], to[2048];
                snprintf(to, sizeof(to), tr("%s/books/%016llx.mark%d"), data_root,
                         (unsigned long long)b->id, j);
                if (j + 1 < b->bookmark_count) {
                    snprintf(from, sizeof(from), tr("%s/books/%016llx.mark%d"), data_root,
                             (unsigned long long)b->id, j + 1);
                    if (rename(from, to))
                        unlink(to);
                } else
                    unlink(to);
            }
            b->bookmark_count--;
            library_save_book(b);
            return;
        }
    if (b->bookmark_count == 32) {
        ui_message("Maximal 32 Lesezeichen pro Buch in dieser Version.");
        return;
    }
    save_anchor(b, page, b->bookmark_count);
    b->bookmarks[b->bookmark_count++] = page;
    library_save_book(b);
}
static int ui_dispatch_input(const Input *in) {
    if(in->kind==INPUT_APP_SERVICE || in->kind==INPUT_STATUS)app_service_poll();
    if(in->kind==INPUT_STATUS) {
        document_excerpt_poll();
        app_periodic_poll(!sleeping && !updating);
        app_system_query_poll();
    }
    if(in->kind==INPUT_APP_SERVICE)return 2;
    if(in->kind==INPUT_STATUS && app_process_pending())app_process_flush();
    if(in->kind==INPUT_APP && updating){app_process_poll();return 2;}
    if(in->kind==INPUT_OVERLAY) {
        PackageProvider role=app_overlay_role(in->x);
        if(role==PROVIDER_ALARM)return app_overlay_poll(in->x) && !sleeping && !updating && extra_ringing() ? 1 : 2;
        if(role==PROVIDER_SLEEP)return app_overlay_poll(in->x) && sleeping ? 1 : 2;
        if(role==PROVIDER_TEXT_INPUT)
            return keyboard_package_poll() && editor_ui.keyboard && !updating && !sleeping ? 1 : 2;
        int changed=role==PROVIDER_STATUS ? status_overlay_poll() :
                    role==PROVIDER_NOTIFICATION ? notification_poll() : package_overlay_poll(in->x);
        return changed && !updating && !sleeping && !editor_ui.keyboard ? 1 : 2;
    }
    if(updating) {
        if(in->kind==INPUT_USB || in->kind==INPUT_EXIT)return 0;
        if(in->kind==INPUT_SLEEP){device_wake();return 2;}
        if(in->kind==INPUT_WAKE){full=1;return 1;}
        if(update_error && in->kind==INPUT_TAP && in->x>=40 && in->x<560 &&
           in->y>=706 && in->y<756) {
            updating=0;full=1;return 1;
        }
        if(!update_error && in->kind==INPUT_STATUS) {
            int changed=release_poll();
            if(release_state()==4) {
                if(release_activate()) {
                    ui_update_progress(4,100,release_version(),NULL);
                    device_present(ui_pixels,0,settings.refresh);
                    return 0;
                }
                update_error="Neustart konnte nicht vorbereitet werden.";
            } else if(release_state()!=1) {
                update_error=*release_error() ? release_error() : "Update nicht verfügbar.";
            }
            if(update_error){device_hold_awake(0);return 1;}
            return changed ? 1 : 2;
        }
        return 2;
    }
    if (in->kind == INPUT_SLEEP) {
        if(sleeping)return 2;
        int handled=polish_power(in);
        if(handled)return 1;
        pressed=-1;
        press_canceled=0;
        editor_ui.pressed_key=-1;
        package_input_checkpoint();
        package_pointer_reset();
        document_excerpt_close();
        app_process_pause();
        apps_pause();
        extra_save();
        service_cancel();
        reader_ui.selecting = 0;
        battery_popup = 0;
        connect_stop();
        ui_save();
        sleeping=1;
        full=1;
        return 1;
    }
    if (in->kind == INPUT_WAKE) {
        full=1;
        if (sleeping) {
            pressed=-1;
            press_canceled=0;
            editor_ui.pressed_key=-1;
            caret_restart();
            activity16=editor_ui.cursor_epoch;
            sleeping = 0;
            device_status(status_ui.battery, sizeof(status_ui.battery), status_ui.wifi, sizeof(status_ui.wifi));
            status_ui.battery_charging = device_charging();
            status_ui.status_time = status_ui.charging_checked = time(NULL);
            extra_tick();
        }
        return 1;
    }
    if (in->kind == INPUT_CANCEL) {
        package_pointer_input(in);
        notification_input(in,0);
        package_overlays_input(in);
        pressed = -1;
        press_canceled = 0;
        editor_ui.pressed_key = -1;
        return 1;
    }
    int visible=notification_visible();
    if(notification_input(in,visible)) { pressed=-1; return 1; }
    if(package_overlays_allowed() &&
       package_overlays_input(in)){pressed=-1;return 2;}
    int pm = polish_power(in);
    if (pm)
        return pm == 2 ? 0 : pm == 3 ? 3 : 1;
    if (battery_popup) {
        if (in->kind == INPUT_TAP || in->kind == INPUT_SWIPE) {
            battery_popup = 0;
            return 1;
        }
        if (in->kind == INPUT_DOWN || in->kind == INPUT_MOVE || in->kind == INPUT_HOLD)
            return 2;
    }
    int input18 = keyboard_intercept18(in);
    if (input18 >= 0)
        return input18;
    int back_release = pressed >= 0 && press_bounds.system && press_bounds.x == 0 &&
                       press_bounds.y == 0 && press_bounds.w == 62 && in->y < STATUS_HEIGHT+8 && in->x < 70;
    int status_tap = in->kind == INPUT_TAP && (in->y < STATUS_HEIGHT || back_release) &&
                     (active_app()!=document_provider() || reader_ui.reader_chrome || app_package_installed(document_provider()));
    int system_pager = !sleeping && !editor_ui.keyboard && !dropdown &&
                       pagination_hit(in);
    if(!sleeping && !editor_ui.keyboard && !dropdown && !status_tap && !system_pager &&
       app_package_installed(active_app()) && package_pointer_input(in)) {
        pressed=-1;
        return 2;
    }

    if (!sleeping && clipboard_input(in))
        return 1;
    if (in->kind == INPUT_APP) {
        if (sleeping) { app_process_poll(); return 2; }
        {int changed=package_ui_poll();return package_exit_requested ? 0 : changed ? 1 : 2;}
    }
    if (in->kind == INPUT_DOWN || in->kind == INPUT_TAP || in->kind == INPUT_SWIPE ||
        in->kind == INPUT_WAKE)
        activity16 = extra_millis();
    Input normalized;
    if (!editor_ui.keyboard && in->kind == INPUT_TAP)
        apps_checkpoint();
    if (in->kind == INPUT_STATUS && sleeping)
        return v1_sleep_tick() ? 1 : 2;
    if (in->kind == INPUT_STATUS) {
        package_clock_tick();
        int blink=caret_tick();
        if (refresh_status(in->x == -1))
            return 1;
        u16_tick();
        int action = extra_tick();
        if (action == 1)
            return 1;
        if(blink)return 1;
        int needs_status=package_clock_wait()>=0;
        if(action && !needs_status)return action;
        if(service_busy() && !needs_status)return 2;
    }
    int intercepted = extra_intercept(in);
    if (intercepted >= 0)
        return intercepted;

    if (!sleeping && active_app()==document_provider() && package_document_visible && in->kind == INPUT_HOLD && !reader_ui.reader_chrome && in->y > 60 &&
        in->y < 735) {
        reader_ui.selection_last_x = reader_ui.selection_x = in->x;
        reader_ui.selection_last_y = reader_ui.selection_y = in->y;
        if (engine_select(page, in->x - 28, in->y - 12, in->x - 27, in->y - 11, 544, 728, zoom,
                          panx, pany, &reader_ui.selection))
            reader_ui.selecting = 1;
        else {
            selection_release = 1;
            ui_message("Kein auswählbarer Text an dieser Stelle.");
        }
        pressed = -1;
        return 1;
    }
    if (selection_release && (in->kind == INPUT_TAP || in->kind == INPUT_SWIPE)) {
        selection_release = 0;
        return 1;
    }
    if (active_app()==document_provider() && reader_ui.selecting == 1 &&
        (in->kind == INPUT_MOVE || in->kind == INPUT_TAP || in->kind == INPUT_SWIPE)) {
        if (in->kind == INPUT_MOVE && abs(in->x - reader_ui.selection_last_x) < 7 &&
            abs(in->y - reader_ui.selection_last_y) < 7)
            return 2;
        reader_ui.selection_last_x = in->x;
        reader_ui.selection_last_y = in->y;
        engine_select(page, reader_ui.selection_x - 28, reader_ui.selection_y - 12, in->x - 28, in->y - 12, 544, 728,
                      zoom, panx, pany, &reader_ui.selection);
        if(in->kind!=INPUT_MOVE) {
            reader_ui.selecting=reader_ui.selection.count ? 2 : 0;
            if(reader_ui.selecting==2) {
                char *event=NULL;size_t length=0;
                FILE *stream=open_memstream(&event,&length);
                if(stream) {
                    fputs("{\"event\":\"intent\",\"operation\":\"document-selection\",\"text\":",stream);
                    json_escape16(stream,reader_ui.selection.text);
                    fputc('}',stream);
                    if(!fclose(stream))app_process_send(event);
                    free(event);
                }
            }
        }
        pressed = -1;
        return 1;
    }

    if (in->kind == INPUT_DOWN) {
        if (sleeping)
            return 2;
        press_canceled = 0;
        down_y = in->y;
        pressed = hit(in->x, in->y);
        if (pressed >= 0)
            press_bounds = widgets[pressed];
        return pressed >= 0 && !press_bounds.quiet ? 3 : 2;
    }
    if (in->kind == INPUT_MOVE) {
        if (pressed >= 0 &&
            (in->x < press_bounds.x - 8 || in->x >= press_bounds.x + press_bounds.w + 8 ||
             in->y < press_bounds.y - 8 || in->y >= press_bounds.y + press_bounds.h + 8)) {
            pressed = -1;
            press_canceled = 1;
            return 1;
        }
        return 2;
    }
    if (in->kind == INPUT_TAP) {
        int was = pressed;
        pressed = -1;
        if (press_canceled) {
            press_canceled = 0;
            return 1;
        }
        if (was >= 0 &&
            (in->x < press_bounds.x - 8 || in->x >= press_bounds.x + press_bounds.w + 8 ||
             in->y < press_bounds.y - 8 || in->y >= press_bounds.y + press_bounds.h + 8))
            return 1;
        if (was >= 0 && press_bounds.system && press_bounds.x == 0 && press_bounds.y == 0 &&
            press_bounds.w == 62) {
            normalized = *in;
            if (normalized.x > 61)
                normalized.x = 61;
            if (normalized.y >= STATUS_HEIGHT)
                normalized.y = STATUS_HEIGHT-1;
            in = &normalized;
        }
    } else if (in->kind == INPUT_SWIPE) {
        pressed = -1;
        if (active_app()==document_provider() ? (down_y < 48 || down_y >= 752) : (down_y < 130 || down_y >= 700))
            return 1;
    }
    if (!sleeping && !editor_ui.keyboard && !dropdown && !battery_popup &&
        pagination.state) {
        if (in->kind == INPUT_SWIPE && abs(in->dx) > abs(in->dy) &&
            (active_app()!=document_provider() || (options[OPT_SWIPE] && zoom <= 1))) {
            pagination_move(in->dx < 0 ? 1 : -1);
            return 1;
        }
        if (pagination_hit(in)) {
            int target = pager_target(in->x,PAGER_X,PAGER_W,*pagination.state,pagination.count);
            if (target != *pagination.state) pagination_move(target-*pagination.state);
            return 1;
        }
    }
    if (in->kind == INPUT_TAP && !sleeping)
        device_activity();
    if (in->kind == INPUT_TAP) {
        caret_restart();
    }
    if(in->kind==INPUT_STATUS)return 2;
    if (in->kind == INPUT_NETWORK) {
        char books[2048];
        snprintf(books,sizeof(books),"%s/books-changed",data_root);
        if(!unlink(books)) {
            library_dirty=1;
            app_process_send("{\"event\":\"refresh\"}");
        }
        app_packages_scan();
        if(active_app()==startup_app() &&
           app_package_installed(active_app()))app_process_send("{\"event\":\"refresh\"}");
        char restart[2048];
        snprintf(restart,sizeof(restart),"%s/../.update/relaunch",data_root);
        if (!access(restart,F_OK)) {
            connect_stop();
            ui_save();
            return 0;
        }
        return 1;
    }
    if (in->kind == INPUT_EXIT || in->kind == INPUT_USB) {
        connect_stop();
        service_cancel();
        ui_save();
        return 0;
    }
    if (in->kind == INPUT_POWER)
        return 2;
    if (sleeping)
        return 2;
    int x = in->x, y = in->y;
    if(app_package_installed(active_app()) && !editor_ui.keyboard && !dropdown &&
       ((in->kind==INPUT_TAP && (y>=STATUS_HEIGHT ||
          (package_fullscreen && package_fullscreen_owner==active_app()))) || in->kind==INPUT_SWIPE || in->kind==INPUT_HOLD))return package_ui_input(in);
    if(in->kind==INPUT_TAP && y<STATUS_HEIGHT && !editor_ui.keyboard &&
       !dropdown && !battery_popup && status_action(in))return 1;
    if (in->kind == INPUT_TAP && y < STATUS_HEIGHT && (active_app()!=document_provider() || reader_ui.reader_chrome || app_package_installed(document_provider()))) {
        if(x<62 && app_package_installed(active_app()) && app_process_frame_back() && !editor_ui.keyboard && !dropdown){app_process_send("{\"event\":\"back\"}");return 2;}
        if (x < 62 && can_back()) {
            apps_pause();
            if (editor_ui.keyboard || dropdown) {
                if (editor_ui.keyboard && keyboard_multiline())
                    commit_keyboard();
                editor_ui.keyboard = dropdown = 0;
                return 1;
            }
            AppId target=startup_app();
            while (history_count) {
                AppId candidate=history[--history_count];
                if (candidate != active_app() && app_package_foreground(candidate)) {
                    target = candidate;
                    break;
                }
            }
            activate_app(target);
            package_back_results();
            if (active_app()==document_provider() && current<0 && !app_package_foreground(document_provider()))
                activate_app(books_provider());
            return 1;
        }
    }
    if (editor_ui.keyboard || dropdown) {
        modal_input(in);
        return 1;
    }
    return app_handle_input(in);
}
int ui_input(const Input *in) {
    AppId from=active_app();
    int package_keyboard=editor_ui.keyboard && editor_ui.keyboard_target==2000;
    int result = ui_dispatch_input(in);
    int attached=editor_ui.keyboard && editor_ui.keyboard_target==2000;
    if(attached!=package_keyboard && from==active_app() && app_package_installed(from))
        app_process_send(attached ? "{\"event\":\"viewport\",\"bottom\":395}" :
                                    "{\"event\":\"viewport\",\"bottom\":800}");
    return result;
}

/* Shared overlays: dropdowns, keyboard and destructive-action confirmation. */
#include "text_editor.h"
#include "keyboard_surface.h"
static void overlays(void) {
    if (editor_ui.keyboard || dropdown) {
        int keep = 0;
        for (int i = 0; i < widget_count; i++)
            if (widgets[i].system)
                widgets[keep++] = widgets[i];
        widget_count = keep;
    }
    if (editor_ui.keyboard) {
        keyboard_draw18();
    }
    ui_y_offset = 0;
    if (dropdown) {
        int height = drop_visible * 48 + 10 + (drop_count > drop_visible ? 44 : 0);
        if (drop_y + height > 790)
            drop_y = 790 - height;
        control_surface(drop_x, drop_y, drop_w, height, RADIUS_LARGE, 0);
        control_surface(drop_x + 2, drop_y + 2, drop_w - 4, height - 4, RADIUS_LARGE-2, 255);
        for (int row = 0; row < drop_visible && drop_first + row < drop_count; row++) {
            int i = drop_first + row;
            int y = drop_y + 5 + row * 48;
            region(drop_x + 5, y, drop_w - 10, 47);
            icon_sized(drop_x + 10, y + 10,
                       dropdown_option_icon(drop_target, i, drop_labels[i]), 28);
            if (i == drop_selected) {
                stroke(drop_x + drop_w - 29, y + 24, drop_x + drop_w - 24, y + 29, 2);
                stroke(drop_x + drop_w - 24, y + 29, drop_x + drop_w - 15, y + 18, 2);
            }
            label_ellipsis(drop_x + 46, y, drop_w - 85, 48,
                           tr(drop_labels[i]), &small, 0);
            if (row + 1 < drop_visible)
                rect(drop_x + 10, y + 47, drop_w - 20, 2, 190);
        }
        if (drop_count > drop_visible) {
            int yy = drop_y + 5 + drop_visible * 48;
            pager_draw(drop_x + (drop_w - 189) / 2, yy, 189, drop_first / drop_visible, (drop_count + drop_visible - 1) / drop_visible);
        }
    }
    if (battery_popup) {
        control_surface(428, 50, 148, 84, RADIUS_MEDIUM, 255);
        key_outline(428, 50, 148, 84);
        char b[32];
        snprintf(b, sizeof(b), "%s%%", status_ui.battery);
        centered(430, 58, 144, 64, b, &body);
    }
}
static void refresh_library(void) {
    int transient=current==BOOK_LIMIT && reader_ui.reader_document_active;
    BookId opened=current>=0 ? library[current].id : 0;
    BookId focused=selected>=0 ? library[selected].id : 0;
    if(current>=0 && !transient) {
        library_progress(&library[current],page,engine_count());
        save_anchor(&library[current],page,-1);
    }
    library_scan();
    if(!transient) {
        current=selected=-1;
        for(int i=0;i<library_count;i++) {
            if(opened && library[i].id==opened)current=i;
            if(focused && library[i].id==focused)selected=i;
        }
        if(opened && current<0) {
            engine_close();
            reader_ui.selecting=0;
            document_excerpt_close();
        }
    }
    library_dirty=0;
}
static void commit_keyboard(void) {
    if (editor_ui.keyboard_target >= 30) {
        int previous_target = editor_ui.keyboard_target;
        package_ui_commit(editor_ui.keyboard_target,editor_ui.edit);
        if (editor_ui.keyboard_target == previous_target) {
            memset(editor_ui.edit, 0, sizeof(editor_ui.edit));
            editor_ui.keyboard = 0;
        }
        pressed = -1;
        return;
    }
    editor_ui.keyboard = 0;
    pressed = -1;
}
static size_t keyboard_capacity(void) {
    return text_input_spec(editor_ui.keyboard_target).capacity;
}
static void modal_input(const Input *in) {
    if (in->kind == INPUT_SWIPE && editor_ui.keyboard && keyboard_multiline() && down_y >= 54 &&
        down_y < 502) {
        editor_scroll18(in->dy > 0 ? -8 : 8);
        return;
    }
    if (in->kind != INPUT_TAP)
        return;
    int x = in->x, y = in->y;
    if (dropdown) {
        unsigned generation = drop_generation;
        if (drop_count > drop_visible && x >= drop_x && x < drop_x + drop_w &&
            y >= drop_y + 5 + drop_visible * 48 && y < drop_y + 5 + drop_visible * 48 + 44) {
            int target = pager_target(x,drop_x+(drop_w-PAGER_W)/2,PAGER_W,
                                      drop_first/drop_visible,(drop_count+drop_visible-1)/drop_visible);
            if (target >= 0) drop_first = target*drop_visible;
            return;
        }
        if (x >= drop_x && x < drop_x + drop_w && y >= drop_y + 5 &&
            y < drop_y + 5 + drop_visible * 48) {
            int choice = drop_first + (y - drop_y - 5) / 48;
            if (choice >= drop_count) {
                dropdown = 0;
                return;
            }
            if(drop_target==2001)package_ui_choice(choice);
        }
        if (generation == drop_generation)
            dropdown = 0;
        return;
    }
    if (editor_ui.keyboard) {
        keyboard_tap18(x, y);
        return;
    }
}
void ui_loading(const char *message) {
    widget_count = 0;
    memset(ui_pixels, 255, sizeof(ui_pixels));
    int w = textwidth("LUMEN", ui_font(UI_DISPLAY));
    drawtext((600 - w) / 2, 285, "LUMEN", ui_font(UI_DISPLAY), 560);
    w = textwidth(message, ui_font(UI_BODY));
    drawtext(w < 560 ? (600 - w) / 2 : 20, 370, message, ui_font(UI_BODY), 560);
}
#include "network_state.h"

int ui_poll_interval(void) {
    if(updating)return update_error ? -1 : 400;
    if (sleeping)
        return v1_sleep_poll();
    int wait=-1,other=extra_poll();
    int tick=package_clock_wait();
    if(tick>=0 && (other<0 || tick<other))other=tick;
    int blink=caret_wait();
    if(blink>=0 && (other<0 || blink<other))other=blink;
    int context=document_excerpt_wait();
    if(context>=0 && (other<0 || context<other))other=context;
    int vp=v1_poll();
    if(vp>=0 && (other<0 || vp<other))other=vp;
    int status_wait = status_ui.battery_charging ? 5000 : 30000;
    if (wait < 0 || status_wait < wait)
        wait = status_wait;
    return other >= 0 && (wait < 0 || other < wait) ? other : wait;
}

#include "document_annotations.h"
#include "runtime_surfaces.h"
#include "power_menu.h"
#include "sleep_surface.h"
#include "alarm_surface.h"
void ui_paint_heading(const char *text, int actions) { heading_actions(text, actions); }
void ui_paint_icon_button(int x, int y, int id) { icon_button(x, y, id); }
void ui_paint_surface(int x,int y,int w,int h,int radius,int gray,int outline) {
    /* Protocol 1 packs small black border widths and grayscale border tones
       into one field. A tone is never an inset. */
    if ((unsigned)gray > 255 || outline < -1 || outline > 255)
        return;
    if (outline <= 0 || outline == gray) {
        control_surface(x,y,w,h,radius,gray);
        return;
    }
    int width = outline <= 3 ? outline : 2;
    int color = outline <= 3 ? 0 : outline;
    control_surface(x,y,w,h,radius,color);
    control_surface(x+width,y+width,w-2*width,h-2*width,radius-width,gray);
}
void ui_paint_hitbox(int x, int y, int w, int h) { region(x, y, w, h); }
void ui_paint_text(int x, int y, int w, int h, const char *text, PaintText role) {
    static const TextRole roles[] = {UI_BODY, UI_HEADING, UI_DISPLAY, UI_SECONDARY};
    if ((unsigned)role < sizeof(roles)/sizeof(roles[0]))
        type_centered(x, y, w, h, text, roles[role]);
}
void ui_paint_line(int x0, int y0, int x1, int y1, int weight, int gray) {
    stroke_color(x0, y0, x1, y1, weight, gray);
}
void ui_paint_ellipse(int x, int y, int rx, int ry, int gray) {
    if (rx < 1 || ry < 1) return;
    int edge = rx < ry ? rx : ry;
    for (int dy = -ry-1; dy <= ry+1; dy++) {
        float v = (float)dy / ry;
        float v2 = v*v;
        for (int dx = -rx-1; dx <= rx+1; dx++) {
            float u = (float)dx / rx;
            float alpha = (1 - u*u - v2) * edge * .5f + .5f;
            if (alpha > 0) blend_pixel(x+dx, y+dy, alpha, gray);
        }
    }
}
void ui_paint_ring(int x, int y, int radius, int weight) { ring(x, y, radius, weight); }
void ui_paint_image(int x, int y, int w, int h, const unsigned char *gray) {
    if (x < 0 || y < 0 || w <= 0 || h <= 0 || x+w > WIDTH || y+h > HEIGHT) return;
    for (int row = 0; row < h; row++)
        memcpy(ui_pixels+(y+row)*WIDTH+x, gray+row*w, (size_t)w);
}
int64_t ui_clock_millis(void) { return extra_millis(); }
int ui_prepare_home(void) {
    if(app_package_deleted(startup_app()))return 1;
    if(!app_process_open(startup_app()))return 0;
    int64_t until=extra_millis()+1800;
    do {
        package_ui_poll();
        if(app_process_frame_ready())return 1;
        if(*app_process_error())return 0;
        struct pollfd fd={.fd=app_process_fd(),.events=POLLIN};
        poll(&fd,1,20);
    } while(extra_millis()<until);
    return 0;
}

static void app_leave(AppId app, void *ctx) {
    package_overlays_leave(app);
    package_input_checkpoint();
    if(editor_ui.autosave)editor_ui.keyboard=0;
    package_pointer_reset();
    app_process_pause();
    (void)ctx;
    if (app == document_provider()) {
        reader_ui.selecting=0;
        document_excerpt_close();
        apps_checkpoint();
        ui_save();
    }
}
static int activate_app(AppId app) {
    if(app!=startup_app() && !app_package_foreground(app))return 0;
    if(reader_ui.reader_document_active && current==BOOK_LIMIT && app!=document_provider()) {
        library_progress(&library[current],page,engine_count());
        save_anchor(&library[current],page,-1);
        engine_close();
        current=selected=-1;
        reader_ui.reader_document_active=0;
    }
    app_runtime_activate(&runtime,app);
    return 1;
}
static void show_library_detail(int index) {
    if(index<0 || index>=library_count)return;
    selected=index;
    if(activate_app(books_provider()) && app_process_open(books_provider())) {
        char event[128];
        snprintf(event,sizeof(event),
                 "{\"event\":\"intent\",\"operation\":\"book-detail\",\"text\":\"%016llx\"}",
                 (unsigned long long)library[index].id);
        app_process_send(event);
    }
}
static int app_send(const AppIntent *intent) {
    if (!intent || intent->app<0 || intent->kind < INTENT_OPEN ||
        intent->kind > INTENT_SEARCH)
        return 0;
    if (intent->kind == INTENT_BOOK && intent->app != document_provider())
        return 0;
    if (intent->app == document_provider() && intent->kind == INTENT_OPEN && !app_package_installed(document_provider())) {
        int i = current >= 0 ? current : library_last();
        if (i >= 0)
            return app_read_book(i);
        AppIntent fallback = {.app = books_provider(), .kind = INTENT_OPEN};
        return app_send(&fallback);
    }
    AppIntent owned = *intent;
    owned.text[sizeof(owned.text) - 1] = 0;
    owned.position.quote[sizeof(owned.position.quote) - 1] = 0;
    intent = &owned;
    if (intent->kind == INTENT_BOOK) {
        for (int i = 0; i < library_count; i++)
            if (library[i].id == intent->object_id) {
                int old = active_app();
                if (!ui_open(i))
                    return 0;
                if (old != document_provider() && history_count < 64)
                    history[history_count++] = old;
                if (intent->has_position)
                    page = engine_restore(&intent->position);
                runtime.intent = *intent;
                runtime.intent.text[sizeof(runtime.intent.text) - 1] = 0;
                return 1;
            }
        ui_message("Buch nicht mehr in der Bibliothek.");
        return 0;
    }
    AppId previous = active_app();
    int old = active_app();
    if(!activate_app(intent->app))return 0;
    runtime.intent=*intent;
    if (previous != intent->app && history_count < 64)
        history[history_count++] = old;
    if(app_package_installed(intent->app)){
        if(!app_process_open(intent->app))return 0;
        char *event=NULL;
        size_t size=0;
        FILE *stream=open_memstream(&event,&size);
        if(!stream)return 0;
        fprintf(stream,"{\"event\":\"intent\",\"kind\":%d,\"object\":%llu,\"text\":",
                intent->kind,(unsigned long long)intent->object_id);
        json_escape16(stream,intent->text);
        const char *operation=intent->kind==INTENT_SEARCH ? "search" : intent->text;
        if(*operation) {
            fputs(",\"operation\":",stream);
            json_escape16(stream,operation);
        }
        fputc('}',stream);
        int ok=!fclose(stream) && app_process_send(event);
        free(event);
        return ok;
    }
    app_process_pause();
    return 0;
}

static void app_render(void) {
    heading("Apps");
    wrapped(28,175,"App-Paket fehlt.",ui_font(UI_BODY),544,2);
}
static int app_handle_input(const Input *in) {
    if(active_app()==startup_app() && in->kind==INPUT_TAP && in->x>=28 && in->x<572) {
        if(in->y>=280 && in->y<328) {
            AppIntent intent={.app=app_package_provider_id(PROVIDER_DEVICE_CONTROL),.kind=INTENT_OPEN};
            app_send(&intent);
        } else if(in->y>=345 && in->y<393) {
            AppIntent intent={.app=app_package_provider_id(PROVIDER_FILE_TRANSFER),.kind=INTENT_OPEN};
            app_send(&intent);
        }
    }
    return 1;
}

int ui_feedback_active(void) {
    return pressed>=0 && pressed<widget_count && !widgets[pressed].quiet && !sleeping && !updating;
}
void ui_feedback_draw(void) {
    if(!ui_feedback_active())return;
    Widget r=press_bounds;
    if(r.paint_w>0 && r.paint_h>0) {
        r.x=r.paint_x;r.y=r.paint_y;r.w=r.paint_w;r.h=r.paint_h;
    }
    int inset=r.w<28 || r.h<28 ? 1 : 2;
    int w=r.w-2*inset,h=r.h-2*inset;
    if(w<8 || h<8)return;
    /* Keep a rounded fallback even when square surfaces are selected. */
    int radius=r.radius-inset;
    if(radius<4)radius=4;
    int profile=rounding_profile();
    if(!profile)profile=CORNER_SUPERELLIPSE;
    if(radius>MAX_CORNER_RADIUS)radius=MAX_CORNER_RADIUS;
    if(radius>w/2)radius=w/2;
    if(radius>h/2)radius=h/2;
    if(radius*2>=(w<h ? w : h)-1)profile=CORNER_CIRCLE;
    const unsigned char *mask=corner_mask(radius,profile);
    int x0=r.x+inset,y0=r.y+inset;
    for(int y=0;y<h;y++) {
        if((unsigned)(y+y0)>=HEIGHT)continue;
        int corner_y=y<h/2 ? y : h-1-y;
        int cut=0;
        if(mask && corner_y<radius)
            while(cut<radius && mask[corner_y*radius+cut]<CORNER_ALPHA/2)cut++;
        int left=r.sides&1 ? cut : 0,right=w-(r.sides&2 ? cut : 0);
        if(left+x0<0)left=-x0;
        if(right+x0>WIDTH)right=WIDTH-x0;
        for(int x=left;x<right;x++) {
            unsigned char *pixel=&ui_pixels[(y+y0)*WIDTH+x+x0];
            /* Leave gray edges and artwork intact so touch-down stays binary. */
            if(*pixel==0 || *pixel==255)*pixel=255-*pixel;
        }
    }
}

static int app_read_book(int index) {
    if (index < 0 || index >= library_count)
        return 0;
    AppIntent request = {.app = document_provider(), .kind = INTENT_BOOK, .object_id = library[index].id};
    return app_send(&request);
}
void ui_close(void) {
    document_excerpt_close();
    app_process_close();
    release_cancel();
    cover_tiles_clear();
    v1_close();
    ui_unicode_close();
}

int ui_power_request(void) {
    return power_request;
}

void ui_power_screen(int restart) {
    widget_count = 0;
    memset(ui_pixels, 255, sizeof(ui_pixels));
    icon(278, 253, restart ? IC_REBOOT : IC_POWER);
    centered(28, 325, 544, 75, restart ? "Kindle startet neu …" : "Kindle wird ausgeschaltet …",
             &editorial);
}
