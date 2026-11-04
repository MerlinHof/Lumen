#define _GNU_SOURCE
#include "api.h"
#include "../../apps/notifications/api.h"
#include "../amazon.h"
#include "../../apps/games/api.h"
#include "../../apps/calculator/api.h"
#include "../connect.h"
#include "../content_service.h"
#include "../engine.h"
#include "font_data.h"
#include "../i18n.h"
#include "../library.h"
#include "../network.h"
#include "../../apps/news/archive_api.h"
#include "../qrcodegen.h"
#include "../services.h"
#include "../app_runtime.h"
#include "../paper_font.h"
#include "../unicode.h"
#include "../text_edit.h"
#include "../text_input.h"
#include "../preferences.h"
#include <ctype.h>
#include <math.h>
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
static AppRuntime runtime;
static void app_leave(AppId app, void *ctx);
static void route_to(int route);
static int app_send(const AppIntent *intent);
static int app_read_book(int index);
static void extra_prepare(int target);
static int app_handle_input(const Input *in);
static void app_render(void);
static int reader_document_active;
static int wiki_article_language = -1;
static int screen = 1, selected = -1, current = -1, view[BOOK_LIMIT], view_count, listpage,
           chapterpage, chapter_count, chaptertab;
static int battery_popup, wifi_enabled = 1, setting_page, reading_page;
static int status_wifi_x = -1, status_battery_x = -1, status_battery_right, status_timer_x = -1,
           status_timer_width;
static int wifi_page, wifi_selected = -1, wifi_scanning, wifi_exit;
static char wifi_ssid[128];
static int store_stage, store_total, store_federated;
static OPDSItem store_results[OPDS_LIMIT];
static char store_urls[12][2048];
static int store_home = 1;
static int store15_grid[4] = {-1, -1, -1, -1};
static int reader_progress_mode;
static pid_t job_pid16 = -1;
static int ink_pending16(void), job_pending16(void);
static void help_overlay16(void), companion_checkpoint16(void),
    companion_close16(void);
static int u17_notes_depth(void), text_editable_hit(const Input *);
static int note_rename_x17;
static void u17_calendar_dirty(void);
static void small_number17(int, int, const char *, int);
static void u17_gray_icon(int, int);
static void models_parse17(void), reader_markdown17(const char *, const char *, int);
static void chat_write_request(FILE *, const char *, const char *);
static void chat_complete(const char *);
static void chat_clear(void);
static void chat_draw(void), chats_draw(void);
static int chat_input(const Input *), chats_input(const Input *);
static int recipes_back(void);
static int recipes_draw17(void), recipes_input17(const Input *), cross_commit17(const char *);
static void games_save17(void), journal_store17(void),
    u17_game_manual(int, const char *, const char *);
static int u17_games_draw(void), u17_games_input(const Input *), recipes_fetch17(const char *);
static void recipes_search17(const char *), recipes_parse17(void), recipes_open_ready17(void);
static void u17_more_choice(int, int);
static int u17_more_input(const Input *), u17_more_draw(void), u17_more_commit(int, const char *);
static void u17_notes_up(void), u17_sleep_row(int), u17_sleep_select(int);
static void u17_choice(int, int), u17_notes_menu(int, int, int), checkbox(int, int, int),
    select_row17(int, const char *, const char *);
static int u17_commit(int, const char *), u17_input(const Input *), u17_draw(void);
static int task_edit17 = -1, notes_kind17, notes_id17;
static int u16_apps_commit(int, const char *), event_on16(int);
static void u16_calendar_choice(int);
static int keyboard_multiline(void);
static void editor_reset18(void);
static void tasks_draw18(void);
static int tasks_input18(const Input *in);
static int keyboard_intercept18(const Input *in);
static int u16_apps_draw(void), u16_apps_input(const Input *), u16_jobs_tick(void);
static int task_matches16(int);
static void note_delete16(int);
static char task_categories16[256][64], task_context16[64];
static int category_task16 = -1, calc_precision16 = 10;
static int u16_input(const Input *), u16_draw(void), u16_commit(int, const char *), u16_tick(void);
static void u16_dropdown(int, int), u16_notes_defaults(void);
static int u15_input(const Input *), u15_draw(void);
static int u15_commit(int, const char *);
static int u15_note_page_count(void);
static void u15_flush(void);
static void u15_note_editor_draw(void), u15_note_editor_touch(int, int), u15_notes_action(int, int),
    u16_notes_defaults(void);
static void u15_sources_prepare(const char *);
static void u15_init(void), u15_note_overlay(void), u15_note_tools(void), u15_reader_progress(void);
static int store_provider_count = 2;
static int store_complete(int done);
static void store_restore(void);
static char store_query[256];
static int opds_page, opds_selected, opds_depth, library_dirty;
static char opds_history[16][2048];
static char opds_source[2048] =
    "https://www.gutenberg.org/ebooks/search.opds/?query=l.de&sort_order=downloads";
static int page, full = 1, sleeping, reader_chrome;
static float zoom = 1, panx, pany;
static Chapter chapters[256];
typedef struct {
    ReaderPosition position;
    char text[2048];
} Annotation;
static Annotation annotations[128];
static int annotation_count, annotation_page, selecting, selection_x, selection_y,
    annotation_delete, selection_last_x, selection_last_y, selection_release;
static int annotations_write(void);
static TextSelection selection;
static TextSelection saved_shading;
static int shading_page = -1, shading_notes = -1, layout_epoch, shading_epoch = -1;
static float shading_zoom, shading_panx, shading_pany;
static void annotations_load(void);
static void annotations_draw(void);
static void annotations_screen(void);
static void wikipedia_screen(void);
static void selection_overlay(void);

static int cursor_visible = 1;
static int64_t cursor_epoch;
static char query[128] = "", battery[24] = "", wifi[32] = "";
static time_t status_time;
static int battery_charging;
static time_t charging_checked;
typedef struct {
    int x, y, w, h, system;
} Widget;
static Widget widgets[1024], press_bounds;
static int widget_count, pressed = -1, press_canceled, down_y, history[64], history_count;
static int keyboard, keyboard_target, keyboard_shift, keyboard_symbols, dropdown, drop_target,
    drop_count, drop_selected, drop_first, drop_visible;
static unsigned drop_generation;
static int drop_x, drop_y, drop_w, setting_category, confirm_delete;
static const char **drop_labels;
static char edit[8192] = "";
static int note_cursor, note_view_start, note_scroll_lock;
static int keyboard_view18, accent_count18, accent_x18, clip_active;
static int keyboard_background_valid18;
static unsigned char keyboard_background18[WIDTH * HEIGHT];
static char accent_keys18[10][8];
static size_t keyboard_capacity(void);
static const char *keyboard_title = "Suchen";
static int hit(int x, int y) {
    for (int i = widget_count - 1; i >= 0; i--)
        if (x >= widgets[i].x && x < widgets[i].x + widgets[i].w && y >= widgets[i].y &&
            y < widgets[i].y + widgets[i].h)
            return i;
    return -1;
}
static void region(int x, int y, int w, int h) {
    if (widget_count < 1000)
        widgets[widget_count++] = (Widget){x, y + ui_y_offset, w, h, 0};
}
static void system_region(int x, int y, int w, int h) {
    int count = widget_count;
    region(x, y, w, h);
    if (widget_count > count)
        widgets[count].system = 1;
}
static int textwidth(const char *s, const Font *f);
static void notification_draw(void);
static int notification_visible(void);
static void overlays(void);
void ui_feedback_draw(void);
static void modal_input(const Input *in);
static void connect_input(const Input *in);
static void refresh_library(void);
static void settings_input(const Input *in);
static void discover_screen(void);
static void discover_detail(void);
static void discover_input(const Input *in);
static void wifi_open(void);
static void wifi_screen(void);
static void wifi_input(const Input *in);
static void settings_home(void);
static void reading_input(const Input *in);
static void apps_init(void);
static void apps_draw(void);
static int apps_input(const Input *in);
static int apps_poll_interval(void);
static void apps_pause(void);
static void apps_checkpoint(void);
static void apps_save(void);
static void apps_note_commit(const char *text);
static void apps_page_turned(void);
static void open_store(void);
static void store_search(void);
static int tasks_fullscreen;
static int has_tabs(void) {
    return (screen == 2 && reader_chrome) || !app_route_fullscreen(screen);
}
static void turn(int);
static void system_loading(int);
static void polish_wiki_history(void);
static int wiki_history_input(const Input *);
static int wiki_refresh_failed(void);
static void wiki_history_scan(void);
static void wiki_history_current(void);
static void wiki_article_opened(void);
static void extra_init(void);
static void extra_draw(void);
static int extra_input(const Input *in);
static int extra_poll(void);
static int extra_tick(void);
static void extra_save(void);
static void extra_commit(int target, const char *text);
static void extra_open(int target);
static void extra_timer_label(char *out, int cap);
static void extra_alarm_label(char *out, int cap);
static int extra_ringing(void);
static int64_t extra_millis(void);
static void extra_overlay(void);
static int extra_intercept(const Input *in);
static void extra_track(unsigned seconds, unsigned pages, uint64_t id);
static void extra_zone(void);
static void extra_wiki_cache(void);
static void extra_wiki_restore(void);
static void quotes_load(void);
static void app_insights(void);
static void key_outline(int x, int y, int w, int h);
static void soft_rect(int x, int y, int w, int h, int r, int color);
static void aa_soft_rect(int x, int y, int w, int h, int r, int color);
static void stroke(int x0, int y0, int x1, int y1, int thickness);
static void commit_keyboard(void);
static void note_editor_draw(void);
static void note_editor_touch(int x, int y);

