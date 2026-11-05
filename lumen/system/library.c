#define _GNU_SOURCE
#include "library.h"
#include "defaults.h"
#include "engine.h"
#include "amazon.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
Book library[BOOK_LIMIT + 1];
int library_count;
const char *library_root = "/mnt/us/documents", *data_root = "/mnt/us/lumen/data";
Settings settings = LUMEN_DEFAULT_SETTINGS;
int options[OPT_COUNT] = LUMEN_DEFAULT_OPTIONS;
static Book *cache;
static int cache_count, index_changed;
unsigned utf8_next(const char **s) {
    unsigned char c = (unsigned char)**s;
    if (!c)
        return 0;
    (*s)++;
    if (c < 128)
        return c;
    unsigned cp;
    int n;
    if ((c & 224) == 192) {
        cp = c & 31;
        n = 1;
    } else if ((c & 240) == 224) {
        cp = c & 15;
        n = 2;
    } else if ((c & 248) == 240) {
        cp = c & 7;
        n = 3;
    } else
        return 0xfffd;
    for (int i = 0; i < n; i++) {
        unsigned char d = (unsigned char)**s;
        if ((d & 192) != 128)
            return 0xfffd;
        (*s)++;
        cp = (cp << 6) | (d & 63);
    }
    if ((n == 1 && cp < 128) || (n == 2 && cp < 2048) || (n == 3 && cp < 65536) || cp > 0x10ffff ||
        (cp >= 0xd800 && cp <= 0xdfff))
        return 0xfffd;
    /* macOS filenames commonly arrive in decomposed form. Compose German umlauts here. */
    return cp;
}
void utf8_fold(const char *in, char *out, size_t max) {
    size_t n = 0;
    while (*in && n + 1 < max) {
        unsigned c = utf8_next(&in);
        if (c >= 0x300 && c <= 0x36f)
            continue;
        if (c >= 192 && c <= 222 && c != 215)
            c += 32;
        if (c == 228 || c == 224 || c == 225 || c == 226)
            c = 'a';
        if (c == 246 || c == 243 || c == 242 || c == 244)
            c = 'o';
        if (c == 252 || c == 250 || c == 249 || c == 251)
            c = 'u';
        if (c == 233 || c == 232 || c == 234)
            c = 'e';
        if (c == 223) {
            out[n++] = 's';
            if (n + 1 < max)
                out[n++] = 's';
            continue;
        }
        if (c < 128)
            out[n++] = (char)tolower(c);
        else
            out[n++] = '?';
    }
    out[n] = 0;
}
static uint64_t hashpath(const char *s) {
    uint64_t h = 14695981039346656037ULL;
    while (*s) {
        h ^= (unsigned char)*s++;
        h *= 1099511628211ULL;
    }
    return h;
}
int atomic_write(const char *path, const void *data, size_t size) {
    char tmp[2048];
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp))
        return 0;
    int fd = open(tmp, O_CREAT | O_TRUNC | O_WRONLY, 0600);
    if (fd < 0)
        return 0;
    size_t done = 0;
    while (done < size) {
        ssize_t n = write(fd, (const char *)data + done, size - done);
        if (n <= 0) {
            close(fd);
            unlink(tmp);
            return 0;
        }
        done += (size_t)n;
    }
    int ok = fsync(fd) == 0;
    if (close(fd))
        ok = 0;
    if (ok)
        ok = rename(tmp, path) == 0;
    if (!ok)
        unlink(tmp);
    return ok;
}
void library_cover_path(const Book *b, char *buf, size_t size) {
    snprintf(buf, size, "%s/covers/%016llx.pgm", data_root, (unsigned long long)b->id);
}
static void statepath(const Book *b, char *buf, size_t n) {
    snprintf(buf, n, "%s/books/%016llx.state", data_root, (unsigned long long)b->id);
}
void library_save_book(Book *b) {
    char path[2048], buf[1024];
    statepath(b, path, sizeof(path));
    int n = snprintf(buf, sizeof(buf), "LUMEN2 %d %d %d %lld %d\n", b->page, b->total, b->favorite,
                     (long long)b->lastread, b->bookmark_count);
    for (int i = 0; i < b->bookmark_count && i < 32; i++)
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%d ", b->bookmarks[i]);
    atomic_write(path, buf, (size_t)n);
}
static void loadstate(Book *b) {
    char path[2048];
    statepath(b, path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f)
        return;
    long long last;
    int p, total, fav, nb;
    if (fscanf(f, "LUMEN2 %d %d %d %lld %d", &p, &total, &fav, &last, &nb) == 5 && p >= 0 &&
        total >= 0 && p < 1000000 && total < 1000000 && nb >= 0 && nb <= 32) {
        b->page = p;
        b->total = total;
        b->favorite = !!fav;
        b->lastread = last;
        b->bookmark_count = 0;
        for (int i = 0; i < nb; i++) {
            int mark;
            if (fscanf(f, "%d", &mark) != 1 || mark < 0 || mark >= 1000000)
                break;
            b->bookmarks[b->bookmark_count++] = mark;
        }
    }
    fclose(f);
}
void library_restore_state(Book *b) { loadstate(b); }
void library_progress(Book *b, int page, int total) {
    b->page = page;
    b->total = total;
    b->lastread = time(NULL);
    library_save_book(b);
}
int library_import_path(const Book *b, char *path, size_t size) {
    char source[2048];
    snprintf(source, sizeof(source), "%s/imports/%016llx.source", data_root,
             (unsigned long long)b->id);
    FILE *f = fopen(source, "r");
    if (!f)
        return 0;
    unsigned long long bytes = 0;
    long long modified = 0;
    int ok = fscanf(f, "%llu %lld", &bytes, &modified) == 2;
    fclose(f);
    if (!ok || bytes != b->size || modified != b->modified)
        return 0;
    snprintf(path, size, "%s/imports/%016llx.epub", data_root, (unsigned long long)b->id);
    struct stat st;
    return !stat(path, &st) && S_ISREG(st.st_mode) && st.st_size > 0;
}
int library_supported(const Book *b) {
    /* Huge dictionary databases need the original dictionary engine rather than full-book reflow.
     */
    if ((!strcasecmp(b->format, "azw") || !strcasecmp(b->format, "azw3") ||
         !strcasecmp(b->format, "kfx")) &&
        (strcasestr(b->title, "dictionary") || strstr(b->title, "wörterbuch") ||
         strstr(b->title, "Wörterbuch")))
        return 0;
    char imported[2048];
    if (library_import_path(b, imported, sizeof(imported)))
        return 1;
    if(amazon_can_prepare(b))return 1;
    if (b->protected_format)
        return 0;
    return !strcasecmp(b->format, "azw3") || !strcasecmp(b->format, "azw") ||
           !strcasecmp(b->format, "epub") || !strcasecmp(b->format, "pdf") ||
           !strcasecmp(b->format, "txt") || !strcasecmp(b->format, "html") ||
           !strcasecmp(b->format, "cbz") || !strcasecmp(b->format, "mobi");
}
static int known(const char *e) {
    const char *list[] = {"epub", "pdf", "txt", "html", "cbz", "mobi", "azw", "azw3", "kfx", NULL};
    for (int i = 0; list[i]; i++)
        if (!strcasecmp(e, list[i]))
            return 1;
    return 0;
}
static void scan(const char *dir, int depth) {
    if (depth > 12 || library_count >= BOOK_LIMIT)
        return;
    DIR *d = opendir(dir);
    if (!d)
        return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.')
            continue;
        char path[1536];
        if (snprintf(path, sizeof(path), "%s/%s", dir, e->d_name) >= (int)sizeof(path))
            continue;
        struct stat st;
        if (lstat(path, &st))
            continue;
        if (S_ISDIR(st.st_mode)) {
            size_t n = strlen(path);
            if (n > 4 && !strcmp(path + n - 4, ".sdr"))
                continue;
            scan(path, depth + 1);
            continue;
        }
        if (!S_ISREG(st.st_mode) || library_count >= BOOK_LIMIT)
            continue;
        const char *ext = strrchr(e->d_name, '.');
        if (!ext || !known(ext + 1) || !strcmp(e->d_name, "JAILBROKEN.txt"))
            continue;
        Book *b = &library[library_count++];
        memset(b, 0, sizeof(*b));
        b->id = hashpath(path);
        b->size = (uint64_t)st.st_size;
        b->modified = st.st_mtime;
        b->added = time(NULL);
        snprintf(b->path, sizeof(b->path), "%s", path);
        snprintf(b->format, sizeof(b->format), "%s", ext + 1);
        for (char *p = b->format; *p; p++)
            *p = (char)tolower((unsigned char)*p);
        int hit = 0;
        for (int i = 0; i < cache_count; i++)
            if (cache[i].id == b->id && !strcmp(cache[i].path, path) && cache[i].size == b->size &&
                cache[i].modified == b->modified) {
                *b = cache[i];
                hit = 1;
                break;
            }
        if (!hit) {
            index_changed = 1;
            snprintf(b->title, sizeof(b->title), "%s", e->d_name);
            char *dot = strrchr(b->title, '.');
            if (dot)
                *dot = 0;
            for (char *p = b->title; *p; p++)
                if (*p == '_')
                    *p = ' ';
            char title[384] = "", author[192] = "";
            if (!strcmp(b->format, "kfx"))
                b->protected_format = 1;
            if (!strcmp(b->format, "azw3") || !strcmp(b->format, "azw") ||
                !strcmp(b->format, "mobi")) {
                amazon_metadata(path, title, sizeof(title), author, sizeof(author),
                                &b->protected_format);
                if (*title)
                    snprintf(b->title, sizeof(b->title), "%s", title);
                if (*author)
                    snprintf(b->author, sizeof(b->author), "%s", author);
            }
            if (library_supported(b) &&
                engine_metadata(path, title, sizeof(title), author, sizeof(author))) {
                if (*title)
                    snprintf(b->title, sizeof(b->title), "%s", title);
                if (*author)
                    snprintf(b->author, sizeof(b->author), "%s", author);
            }
            if (!strcasecmp(b->format, "epub") || !strcasecmp(b->format, "pdf")) {
                char cover[2048];
                library_cover_path(b, cover, sizeof(cover));
                engine_thumbnail(path, cover);
            }
        }
        if (!*b->author && hit) {
            char imported[2048], title[384] = "", author[192] = "";
            if (library_import_path(b, imported, sizeof(imported)))
                engine_metadata(imported, title, sizeof(title), author, sizeof(author));
            if (!*author && (!strcmp(b->format, "azw") || !strcmp(b->format, "azw3") ||
                             !strcmp(b->format, "mobi")))
                amazon_metadata(path, title, sizeof(title), author, sizeof(author),
                                &b->protected_format);
            if (*author) {
                snprintf(b->author, sizeof(b->author), "%s", author);
                index_changed = 1;
            }
        }
        char hd[2048], original[2048];
        snprintf(hd, sizeof(hd), "%s/covers/%016llx.hi.pgm", data_root, (unsigned long long)b->id);
        if (access(hd, F_OK)) {
            int ready = 0;
            if (library_import_path(b, original, sizeof(original)))
                ready = engine_thumbnail_hd(original, hd);
            else if (!strcmp(b->format, "epub") || !strcmp(b->format, "pdf"))
                ready = engine_thumbnail_hd(b->path, hd);
            if (!ready) {
                const char *asin = strchr(e->d_name, 'B');
                while (asin) {
                    char code[11];
                    int valid = strlen(asin) >= 10;
                    for (int j = 0; j < 10 && valid; j++)
                        valid = isalnum((unsigned char)asin[j]);
                    if (valid) {
                        memcpy(code, asin, 10);
                        code[10] = 0;
                        snprintf(original, sizeof(original),
                                 "%s/../system/thumbnails/thumbnail_%s_EBOK_portrait.jpg",
                                 library_root, code);
                        if (!access(original, R_OK) && engine_image_fit(original, hd, 360, 492))
                            break;
                    }
                    asin = strchr(asin + 1, 'B');
                }
            }
        }
        char cp[2048];
        library_cover_path(b, cp, sizeof(cp));
        if (access(cp, F_OK)) {
            const char *scan = e->d_name;
            while ((scan = strchr(scan, 'B'))) {
                char asin[11];
                int valid = 1;
                for (int k = 0; k < 10; k++)
                    if (!scan[k] || !isalnum((unsigned char)scan[k])) {
                        valid = 0;
                        break;
                    }
                if (valid) {
                    memcpy(asin, scan, 10);
                    asin[10] = 0;
                    char thumb[2048];
                    snprintf(thumb, sizeof(thumb),
                             "%s/../system/thumbnails/thumbnail_%s_EBOK_portrait.jpg", library_root,
                             asin);
                    if (!access(thumb, R_OK)) {
                        engine_image_thumbnail(thumb, cp);
                        break;
                    }
                }
                scan++;
            }
        }
        loadstate(b);
        char mp[2048];
        snprintf(mp, sizeof(mp), "%s/books/%016llx.meta", data_root, (unsigned long long)b->id);
        FILE *mf = fopen(mp, "r");
        if (mf) {
            if (fgets(b->title, sizeof(b->title), mf))
                b->title[strcspn(b->title, "\r\n")] = 0;
            if (fgets(b->author, sizeof(b->author), mf))
                b->author[strcspn(b->author, "\r\n")] = 0;
            fclose(mf);
        }
    }
    closedir(d);
}
int library_scan(void) {
    char path[2048];
    mkdir(data_root, 0700);
    snprintf(path, sizeof(path), "%s/books", data_root);
    mkdir(path, 0700);
    snprintf(path, sizeof(path), "%s/covers", data_root);
    mkdir(path, 0700);
    snprintf(path, sizeof(path), "%s/index-v3.bin", data_root);
    FILE *f = fopen(path, "rb");
    cache = NULL;
    cache_count = 0;
    if (f) {
        uint32_t hdr[3];
        if (fread(hdr, sizeof(hdr), 1, f) == 1 && hdr[0] == 0x4c554d32 && hdr[1] == sizeof(Book) &&
            hdr[2] <= BOOK_LIMIT) {
            cache = calloc(hdr[2] ? hdr[2] : 1, sizeof(Book));
            if (cache && fread(cache, sizeof(Book), hdr[2], f) == hdr[2])
                cache_count = (int)hdr[2];
            for (int i = 0; i < cache_count; i++) {
                cache[i].path[sizeof(cache[i].path) - 1] = 0;
                cache[i].title[sizeof(cache[i].title) - 1] = 0;
                cache[i].author[sizeof(cache[i].author) - 1] = 0;
                cache[i].format[sizeof(cache[i].format) - 1] = 0;
            }
        }
        fclose(f);
    }
    library_count = 0;
    index_changed = 0;
    scan(library_root, 0);
    if (cache_count != library_count)
        index_changed = 1;
    free(cache);
    cache = NULL;
    cache_count = 0;
    size_t size = 12 + sizeof(Book) * (size_t)library_count;
    unsigned char *buf = index_changed ? malloc(size) : NULL;
    if (buf) {
        uint32_t hdr[3] = {0x4c554d32, sizeof(Book), (uint32_t)library_count};
        memcpy(buf, hdr, 12);
        memcpy(buf + 12, library, sizeof(Book) * (size_t)library_count);
        atomic_write(path, buf, size);
        free(buf);
    }
    return library_count;
}
static int sorting;
static int compare(const void *a, const void *b) {
    const Book *x = &library[*(const int *)a], *y = &library[*(const int *)b];
    if (sorting == 2 && x->lastread != y->lastread)
        return x->lastread > y->lastread ? -1 : 1;
    if (sorting == 3 && x->added != y->added)
        return x->added > y->added ? -1 : 1;
    if (sorting == 5) {
        double px = x->total ? (double)(x->page + 1) / x->total : 0,
               py = y->total ? (double)(y->page + 1) / y->total : 0;
        if (px != py)
            return px > py ? -1 : 1;
    }
    if (sorting == 4 && x->size != y->size)
        return x->size > y->size ? -1 : 1;
    char sx[512], sy[512];
    utf8_fold(sorting == 1 ? x->author : x->title, sx, sizeof(sx));
    utf8_fold(sorting == 1 ? y->author : y->title, sy, sizeof(sy));
    int c = strcmp(sx, sy);
    return c ? c : strcmp(x->title, y->title);
}
int library_view(int *view, const char *query, int filter, int sort) {
    int n = 0;
    char q[256];
    utf8_fold(query, q, sizeof(q));
    for (int i = 0; i < library_count; i++) {
        Book *b = &library[i];
        if (filter == 1 && !library_supported(b))
            continue;
        if (filter == 2 && strcasecmp(b->format, "epub"))
            continue;
        if (filter == 3 && strcasecmp(b->format, "pdf"))
            continue;
        if (filter == 4 && !b->favorite)
            continue;
        if (filter == 5 && (!b->lastread || b->page + 1 >= b->total))
            continue;
        char hay[768];
        snprintf(hay, sizeof(hay), "%s %s", b->title, b->author);
        char folded[768];
        utf8_fold(hay, folded, sizeof(folded));
        if (*q && !strstr(folded, q))
            continue;
        view[n++] = i;
    }
    sorting = sort;
    qsort(view, (size_t)n, sizeof(int), compare);
    return n;
}
int library_last(void) {
    int idx = -1;
    for (int i = 0; i < library_count; i++)
        if (library[i].lastread && (idx < 0 || library[i].lastread > library[idx].lastread))
            idx = i;
    return idx;
}
void library_settings_load(void) {
    settings = factory_settings;
    memcpy(options, factory_options, sizeof(options));
    options[OPT_UI_REV] = 0;
    mkdir(data_root, 0700);
    char op[2048];
    snprintf(op, sizeof(op), "%s/options-v3.txt", data_root);
    FILE *of = fopen(op, "r");
    int new_options = !of;
    if (of) {
        int key, value;
        while (fscanf(of, "%d %d", &key, &value) == 2) {
            if (key >= 0 && key < OPT_COUNT && key != OPT_RESERVED_27 && value >= 0 &&
                value <= option_maximum[key])
                options[key] = value;
        }
        fclose(of);
    }
    if (options[OPT_PORT] < 1024)
        options[OPT_PORT] = 8080;
    if (options[OPT_TIMEOUT] < 1 || options[OPT_TIMEOUT] > 60)
        options[OPT_TIMEOUT] = 10;
    char p[2048];
    snprintf(p, sizeof(p), "%s/settings.txt", data_root);
    FILE *f = fopen(p, "r");
    Settings s;
    int new_settings = !f;
    if (f) {
        if (fscanf(f, "LUMEN2 %d %d %d %d %d %d %d", &s.size, &s.leading, &s.margin, &s.refresh,
                   &s.grid, &s.sort, &s.filter) == 7 &&
            s.size >= 16 && s.size <= 34 && s.leading >= 110 && s.leading <= 190 && s.margin >= 0 &&
            s.margin <= 30 && s.refresh >= 1 && s.refresh <= 20 && s.grid >= 0 && s.grid <= 1 &&
            s.sort >= 0 && s.sort <= 5 && s.filter >= 0 && s.filter <= 5)
            settings = s;
        fclose(f);
    }
    if (options[OPT_UI_REV] < 10) {
        options[OPT_AUTO_TIME] = 1;
        options[OPT_REMOTE] = 0;
    }
    if (options[OPT_UI_REV] < 8) {
        options[OPT_BATTERY_PERCENT] = 0;
        options[OPT_COVER_CROP] = 1;
        options[OPT_ROUNDING] = 1;
        if (settings.size == 22)
            settings.size = 24;
    }
    if (options[OPT_UI_REV] < 9) {
        options[OPT_APPS] = 1;
        options[OPT_TAB_LABELS] = 0;
        options[OPT_LOCK_HINT] = 0;
        options[OPT_UI_REV] = 9;
    }
    options[OPT_UI_REV] = 11;
    if (new_options || new_settings) library_settings_save();
}
void library_settings_save(void) {
    library_options_save();
    char p[2048], s[160];
    snprintf(p, sizeof(p), "%s/settings.txt", data_root);
    int n =
        snprintf(s, sizeof(s), "LUMEN2 %d %d %d %d %d %d %d\n", settings.size, settings.leading,
                 settings.margin, settings.refresh, settings.grid, settings.sort, settings.filter);
    atomic_write(p, s, (size_t)n);
}

