#include "../dictionary.h"
#define _GNU_SOURCE
#include "api.h"
#include "paint.h"
#include "../../apps/notifications/api.h"
#include "../amazon.h"
#include "../../apps/games/api.h"
#include "../../apps/calculator/api.h"
#include "../connect.h"
#include "../photo_image.h"
#include "../version.h"
#include "../wireless_update.h"
#include "../release_update.h"
#include "../app_packages.h"
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
#include "../../apps/wifi/shell_state.h"
#include "../../apps/store/shell_state.h"
#include "../../apps/notes/shell_state.h"
#include "../../apps/todo/shell_state.h"
#include "../../apps/wikipedia/shell_state.h"
#include "../../apps/settings/shell_state.h"
#include "../../apps/timer/shell_state.h"
#include "editor_state.h"
#include "../../apps/status_bar/shell_state.h"
#include "../../apps/library/state.h"
#include "../../apps/reader/state.h"
static AppRuntime runtime;
static int ui_route(void) { return runtime.route; }
static void app_leave(AppId app, void *ctx);
static void route_to(int route);
static int app_send(const AppIntent *intent);
static int package_ui_poll(void);
static void package_ui_draw(void),package_clock_tick(void);
static int package_clock_wait(void);
static int package_ui_input(const Input *);
static int package_ui_commit(int target,const char *text);
static void package_ui_choice(int choice);
static int app_read_book(int index);
static void extra_prepare(int target);
static int app_handle_input(const Input *in);
static void app_render(void);
static int selected = -1, current = -1;
static int battery_popup;

static pid_t job_pid16 = -1;
static int ink_pending16(void), job_pending16(void);
static void help_overlay16(void), companion_checkpoint16(void),
    companion_close16(void);
static int u17_notes_depth(void), text_editable_hit(const Input *);
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
static int u16_apps_commit(int, const char *), event_on16(int);
static void u16_calendar_choice(int);
static int keyboard_multiline(void);
static void u15_note_editor_touch(int,int);
static void u15_note_scroll(int);
static void editor_reset18(void);
static void tasks_draw18(void);
static int tasks_input18(const Input *in);
static int keyboard_intercept18(const Input *in);
static int u16_apps_draw(void), u16_apps_input(const Input *), u16_jobs_tick(void);
static void note_delete16(int);
static int calc_precision16 = 10;
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
static int store_complete(int done);
static void store_restore(void);

static int page, full = 1, sleeping;
static float zoom = 1, panx, pany;
static int selection_release;
static int64_t activity16;
static int annotations_write(void);
static void annotations_load(void);
static void annotations_draw(void);
static void annotations_screen(void);
static void wikipedia_screen(void);
static void selection_overlay(void);


typedef struct {
    int x, y, w, h, system, radius, sides, quiet;
} Widget;
static Widget widgets[1024], press_bounds;
static int widget_count, pressed = -1, press_canceled, down_y, history[64], history_count;
static int dropdown, drop_target,
    drop_count, drop_selected, drop_first, drop_visible;
static unsigned drop_generation;
static int drop_x, drop_y, drop_w, setting_category, confirm_delete;
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
static void notification_draw(void),notification_dismiss(void);
static int notification_visible(void),notification_poll(void),notification_input(const Input*,int);
static int receive_notification(const void*,size_t,int64_t);
static int notification_visible(void);
static void overlays(void);
void ui_feedback_draw(void);
static void modal_input(const Input *in);
static void refresh_library(void);
static void settings_input(const Input *in);
static const int lock_mode_ids[5];
static const int clock_style_ids[14];
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
static void app_insights(void);
static void key_outline(int x, int y, int w, int h);
enum { READER_TOOLBAR_END = 126, READER_CONTENTS_X = 468, READER_SETTINGS_X = 528 };
static void control_surface(int x, int y, int w, int h, int r, int color);
static void outline_surface(int x, int y, int w, int h, int r, int color);
static void segment_fill(int x, int y, int w, int h, int r, int color, int sides);
static void stroke(int x0, int y0, int x1, int y1, int thickness);
static void commit_keyboard(void);
static void note_editor_draw(void);
static void note_editor_touch(int x, int y);

static void keyboard_insert(const char *s);
static void keyboard_backspace(void);
static void system_screen(void);
static void games_init(int target);
static void app_chess(void);
static int games_input(const Input *in);
static int system_input(const Input *in);
static void new_apps_init(void);
static void new_apps_open(int route);
static void new_apps_save(void);
static void clock_face(int fullscreen);
static void new_apps_draw(void);
static int new_apps_input(const Input *);
static int new_apps_commit(int, const char *);
static int new_apps_poll(void);
static int new_apps_tick(void);

static void v1_init(void), v1_save(void), v1_open(int), v1_draw(void);
static int v1_input(const Input *), v1_commit(int, const char *), v1_tick(void), v1_poll(void);
static int v1_notes_input(const Input *), clipboard_input(const Input *);
static void v1_notes_draw(void), v1_note_tools(void);
static void clipboard_draw(void), v1_effect(void), v1_close(void);
static int v1_sleep_photo(void), v1_chess_tick(void);
static void v1_note_created(int), v1_timer_ring(int), v1_timer_started(void);
static int v1_sleep_poll(void), v1_sleep_tick(void);
static void header(void);
static int status_overlay_poll(void);
static int keyboard_package_poll(void);
static void package_back_results(void);
static void launcher_delete_confirm(int);
static void polish_photos_grid(void);
static void polish_tasks(void);
static int polish_tasks_input(const Input *);
static void polish_wiki_refetched(void);
static int article_wiki_open(void);
static void article_wiki_request(void);
static int wiki_reader_pending;
static void polish_wiki_history(void);
static void polish_view(void);
static void polish_draw(void), polish_open(int), polish_close(void);
static int polish_input(const Input *), polish_tick(void), polish_poll(void),
    polish_commit(int, const char *), polish_power(const Input *);
static void polish_wiki_archive(void), polish_wiki_saved(void);
static void polish_notes(void);
static int photo_grid_hit(int x, int y);
static void photo_grid_input(int x, int y);
static void photo_open_folder_menu(int assign, int index);
static void photo_folder_choice(int target, int choice);
static int sensor_graph_input(const Input *in);
static void photo_actions_open(int index);
static void photo_actions_choice(int choice);
static void photo_setting_choice(int target,int choice);
static void photo_create_folder(const char *name);
static void photo_duration_choice(int choice);
static int photo_gallery_active(void), photo_back(void), ha_back(void);
static void clock_show_face(void);
static void photo_app_leave(void);
static int polish_photo_full(void);
static int polish_notes_input(const Input *), polish_news_input(const Input *);
static void polish_news_draw(void), polish_photo(void), polish_brain(void), polish_language(void);
static int polish_photo_input(const Input *), polish_brain_input(const Input *),
    polish_language_input(const Input *);
