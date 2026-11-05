#ifndef LUMEN_LIBRARY_H
#define LUMEN_LIBRARY_H
#include <stdint.h>
#include <stddef.h>
#define BOOK_LIMIT 2048
typedef struct {
    uint64_t id, size;
    int64_t modified, added, lastread;
    char path[1536], title[384], author[192], format[12];
    int page, total, favorite, bookmark_count, protected_format;
    int bookmarks[32];
} Book;
typedef struct {
    int size, leading, margin, refresh, grid, sort, filter;
} Settings;
extern Book library[BOOK_LIMIT + 1];
extern int library_count;
extern const char *library_root, *data_root;
extern Settings settings;
enum {
    OPT_FONT,
    OPT_ALIGN,
    OPT_COVERS,
    OPT_AUTHORS,
    OPT_CLOCK,
    OPT_BATTERY,
    OPT_WIFI,
    OPT_INVERT,
    OPT_SWIPE,
    OPT_TAP,
    OPT_RESUME,
    OPT_LOCK,
    OPT_PORT,
    OPT_TIMEOUT,
    OPT_AUTOSTART,
    OPT_CONFIRM,
    OPT_LETTERSPACE,
    OPT_PARAGRAPH,
    OPT_STARTPAGE,
    OPT_LANGUAGE,
    OPT_LOCK_HINT,
    OPT_DATE,
    OPT_BATTERY_PERCENT,
    OPT_COVER_CROP,
    OPT_ROUNDING,
    OPT_PROGRESS,
    OPT_APPS,
    OPT_LOCK_BLUR,
    OPT_UI_REV,
    OPT_TAB_LABELS,
    OPT_AUTO_TIME,
    OPT_ZONE,
    OPT_REMOTE,
    OPT_KEYBOARD_LANGUAGE,
    OPT_COUNT
};
extern int options[OPT_COUNT];
void library_options_save(void);
int library_rename(int index, const char *name);
int library_delete(int index);
int library_metadata(int index, const char *title, const char *author);
int library_scan(void);
void library_progress(Book *b, int page, int total);
void library_save_book(Book *b);
void library_restore_state(Book *b);
int library_view(int *view, const char *query, int filter, int sort);
int library_supported(const Book *b);
int library_import_path(const Book *b, char *path, size_t size);
void library_settings_load(void);
void library_defaults(void);
void library_settings_save(void);
int library_last(void);
void library_cover_path(const Book *b, char *buf, size_t size);
int atomic_write(const char *path, const void *data, size_t size);
unsigned utf8_next(const char **s);
void utf8_fold(const char *in, char *out, size_t max);
int library_config_export(const char *path);
int library_config_import(const char *path);

#endif