void library_options_save(void) {
    char p[2048], s[512];
    snprintf(p, sizeof(p), "%s/options-v3.txt", data_root);
    int n = 0;
    for (int i = 0; i < OPT_COUNT; i++)
        if (i != OPT_RESERVED_27)
            n += snprintf(s + n, sizeof(s) - (size_t)n, "%d %d\n", i, options[i]);
    atomic_write(p, s, (size_t)n);
}
static int validname(const char *s) {
    return *s && strlen(s) < 241 && s[0] != '.' && !strchr(s, '/') && !strchr(s, '\\') &&
           !strchr(s, '\n') && !strchr(s, '\r');
}
int library_rename(int index, const char *name) {
    if (index < 0 || index >= library_count || !validname(name))
        return 0;
    Book *b = &library[index];
    char path[2048];
    snprintf(path, sizeof(path), "%s", b->path);
    char *slash = strrchr(path, '/');
    if (!slash || ((size_t)(slash - path) + strlen(name) + 2) > sizeof(path))
        return 0;
    strcpy(slash + 1, name);
    int reserve = open(path, O_CREAT | O_EXCL | O_WRONLY, 0600);
    if (reserve < 0)
        return 0;
    close(reserve);
    if (rename(b->path, path)) {
        unlink(path);
        return 0;
    }
    unsigned long long oldid = (unsigned long long)b->id;
    char oldstate[2048];
    statepath(b, oldstate, sizeof(oldstate));
    snprintf(b->path, sizeof(b->path), "%s", path);
    b->id = hashpath(path);
    library_save_book(b);
    const char *suffixes[] = {"meta", "position"};
    char from[2048], to[2048];
    for (int i = 0; i < 34; i++) {
        char suffix[32];
        if (i < 2)
            snprintf(suffix, sizeof(suffix), "%s", suffixes[i]);
        else
            snprintf(suffix, sizeof(suffix), "mark%d", i - 2);
        snprintf(from, sizeof(from), "%s/books/%016llx.%s", data_root, oldid, suffix);
        snprintf(to, sizeof(to), "%s/books/%016llx.%s", data_root, (unsigned long long)b->id,
                 suffix);
        rename(from, to);
    }
    snprintf(from, sizeof(from), "%s/covers/%016llx.pgm", data_root, oldid);
    library_cover_path(b, to, sizeof(to));
    rename(from, to);
    const char *imports[] = {"epub", "source"};
    for (int i = 0; i < 2; i++) {
        snprintf(from, sizeof(from), "%s/imports/%016llx.%s", data_root, oldid, imports[i]);
        snprintf(to, sizeof(to), "%s/imports/%016llx.%s", data_root, (unsigned long long)b->id,
                 imports[i]);
        rename(from, to);
    }
    unlink(oldstate);
    return 1;
}
int library_delete(int index) {
    if (index < 0 || index >= library_count)
        return 0;
    Book *b = &library[index];
    struct stat st;
    if (lstat(b->path, &st) || !S_ISREG(st.st_mode) || unlink(b->path))
        return 0;
    char p[2048];
    library_cover_path(b, p, sizeof(p));
    unlink(p);
    statepath(b, p, sizeof(p));
    unlink(p);
    memmove(b, b + 1, (size_t)(library_count - index - 1) * sizeof(Book));
    library_count--;
    return 1;
}
int library_metadata(int index, const char *title, const char *author) {
    if (index < 0 || index >= library_count)
        return 0;
    Book *b = &library[index];
    char path[2048], buf[768];
    snprintf(path, sizeof(path), "%s/books/%016llx.meta", data_root, (unsigned long long)b->id);
    if (strchr(title, '\n') || strchr(author, '\n') || strlen(title) >= sizeof(b->title) ||
        strlen(author) >= sizeof(b->author))
        return 0;
    snprintf(buf, sizeof(buf), "%s\n%s\n", title, author);
    if (!atomic_write(path, buf, strlen(buf)))
        return 0;
    char saved_title[384], saved_author[192];
    snprintf(saved_title, sizeof(saved_title), "%s", title);
    snprintf(saved_author, sizeof(saved_author), "%s", author);
    memcpy(b->title, saved_title, strlen(saved_title) + 1);
    memcpy(b->author, saved_author, strlen(saved_author) + 1);
    return 1;
}
int library_config_export(const char *path) {
    char buf[2048];
    int n = snprintf(buf, sizeof(buf), "LUMEN-CONFIG 3\n");
    int base[7] = {settings.size, settings.leading, settings.margin, settings.refresh,
                   settings.grid, settings.sort,    settings.filter};
    for (int i = 0; i < 7; i++)
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, "setting.%d %d\n", i, base[i]);
    for (int i = 0; i < OPT_COUNT; i++)
        if (i != OPT_RESERVED_27)
            n += snprintf(buf + n, sizeof(buf) - (size_t)n, "option.%d %d\n", i, options[i]);
    return atomic_write(path, buf, (size_t)n);
}
int library_config_import(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f)
        return 0;
    char line[160];
    if (!fgets(line, sizeof(line), f) || strcmp(line, "LUMEN-CONFIG 3\n")) {
        fclose(f);
        return 0;
    }
    int candidate[7] = {settings.size, settings.leading, settings.margin, settings.refresh,
                        settings.grid, settings.sort,    settings.filter};
    int copy[OPT_COUNT];
    memcpy(copy, options, sizeof(copy));
    int low[7] = {16, 110, 0, 1, 0, 0, 0}, high[7] = {34, 190, 30, 20, 1, 5, 5};
    int ok = 1;
    while (fgets(line, sizeof(line), f)) {
        char kind[20], extra;
        int key, value;
        if (sscanf(line, "%19[^.].%d %d %c", kind, &key, &value, &extra) != 3) {
            ok = 0;
            break;
        }
        if (!strcmp(kind, "option") && key == OPT_RESERVED_27)
            continue;
        if (!strcmp(kind, "setting") && key >= 0 && key < 7 && value >= low[key] &&
            value <= high[key])
            candidate[key] = value;
        else if (!strcmp(kind, "option") && key >= 0 && key < OPT_COUNT && value >= 0 &&
                 value <= option_maximum[key])
            copy[key] = value;
        else {
            ok = 0;
            break;
        }
    }
    fclose(f);
    if (copy[OPT_PORT] < 1024 || copy[OPT_TIMEOUT] < 1)
        ok = 0;
    if (!ok)
        return 0;
    settings = (Settings){candidate[0], candidate[1], candidate[2], candidate[3],
                          candidate[4], candidate[5], candidate[6]};
    memcpy(options, copy, sizeof(copy));
    library_settings_save();
    return 1;
}

void library_defaults(void) {
    settings = factory_settings;
    memcpy(options, factory_options, sizeof(options));
    library_settings_save();
}