static void register_builtin_views(void);
#include "keyboard_request.h"
static void open_keyboard(int target, const char *title, const char *value) {
    const KeyboardRequest request = {target, title, value};
    app_call("keyboard", &request, sizeof(request), extra_millis());
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
static int wrapped_raw(int x, int y, const char *s, const Font *f, int width, int lines) {
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
        drawtext_raw(x, y + row * lh, buf, f, width);
        s = p;
        while (*s == ' ' || *s == '\n')
            s++;
    }
    return row;
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
    stroke(x + w - 27, y + h / 2 - 3, x + w - 20, y + h / 2 + 4, 2);
    stroke(x + w - 20, y + h / 2 + 4, x + w - 13, y + h / 2 - 3, 2);
}
static void segments(int x, int y, int w, int h, const char *const *labels, int count,
                     int selected) {
    if (count < 1) return;
    control_surface(x, y, w, h, 13, UI_INK);
    for (int i = 0; i < count; i++) {
        int left = x + w * i / count;
        int right = x + w * (i + 1) / count;
        region(left, y, right - left, h);
        widget_shape(left, y, right-left, h, RADIUS_MEDIUM, 0, 1);
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
static int can_back(void) {
    return (app_package_installed(app_for_route(ui_route())) && strstr(app_process_frame(),"\"back\":true")) || editor_ui.keyboard || dropdown || confirm_delete || battery_popup ||
           (ui_route() == 29 && u17_notes_depth()) || (ui_route() == 10 && store_ui.opds_depth) ||
           history_count || ui_route() != 131;
}
static void progress(int x, int y, int w, int n, int total) {
    rect(x, y, w, 4, 180);
    if (total > 0)
        rect(x, y, (int)((long long)w * n / total), 4, 0);
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
static void cover_draw(Book *b, int x, int y, int w, int h, int fill) {
    if (!fill) rounded_clip_begin(x, y, w, h);
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
static void cover_size(Book *b, int x, int y, int w, int h) {
    cover_draw(b,x,y,w,h,0);
    if (ui_route() == 1 || ui_route() == 0)
        outline_surface(x,y,w,h,RADIUS_LARGE,198);
}
static void cover(Book *b, int x, int y, int scale) {
    cover_size(b, x, y, scale == 2 ? 52 : 120 / scale, scale == 2 ? 72 : 164 / scale);
}
#include "../../apps/library/layout.h"
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
static void pager(int y, int *state, int pages) {
    if (*state < 0)
        *state = 0;
    if (*state >= pages)
        *state = pages > 0 ? pages - 1 : 0;
    pagination_bind(state, pages, y, 1, NULL);
    if (pages <= 1)
        return;
    pager_draw(PAGER_X, y, PAGER_W, *state, pages);
}
static void system_loading(int target) {
    if (target != 2 && target != 79 && target != 40)
        return;
#ifndef LUMEN_HOST
    memset(ui_pixels, 255, sizeof(ui_pixels));
    aa_arc(300, 370, 15, 35, 310, 3);
    device_present_async(ui_pixels);
#endif
}
#include "widgets.h"
#include "typography.h"
#include "selection_menu.h"
#include "text_flow.h"
#include "list.h"
#include "titlebar.h"
#include "../../apps/library/shelf.h"
void ui_message(const char *s) {
    NotificationRequest request={.duration_ms=6000};
    snprintf(request.text,sizeof(request.text),"%s",tr(s));
    app_call("notifications",&request,sizeof(request),extra_millis());
}
void ui_set_screen(int n) {
    if (n == 0)
        n = 1;
    if (n == 104)
        n = 3;
    if (n == 6) {
        route_to(1);
        open_keyboard(0, "Bibliothek durchsuchen", library_ui.query);
    } else
        route_to(n);
    full = 1;
}
void ui_init(void) {
    app_packages_scan();app_packages_seed();
    app_overlay_send(APP_OVERLAY_KEYBOARD,"lumen.keyboard","{\"event\":\"prepare\"}");
    app_runtime_init(&runtime, app_leave, NULL);
    register_builtin_views();
    app_endpoint_register("notifications",receive_notification);
    app_endpoint_register("keyboard", keyboard_receive);
    library_settings_load();
    apps_init();
    extra_init();
    new_apps_init();
    if (options[OPT_LANGUAGE] < 0 || options[OPT_LANGUAGE] > 2)
        options[OPT_LANGUAGE] = 0;
    options[OPT_CONFIRM] = 1;
    settings.grid = 1;
    settings.filter = 0;

    library_ui.query[0] = 0;
    route_to(131);
    update_view();
    char sourcepath[2048];
    snprintf(sourcepath, sizeof(sourcepath), tr("%s/opds-source.txt"), data_root);
    FILE *source = fopen(sourcepath, "r");
    if (source) {
        if (fgets(store_ui.opds_source, sizeof(store_ui.opds_source), source))
            store_ui.opds_source[strcspn(store_ui.opds_source, "\r\n")] = 0;
        fclose(source);
    }
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
    app_pages_checkpoint();
    games_save17();
    if (ui_route() == 126)
        journal_store17();
    companion_checkpoint16();
    u15_flush();
    if (editor_ui.keyboard && (editor_ui.keyboard_target == 30 || editor_ui.keyboard_target == 60 || editor_ui.keyboard_target == 171 ||
                     editor_ui.keyboard_target == 174))
        extra_commit(editor_ui.keyboard_target, editor_ui.edit);
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
    reader_ui.selecting = selection_release = 0;
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
    if (b->size > 128*1024 || (index != BOOK_LIMIT && strcmp(b->format,"txt") && strcmp(b->format,"html"))) system_loading(2);
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
    reader_ui.chapter_count = engine_outline(reader_ui.chapters, 256);
    reader_ui.chapterpage = reader_ui.chaptertab = 0;
    zoom = 1;
    panx = pany = 0;
    route_to(2);
    reader_ui.reader_chrome = 0;
    full = 1;
    library_progress(b, page, engine_count());
    annotations_load();
    reader_ui.selecting = 0;
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
static void more_settings(void);
#include "../../apps/library/view.h"
#include "../../apps/reader/view.h"
#include "../../apps/reader/settings.h"
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
#include "../../apps/lock_screen/view.h"
static int updating;
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
void ui_draw(void) {
    if(updating) {
        int percent=0,step=release_progress(&percent);
        ui_update_progress(step,percent,release_version(),update_error);
        return;
    }
    widget_count = 0;
    pagination = (Pagination){0};
    if (editor_ui.keyboard && editor_ui.keyboard_target != 30 && editor_ui.keyboard_background_valid18 && !sleeping) {
        memcpy(ui_pixels, editor_ui.keyboard_background18, sizeof(ui_pixels));
        if (ui_route() != 2)
            app_view_draw(APP_STATUS_BAR);
        goto input_overlays18;
    }
    memset(ui_pixels, 255, sizeof(ui_pixels));
    if (sleeping) {
        app_view_draw(APP_LOCK_SCREEN);
        return;
    }
    if(app_package_installed(app_for_route(ui_route()))) {
        app_view_draw(APP_STATUS_BAR);
        package_ui_draw();
        if(editor_ui.keyboard){memcpy(editor_ui.keyboard_background18,ui_pixels,sizeof(ui_pixels));editor_ui.keyboard_background_valid18=1;}
        goto input_overlays18;
    }
    if(ui_route()==131) {
        heading("Startseite");
        wrapped(28,175,"Startseiten-App fehlt.",ui_font(UI_BODY),544,2);
        button_h(28,280,544,48,"Einstellungen",0);
        button_h(28,345,544,48,"Dateiübertragung",0);
        return;
    }
    if (ui_route() == 28 && clock_ui.clock_fullscreen) {
        clock_face(1);
        return;
    }
    if (polish_photo_full()) {
        polish_photo();
        return;
    }
    if (ui_route() != 2)
        app_view_draw(APP_STATUS_BAR);
    if (!u17_draw() && !u16_draw() && !u15_draw())
        app_render();
    help_overlay16();
    if (ui_route() == 79 || ui_route() == 125) {
        rect(250, 72, 266, 48, 255);
        dropdown_button(270, 74, 246, 44, ui_route() == 79 ? "Sternhimmel" : "Sonnensystem");
    }
    if (ui_route() == 2 && reader_ui.reader_chrome)
        app_view_draw(APP_STATUS_BAR);
    if (ui_route() == 2 && reader_ui.selecting == 2)
        selection_overlay();
    if (editor_ui.keyboard && editor_ui.keyboard_target != 30) {
        memcpy(editor_ui.keyboard_background18, ui_pixels, sizeof(ui_pixels));
        editor_ui.keyboard_background_valid18 = 1;
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
static unsigned navigation_rewinds;
static int ui_dispatch_input(const Input *in) {
    if(in->kind==INPUT_STATUS && app_process_pending())app_process_flush();
    if(in->kind==INPUT_APP && updating){app_process_poll();return 2;}
    if(in->kind==INPUT_OVERLAY) {
        if(in->x==APP_OVERLAY_KEYBOARD)
            return keyboard_package_poll() && editor_ui.keyboard && !updating && !sleeping ? 1 : 2;
        int changed=in->x==APP_OVERLAY_STATUS ? status_overlay_poll() : notification_poll();
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
    if (in->kind == INPUT_CANCEL) {
        notification_input(in,0);
        pressed = -1;
        press_canceled = 0;
        editor_ui.pressed_key = -1;
        return 1;
    }
    int visible=notification_visible();
    if(notification_input(in,visible)) { pressed=-1; return 1; }
    int pm = polish_power(in);
    if (pm)
        return pm == 2 ? 0 : pm == 3 ? 3 : 1;
    if (!sleeping && !app_package_installed(APP_PHOTOS) && in->kind == INPUT_HOLD && photo_gallery_active() &&
        !dropdown && !editor_ui.keyboard) {
        int index = photo_grid_hit(in->x, in->y);
        if (index >= 0) {
            photo_actions_open(index);
            selection_release = 1;
            return 1;
        }
    }
    if (battery_popup) {
        if (in->kind == INPUT_TAP || in->kind == INPUT_SWIPE) {
            battery_popup = 0;
            return 1;
        }
        if (in->kind == INPUT_DOWN || in->kind == INPUT_MOVE || in->kind == INPUT_HOLD)
            return 2;
    }
    if (!app_package_installed(APP_PHOTOS) && polish_photo_full() && in->kind == INPUT_TAP)
        return polish_photo_input(in);
    int input18 = keyboard_intercept18(in);
    if (input18 >= 0)
        return input18;
    int back_release = pressed >= 0 && press_bounds.system && press_bounds.x == 0 &&
                       press_bounds.y == 0 && press_bounds.w == 62 && in->y < STATUS_HEIGHT+8 && in->x < 70;
    int status_tap = in->kind == INPUT_TAP && (in->y < STATUS_HEIGHT || back_release) &&
                     (ui_route() != 2 || reader_ui.reader_chrome || app_package_installed(APP_READER));
    int system_pager = !sleeping && !editor_ui.keyboard && !dropdown && !confirm_delete &&
                       pagination_hit(in);
    int update17 = (status_tap || system_pager || app_package_installed(app_for_route(ui_route()))) ? -1 : u17_input(in);
    if (update17 >= 0) {
        pressed = -1;
        return update17;
    }
    int update16 = (status_tap || system_pager || app_package_installed(app_for_route(ui_route()))) ? -1 : u16_input(in);
    if (update16 >= 0) {
        pressed = -1;
        return update16;
    }
    int update15 = (status_tap || system_pager || app_package_installed(app_for_route(ui_route()))) ? -1 : u15_input(in);
    if (update15 >= 0) {
        pressed = -1;
        return update15;
    }
    if (!sleeping && clipboard_input(in))
        return 1;
    if (in->kind == INPUT_APP) {
        if (sleeping) { app_process_poll(); return 2; }
        return package_ui_poll() ? 1 : 2;
    }
    if (in->kind == INPUT_DOWN || in->kind == INPUT_TAP || in->kind == INPUT_SWIPE)
        activity16 = extra_millis();
    int network_poll = 0;
    Input normalized;
    if (!editor_ui.keyboard && in->kind == INPUT_TAP)
        apps_checkpoint();
    if (in->kind == INPUT_STATUS && sleeping)
        return v1_sleep_tick() ? 1 : 2;
    if (in->kind == INPUT_STATUS) {
        package_clock_tick();
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
            int needs_status = ui_route() == 12 ||
                               ((ui_route() == 25 || ui_route() == 26) && apps_poll_interval() >= 0);
            if (action && !needs_status)
                return action;
            if (service_busy() && !needs_status)
                return 2;
        }
    }
    if (!sleeping && ui_route() == 28 && clock_ui.clock_fullscreen && in->kind == INPUT_TAP) {
        clock_ui.clock_fullscreen = 0;
        return 1;
    }
    int intercepted = extra_intercept(in);
    if (intercepted >= 0)
        return intercepted;

    if (!sleeping && !app_package_installed(APP_LIBRARY) && !dropdown && (ui_route() == 0 || ui_route() == 1) && in->kind == INPUT_HOLD) {
        int index = -1, top = library_top(), begin = library_begin();
        if (featured() >= 0 && library_ui.listpage == 0 && in->x >= 28 && in->x < 572 &&
            in->y >= 80 && in->y < 312)
            index = featured();
        else if (in->x >= 28 && in->y >= top) {
            int col = (in->x - 28) / 188;
            int row = (in->y - top) / library_row_height();
            int cell = row * 3 + col;
            if (col < 3 && (in->x - 28) % 188 < 168 && (in->y - top) % library_row_height() < library_row_height()-4 &&
                cell < library_per() && begin + cell < library_ui.view_count)
                index = library_ui.view[begin + cell];
        }
        if (index >= 0) {
            static const char *pin[] = {"Anheften", "Löschen"};
            static const char *unpin[] = {"Loslösen", "Löschen"};
            selected = index;
            open_dropdown(901, library[index].favorite ? unpin : pin, 2, -1,
                          in->x - 40 < 12 ? 12 : in->x - 40, in->y + 14, 236);
            selection_release = 1;
            return 1;
        }
    }
    if (!app_package_installed(APP_MINES) && ui_route() == 23 && in->kind == INPUT_HOLD && !sleeping)
        return apps_input(in);
    if (!sleeping && !app_package_installed(APP_READER) && ui_route() == 2 && in->kind == INPUT_HOLD && !reader_ui.reader_chrome && in->y > 60 &&
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
    if (ui_route() == 2 && reader_ui.selecting == 1 &&
        (in->kind == INPUT_MOVE || in->kind == INPUT_TAP || in->kind == INPUT_SWIPE)) {
        if (in->kind == INPUT_MOVE && abs(in->x - reader_ui.selection_last_x) < 7 &&
            abs(in->y - reader_ui.selection_last_y) < 7)
            return 2;
        reader_ui.selection_last_x = in->x;
        reader_ui.selection_last_y = in->y;
        engine_select(page, reader_ui.selection_x - 28, reader_ui.selection_y - 12, in->x - 28, in->y - 12, 544, 728,
                      zoom, panx, pany, &reader_ui.selection);
        if (in->kind != INPUT_MOVE)
            reader_ui.selecting = reader_ui.selection.count ? 2 : 0;
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
        if (ui_route() == 2 ? (down_y < 48 || down_y >= 752) : (down_y < 130 || down_y >= 700))
            return 1;
    }
    if (!sleeping && !editor_ui.keyboard && !dropdown && !confirm_delete && !battery_popup &&
        pagination.state) {
        if (in->kind == INPUT_SWIPE && abs(in->dx) > abs(in->dy) &&
            (ui_route() != 2 || (options[OPT_SWIPE] && zoom <= 1))) {
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
        editor_ui.cursor_visible = 1;
        editor_ui.cursor_epoch = extra_millis();
    }
    if (in->kind == INPUT_STATUS) {
        if (ui_route() >= 20 && ui_route() <= 26 && !sleeping)
            return apps_input(in);
        if (ui_route() == 12 && wifi_ui.wifi_enabled) {
            char old[128];
            snprintf(old, sizeof(old), "%s", wifi_ui.wifi_ssid);
            network_status(wifi_ui.wifi_ssid, sizeof(wifi_ui.wifi_ssid));
            if (wifi_ui.wifi_scanning || strcmp(old, wifi_ui.wifi_ssid))
                network_results();
            wifi_ui.wifi_scanning = 0;
        }
        return ui_route() == 12 ? 1 : 2;
    }
    if (in->kind == INPUT_NETWORK) {
        char books[2048];
        snprintf(books,sizeof(books),"%s/books-changed",data_root);
        if (!unlink(books)) {
            if (ui_route()==0 || ui_route()==1) refresh_library();
            else library_ui.library_dirty=1;
        }
        app_packages_scan();
        if(!network_poll && app_for_route(ui_route())==APP_HOME &&
           app_package_installed(app_for_route(ui_route())))app_process_send("{\"event\":\"refresh\"}");
        char restart[2048];
        snprintf(restart,sizeof(restart),"%s/../.update/relaunch",data_root);
        if (!access(restart,F_OK)) {
            connect_stop();
            ui_save();
            return 0;
        }
        int done = discover_complete();
        if (network_poll && !done)
            return 2;
        if (ui_route() == 10 && store_ui.store_stage && done && store_complete(done))
            return 1;
        if (done == 3) {
            polish_wiki_refetched();
            extra_wiki_cache();
            if (ui_route() == 36)
                article_wiki_request();
            return 1;
        }
        if (done < 0) {
            char message[256];
            snprintf(message, sizeof(message), "%s", discover_error());
            if (ui_route() == 36 && wiki_refresh_failed())
                return 1;
            if (ui_route() == 36)
                extra_wiki_restore();
            ui_message(message);
        } else if (done == 2) {
            if (ui_route() == 2)
                library_ui.library_dirty = 1;
            else
                refresh_library();
            ui_message("Buch zur Bibliothek hinzugefügt.");
        } else if (done == 1) {
            store_ui.opds_page = 0;
            if (ui_route() == 10 && opds_count == 1 && opds_items[0].acquisition) {
                store_ui.opds_selected = 0;
                route_to(11);
                return 1;
            }
            if (ui_route() == 10 || ui_route() == 11)
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
        app_process_pause();
        apps_pause();
        extra_save();
        service_cancel();
        reader_ui.selecting = 0;
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
                if (device_wake()) {
                    sleeping = 0;
                    full = 1;
                }
            }
            was_sleeping = -1;
        }
        return 1;
    }
    if (in->kind == INPUT_WAKE) {
        if (sleeping) {
            sleeping = 0;
            full = 1;
            device_status(status_ui.battery, sizeof(status_ui.battery), status_ui.wifi, sizeof(status_ui.wifi));
            status_ui.battery_charging = device_charging();
            status_ui.status_time = status_ui.charging_checked = time(NULL);
            extra_tick();
        }
        return 1;
    }
    if (sleeping)
        return 2;
    int x = in->x, y = in->y;
    if (ui_route() == 2 && reader_ui.selecting == 2 && in->kind == INPUT_TAP) {
        int action = selection_menu_action(READER_SELECTION_Y, x, y);
        if (action > 0 && action < 4) {
            if (action == 3) {
                if (reader_ui.annotation_count >= 128)
                    ui_message("Maximal 128 Markierungen pro Buch.");
                else {
                    Annotation *a = &reader_ui.annotations[reader_ui.annotation_count++];
                    memset(a, 0, sizeof(*a));
                    engine_position(page, &a->position);
                    snprintf(a->text, sizeof(a->text), "%s", reader_ui.selection.text);
                    snprintf(a->position.quote, sizeof(a->position.quote), "%s", reader_ui.selection.text);
                    if (!annotations_write()) {
                        reader_ui.annotation_count--;
                        ui_message("Speichern fehlgeschlagen.");
                    } else
                        ui_message("Markierung gespeichert.");
                }
            } else if (action == 1) {
                AppIntent request = {.app = APP_WIKIPEDIA, .kind = INTENT_SEARCH};
                snprintf(request.text, sizeof(request.text), "%s", reader_ui.selection.text);
                app_send(&request);
            } else {
                const char *definition = dictionary_lookup(reader_ui.selection.text, options[OPT_LANGUAGE], NULL);
                char translation[420];
                if (definition) ui_message(definition);
                else if (dictionary_translate(reader_ui.selection.text, options[OPT_LANGUAGE], translation,
                                              sizeof(translation))) ui_message(translation);
                else ui_message("Kein Eintrag im Offline-Wörterbuch.");
            }
        }
        reader_ui.selecting = 0;
        pressed = -1;
        return 1;
    }
    if (ui_route() == 14 && !confirm_delete && in->kind == INPUT_TAP && y >= 155 && y < 655) {
        int i = reader_ui.annotation_page * 5 + (y - 155) / 100;
        if (i < reader_ui.annotation_count) {
            if (x >= 510) {
                confirm_delete = 2;
                reader_ui.annotation_delete = i;
            } else {
                page = engine_restore(&reader_ui.annotations[i].position);
                route_to(2);
                reader_ui.reader_chrome = 0;
            }
        }
        return 1;
    }
    if (ui_route() == 14 && !confirm_delete && in->kind == INPUT_TAP && y >= 745 && y < 789) {
        if (x < 300 && reader_ui.annotation_page)
            reader_ui.annotation_page--;
        else if (x >= 300 && (reader_ui.annotation_page + 1) * 5 < reader_ui.annotation_count)
            reader_ui.annotation_page++;
        return 1;
    }

    if(app_package_installed(app_for_route(ui_route())) && !editor_ui.keyboard && !dropdown && !confirm_delete &&
       ((in->kind==INPUT_TAP && y>=STATUS_HEIGHT) || in->kind==INPUT_SWIPE || in->kind==INPUT_HOLD))return package_ui_input(in);
    if (in->kind == INPUT_TAP && ui_route() == 2 && reader_ui.reader_chrome && y >= READER_TOOLBAR_END) {
        reader_ui.reader_chrome = 0;
        return 1;
    }
    if (in->kind == INPUT_TAP && ui_route() == 2 && !reader_ui.reader_chrome && (y < 78 || y >= 722)) {
        reader_ui.reader_chrome = 1;
        return 1;
    }
    if (in->kind == INPUT_TAP && y < STATUS_HEIGHT && (ui_route() != 2 || reader_ui.reader_chrome || app_package_installed(APP_READER)) &&
        !editor_ui.keyboard && !dropdown && !confirm_delete && !battery_popup) {
        int date = x >= status_ui.status_date_x && x < status_ui.status_date_x + status_ui.status_date_w;
        int clock = x >= status_ui.status_clock_x && x < status_ui.status_clock_x + status_ui.status_clock_w;
        if (date || clock) {
            if (clock) clock_show_face();
            AppIntent request = {.app = date ? APP_CALENDAR : APP_TIMER, .kind = INTENT_OPEN};
            app_send(&request);
            return 1;
        }
    }
    if (in->kind == INPUT_TAP && y < STATUS_HEIGHT && (ui_route() != 2 || reader_ui.reader_chrome || app_package_installed(APP_READER)) &&
        !editor_ui.keyboard && !dropdown && !confirm_delete && !battery_popup && status_ui.status_timer_x >= 0 &&
        x >= status_ui.status_timer_x && x < status_ui.status_timer_x + status_ui.status_timer_width) {
        AppIntent request = {.app = APP_TIMER, .kind = INTENT_OPEN};
        app_send(&request);
        return 1;
    }
    if (in->kind == INPUT_TAP && y < STATUS_HEIGHT && (ui_route() != 2 || reader_ui.reader_chrome || app_package_installed(APP_READER))) {
        if (x < 62 && ui_route() == 3 && !editor_ui.keyboard && !dropdown && !confirm_delete)
            apply_reading_settings();
        if (x < 62 && !app_package_installed(APP_NOTES) && ui_route() == 29 && u17_notes_depth() && !editor_ui.keyboard && !dropdown) {
            u17_notes_up();
            return 1;
        }
        if (x < 62 && !editor_ui.keyboard && !dropdown && !confirm_delete && !app_package_installed(app_for_route(ui_route())) && (photo_back() || recipes_back() || ha_back()))
            return 1;
        if(x<62 && app_package_installed(app_for_route(ui_route())) && strstr(app_process_frame(),"\"back\":true") && !editor_ui.keyboard && !dropdown){app_process_send("{\"event\":\"back\"}");return 2;}
        if (x < 62 && can_back()) {
            apps_pause();
            if (editor_ui.keyboard || dropdown || confirm_delete) {
                if (editor_ui.keyboard && keyboard_multiline())
                    commit_keyboard();
                editor_ui.keyboard = dropdown = confirm_delete = 0;
                return 1;
            }
            if (ui_route() == 10 && store_ui.opds_depth) {
                discover_cancel();
                if (!strcmp(store_ui.opds_history[--store_ui.opds_depth], "lumen:search"))
                    store_restore();
                else
                    discover_start(store_ui.opds_history[store_ui.opds_depth], NULL);
                return 1;
            }
            int target = 131;
            navigation_rewinds++;
            while (history_count) {
                int candidate = history[--history_count];
                if (candidate != ui_route() && app_route_valid(candidate)) {
                    target = candidate;
                    break;
                }
            }
            route_to(target);
            package_back_results();
            if (ui_route() == 0)
                route_to(1);
            if ((ui_route() == 2 || ui_route() == 4 || ui_route() == 5) && current < 0)
                route_to(1);
            if (ui_route() == 1) {
                if (library_ui.library_dirty) refresh_library();
                else update_view();
            }
            return 1;
        }
        if (status_ui.status_wifi_x >= 0 && x >= status_ui.status_wifi_x - 5 && x < status_ui.status_wifi_x + 39 &&
            ui_route() != 12) {
            if (history_count < 64)
                history[history_count++] = ui_route();
            apps_pause();
            route_to(12);
            wifi_open();
            return 1;
        }
    }
    if (in->kind == INPUT_TAP && y < STATUS_HEIGHT && !editor_ui.keyboard && !dropdown && !confirm_delete &&
        (options[OPT_BATTERY] || options[OPT_BATTERY_PERCENT]) &&
        status_ui.status_battery_x >= 0 && x >= status_ui.status_battery_x - 6 && x < status_ui.status_battery_right + 6 &&
        (ui_route() != 2 || reader_ui.reader_chrome || app_package_installed(APP_READER))) {
        battery_popup = 1;
        return 1;
    }
    if (editor_ui.keyboard || dropdown || confirm_delete) {
        modal_input(in);
        return 1;
    }
    if (ui_route() == 0)
        route_to(1);
    return app_handle_input(in);
}
int ui_input(const Input *in) {
    int from = ui_route(), depth = history_count;
    int package_keyboard=editor_ui.keyboard && editor_ui.keyboard_target==2000;
    unsigned rewinds = navigation_rewinds;
    int result = ui_dispatch_input(in);
    int attached=editor_ui.keyboard && editor_ui.keyboard_target==2000;
    if(attached!=package_keyboard && from==ui_route() && app_package_installed(app_for_route(from)))
        app_process_send(attached ? "{\"event\":\"viewport\",\"bottom\":395}" :
                                    "{\"event\":\"viewport\",\"bottom\":800}");
    if (in->kind == INPUT_TAP && from && from != ui_route() &&
        app_for_route(from) == app_for_route(ui_route()) &&
        rewinds == navigation_rewinds && depth == history_count && depth < 64 &&
        (!depth || history[depth-1] != from))
        history[history_count++] = from;
    return result;
}

/* Shared overlays: dropdowns, keyboard and destructive-action confirmation. */
#include "text_editor.h"
#include "keyboard_surface.h"
static void overlays(void) {
    if (editor_ui.keyboard || dropdown || confirm_delete) {
        int keep = 0;
        for (int i = 0; i < widget_count; i++)
            if (widgets[i].system)
                widgets[keep++] = widgets[i];
        widget_count = keep;
    }
    if (editor_ui.keyboard) {
        app_view_draw(APP_KEYBOARD);
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
    if (confirm_delete) {
        control_surface(20, 270, 554, 260, 12, 0);
        control_surface(22, 272, 550, 256, 10, 255);
        drawtext(42, 292, confirm_delete == 2 ? "Markierung löschen?" : "Buch wirklich löschen?",
                 &body, 510);
        wrapped(42, 340,
                confirm_delete == 2 ? reader_ui.annotations[reader_ui.annotation_delete].text : library[selected].title,
                &small, 500, 2);
        button(42, 446, 235, "Abbrechen", 0);
        button(309, 446, 235, "Löschen", 0);
    }
    if (battery_popup) {
        control_surface(428, 50, 148, 84, RADIUS_MEDIUM, 255);
        key_outline(428, 50, 148, 84);
        char b[32];
        snprintf(b, sizeof(b), "%s%%", status_ui.battery);
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
    library_ui.library_dirty = 0;
    update_view();
}
static void commit_keyboard(void) {
    if (editor_ui.keyboard_target == TEXT_INPUT_PHOTO_FOLDER) {
        photo_create_folder(editor_ui.edit);
        editor_ui.keyboard = 0;
        pressed = -1;
        return;
    }
    if (editor_ui.keyboard_target >= 30) {
        int previous_target = editor_ui.keyboard_target;
        extra_commit(editor_ui.keyboard_target, editor_ui.edit);
        if (editor_ui.keyboard_target == previous_target) {
            memset(editor_ui.edit, 0, sizeof(editor_ui.edit));
            editor_ui.keyboard = 0;
        }
        pressed = -1;
        return;
    }
    if (editor_ui.keyboard_target == 8) {
        apps_note_commit(editor_ui.edit);
        editor_ui.keyboard = 0;
        pressed = -1;
        return;
    }
    if (editor_ui.keyboard_target == 6) {
        snprintf(store_ui.store_query, sizeof(store_ui.store_query), "%s", editor_ui.edit);
        store_search();
    } else if (editor_ui.keyboard_target == 5 || editor_ui.keyboard_target == 32) {
        if (network_connect(wifi_ui.wifi_selected, editor_ui.edit))
            ui_message("Verbindung wird hergestellt …");
        else
            ui_message(network_error());
        memset(editor_ui.edit, 0, sizeof(editor_ui.edit));
    } else if (editor_ui.keyboard_target == 4) {
        snprintf(store_ui.opds_source, sizeof(store_ui.opds_source), tr("%s"), editor_ui.edit);
        char p[2048];
        snprintf(p, sizeof(p), tr("%s/opds-source.txt"), data_root);
        atomic_write(p, store_ui.opds_source, strlen(store_ui.opds_source));
    } else if (editor_ui.keyboard_target == 0) {
        snprintf(library_ui.query, sizeof(library_ui.query), tr("%s"), editor_ui.edit);
        library_ui.listpage = 0;
        update_view();
    } else if (selected >= 0) {
        if (editor_ui.keyboard_target == 1) {
            if (current == selected) {
                ui_save();
                engine_close();
                current = -1;
            }
            if (!library_rename(selected, editor_ui.edit))
                ui_message("Umbenennen fehlgeschlagen. Name muss frei sein.");
        } else if (editor_ui.keyboard_target == 2) {
            if (!library_metadata(selected, editor_ui.edit, library[selected].author))
                ui_message("Titel konnte nicht gespeichert werden.");
        } else if (editor_ui.keyboard_target == 3) {
            if (!library_metadata(selected, library[selected].title, editor_ui.edit))
                ui_message("Autor konnte nicht gespeichert werden.");
        }
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
        if (editor_ui.keyboard_target == 30) u15_note_scroll(in->dy > 0 ? -8 : 8);
        else editor_scroll18(in->dy > 0 ? -8 : 8);
        return;
    }
    if (in->kind != INPUT_TAP)
        return;
    int x = in->x, y = in->y;
    if (confirm_delete == 2) {
        if (y >= 446 && y < 494) {
            if (x >= 309 && reader_ui.annotation_delete < reader_ui.annotation_count) {
                Annotation backup = reader_ui.annotations[reader_ui.annotation_delete];
                memmove(reader_ui.annotations + reader_ui.annotation_delete, reader_ui.annotations + reader_ui.annotation_delete + 1,
                        (size_t)(reader_ui.annotation_count - reader_ui.annotation_delete - 1) * sizeof(Annotation));
                reader_ui.annotation_count--;
                if (!annotations_write()) {
                    memmove(reader_ui.annotations + reader_ui.annotation_delete + 1, reader_ui.annotations + reader_ui.annotation_delete,
                            (size_t)(reader_ui.annotation_count - reader_ui.annotation_delete) * sizeof(Annotation));
                    reader_ui.annotations[reader_ui.annotation_delete] = backup;
                    reader_ui.annotation_count++;
                    ui_message("Speichern fehlgeschlagen.");
                }
                if (reader_ui.annotation_page && reader_ui.annotation_page * 5 >= reader_ui.annotation_count)
                    reader_ui.annotation_page--;
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
            if(drop_target==2001){package_ui_choice(choice);}
            else if(drop_target==2004){launcher_delete_confirm(choice);}
            else if (drop_target == 941 || drop_target == 942) {
                photo_setting_choice(drop_target,choice);
            } else if (drop_target == 940) {
                photo_actions_choice(choice);
            } else if (drop_target >= 931 && drop_target <= 950) {
                u17_choice(drop_target, choice);
            } else if (drop_target >= 910 && drop_target <= 930) {
                u16_dropdown(drop_target, choice);
            } else if (drop_target == 903 || drop_target == 904) {
                photo_folder_choice(drop_target, choice);
            } else if (drop_target == 905) {
                photo_duration_choice(choice);
            } else if (drop_target == 902) {
                clock_ui.clock_style = clock_style_ids[choice];
                extra_save();
            } else if (drop_target == 901 && selected >= 0 && selected < library_count) {
                if (choice == 0) {
                    library[selected].favorite ^= 1;
                    library_save_book(&library[selected]);
                } else {
                    confirm_delete = 1;
                }
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
                    options[option] = option == OPT_LOCK ? lock_mode_ids[choice] : choice;
                if (option == OPT_ZONE)
                    extra_zone();
                engine_options(options[OPT_FONT], options[OPT_ALIGN], options[OPT_LETTERSPACE],
                               options[OPT_PARAGRAPH]);
            }
            library_ui.listpage = 0;
            update_view();
            library_settings_save();
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
#include "../../apps/settings/preferences.h"
#include "../../apps/store/entry.h"
#include "../../apps/settings/options.h"
#include "../../apps/store/view.h"
#include "../../apps/wifi/app.h"
int ui_poll_interval(void) {
    if(updating)return update_error ? -1 : 400;
    if (sleeping)
        return v1_sleep_poll();
    int wait = ui_route() >= 20 ? apps_poll_interval() : (ui_route() == 12 ? 2000 : -1),
        other = extra_poll();
    int tick=package_clock_wait();
    if(tick>=0 && (other<0 || tick<other))other=tick;
    int np = new_apps_poll();
    int pp = polish_poll();
    if (pp >= 0 && (np < 0 || pp < np))
        np = pp;
    int vp = v1_poll();
    if (vp >= 0 && (np < 0 || vp < np))
        np = vp;
    if (np >= 0 && (other < 0 || np < other))
        other = np;
    int status_wait = status_ui.battery_charging ? 5000 : 30000;
    if (job_pending16())
        status_wait = 100;
    else if (ink_pending16())
        status_wait = 1000;
    if (wait < 0 || status_wait < wait)
        wait = status_wait;
    return other >= 0 && (wait < 0 || other < wait) ? other : wait;
}

#include "../../apps/reader/annotations_view.h"
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

#include "../../apps/reader/annotations_store.h"
#include "../../apps/host/compose.h"
void ui_paint_heading(const char *text, int actions) { heading_actions(text, actions); }
void ui_paint_icon_button(int x, int y, int id) { icon_button(x, y, id); }
void ui_paint_surface(int x,int y,int w,int h,int radius,int gray,int outline) {
    control_surface(x,y,w,h,radius,outline ? 0 : gray);
    if (outline) control_surface(x+outline,y+outline,w-2*outline,h-2*outline,radius-outline,gray);
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
static void register_builtin_views(void) {
    app_view_register(APP_STATUS_BAR, header, NULL);
    app_view_register(APP_LOCK_SCREEN, lock_screen_draw, NULL);
    app_view_register(APP_KEYBOARD, keyboard_draw18, NULL);


}


int ui_prepare_home(void) {
    if(app_package_deleted(APP_HOME))return 1;
    if(!app_process_open(APP_HOME))return 0;
    int64_t until=extra_millis()+1800;
    do {
        package_ui_poll();
        if(strstr(app_process_frame(),"\"ready\":true"))return 1;
        if(*app_process_error())return 0;
        struct pollfd fd={.fd=app_process_fd(),.events=POLLIN};
        poll(&fd,1,20);
    } while(extra_millis()<until);
    return 0;
}

static void app_leave(AppId app, void *ctx) {
    app_process_pause();
    app_page_checkpoint(app);
    if (app == APP_PHOTOS) photo_app_leave();
    (void)ctx;
    games_save17();
    if (app == APP_JOURNAL)
        journal_store17();
    companion_checkpoint16();
    if (app >= APP_2048)
        new_apps_save();
    if (app == APP_NOTES && editor_ui.keyboard && editor_ui.keyboard_target == 30)
        commit_keyboard();
    if (app == APP_NOTES)
        v1_save();
    if (app == APP_TASKS) memset(task_recent_done,0,sizeof(task_recent_done));


    if (app == APP_BLOCKS || app == APP_SNAKE)
        apps_pause();
    if (app == APP_READER) {
        apps_checkpoint();
        ui_save();
    }
    if (app == APP_STORE && store_ui.store_stage) {
        discover_cancel();
        store_ui.store_stage = 0;
    }
    if (app == APP_READER && ui_route() == 3)
        apply_reading_settings();
}
static void route_to(int route) {
    if (!app_route_valid(route))
        return;
    if (reader_ui.reader_document_active && current == BOOK_LIMIT && route != 2 && route != 3 && route != 4 &&
        route != 14 && route != 104) {
        library_progress(&library[current], page, engine_count());
        save_anchor(&library[current], page, -1);
        engine_close();
        current = selected = -1;
        reader_ui.reader_document_active = 0;
    }
    if (route != 2 && app_for_route(route) != app_for_route(ui_route()))
        system_loading(route);
    /* Legacy route IDs remain stable for persisted navigation and native tests. */
    app_runtime_route(&runtime, route);
}
static int app_send(const AppIntent *intent) {
    if (!intent || !app_definition(intent->app) || intent->kind < INTENT_OPEN ||
        intent->kind > INTENT_SETTINGS)
        return 0;
    if (intent->kind == INTENT_BOOK && intent->app != APP_READER)
        return 0;
    if (intent->app == APP_READER && intent->kind == INTENT_OPEN && !app_package_installed(APP_READER)) {
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
                int old = ui_route();
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
    if (intent->app != app_for_route(ui_route()))
        if (intent->app != APP_READER) system_loading(app_definition(intent->app)->entry);
    AppId previous = app_for_route(ui_route());
    int old = ui_route();
    if (!app_runtime_deliver(&runtime, intent))
        return 0;
    if (previous != intent->app && history_count < 64)
        history[history_count++] = old;
    if(app_package_installed(intent->app)){
        int ok=app_process_open(intent->app);char*event=NULL;size_t n=0;FILE*f=open_memstream(&event,&n);
        fprintf(f,"{\"event\":\"intent\",\"kind\":%d,\"object\":%llu,\"text\":",intent->kind,(unsigned long long)intent->object_id);json_escape16(f,intent->text);fputc('}',f);fclose(f);if(ok)app_process_send(event);free(event);return ok;
    }
    app_process_pause();
    if (intent->app == APP_STORE) {
        snprintf(store_ui.store_query, sizeof(store_ui.store_query), "%s", runtime.intent.text);
        open_store();
    } else if (intent->app == APP_WIKIPEDIA) {
        snprintf(wiki_query, sizeof(wiki_query), "%s", runtime.intent.text);
        wiki_page = 0;
        wiki_history_scan();
        if (*wiki_query) {
            wiki_ui.wiki_article_language = options[OPT_LANGUAGE];
            discover_lookup(wiki_query, options[OPT_LANGUAGE]);
        }
    } else if (intent->app == APP_LIBRARY) {
        if (library_ui.library_dirty) refresh_library();
        snprintf(library_ui.query, sizeof(library_ui.query), "%s",
                 intent->kind == INTENT_SEARCH ? runtime.intent.text : "");
        library_ui.listpage = 0;
        update_view();
    } else if (intent->app == APP_SETTINGS && intent->kind == INTENT_SETTINGS) {
        if (app_route_valid(intent->section) && app_for_route(intent->section) == APP_SETTINGS)
            route_to(intent->section);
        if (ui_route() == 12)
            wifi_open();
        if (ui_route() == 40)
            storage_scan();
        if (ui_route() == 39)
            extra_prepare(39);
    } else if (intent->app == APP_RECIPES) {
        recipe_search16 = 0;
        companion_page16 = 0;
        recipe_archive16();
    } else if (app_definition(intent->app)->entry >= 27)
        extra_prepare(ui_route());
    else if (intent->app == APP_CALENDAR) {
        time_t now=time(NULL);
        struct tm today;
        localtime_r(&now,&today);
        calendar_year=today.tm_year+1900;
        calendar_month=today.tm_mon+1;
        calendar_day=today.tm_mday;
        app_calendar_load();
    }
    else if (ui_route() >= 23 && ui_route() <= 26)
        app_open(ui_route());
    if (intent->app == APP_NOTES && intent->object_id) {
        note_load((int)intent->object_id);
        route_to(30);
    }
    return 1;
}
#include "../../apps/host/dispatch.h"

void ui_feedback_draw(void) {
    if (pressed < 0 || pressed >= widget_count)
        return;
    Widget r = widgets[pressed];
    if (r.quiet) return;
    /* Invert the actual inset of the shared curve and preserve the outer rim. */
    int inset = 2, w = r.w - 2*inset, h = r.h - 2*inset;
    if (w <= 0 || h <= 0) return;
    int radius = r.radius;
    if (radius > r.w/2) radius = r.w/2;
    if (radius > r.h/2) radius = r.h/2;
    int profile = rounding_profile();
    if (profile && radius*2 >= (r.w < r.h ? r.w : r.h)-1) profile = CORNER_CIRCLE;
    radius -= inset;
    const unsigned char *mask = radius > 0 && profile ? corner_mask(radius, profile) : NULL;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int px = r.x + inset + x, py = r.y + inset + y;
            if ((unsigned)px >= WIDTH || (unsigned)py >= HEIGHT) continue;
            int cx = x < w/2 ? x : w-1-x, cy = y < h/2 ? y : h-1-y;
            int side = x < w/2 ? 1 : 2;
            int alpha = mask && (r.sides & side) && cx < radius && cy < radius
                            ? mask[cy*radius+cx] : CORNER_ALPHA;
            unsigned char *pixel = &ui_pixels[py*WIDTH+px];
            *pixel = corner_blend(*pixel, 255-*pixel, alpha);
        }
    }
}

static int app_read_book(int index) {
    if (index < 0 || index >= library_count)
        return 0;
    AppIntent request = {.app = APP_READER, .kind = INTENT_BOOK, .object_id = library[index].id};
    return app_send(&request);
}
void ui_close(void) {
    app_process_close();
    release_cancel();
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