static void keyboard_insert(const char *s);
static void keyboard_backspace(void);
static void system_screen(void);
static void games_init(int target);
static void app_chess(void);
static void app_tic(void);
static int games_input(const Input *in);
static int system_input(const Input *in);
static void suit_icon(int, int, int);
static void new_apps_init(void);
static void new_apps_open(int route);
static void new_apps_save(void);
static void signal_save(void);
static void new_apps_draw(void);
static int new_apps_input(const Input *);
static int new_apps_commit(int, const char *);
static int new_apps_poll(void);
static int new_apps_tick(void);

static void v1_init(void), v1_save(void), v1_open(int), v1_draw(void);
static int v1_input(const Input *), v1_commit(int, const char *), v1_tick(void), v1_poll(void);
static int v1_notes_input(const Input *), clipboard_input(const Input *);
static void v1_notes_draw(void), v1_note_tools(void), v1_launcher(void),
    v1_launcher_input(int, int);
static void clipboard_draw(void), v1_effect(void), v1_close(void);
static int v1_sleep_photo(void), v1_chess_tick(void);
static void v1_note_created(int), v1_timer_ring(int), v1_timer_started(void);
static int v1_sleep_poll(void), v1_sleep_tick(void);
static void app_icon(int, int, int);
static void polish_photos_grid(void);
static void polish_tasks(void);
static int polish_tasks_input(const Input *);
static void polish_wiki_refetched(void);
static int article_wiki_open(void);
static void article_wiki_request(void);
static int wiki_reader_pending;
static void polish_wiki_history(void);
static void polish_view(void), polish_order(void);
static void polish_init(void), polish_draw(void), polish_open(int), polish_close(void);
static int polish_input(const Input *), polish_tick(void), polish_poll(void),
    polish_commit(int, const char *), polish_power(const Input *);
static void polish_wiki_archive(void), polish_wiki_saved(void);
static void polish_notes(void);
static int polish_photo_full(void);
static int polish_notes_input(const Input *), polish_news_input(const Input *);
static void polish_news_draw(void), polish_photo(void), polish_brain(void), polish_language(void);
static int polish_photo_input(const Input *), polish_brain_input(const Input *),
    polish_language_input(const Input *);
