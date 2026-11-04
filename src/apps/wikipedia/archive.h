/* App-owned module; assembled by apps/host/compose.h. */
typedef struct {
    unsigned magic;
    int language;
    int64_t seen;
    char title[256], image[2048], name[512], text[524288];
} WikiSaved;
static struct {
    uint64_t id;
    int64_t seen, opened;
    int language;
    char title[256];
} wiki_entries[2048];
static int wiki_entry_count, wiki_history_page, wiki_delete = -1;
static uint64_t wiki_saved_hash, wiki_refresh_id;
static int wiki_history_dirty = 1;
static void wiki_cache_path(char *out, int n, uint64_t id, const char *ext) {
    snprintf(out, n, "%s/wiki-archive/%016llx.%s", data_root, (unsigned long long)id, ext);
}
static uint64_t wiki_key(void) {
    char b[512];
    snprintf(b, sizeof(b), "%d:%s",
             wiki_article_language < 0 ? options[OPT_LANGUAGE] : wiki_article_language,
             discover_lookup_title());
    return press_id(b);
}
static int copy_file(const char *from, const char *to) {
    FILE *f = fopen(from, "rb");
    if (!f)
        return 0;
    FILE *g = fopen(to, "wb");
    if (!g) {
        fclose(f);
        return 0;
    }
    char b[8192];
    size_t n;
    int ok = 1;
    while ((n = fread(b, 1, sizeof(b), f)))
        if (fwrite(b, 1, n, g) != n) {
            ok = 0;
            break;
        }
    if (ferror(f))
        ok = 0;
    if (fclose(g))
        ok = 0;
    fclose(f);
    return ok;
}
static void polish_wiki_saved(void) {
    if (!*discover_lookup_title() || !wiki_image_available())
        return;
    char from[2048], to[2048];
    wiki_cache_path(to, sizeof(to), wiki_key(), "deleted");
    if (!access(to, F_OK))
        return;
    local_path(from, sizeof(from), "wiki-image.pgm");
    wiki_cache_path(to, sizeof(to), wiki_key(), "pgm");
    copy_file(from, to);
}
static void polish_wiki_archive(void) {
    if (discover_busy() || !*discover_lookup_title() || !*discover_lookup_text())
        return;
    uint64_t hash = press_id(discover_lookup_text()) ^ wiki_key();
    if (hash == wiki_saved_hash)
        return;
    char tomb[2048];
    wiki_cache_path(tomb, sizeof(tomb), wiki_key(), "deleted");
    if (!access(tomb, F_OK)) {
        wiki_saved_hash = hash;
        return;
    }
    char dir[2048];
    local_path(dir, sizeof(dir), "wiki-archive");
    mkdir(dir, 0700);
    WikiSaved *a = calloc(1, sizeof(*a));
    if (!a)
        return;
    a->magic = 0x574b3131;
    a->language = wiki_article_language < 0 ? options[OPT_LANGUAGE] : wiki_article_language;
    a->seen = time(NULL);
    snprintf(a->title, 256, "%s", discover_lookup_title());
    snprintf(a->text, sizeof(a->text), "%s", discover_lookup_text());
    snprintf(a->image, 2048, "%s", discover_lookup_image());
    snprintf(a->name, 512, "%s", discover_lookup_image_name());
    char p[2048];
    wiki_cache_path(p, sizeof(p), wiki_key(), "bin");
    if (atomic_write(p, a, sizeof(*a))) {
        wiki_saved_hash = hash;
        wiki_history_dirty = 1;
    }
    free(a);
    polish_wiki_saved();
}
static void wiki_title_key(const char *title, char *out, int cap) {
    utf8_fold(title, out, cap);
}
static int wiki_same_title(const char *a, const char *b) {
    char x[768], y[768];
    wiki_title_key(a, x, sizeof(x));
    wiki_title_key(b, y, sizeof(y));
    return !strcmp(x, y);
}
static void wiki_open_path(char *p, int cap, const char *title) {
    char key[768];
    wiki_title_key(title, key, sizeof(key));
    snprintf(p, cap, "%s/wiki-archive/%016llx.opened", data_root,
             (unsigned long long)press_id(key));
}
static void wiki_article_opened(void) {
    char p[2048], stamp[64];
    wiki_open_path(p, sizeof(p), discover_lookup_title());
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    static long long last;
    long long opened = (long long)now.tv_sec * 1000 + now.tv_nsec / 1000000;
    if (opened <= last)
        opened = last + 1;
    last = opened;
    snprintf(stamp, sizeof(stamp), "%lld", opened);
    atomic_write(p, stamp, strlen(stamp));
    wiki_history_dirty = 1;
}
static void wiki_delete_copies(const char *title) {
    char dir[2048], p[2048];
    local_path(dir, sizeof(dir), "wiki-archive");
    DIR *d = opendir(dir);
    if (!d)
        return;
    struct dirent *e;
    while ((e = readdir(d))) {
        size_t n = strlen(e->d_name);
        if (n != 20 || strcmp(e->d_name + 16, ".bin"))
            continue;
        snprintf(p, sizeof(p), "%s/%s", dir, e->d_name);
        FILE *f = fopen(p, "rb");
        if (!f)
            continue;
        struct {
            unsigned magic;
            int language;
            int64_t seen;
            char title[256];
        } h;
        int ok = fread(&h, sizeof(h), 1, f) == 1 && h.magic == 0x574b3131;
        fclose(f);
        h.title[255] = 0;
        if (ok && wiki_same_title(title, h.title)) {
            unlink(p);
            char *ext = p + strlen(p) - 3;
            memcpy(ext, "pgm", 4);
            unlink(p);
            memcpy(ext, "bin", 4);
            unsigned long long id = 0;
            sscanf(e->d_name, "%16llx", &id);
            wiki_cache_path(p, sizeof(p), id, "deleted");
            atomic_write(p, "1", 1);
        }
    }
    closedir(d);
    wiki_history_dirty = 1;
}
static void wiki_history_scan(void) {
    wiki_history_dirty = 0;
    wiki_entry_count = 0;
    char p[2048];
    local_path(p, sizeof(p), "wiki-archive");
    DIR *d = opendir(p);
    if (!d)
        return;
    struct dirent *e;
    while ((e = readdir(d)) && wiki_entry_count < 2048) {
        unsigned long long id;
        char tail;
        if (sscanf(e->d_name, "%16llx.bin%c", &id, &tail) != 1 || strlen(e->d_name) != 20)
            continue;
        wiki_cache_path(p, sizeof(p), id, "bin");
        FILE *f = fopen(p, "rb");
        if (!f)
            continue;
        struct {
            unsigned magic;
            int language;
            int64_t seen;
            char title[256];
        } h;
        int ok = fread(&h, sizeof(h), 1, f) == 1 && h.magic == 0x574b3131;
        fclose(f);
        if (ok) {
            h.title[255] = 0;
            int found = -1;
            for (int i = 0; i < wiki_entry_count; i++)
                if (wiki_same_title(h.title, wiki_entries[i].title)) {
                    found = i;
                    break;
                }
            if (found >= 0 && wiki_entries[found].seen >= h.seen)
                continue;
            int slot = found >= 0 ? found : wiki_entry_count++;
            wiki_entries[slot].language = h.language >= 0 && h.language < 4 ? h.language : 0;
            wiki_entries[slot].id = id;
            wiki_entries[slot].seen = h.seen;
            snprintf(wiki_entries[slot].title, 256, "%s", h.title);
            wiki_open_path(p, sizeof(p), h.title);
            FILE *opened = fopen(p, "r");
            long long stamp = 0;
            if (opened) {
                fscanf(opened, "%lld", &stamp);
                fclose(opened);
            }
            wiki_entries[slot].opened = stamp ? stamp : h.seen * 1000;
        }
    }
    closedir(d);
    for (int i = 0; i < wiki_entry_count; i++)
        for (int j = i + 1; j < wiki_entry_count; j++)
            if (wiki_entries[j].opened > wiki_entries[i].opened) {
                unsigned char b[sizeof(wiki_entries[0])];
                memcpy(b, &wiki_entries[i], sizeof(b));
                memcpy(&wiki_entries[i], &wiki_entries[j], sizeof(b));
                memcpy(&wiki_entries[j], b, sizeof(b));
            }
}
static void wiki_history_current(void) {
    if (wiki_history_dirty)
        wiki_history_scan();
}
static void polish_wiki_history(void) {
    heading_actions("Wikipedia", 1);
    search_action();
    for (int r = 0; r < 7 && wiki_history_page * 7 + r < wiki_entry_count; r++) {
        int i = wiki_history_page * 7 + r, y = 151 + r * 78;
        time_t stamp = wiki_entries[i].seen;
        char b[100];
        strftime(b, sizeof(b), "%d.%m.%Y · %H:%M", localtime(&stamp));
        ListLayout l = list_row(28, y, 544, 78, -1, 76, wiki_entries[i].title, b, ui_font(UI_PRIMARY), 1);
        icon_button(522, l.action_y, IC_TRASH);
    }
    if (!wiki_entry_count)
        wrapped(30, 180, "Gelesene Artikel bleiben hier offline gespeichert, bis du sie löschst.",
                ui_font(UI_TITLE), 540, 5);
    pager(746, &wiki_history_page, (wiki_entry_count + 6) / 7);
    if (wiki_delete >= 0) {
        pagination = (Pagination){0};
        rect(20, 270, 560, 220, 255);
        key_outline(20, 270, 560, 220);
        centered(28, 290, 544, 70, "Artikel löschen?", ui_font(UI_PRIMARY));
        button_h(35, 405, 250, 55, "Abbrechen", 0);
        button_h(310, 405, 250, 55, "Löschen", 0);
    }
}
static int wiki_history_input(const Input *in) {
    if (in->kind != INPUT_TAP)
        return 2;
    int x = in->x, y = in->y;
    if (search_action_hit(x, y) && wiki_delete < 0) {
        wiki_refresh_id = 0;
        open_keyboard(34, "Wikipedia durchsuchen", wiki_query);
        return 1;
    }
    if (wiki_delete >= 0) {
        if (y >= 405 && y < 460) {
            if (x >= 300 && wiki_delete < wiki_entry_count) {
                int deleting_current = wiki_entries[wiki_delete].id == wiki_key();
                discover_cancel();
                char p[2048], source[512];
                const char *langs[] = {"de", "en", "fr", "zh"};
                snprintf(source, sizeof(source), "https://%s.wikipedia.org/wiki/%s",
                         langs[wiki_entries[wiki_delete].language],
                         wiki_entries[wiki_delete].title);
                uint64_t document = press_id(source);
                snprintf(p, sizeof(p), "%s/reader-documents/%016llx.html", data_root,
                         (unsigned long long)document);
                unlink(p);
                snprintf(p, sizeof(p), "%s/reader-documents/%016llx.pgm", data_root,
                         (unsigned long long)document);
                unlink(p);
                wiki_cache_path(p, sizeof(p), wiki_entries[wiki_delete].id, "bin");
                unlink(p);
                wiki_cache_path(p, sizeof(p), wiki_entries[wiki_delete].id, "deleted");
                atomic_write(p, "1", 1);
                wiki_cache_path(p, sizeof(p), wiki_entries[wiki_delete].id, "pgm");
                unlink(p);
                local_path(p, sizeof(p), "wiki-last.xml");
                unlink(p);
                if (deleting_current) {
                    discover_restore_lookup("", "", "", "");
                    wiki_query[0] = 0;
                    extra_write_text("wiki-query.txt", "");
                    local_path(p, sizeof(p), "wiki-image.pgm");
                    unlink(p);
                    local_path(p, sizeof(p), "wiki-image-url.txt");
                    unlink(p);
                }
                wiki_saved_hash = press_id(discover_lookup_text()) ^ wiki_key();
                wiki_delete_copies(wiki_entries[wiki_delete].title);
                wiki_history_scan();
            }
            wiki_delete = -1;
        }
        return 1;
    }
    if (y >= 151 && y < 697) {
        int i = wiki_history_page * 7 + (y - 151) / 78;
        if (i < wiki_entry_count) {
            if (x >= 510) {
                wiki_delete = i;
                return 1;
            }
            char p[2048];
            wiki_cache_path(p, sizeof(p), wiki_entries[i].id, "bin");
            FILE *f = fopen(p, "rb");
            WikiSaved *a = malloc(sizeof(*a));
            int ok = f && a && fread(a, sizeof(*a), 1, f) == 1 && a->magic == 0x574b3131;
            if (f)
                fclose(f);
            if (ok) {
                a->title[255] = a->image[2047] = a->name[511] = a->text[524287] = 0;
                discover_cancel();
                wiki_article_language = a->language >= 0 && a->language < 4 ? a->language : 0;
                discover_restore_lookup(a->title, a->text, a->image, a->name);
                wiki_saved_hash = press_id(a->text) ^ wiki_key();
                wiki_cache_path(p, sizeof(p), wiki_entries[i].id, "pgm");
                char target[2048];
                local_path(target, sizeof(target), "wiki-image.pgm");
                if (copy_file(p, target))
                    extra_write_text("wiki-image-url.txt", a->image);
                wiki_hash = 0;
                wiki_page = 0;
                extra_open(36);
                char state[128];
                network_status(state, sizeof(state));
                if (device_wifi_enabled() && *state && strcmp(state, "Offline")) {
                    snprintf(wiki_query, sizeof(wiki_query), "%s", a->title);
                    wiki_refresh_id = wiki_entries[i].id;
                    if (!discover_lookup(wiki_query, wiki_article_language))
                        wiki_refresh_failed();
                } else
                    article_wiki_open();
            }
            free(a);
        }
    } else if (y >= 746 && y < 790) {
        if (x < 300 && wiki_history_page)
            wiki_history_page--;
        else if (x >= 300 && (wiki_history_page + 1) * 7 < wiki_entry_count)
            wiki_history_page++;
    }
    return 1;
}