static void tabbar(void);
static void open_keyboard(int target, const char *title, const char *value) {
    keyboard = 1;
    dropdown = 0;
    keyboard_target = target;
    keyboard_title = title;
    snprintf(edit, sizeof(edit), "%s", value);
    note_cursor = strlen(edit);
    note_view_start = note_scroll_lock = keyboard_view18 = accent_count18 = 0;
    keyboard_shift = keyboard_symbols = 0;
    keyboard_background_valid18 = 0;
    editor_reset18();
    cursor_visible = 1;
    cursor_epoch = extra_millis();
    pressed = -1;
}
static void open_dropdown(int target, const char **labels, int count, int selected, int x, int y,
                          int w) {
    dropdown = 1;
    drop_generation++;
    keyboard = 0;
    drop_target = target;
    drop_labels = labels;
    drop_count = count;
    drop_visible = count < 5 ? count : 5;
    drop_first = selected >= 0 ? (selected / 5) * 5 : 0;
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
static void wrapped_raw(int x, int y, const char *s, const Font *f, int width, int lines) {
    int lh = f->h - 4;
    for (int row = 0; row < lines && *s; row++) {
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
        drawtext_raw(x, y + row * lh, buf, f, width);
        s = p;
        while (*s == ' ' || *s == '\n')
            s++;
    }
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
    key_outline(x, y, w, h);
    if (chosen)
        rect(x + 8, y + h - 5, w - 16, 3, 0);
    centered(x, y, w, h, label, &small);
}
static void dropdown_button(int x, int y, int w, int h, const char *label) {
    region(x, y, w, h);
    key_outline(x, y, w, h);
    centered(x + 2, y, w - 34, h, label, &small);
    stroke(x + w - 27, y + h / 2 - 3, x + w - 20, y + h / 2 + 4, 2);
    stroke(x + w - 20, y + h / 2 + 4, x + w - 13, y + h / 2 - 3, 2);
}
static void segments(int x, int y, int w, int h, const char *const *labels, int count,
                     int selected) {
    key_outline(x, y, w, h);
    for (int i = 0; i < count; i++) {
        int left = x + w * i / count, right = x + w * (i + 1) / count;
        region(left, y, right - left, h);
        if (i)
            rect(left, y + 2, 2, h - 4, 96);
        centered(left + 2, y, right - left - 4, h, labels[i], &small);
        if (i == selected) {
            rect(left + 9, y + h - 5, right - left - 18, 3, 0);
        }
    }
}
static void disc(int x, int y, int r, int color);
static void aa_arc(int x, int y, int r, int from, int to, int thickness);
static void aa_disc(int x, int y, int r, int color);
static void wifi_symbol(int x, int y) {
    /* One shared bold Wi-Fi symbol in both the header and network settings. */
    aa_arc(x + 15, y + 27, 9, 220, 320, 3);
    aa_arc(x + 15, y + 27, 15, 220, 320, 3);
    aa_disc(x + 15, y + 27, 2, 0);
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
static void aa_soft_rect(int,int,int,int,int,int);
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
/* Shared 4x4 coverage masks for all rounded surfaces and image clips.
   Computed once per radius, reused for every button, card and cover. */
static unsigned char corner_masks[33][1024], corner_ready[33];
static float corner_inside(int r, int x, int y) {
    if (r < 1 || x >= r || y >= r)
        return 1;
    if (r > 32)
        r = 32;
    if (!corner_ready[r]) {
        for (int j = 0; j < r; j++) {
            for (int i = 0; i < r; i++) {
                int n = 0;
                for (int sy = 0; sy < 4; sy++) {
                    for (int sx = 0; sx < 4; sx++) {
                        float dx = r - i - (sx + .5f) / 4, dy = r - j - (sy + .5f) / 4;
                        n += dx * dx + dy * dy <= r * r;
                    }
                }
                corner_masks[r][j * 32 + i] = n;
            }
        }
        corner_ready[r] = 1;
    }
    return corner_masks[r][y * 32 + x] / 16.f;
}
static void aa_soft_rect(int x, int y, int w, int h, int r, int color) {
    if (r < 1) {
        rect(x, y, w, h, color);
        return;
    }
    if (r > w / 2)
        r = w / 2;
    if (r > h / 2)
        r = h / 2;
    rect(x + r, y, w - 2 * r, h, color);
    rect(x, y + r, r, h - 2 * r, color);
    rect(x + w - r, y + r, r, h - 2 * r, color);
    for (int j = 0; j < r; j++)
        for (int i = 0; i < r; i++) {
            float a = corner_inside(r, i, j);
            blend_pixel(x + i, y + j, a, color);
            blend_pixel(x + w - 1 - i, y + j, a, color);
            blend_pixel(x + i, y + h - 1 - j, a, color);
            blend_pixel(x + w - 1 - i, y + h - 1 - j, a, color);
        }
}
static void soft_rect(int x, int y, int w, int h, int r, int color) {
    (void)r;
    rect(x, y, w, h, color);
}
static void toggle(int x, int y, int on) {
    region(x - 10, y - 8, 90, 48);
    aa_soft_rect(x, y, 68, 32, 16, 0);
    if (!on)
        aa_soft_rect(x + 2, y + 2, 64, 28, 14, 255);
    aa_disc(x + (on ? 52 : 16), y + 16, 12, on ? 255 : 0);
}
static void key_outline(int x, int y, int w, int h) {
    if (w < 4 || h < 4)
        return;
    rect(x, y, w, 2, 48);
    rect(x, y + h - 2, w, 2, 48);
    rect(x, y + 2, 2, h - 4, 48);
    rect(x + w - 2, y + 2, 2, h - 4, 48);
}
static int can_back(void) {
    return keyboard || dropdown || confirm_delete || battery_popup ||
           (screen == 29 && u17_notes_depth()) || (screen == 10 && opds_depth) ||
           (screen != 1 && screen != 13 && screen != 10 && screen != 20);
}
static void progress(int x, int y, int w, int n, int total) {
    rect(x, y, w, 4, 180);
    if (total > 0)
        rect(x, y, (int)((long long)w * n / total), 4, 0);
}
#include "image_sampling.h"
#include "cover_cache.h"
static unsigned char clip_background[4][81];
static void rounded_clip_end(int x, int y, int w, int h, int enabled) {
    if (!enabled)
        return;
    for (int j = 0; j < 9; j++)
        for (int i = 0; i < 9; i++) {
            int xx[4] = {x + i, x + w - 1 - i, x + i, x + w - 1 - i},
                yy[4] = {y + j, y + j, y + h - 1 - j, y + h - 1 - j};
            int a = (int)((1 - corner_inside(9, i, j)) * 16);
            for (int c = 0; c < 4; c++)
                if (xx[c] >= 0 && xx[c] < WIDTH && yy[c] >= 0 && yy[c] < HEIGHT) {
                    int k = yy[c] * WIDTH + xx[c], v = ui_pixels[k];
                    ui_pixels[k] =
                        (unsigned char)((v * (16 - a) + clip_background[c][j * 9 + i] * a + 8) /
                                        16);
                }
        }
}
static void cover_bounds(const unsigned char *p, int cw, int ch, int *x0, int *y0, int *x1,
                         int *y1) {
    *x0 = 0;
    *y0 = 0;
    *x1 = cw;
    *y1 = ch;
    if (!options[OPT_COVER_CROP])
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
static void rounded_clip_begin(int x, int y, int w, int h) {
    for (int j = 0; j < 9; j++)
        for (int i = 0; i < 9; i++) {
            int xx[4] = {x + i, x + w - 1 - i, x + i, x + w - 1 - i},
                yy[4] = {y + j, y + j, y + h - 1 - j, y + h - 1 - j};
            for (int c = 0; c < 4; c++)
                clip_background[c][j * 9 + i] =
                    (xx[c] >= 0 && xx[c] < WIDTH && yy[c] >= 0 && yy[c] < HEIGHT)
                        ? ui_pixels[yy[c] * WIDTH + xx[c]]
                        : 255;
        }
}
static void cover_size(Book *b, int x, int y, int w, int h) {
    rounded_clip_begin(x, y, w, h);
    char path[2048];
    library_cover_path(b, path, sizeof(path));
    char hd[2048];
    snprintf(hd, sizeof(hd), "%s/covers/%016llx.hi.pgm", data_root, (unsigned long long)b->id);
    FILE *f = fopen(hd, "rb");
    if (!f)
        f = fopen(path, "rb");
    if (!f && b->id >= 0x7000000000000000ULL && b->id < 0x7000000001000000ULL) {
        snprintf(path, sizeof(path), tr("%s/../catalogs/covers/%016llx.pgm"), data_root,
                 (unsigned long long)b->id);
        f = fopen(path, "rb");
    }
    if (f) {
        struct stat source;
        int cacheable = !fstat(fileno(f), &source);
        if (cacheable && cover_tile_get(&source, x, y, w, h)) {
            fclose(f);
            rounded_clip_end(x, y, w, h, options[OPT_COVER_RADIUS]);
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
                cover_bounds(pix, cw, ch, &x0, &y0, &x1, &y1);
                int sw = x1 - x0, sh = y1 - y0;
                if (options[OPT_COVER_CROP]) {
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
                rounded_clip_end(x, y, w, h, options[OPT_COVER_RADIUS]);
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
    rounded_clip_end(x, y, w, h, options[OPT_COVER_RADIUS]);
}
static void cover(Book *b, int x, int y, int scale) {
    cover_size(b, x, y, scale == 2 ? 52 : 120 / scale, scale == 2 ? 72 : 164 / scale);
}
static int featured(void) {
    return library_last();
}
static int library_per(void) {
    return featured() >= 0 && listpage == 0 ? 3 : 6;
}
static int library_top(void) {
    return featured() >= 0 && listpage == 0 ? 365 : 115;
}
static int library_begin(void) {
    return featured() >= 0 ? (listpage ? 3 + (listpage - 1) * 6 : 0) : listpage * 6;
}
static int library_pages(void) {
    return featured() >= 0 ? 1 + (view_count > 3 ? (view_count - 3 + 5) / 6 : 0)
                           : (view_count ? (view_count + 5) / 6 : 1);
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
static void pagination_bind(int *state, int count, int y, int visible, void (*move)(int)) {
    pagination = (Pagination){state, count, y, visible, move};
}
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
static void pager(int y, int *state, int pages) {
    if (*state < 0)
        *state = 0;
    if (*state >= pages)
        *state = pages > 0 ? pages - 1 : 0;
    pagination_bind(state, pages, y, 1, NULL);
    if (pages <= 1)
        return;
    page_arrow(205, y, IC_PREV, *state > 0);
    page_arrow(350, y, IC_NEXT, *state + 1 < pages);
    char b[32];
    snprintf(b, sizeof(b), "%d / %d", *state + 1, pages);
    centered(259, y, 84, 44, b, &caption);
}
static void system_loading(int target) {
    if (target != 2 && target != 79 && target != 40)
        return;
#ifndef LUMEN_HOST
    memset(ui_pixels, 255, sizeof(ui_pixels));
    aa_arc(300, 370, 15, 35, 310, 3);
    device_present(ui_pixels, 0, 0);
#endif
}
#include "squircle.h"
#include "widgets.h"
#include "typography.h"
#include "text_flow.h"
#include "list.h"
#include "titlebar.h"
static void update_view(void) {
    view_count = library_view(view, query, 0, settings.sort);
    /* Keep installer fixtures and our welcome note out of the personal shelves. */
    for (int i = 0; i < view_count;) {
        const char *name = strrchr(library[view[i]].path, '/');
        name = name ? name + 1 : library[view[i]].path;
        if (!strcmp(name, "LUMEN Lesetest.epub") || !strcmp(name, "LUMEN – Willkommen.txt")) {
            memmove(view + i, view + i + 1, (size_t)(view_count - i - 1) * sizeof(int));
            view_count--;
        } else
            i++;
    }
    int last = featured();
    if (last >= 0)
        for (int i = 0; i < view_count; i++)
            if (view[i] == last) {
                memmove(view + i, view + i + 1, (size_t)(view_count - i - 1) * sizeof(int));
                view_count--;
                break;
            }
    int pages = library_pages();
    if (listpage >= pages)
        listpage = pages - 1;
}
void ui_message(const char *s) {
    NotificationRequest request={.duration_ms=6000};
    snprintf(request.text,sizeof(request.text),"%s",tr(s));
    notifications_register();
    app_call("notifications",&request,sizeof(request),extra_millis());
}
void ui_set_screen(int n) {
    if (n == 0)
        n = 1;
    if (n == 6) {
        route_to(1);
        open_keyboard(0, "Bibliothek durchsuchen", query);
    } else
        route_to(n);
    full = 1;
}
void ui_init(void) {
    app_runtime_init(&runtime, app_leave, NULL);
    library_settings_load();
    apps_init();
    extra_init();
    new_apps_init();
    if (options[OPT_LANGUAGE] < 0 || options[OPT_LANGUAGE] > 2)
        options[OPT_LANGUAGE] = 0;
    options[OPT_CONFIRM] = 1;
    settings.grid = 1;
    settings.filter = 0;

    query[0] = 0;
    route_to(1);
    update_view();
    if (options[OPT_STARTPAGE])
        route_to(1);
    char sourcepath[2048];
    snprintf(sourcepath, sizeof(sourcepath), tr("%s/opds-source.txt"), data_root);
    FILE *source = fopen(sourcepath, "r");
    if (source) {
        if (fgets(opds_source, sizeof(opds_source), source))
            opds_source[strcspn(opds_source, "\r\n")] = 0;
        fclose(source);
    }
    device_status(battery, sizeof(battery), wifi, sizeof(wifi));
    battery_charging = device_charging();
    status_time = charging_checked = time(NULL);
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
    signal_save();
    games_save17();
    if (screen == 126)
        journal_store17();
    companion_checkpoint16();
    u15_flush();
    if (keyboard && (keyboard_target == 30 || keyboard_target == 60 || keyboard_target == 171 ||
                     keyboard_target == 174))
        extra_commit(keyboard_target, edit);
    apps_checkpoint();
    apps_save();
    extra_save();
    new_apps_save();
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
    layout_epoch++;
    ReaderPreferences p = preferences_reader();
    engine_options(p.font, p.alignment, p.letterspace, p.paragraph);
    page = engine_relayout(p.size, p.leading, p.margin, page);
    memcpy(reader_layout, next, sizeof(next));
    reader_layout_valid = 1;
    chapter_count = engine_outline(chapters, 256);
    library_progress(&library[current], page, engine_count());
}
int ui_open(int index) {
    layout_epoch++;
    if (index < 0 || (index >= library_count && !(index == BOOK_LIMIT && reader_document_active)))
        return 0;
    selecting = selection_release = 0;
    notification_dismiss();
    Book *b = &library[index];
    if (!library_supported(b)) {
        selected = index;
        route_to(7);
        full = 1;
        ui_message("Dieses Amazon-Format bitte im Original-Reader öffnen.");
        return 0;
    }
    if (current >= 0) {
        library_progress(&library[current], page, engine_count());
        save_anchor(&library[current], page, -1);
    }
    system_loading(2);
    engine_close();
    if (index != BOOK_LIMIT)
        reader_document_active = 0;
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
            selected = index;
            route_to(7);
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
    chapter_count = engine_outline(chapters, 256);
    chapterpage = chaptertab = 0;
    zoom = 1;
    panx = pany = 0;
    route_to(2);
    reader_chrome = 0;
    full = 1;
    library_progress(b, page, engine_count());
    annotations_load();
    selecting = 0;
    return 1;
}
static int refresh_status(int force) {
    time_t now = time(NULL);
    if (force || !status_time || now < status_time || now - status_time >= 30) {
        device_status(battery, sizeof(battery), wifi, sizeof(wifi));
        int before = battery_charging;
        battery_charging = device_charging();
        status_time = charging_checked = now;
        return before != battery_charging;
    }
    if (now < charging_checked || now - charging_checked >= (battery_charging ? 5 : 30)) {
        int before = battery_charging;
        battery_charging = device_charging();
        charging_checked = now;
        return before != battery_charging;
    }
    return 0;
}
#include "chrome.h"
static void connect_screen(void);
static void more_settings(void);
#include "../../apps/library/view.h"
#include "../../apps/reader/view.h"
#include "../../apps/settings/reading.h"
#include "../../apps/reader/menus.h"
#include "../../apps/library/details.h"
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
static void lock_backdrop(Book *b) {
    static unsigned char cached[WIDTH * HEIGHT];
    static uint64_t id;
    static time_t stamp;
    static int ready;
    char path[2048];
    library_cover_path(b, path, sizeof(path));
    struct stat st;
    if (stat(path, &st))
        return;
    if (!ready || id != b->id || stamp != st.st_mtime) {
        FILE *f = fopen(path, "rb");
        if (!f)
            return;
        char magic[3];
        int w, h, max;
        unsigned char pix[120 * 164], tmp[120 * 164];
        int ok = fscanf(f, "%2s %d %d %d", magic, &w, &h, &max) == 4 && !strcmp(magic, "P5") &&
                 w == 120 && h == 164 && max == 255;
        if (ok) {
            fgetc(f);
            ok = fread(pix, 1, sizeof(pix), f) == sizeof(pix);
        }
        fclose(f);
        if (!ok)
            return;
        for (int pass = 0; pass < 3; pass++) {
            for (int y = 0; y < 164; y++)
                for (int x = 0; x < 120; x++) {
                    int sum = 0, n = 0;
                    for (int k = -5; k <= 5; k++) {
                        int xx = x + k;
                        if (xx >= 0 && xx < 120) {
                            sum += pix[y * 120 + xx];
                            n++;
                        }
                    }
                    tmp[y * 120 + x] = sum / n;
                }
            for (int y = 0; y < 164; y++)
                for (int x = 0; x < 120; x++) {
                    int sum = 0, n = 0;
                    for (int k = -5; k <= 5; k++) {
                        int yy = y + k;
                        if (yy >= 0 && yy < 164) {
                            sum += tmp[yy * 120 + x];
                            n++;
                        }
                    }
                    pix[y * 120 + x] = sum / n;
                }
        }
        for (int y = 0; y < HEIGHT; y++)
            for (int x = 0; x < WIDTH; x++) {
                float fx = x * 119.f / (WIDTH - 1), fy = y * 163.f / (HEIGHT - 1);
                int xx = (int)fx, yy = (int)fy;
                int nx = xx < 119 ? xx + 1 : xx, ny = yy < 163 ? yy + 1 : yy;
                float a = fx - xx, c = fy - yy;
                float v = (1 - c) * ((1 - a) * pix[yy * 120 + xx] + a * pix[yy * 120 + nx]) +
                          c * ((1 - a) * pix[ny * 120 + xx] + a * pix[ny * 120 + nx]);
                cached[y * WIDTH + x] = (unsigned char)(180 + v * 75 / 255);
            }
        id = b->id;
        stamp = st.st_mtime;
        ready = 1;
    }
    memcpy(ui_pixels, cached, sizeof(cached));
}
void ui_draw(void) {
    widget_count = 0;
    pagination = (Pagination){0};
    if (keyboard && keyboard_background_valid18 && !sleeping) {
        memcpy(ui_pixels, keyboard_background18, sizeof(ui_pixels));
        if (screen != 2)
            header();
        goto input_overlays18;
    }
    memset(ui_pixels, 255, sizeof(ui_pixels));
    if (sleeping) {
        int lock_book = current >= 0 && current < library_count ? current : library_last();
        if (options[OPT_LOCK] == 4 && v1_sleep_photo())
            return;
        if (options[OPT_LOCK] == 3) {
            char path[2048], magic[3];
            int w, h, max;
            snprintf(path, sizeof(path), "%s/lockscreen.pgm", data_root);
            FILE *f = fopen(path, "rb");
            int ok = 0;
            if (f) {
                if (fscanf(f, "%2s %d %d %d", magic, &w, &h, &max) == 4 && !strcmp(magic, "P5") &&
                    w == WIDTH && h == HEIGHT && max == 255) {
                    fgetc(f);
                    ok = fread(ui_pixels, 1, sizeof(ui_pixels), f) == sizeof(ui_pixels);
                }
                fclose(f);
            }
            if (ok)
                return;
            memset(ui_pixels, 255, sizeof(ui_pixels));
        }
        if (lock_book >= 0 && options[OPT_LOCK] == 0 && options[OPT_LOCK_BLUR])
            lock_backdrop(&library[lock_book]);

        if (lock_book >= 0 && options[OPT_LOCK] == 0) {
            cover_size(&library[lock_book], 180, 135, 240, 328);
            int lines = centered_lines(44, 500, 512, library[lock_book].title, &editorial, 3);
            char author[320];
            centered_lines(44, 510 + lines * (editorial.h - 4), 512,
                           author_label(library[lock_book].author, author, sizeof(author)), &small,
                           2);
        } else if (options[OPT_LOCK] == 2) {
            time_t now = time(NULL);
            char t[32];
            strftime(t, sizeof(t), "%H:%M", localtime(&now));
            centered(28, 280, 544, 100, t, &title);
        } else
            centered(28, 300, 544, 80, "Ein Moment der Ruhe.", &editorial);
        if (options[OPT_LOCK_HINT])
            centered(28, 704, 544, 48, "Mit der Power-Taste weiterlesen", &small);
        return;
    }
    if (polish_photo_full()) {
        polish_photo();
        return;
    }
    if (screen != 2)
        header();
    if (!u17_draw() && !u16_draw() && !u15_draw())
        app_render();
    help_overlay16();
    if (screen == 79 || screen == 125) {
        rect(250, 72, 266, 48, 255);
        dropdown_button(270, 74, 246, 44, screen == 79 ? "Sternhimmel" : "Sonnensystem");
    }
    if (screen == 2 && reader_chrome)
        header();
    if (has_tabs())
        tabbar();
    if (screen == 2 && selecting == 2)
        selection_overlay();
    if (keyboard) {
        memcpy(keyboard_background18, ui_pixels, sizeof(ui_pixels));
        keyboard_background_valid18 = 1;
    }
input_overlays18:
    overlays();
    extra_overlay();
    clipboard_draw();
    polish_draw();
    v1_effect();
    notification_draw();

    if (options[OPT_INVERT])
        for (size_t i = 0; i < sizeof(ui_pixels); i++)
            ui_pixels[i] = 255 - ui_pixels[i];
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
int ui_input(const Input *in) {
    int visible=notification_visible();
    if(notification_input(in,visible)) { pressed=-1; return 1; }
    int pm = polish_power(in);
    if (pm)
        return pm == 2 ? 0 : pm == 3 ? 3 : 1;
    if (polish_photo_full() && in->kind == INPUT_TAP)
        return polish_photo_input(in);
    int input18 = keyboard_intercept18(in);
    if (input18 >= 0)
        return input18;
    int update17 = u17_input(in);
    if (update17 >= 0) {
        pressed = -1;
        return update17;
    }
    int update16 = u16_input(in);
    if (update16 >= 0) {
        pressed = -1;
        return update16;
    }
    int update15 = u15_input(in);
    if (update15 >= 0) {
        pressed = -1;
        return update15;
    }
    if (!sleeping && clipboard_input(in))
        return 1;
    int network_poll = 0;
    Input normalized;
    if (!keyboard && in->kind == INPUT_TAP)
        apps_checkpoint();
    if (in->kind == INPUT_STATUS && sleeping)
        return v1_sleep_tick() ? 1 : 2;
    if (in->kind == INPUT_STATUS) {
        if (refresh_status(in->x == -1))
            return 1;
        int tick16 = u16_tick();
        if (tick16)
            return tick16;
        int action = extra_tick();
        if (action == 1)
            return 1;
        if (discover_busy()) {
            normalized = *in;
            normalized.kind = INPUT_NETWORK;
            in = &normalized;
            network_poll = 1;
        } else {
            int needs_status = screen == 12 || screen == 8 ||
                               ((screen == 25 || screen == 26) && apps_poll_interval() >= 0);
            if (action && !needs_status)
                return action;
            if (service_busy() && !needs_status)
                return 2;
        }
    }
    int intercepted = extra_intercept(in);
    if (intercepted >= 0)
        return intercepted;

    if (screen == 23 && in->kind == INPUT_HOLD && !sleeping)
        return apps_input(in);
    if (!sleeping && screen == 2 && in->kind == INPUT_HOLD && !reader_chrome && in->y > 60 &&
        in->y < 735) {
        selection_last_x = selection_x = in->x;
        selection_last_y = selection_y = in->y;
        if (engine_select(page, in->x - 28, in->y - 12, in->x - 27, in->y - 11, 544, 728, zoom,
                          panx, pany, &selection))
            selecting = 1;
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
    if (screen == 2 && selecting == 1 &&
        (in->kind == INPUT_MOVE || in->kind == INPUT_TAP || in->kind == INPUT_SWIPE)) {
        if (in->kind == INPUT_MOVE && abs(in->x - selection_last_x) < 7 &&
            abs(in->y - selection_last_y) < 7)
            return 2;
        selection_last_x = in->x;
        selection_last_y = in->y;
        engine_select(page, selection_x - 28, selection_y - 12, in->x - 28, in->y - 12, 544, 728,
                      zoom, panx, pany, &selection);
        if (in->kind != INPUT_MOVE)
            selecting = selection.count ? 2 : 0;
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
        return pressed >= 0 ? 3 : 2;
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
            if (normalized.y > 47)
                normalized.y = 47;
            in = &normalized;
        }
    } else if (in->kind == INPUT_SWIPE) {
        pressed = -1;
        if (screen == 2 ? (down_y < 48 || down_y >= 752) : (down_y < 130 || down_y >= 700))
            return 1;
    }
    if (!sleeping && !keyboard && !dropdown && !confirm_delete && !battery_popup &&
        pagination.state) {
        if (in->kind == INPUT_SWIPE && abs(in->dx) > abs(in->dy) &&
            (screen != 2 || (options[OPT_SWIPE] && zoom <= 1))) {
            pagination_move(in->dx < 0 ? 1 : -1);
            return 1;
        }
        if (in->kind == INPUT_TAP && pagination.visible && in->y >= pagination.y &&
            in->y < pagination.y + 44 &&
            ((in->x >= 205 && in->x < 249) || (in->x >= 350 && in->x < 394))) {
            pagination_move(in->x < 300 ? -1 : 1);
            return 1;
        }
    }
    if (in->kind == INPUT_TAP && !sleeping)
        device_activity();
    if (in->kind == INPUT_TAP) {
        cursor_visible = 1;
        cursor_epoch = extra_millis();
    }
    if (in->kind == INPUT_STATUS) {
        if (screen >= 20 && screen <= 26 && !sleeping)
            return apps_input(in);
        if (screen == 12 && wifi_enabled) {
            char old[128];
            snprintf(old, sizeof(old), "%s", wifi_ssid);
            network_status(wifi_ssid, sizeof(wifi_ssid));
            if (wifi_scanning || strcmp(old, wifi_ssid))
                network_results();
            wifi_scanning = 0;
        }
        refresh_status(0);
        return 1;
    }
    if (in->kind == INPUT_NETWORK) {
        int done = discover_complete();
        if (network_poll && !done)
            return 2;
        if (screen == 10 && store_stage && done && store_complete(done))
            return 1;
        if (done == 3) {
            polish_wiki_refetched();
            extra_wiki_cache();
            if (screen == 36)
                article_wiki_request();
            return 1;
        }
        if (done < 0) {
            char message[256];
            snprintf(message, sizeof(message), "%s", discover_error());
            if (screen == 36 && wiki_refresh_failed())
                return 1;
            if (screen == 36)
                extra_wiki_restore();
            ui_message(message);
        } else if (done == 2) {
            if (screen == 2)
                library_dirty = 1;
            else
                refresh_library();
            ui_message("Buch zur Bibliothek hinzugefügt.");
        } else if (done == 1) {
            opds_page = 0;
            if (screen == 10 && opds_count == 1 && opds_items[0].acquisition) {
                opds_selected = 0;
                route_to(11);
                return 1;
            }
            if (screen == 10 || screen == 11)
                route_to(10);
            else
                ui_message("Katalog geladen. Unter Bücher entdecken öffnen.");
        }
        return 1;
    }
    if (in->kind == INPUT_EXIT || in->kind == INPUT_USB) {
        connect_stop();
        discover_cancel();
        service_cancel();
        ui_save();
        return 0;
    }
    if (in->kind == INPUT_SLEEP) {
        apps_pause();
        extra_save();
        service_cancel();
        selecting = 0;
        battery_popup = 0;
        if (!sleeping) {
            connect_stop();
            discover_cancel();
            ui_save();
            sleeping = 1;
            full = 1;
        }
        return 1;
    }
    if (in->kind == INPUT_POWER) {
        static int was_sleeping = -1;
        if (in->x == 1)
            was_sleeping = sleeping;
        else {
            if (sleeping && was_sleeping != 0) {
                device_wake();
                sleeping = 0;
                full = 1;
            }
            was_sleeping = -1;
        }
        return 1;
    }
    if (in->kind == INPUT_WAKE) {
        if (sleeping) {
            sleeping = 0;
            full = 1;
            device_status(battery, sizeof(battery), wifi, sizeof(wifi));
            battery_charging = device_charging();
            status_time = charging_checked = time(NULL);
            extra_tick();
        }
        return 1;
    }
    if (sleeping)
        return 2;
    int x = in->x, y = in->y;
    if (screen == 2 && selecting == 2 && in->kind == INPUT_TAP) {
        if (y >= 670 && y < 718) {
            if (x < 200) {
                if (annotation_count >= 128)
                    ui_message("Maximal 128 Markierungen pro Buch.");
                else {
                    Annotation *a = &annotations[annotation_count++];
                    memset(a, 0, sizeof(*a));
                    engine_position(page, &a->position);
                    snprintf(a->text, sizeof(a->text), "%s", selection.text);
                    snprintf(a->position.quote, sizeof(a->position.quote), "%s", selection.text);
                    if (!annotations_write()) {
                        annotation_count--;
                        ui_message("Speichern fehlgeschlagen.");
                    } else
                        ui_message("Markierung gespeichert.");
                }
            } else if (x < 390) {
                AppIntent request = {.app = APP_WIKIPEDIA, .kind = INTENT_SEARCH};
                snprintf(request.text, sizeof(request.text), "%s", selection.text);
                app_send(&request);
            }
        }
        selecting = 0;
        pressed = -1;
        return 1;
    }
    if (screen == 14 && !confirm_delete && in->kind == INPUT_TAP && y >= 155 && y < 655) {
        int i = annotation_page * 5 + (y - 155) / 100;
        if (i < annotation_count) {
            if (x >= 510) {
                confirm_delete = 2;
                annotation_delete = i;
            } else {
                page = engine_restore(&annotations[i].position);
                route_to(2);
                reader_chrome = 0;
            }
        }
        return 1;
    }
    if (screen == 14 && !confirm_delete && in->kind == INPUT_TAP && y >= 679 && y < 723) {
        if (x < 300 && annotation_page)
            annotation_page--;
        else if (x >= 300 && (annotation_page + 1) * 5 < annotation_count)
            annotation_page++;
        return 1;
    }

    if (in->kind == INPUT_TAP && screen == 2 && reader_chrome && y >= 164 && y < 736) {
        reader_chrome = 0;
        return 1;
    }
    if (in->kind == INPUT_TAP && screen == 2 && !reader_chrome && (y < 78 || y >= 722)) {
        reader_chrome = 1;
        return 1;
    }
    if (in->kind == INPUT_TAP && y < 48 && (screen != 2 || reader_chrome) && status_timer_x >= 0 &&
        x >= status_timer_x && x < status_timer_x + status_timer_width) {
        AppIntent request = {.app = APP_TIMER, .kind = INTENT_OPEN};
        app_send(&request);
        return 1;
    }
    if (in->kind == INPUT_TAP && y < 48 && (screen != 2 || reader_chrome)) {
        if (x < 62 && (screen == 3 || screen == 9) && !keyboard && !dropdown && !confirm_delete)
            apply_reading_settings();
        if (x < 62 && screen == 29 && u17_notes_depth() && !keyboard && !dropdown) {
            u17_notes_up();
            return 1;
        }
        if (x < 62 && !keyboard && !dropdown && recipes_back())
            return 1;
        if (x < 62 && can_back()) {
            apps_pause();
            if (battery_popup) {
                battery_popup = 0;
                return 1;
            }
            if (keyboard || dropdown || confirm_delete) {
                if (keyboard && keyboard_multiline())
                    commit_keyboard();
                keyboard = dropdown = confirm_delete = 0;
                return 1;
            }
            if (screen == 10 && opds_depth) {
                discover_cancel();
                if (!strcmp(opds_history[--opds_depth], "lumen:search"))
                    store_restore();
                else
                    discover_start(opds_history[opds_depth], NULL);
                return 1;
            }
            int target = 1;
            while (history_count) {
                int candidate = history[--history_count];
                if (candidate != screen && app_route_valid(candidate)) {
                    target = candidate;
                    break;
                }
            }
            route_to(target);
            if (screen == 0)
                route_to(1);
            if ((screen == 2 || screen == 4 || screen == 5) && current < 0)
                route_to(1);
            if (screen == 1)
                update_view();
            return 1;
        }
        if (status_wifi_x >= 0 && x >= status_wifi_x - 5 && x < status_wifi_x + 39 &&
            screen != 12) {
            if (history_count < 64)
                history[history_count++] = screen;
            apps_pause();
            route_to(12);
            wifi_open();
            return 1;
        }
    }
    if (in->kind == INPUT_TAP && y < 48 && (options[OPT_BATTERY] || options[OPT_BATTERY_PERCENT]) &&
        status_battery_x >= 0 && x >= status_battery_x - 6 && x < status_battery_right + 6 &&
        (screen != 2 || reader_chrome)) {
        battery_popup = !battery_popup;
        return 1;
    }
    if (battery_popup && in->kind == INPUT_TAP) {
        battery_popup = 0;
        return 1;
    }
    if (in->kind == INPUT_TAP && has_tabs() && y >= 736 &&
        (keyboard || dropdown || confirm_delete)) {
        keyboard = dropdown = confirm_delete = 0;
        memset(edit, 0, sizeof(edit));
    }
    if (keyboard || dropdown || confirm_delete) {
        modal_input(in);
        return 1;
    }
    if (screen == 0)
        route_to(1);
    if (in->kind == INPUT_TAP && has_tabs() && y >= 736) {
        apps_pause();
        if (screen == 3 || screen == 9)
            apply_reading_settings();
        history_count = 0;
        int tab = x / (options[OPT_APPS] ? 150 : 200);
        const AppId tabs[] = {APP_LIBRARY, APP_SETTINGS, APP_STORE, APP_LAUNCHER};
        if (tab >= 0 && tab < (options[OPT_APPS] ? 4 : 3)) {
            AppIntent request = {.app = tabs[tab], .kind = INTENT_OPEN};
            app_send(&request);
            history_count = 0;
        }
        return 1;
    }
    return app_handle_input(in);
}
/* Shared overlays: dropdowns, keyboard and destructive-action confirmation. */
#include "text_editor.h"
#include "keyboard.h"
static void overlays(void) {
    if (keyboard || dropdown || confirm_delete) {
        int keep = 0;
        for (int i = 0; i < widget_count; i++)
            if (widgets[i].system)
                widgets[keep++] = widgets[i];
        widget_count = keep;
    }
    if (keyboard) {
        keyboard_draw18();
    }
    ui_y_offset = 0;
    if (dropdown) {
        int height = drop_visible * 48 + 10 + (drop_count > 5 ? 38 : 0);
        if (drop_y + height > (has_tabs() ? 728 : 790))
            drop_y = (has_tabs() ? 728 : 790) - height;
        soft_rect(drop_x, drop_y, drop_w, height, 9, 0);
        soft_rect(drop_x + 2, drop_y + 2, drop_w - 4, height - 4, 0, 255);
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
            label_ellipsis(drop_x + 46, y + 3, drop_w - 85, 42,
                           drop_labels[i], &small, 0);
            if (row + 1 < drop_visible)
                rect(drop_x + 10, y + 47, drop_w - 20, 2, 190);
        }
        if (drop_count > 5) {
            int yy = drop_y + 5 + drop_visible * 48;
            page_arrow(drop_x + 8, yy - 2, IC_PREV, drop_first > 0);
            page_arrow(drop_x + drop_w - 52, yy - 2, IC_NEXT, drop_first + 5 < drop_count);
        }
    }
    if (confirm_delete) {
        soft_rect(20, 270, 554, 260, 12, 0);
        soft_rect(22, 272, 550, 256, 10, 255);
        drawtext(42, 292, confirm_delete == 2 ? "Markierung löschen?" : "Buch wirklich löschen?",
                 &body, 510);
        wrapped(42, 340,
                confirm_delete == 2 ? annotations[annotation_delete].text : library[selected].title,
                &small, 500, 2);
        button(42, 446, 235, "Abbrechen", 0);
        button(309, 446, 235, "Löschen", 0);
    }
    if (battery_popup) {
        soft_rect(428, 50, 148, 84, 9, 0);
        soft_rect(430, 52, 144, 80, 0, 255);
        char b[32];
        snprintf(b, sizeof(b), "%s%%", battery);
        centered(430, 58, 144, 64, b, &body);
    }
    if (pressed >= 0 && pressed < widget_count)
        ui_feedback_draw();
}
static void refresh_library(void) {
    ui_save();
    engine_close();
    current = -1;
    selected = -1;
    library_scan();
    update_view();
}
static void commit_keyboard(void) {
    if (keyboard_target >= 30) {
        int previous_target = keyboard_target;
        extra_commit(keyboard_target, edit);
        if (keyboard_target == previous_target) {
            memset(edit, 0, sizeof(edit));
            keyboard = 0;
        }
        pressed = -1;
        return;
    }
    if (keyboard_target == 8) {
        apps_note_commit(edit);
        keyboard = 0;
        pressed = -1;
        return;
    }
    if (keyboard_target == 6) {
        snprintf(store_query, sizeof(store_query), "%s", edit);
        store_search();
    } else if (keyboard_target == 5 || keyboard_target == 32) {
        if (network_connect(wifi_selected, edit))
            ui_message("Verbindung wird hergestellt …");
        else
            ui_message(network_error());
        memset(edit, 0, sizeof(edit));
    } else if (keyboard_target == 4) {
        snprintf(opds_source, sizeof(opds_source), tr("%s"), edit);
        char p[2048];
        snprintf(p, sizeof(p), tr("%s/opds-source.txt"), data_root);
        atomic_write(p, opds_source, strlen(opds_source));
    } else if (keyboard_target == 0) {
        snprintf(query, sizeof(query), tr("%s"), edit);
        listpage = 0;
        update_view();
    } else if (selected >= 0) {
        if (keyboard_target == 1) {
            if (current == selected) {
                ui_save();
                engine_close();
                current = -1;
            }
            if (!library_rename(selected, edit))
                ui_message("Umbenennen fehlgeschlagen. Name muss frei sein.");
        } else if (keyboard_target == 2) {
            if (!library_metadata(selected, edit, library[selected].author))
                ui_message("Titel konnte nicht gespeichert werden.");
        } else if (keyboard_target == 3) {
            if (!library_metadata(selected, library[selected].title, edit))
                ui_message("Autor konnte nicht gespeichert werden.");
        }
    }
    keyboard = 0;
    pressed = -1;
}
static size_t keyboard_capacity(void) {
    return text_input_spec(keyboard_target).capacity;
}
static void modal_input(const Input *in) {
    if (in->kind == INPUT_SWIPE && keyboard && keyboard_multiline() && down_y >= 54 &&
        down_y < 502) {
        editor_scroll18(in->dy > 0 ? -8 : 8);
        return;
    }
    if (in->kind != INPUT_TAP)
        return;
    int x = in->x, y = in->y;
    if (confirm_delete == 2) {
        if (y >= 446 && y < 494) {
            if (x >= 309 && annotation_delete < annotation_count) {
                Annotation backup = annotations[annotation_delete];
                memmove(annotations + annotation_delete, annotations + annotation_delete + 1,
                        (size_t)(annotation_count - annotation_delete - 1) * sizeof(Annotation));
                annotation_count--;
                if (!annotations_write()) {
                    memmove(annotations + annotation_delete + 1, annotations + annotation_delete,
                            (size_t)(annotation_count - annotation_delete) * sizeof(Annotation));
                    annotations[annotation_delete] = backup;
                    annotation_count++;
                    ui_message("Speichern fehlgeschlagen.");
                }
                if (annotation_page && annotation_page * 5 >= annotation_count)
                    annotation_page--;
            }
            confirm_delete = 0;
        }
        return;
    }
    if (confirm_delete) {
        if (y >= 446 && y < 494) {
            if (x >= 309) {
                ui_save();
                engine_close();
                current = -1;
                if (!library_delete(selected))
                    ui_message("Löschen fehlgeschlagen.");
                else {
                    selected = -1;
                    route_to(1);
                    update_view();
                }
            }
            confirm_delete = 0;
        }
        return;
    }
    if (dropdown) {
        unsigned generation = drop_generation;
        if (drop_count > 5 && x >= drop_x && x < drop_x + drop_w &&
            y >= drop_y + 5 + drop_visible * 48 && y < drop_y + 5 + drop_visible * 48 + 38) {
            if (x < drop_x + drop_w / 2 && drop_first > 0)
                drop_first -= 5;
            else if (x >= drop_x + drop_w / 2 && drop_first + 5 < drop_count)
                drop_first += 5;
            return;
        }
        if (x >= drop_x && x < drop_x + drop_w && y >= drop_y + 5 &&
            y < drop_y + 5 + drop_visible * 48) {
            int choice = drop_first + (y - drop_y - 5) / 48;
            if (choice >= drop_count) {
                dropdown = 0;
                return;
            }
            if (drop_target >= 931 && drop_target <= 950) {
                u17_choice(drop_target, choice);
            } else if (drop_target >= 910 && drop_target <= 930) {
                u16_dropdown(drop_target, choice);
            } else if (drop_target == 900) {
                settings.sort = choice;
                library_settings_save();
                update_view();

            } else if (drop_target == 0)
                settings.sort = choice;
            else if (drop_target == 1)
                settings.filter = choice;
            else if (drop_target == 2)
                setting_category = choice;
            else if (drop_target >= 100) {
                int option = drop_target - 100;
                if (option >= 0 && option < OPT_COUNT)
                    options[option] = choice;
                if (option == OPT_ZONE)
                    extra_zone();
                engine_options(options[OPT_FONT], options[OPT_ALIGN], options[OPT_LETTERSPACE],
                               options[OPT_PARAGRAPH]);
            }
            listpage = 0;
            update_view();
            library_settings_save();
        }
        if (generation == drop_generation)
            dropdown = 0;
        return;
    }
    if (keyboard) {
        keyboard_tap18(x, y);
        return;
    }
}
#include "../../apps/transfer/view.h"
#include "../../apps/settings/preferences.h"
#include "../../apps/store/entry.h"
#include "../../apps/settings/options.h"
#include "../../apps/store/view.h"
#include "../../apps/wifi/app.h"
int ui_poll_interval(void) {
    if (sleeping)
        return v1_sleep_poll();
    int wait = screen >= 20 ? apps_poll_interval() : (screen == 12 || screen == 8 ? 2000 : -1),
        other = extra_poll();
    int np = new_apps_poll();
    int pp = polish_poll();
    if (pp >= 0 && (np < 0 || pp < np))
        np = pp;
    int vp = v1_poll();
    if (vp >= 0 && (np < 0 || vp < np))
        np = vp;
    if (np >= 0 && (other < 0 || np < other))
        other = np;
    int status_wait = battery_charging ? 5000 : 30000;
    if (job_pending16())
        status_wait = 100;
    else if (ink_pending16())
        status_wait = 1000;
    if (wait < 0 || status_wait < wait)
        wait = status_wait;
    return other >= 0 && (wait < 0 || other < wait) ? other : wait;
}

static void annotations_load(void) {
    annotation_count = annotation_page = 0;
    if (current < 0)
        return;
    char path[2048];
    snprintf(path, sizeof(path), "%s/books/%016llx.notes-v1", data_root,
             (unsigned long long)library[current].id);
    FILE *f = fopen(path, "rb");
    if (!f)
        return;
    annotation_count = (int)fread(annotations, sizeof(Annotation), 128, f);
    fclose(f);
    for (int i = 0; i < annotation_count; i++) {
        annotations[i].text[2047] = 0;
        annotations[i].position.quote[127] = 0;
        if (annotations[i].position.chapter < 0 || annotations[i].position.chapter > 100000)
            annotations[i].position.chapter = 0;
    }
}
static void shade_selection(const TextSelection *s, int shade) {
    for (int i = 0; i < s->count; i++) {
        TextRect r = s->rects[i];
        int x0 = (int)r.x0 + 28, y0 = (int)r.y0 + 12, x1 = (int)ceilf(r.x1) + 28,
            y1 = (int)ceilf(r.y1) + 12;
        if (x0 < 28)
            x0 = 28;
        if (x1 > 572)
            x1 = 572;
        if (y0 < 12)
            y0 = 12;
        if (y1 > 740)
            y1 = 740;
        for (int y = y0; y < y1; y++)
            for (int x = x0; x < x1; x++)
                if (ui_pixels[y * WIDTH + x] > shade)
                    ui_pixels[y * WIDTH + x] = shade;
    }
}
static void annotations_draw(void) {
    if (!annotation_count) {
        if (selecting)
            shade_selection(&selection, 180);
        return;
    }
    if (shading_page != page || shading_notes != annotation_count ||
        shading_epoch != layout_epoch || shading_zoom != zoom || shading_panx != panx ||
        shading_pany != pany) {
        saved_shading.count = 0;
        ReaderPosition pos;
        if (current >= 0 && engine_position(page, &pos)) {
            for (int i = 0; i < annotation_count && saved_shading.count < 64; i++)
                if (annotations[i].position.chapter == pos.chapter) {
                    TextSelection found;
                    if (engine_find_text(page, annotations[i].text, 544, 728, zoom, panx, pany,
                                         &found))
                        for (int j = 0; j < found.count && saved_shading.count < 64; j++)
                            saved_shading.rects[saved_shading.count++] = found.rects[j];
                }
        }
        shading_page = page;
        shading_notes = annotation_count;
        shading_epoch = layout_epoch;
        shading_zoom = zoom;
        shading_panx = panx;
        shading_pany = pany;
    }
    shade_selection(&saved_shading, 215);
    if (selecting)
        shade_selection(&selection, 180);
}
static void selection_overlay(void) {
    rect(18, 574, 564, 160, 255);
    rect(18, 574, 564, 1, 0);
    wrapped_raw(30, 588, selection.text, &small, 540, 2);
    button(22, 670, 174, "Markieren", 0);
    button(206, 670, 174, "Wikipedia", 0);
    button(390, 670, 188, "Abbrechen", 0);
    button_h(390, 628, 188, 36, "Kopieren", 0);
}
static void annotations_screen(void) {
    heading("Markierungen");
    if (!annotation_count)
        wrapped(28, 180,
                "Text im Buch lange berühren und ziehen. Danach markieren oder in Wikipedia "
                "nachschlagen.",
                &editorial, 544, 5);
    for (int r = 0; r < 5 && annotation_page * 5 + r < annotation_count; r++) {
        int y = 155 + r * 100;
        region(28, y, 544, 98);
        wrapped_raw(30, y + 2, annotations[annotation_page * 5 + r].text, &small, 470, 3);
        icon_button(522, y + 25, IC_TRASH);
        rect(28, y + 98, 544, 2, 190);
    }
    pager(679, &annotation_page, (annotation_count + 4) / 5);
}
static void wikipedia_screen(void) {
    heading("Wikipedia");
    if (discover_busy())
        wrapped(28, 180, "Wikipedia wird geladen …", &editorial, 544, 3);
    else if (*discover_lookup_text()) {
        wrapped_raw(28, 139, discover_lookup_title(), &body, 544, 2);
        wrapped_raw(28, 230, discover_lookup_text(), &small, 544, 16);
        drawtext(28, 704, "Wikipedia · CC BY-SA", &caption, 544);
    } else
        wrapped(28, 180, "Kein Wikipedia-Eintrag gefunden.", &editorial, 544, 3);
}

static int annotations_write(void) {
    if (current < 0)
        return 0;
    char path[2048];
    snprintf(path, sizeof(path), "%s/books/%016llx.notes-v1", data_root,
             (unsigned long long)library[current].id);
    return atomic_write(path, annotations, sizeof(Annotation) * (size_t)annotation_count);
}

#include "../../apps/host/compose.h"

static void app_leave(AppId app, void *ctx) {
    if(app==APP_SIGNAL)signal_save();
    (void)ctx;
    games_save17();
    if (app == APP_JOURNAL)
        journal_store17();
    companion_checkpoint16();
    if (app >= APP_2048)
        new_apps_save();
    if (app == APP_NOTES && keyboard && keyboard_target == 30)
        commit_keyboard();
    if (app == APP_NOTES)
        v1_save();


    if (app == APP_BLOCKS || app == APP_SNAKE)
        apps_pause();
    if (app == APP_READER) {
        apps_checkpoint();
        ui_save();
    }
    if (app == APP_STORE && store_stage) {
        discover_cancel();
        store_stage = 0;
    }
    if (app == APP_SETTINGS)
        apply_reading_settings();
}
static void route_to(int route) {
    if (!app_route_valid(route))
        return;
    if (reader_document_active && current == BOOK_LIMIT && route != 2 && route != 3 && route != 4 &&
        route != 14 && route != 104) {
        library_progress(&library[current], page, engine_count());
        save_anchor(&library[current], page, -1);
        engine_close();
        current = selected = -1;
        reader_document_active = 0;
    }
    if (app_for_route(route) != app_for_route(screen))
        system_loading(route);
    /* Legacy route IDs remain stable for persisted navigation and native tests. */
    if (app_runtime_route(&runtime, route))
        screen = route;
}
static int app_send(const AppIntent *intent) {
    if (!intent || !app_definition(intent->app) || intent->kind < INTENT_OPEN ||
        intent->kind > INTENT_SETTINGS)
        return 0;
    if (intent->kind == INTENT_BOOK && intent->app != APP_READER)
        return 0;
    if (intent->app == APP_READER && intent->kind == INTENT_OPEN) {
        int i = current >= 0 ? current : featured();
        if (i >= 0)
            return app_read_book(i);
        AppIntent fallback = {.app = APP_LIBRARY, .kind = INTENT_OPEN};
        return app_send(&fallback);
    }
    AppIntent owned = *intent;
    owned.text[sizeof(owned.text) - 1] = 0;
    owned.position.quote[sizeof(owned.position.quote) - 1] = 0;
    intent = &owned;
    if (intent->kind == INTENT_BOOK) {
        for (int i = 0; i < library_count; i++)
            if (library[i].id == intent->object_id) {
                int old = screen;
                if (!ui_open(i))
                    return 0;
                if (old != 2 && history_count < 64)
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
    if ((intent->app == APP_NUMSCRATCH || intent->app == APP_DRAWING))
        return 0;
    if (intent->app != app_for_route(screen))
        system_loading(app_definition(intent->app)->entry);
    AppId previous = app_for_route(screen);
    int old = screen;
    if (!app_runtime_deliver(&runtime, intent))
        return 0;
    if (previous != intent->app && history_count < 64)
        history[history_count++] = old;
    screen = runtime.route;
    if (intent->app == APP_STORE) {
        snprintf(store_query, sizeof(store_query), "%s", runtime.intent.text);
        open_store();
    } else if (intent->app == APP_WIKIPEDIA) {
        snprintf(wiki_query, sizeof(wiki_query), "%s", runtime.intent.text);
        wiki_page = 0;
        wiki_history_scan();
        if (*wiki_query) {
            wiki_article_language = options[OPT_LANGUAGE];
            discover_lookup(wiki_query, options[OPT_LANGUAGE]);
        }
    } else if (intent->app == APP_LIBRARY) {
        snprintf(query, sizeof(query), "%s",
                 intent->kind == INTENT_SEARCH ? runtime.intent.text : "");
        listpage = 0;
        update_view();
    } else if (intent->app == APP_SETTINGS && intent->kind == INTENT_SETTINGS) {
        if (app_route_valid(intent->section) && app_for_route(intent->section) == APP_SETTINGS)
            route_to(intent->section);
        if (screen == 12)
            wifi_open();
        if (screen == 40)
            storage_scan();
        if (screen == 39)
            extra_prepare(39);
    } else if (app_definition(intent->app)->entry >= 27)
        extra_prepare(screen);
    else if (intent->app == APP_CALENDAR)
        app_calendar_load();
    else if (screen >= 23 && screen <= 26)
        app_open(screen);
    if (intent->app == APP_NOTES && intent->object_id) {
        note_load((int)intent->object_id);
        route_to(30);
    }
    if (intent->app == APP_QUOTES)
        quotes_load();
    return 1;
}
#include "../../apps/host/dispatch.h"

void ui_feedback_draw(void) {
    if (pressed < 0 || pressed >= widget_count)
        return;
    Widget r = widgets[pressed];
    for (int y = r.y + 2; y < r.y + r.h - 2 && y < HEIGHT; y++)
        if (y >= 0)
            for (int x = r.x + 2; x < r.x + r.w - 2 && x < WIDTH; x++)
                if (x >= 0)
                    ui_pixels[y * WIDTH + x] = 255 - ui_pixels[y * WIDTH + x];
}

static int app_read_book(int index) {
    if (index < 0 || index >= library_count)
        return 0;
    AppIntent request = {.app = APP_READER, .kind = INTENT_BOOK, .object_id = library[index].id};
    return app_send(&request);
}
void ui_close(void) {
    cover_tiles_clear();
    free(weather_snapshot);
    weather_snapshot = NULL;
    weather_snapshot_valid = 0;
    chat_clear();
    companion_close16();
    u15_flush();
    polish_close();
    v1_close();
    news_cancel();
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

int ui_batch_input(void) {
    return keyboard || (screen == 30 && note_ink_mode);
}
