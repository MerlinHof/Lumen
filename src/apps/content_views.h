/* 1.1 app polish: shared chrome, permanent reading caches, deterministic flows. */
#include "../press.h"
static int power_menu, power_hold, power_request;
static int64_t power_menu_wake_until;
static int polish_order_done;
static int tree_id[1536], tree_type[1536], tree_count, tree_page;
static void polish_init(void) {}
static void polish_order(void) {
    if (polish_order_done)
        return;
    polish_order_done = 1;
    char p[2048];
    local_path(p, sizeof(p), "launcher-order16.txt");
    if (!access(p, F_OK))
        return;
    const int first[] = {APP_CALCULATOR, APP_WIKIPEDIA,  APP_QUOTES,         APP_NEWS,
                         APP_CALENDAR,   APP_TIMER,      APP_NOTES,          APP_WEATHER,
                         APP_INSIGHTS,   APP_PHOTOS,     APP_HOME_ASSISTANT, APP_TASKS,
                         APP_LANGUAGE,   APP_FLASHCARDS, APP_PLANETARIUM,    APP_JOURNAL,
                         APP_STUDY,      APP_RECIPES,    APP_PERIODIC,       APP_FILES,
                         APP_LLM,        APP_FOCUS};
    int order[APP_COUNT], seen[APP_COUNT] = {0}, n = 0;
    for (unsigned i = 0; i < sizeof(first) / sizeof(*first); i++) {
        order[n++] = first[i];
        seen[first[i]] = 1;
    }
    for (int i = 0; i < APP_COUNT; i++) {
        int id = launcher_order[i];
        if (!seen[id] && (id < APP_MINES || id == APP_STATUS_BAR || id == APP_TAB_BAR ||
                          id == APP_LOCK_SCREEN || id == APP_KEYBOARD)) {
            order[n++] = id;
            seen[id] = 1;
        }
    }
    for (int i = 0; i < APP_COUNT; i++)
        if (!seen[launcher_order[i]])
            order[n++] = launcher_order[i];
    memcpy(launcher_order, order, sizeof(order));
    v1_dirty = 1;
    v1_save();
    atomic_write(p, "1", 1);
}
static void tree_build(void) {
    tree_count = 0;
    for (int i = 0; i < note_tree.nf; i++)
        if (note_tree.folder[i].parent == note_tree.current) {
            tree_type[tree_count] = 1;
            tree_id[tree_count++] = i;
        }
    for (int i = 0; i < note_count; i++)
        if (v1_folder_for(note_ids[i]) == note_tree.current) {
            tree_type[tree_count] = 0;
            tree_id[tree_count++] = note_ids[i];
        }
    if (tree_page * 7 >= tree_count)
        tree_page = tree_count ? (tree_count - 1) / 7 : 0;
}
static void polish_notes(void) {
    u16_notes_defaults();
    const char *name = note_tree.current ? v1_folder_name(note_tree.current) : tr("Notizen");
    label_ellipsis(28, 74, note_tree.current ? 350 : 410, 44, name, &editorial, 0);
    note_rename_x17 = 36 + (int)fmin(350, textwidth_raw(name, &editorial));
    if (note_tree.current) {
        icon_button(note_rename_x17, 74, IC_EDIT);
        u17_gray_icon(note_rename_x17, 74);
    }
    icon_button(468, 74, IC_FOLDER_PLUS);
    icon_button(528, 74, IC_NOTE_PLUS);
    tree_build();
    if (!tree_count) {
        ink_color = 140;
        drawtext(30, 170, "Dieser Ordner ist leer.", &small, 540);
        ink_color = 0;
    }
    for (int r = 0; r < 7 && tree_page * 7 + r < tree_count; r++) {
        int i = tree_page * 7 + r, y = 151 + r * 78;
        ListLayout l;
        if (tree_type[i]) {
            int folder = note_tree.folder[tree_id[i]].id, children = 0;
            for (int f = 0; f < note_tree.nf; f++)
                children += note_tree.folder[f].parent == folder;
            for (int n = 0; n < note_count; n++)
                children += v1_folder_for(note_ids[n]) == folder;
            char count[100];
            if (children == 1)
                snprintf(count, sizeof(count), "%s", tr("1 direkter Eintrag"));
            else
                snprintf(count, sizeof(count), tr("%d direkte Einträge"), children);
            l = list_row(28, y, 544, 78, IC_FOLDER, 76, note_tree.folder[tree_id[i]].name, count,
                         &body, 1);
        } else {
            time_t stamp = tree_id[i];
            char date[50];
            strftime(date, sizeof(date), "%d.%m.%Y", localtime(&stamp));
            l = list_row(28, y, 544, 78, IC_NOTE, 76, note_list_title(tree_id[i]), date, &body, 1);
        }
        icon_button(522, l.action_y, IC_MORE);
    }
    pager(746, &tree_page, (tree_count + 6) / 7);
}
static int polish_notes_input(const Input *in) {
    if (in->kind != INPUT_TAP)
        return 2;
    int x = in->x, y = in->y;
    if (y >= 74 && y < 118) {
        if (x >= 516) {
            note_selected = -1;
            note_text[0] = 0;
            extra_page = 0;
            extra_open(30);
            open_keyboard(30, "Notiz", "");
            note_cursor = note_view_start = 0;
        } else if (x >= 456)
            open_keyboard(120, "Neuer Ordner", "");
        else if (note_tree.current && x >= note_rename_x17 && x < note_rename_x17 + 44)
            open_keyboard(162, "Ordner umbenennen", v1_folder_name(note_tree.current));
        return 1;
    }
    tree_build();
    if (y >= 151 && y < 697) {
        int i = tree_page * 7 + (y - 151) / 78;
        if (i < tree_count) {
            int id = tree_type[i] ? note_tree.folder[tree_id[i]].id : tree_id[i];
            if (x >= 510)
                u17_notes_menu(tree_type[i], id, y);
            else if (tree_type[i]) {
                note_tree.current = id;
                tree_page = 0;
            } else {
                note_load(id);
                extra_page = 0;
                extra_open(30);
            }
        }
    }
    return 1;
}
static void polish_tasks(void) {
    tasks_draw18();
}
static int polish_tasks_input(const Input *in) {
    return tasks_input18(in);
}
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
        ListLayout l = list_row(28, y, 544, 78, -1, 76, wiki_entries[i].title, b, &body, 1);
        icon_button(522, l.action_y, IC_TRASH);
    }
    if (!wiki_entry_count)
        wrapped(30, 180, "Gelesene Artikel bleiben hier offline gespeichert, bis du sie löschst.",
                &editorial, 540, 5);
    pager(746, &wiki_history_page, (wiki_entry_count + 6) / 7);
    if (wiki_delete >= 0) {
        pagination = (Pagination){0};
        rect(20, 270, 560, 220, 255);
        key_outline(20, 270, 560, 220);
        centered(28, 290, 544, 70, "Artikel löschen?", &body);
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
static int photo_fill, photo_controls = 1, photo_rotation, photo_grid_page = -1;
static uint64_t photo_grid_key;
static unsigned char photo_grid[16][136 * 136];
static int photo_bx, photo_by, photo_bw = 600, photo_bh = 800;
static char photo_bounds_key[256];
static void photo_bounds(void) {
    if (!strcmp(photo_bounds_key, photo_loaded))
        return;
    snprintf(photo_bounds_key, 256, "%s", photo_loaded);
    int top = 0, bottom = 799, left = 0, right = 599;
    for (; top < 799; top++) {
        int content = 0;
        for (int x = 0; x < 600; x++)
            if (photo_pixels[top * 600 + x] < 247) {
                content = 1;
                break;
            }
        if (content)
            break;
    }
    for (; bottom > top; bottom--) {
        int content = 0;
        for (int x = 0; x < 600; x++)
            if (photo_pixels[bottom * 600 + x] < 247) {
                content = 1;
                break;
            }
        if (content)
            break;
    }
    for (; left < 599; left++) {
        int content = 0;
        for (int y = top; y <= bottom; y++)
            if (photo_pixels[y * 600 + left] < 247) {
                content = 1;
                break;
            }
        if (content)
            break;
    }
    for (; right > left; right--) {
        int content = 0;
        for (int y = top; y <= bottom; y++)
            if (photo_pixels[y * 600 + right] < 247) {
                content = 1;
                break;
            }
        if (content)
            break;
    }
    if (right - left < 10 || bottom - top < 10) {
        left = top = 0;
        right = 599;
        bottom = 799;
    }
    photo_bx = left;
    photo_by = top;
    photo_bw = right - left + 1;
    photo_bh = bottom - top + 1;
}
static void polish_photos_grid(void) {
    heading_actions("Fotos", 1);
    icon_button(528, 74, IC_PLUS);
    uint64_t key = photo_count ? press_id(photo_names[0]) ^ photo_count : 0;
    if (photo_grid_page != photo_page || photo_grid_key != key) {
        char dir[2048];
        local_path(dir, sizeof(dir), "photos/.thumbs");
        mkdir(dir, 0700);
        for (int r = 0; r < 16 && photo_page * 16 + r < photo_count; r++) {
            int i = photo_page * 16 + r;
            char path[2048], source[2048], name[300];
            snprintf(name, sizeof(name), "photos/.thumbs/%s", photo_names[i]);
            local_path(path, sizeof(path), name);
            if (access(path, R_OK)) {
                snprintf(name, sizeof(name), "photos/%s", photo_names[i]);
                local_path(source, sizeof(source), name);
                engine_photo_thumbnail(source, path);
            }
            FILE *f = fopen(path, "rb");
            int w, h, m;
            char magic[3];
            memset(photo_grid[r], 255, sizeof(photo_grid[r]));
            if (f) {
                if (fscanf(f, "%2s %d %d %d", magic, &w, &h, &m) == 4 && !strcmp(magic, "P5") &&
                    w == 136 && h == 136 && m == 255) {
                    fgetc(f);
                    fread(photo_grid[r], 1, sizeof(photo_grid[r]), f);
                }
                fclose(f);
            }
        }
        photo_grid_page = photo_page;
        photo_grid_key = key;
    }
    for (int r = 0; r < 16 && photo_page * 16 + r < photo_count; r++) {
        int x = 19 + (r % 4) * 142, y = 142 + (r / 4) * 142;
        region(x, y, 136, 136);
        for (int yy = 0; yy < 136; yy++)
            for (int xx = 0; xx < 136; xx++)
                ui_pixels[(y + yy) * 600 + x + xx] =
                    image_tone(photo_grid[r][yy * 136 + xx], x + xx, y + yy);
    }
    if (!photo_count)
        wrapped(30, 180,
                "Fotos vom Handy über LUMEN Connect hochladen. Auch Zeichnungen lassen sich als "
                "Foto speichern.",
                &editorial, 540, 5);
    pager(746, &photo_page, (photo_count + 15) / 16);
}
static int polish_photo_full(void) {
    return screen == 72 && photo_fill && !photo_controls && !v1_delete && !power_menu;
}
static void photo_page_move(int delta) {
    photo_selected += delta;
    photo_rotation = 0;
}
static void polish_photo(void) {
    if (!v1_photo_load(photo_selected))
        return;
    photo_bounds();
    pagination_bind(&photo_selected, photo_count, 0, 0, photo_page_move);
    int W = photo_rotation % 2 ? photo_bh : photo_bw, H = photo_rotation % 2 ? photo_bw : photo_bh;
    int area_y = photo_fill ? 0 : 52, area_h = photo_fill ? 800 : 680;
    double scale =
        photo_fill ? fmax(600. / W, (double)area_h / H) : fmin(600. / W, (double)area_h / H);
    int left = (600 - W * scale) / 2, top = area_y + (area_h - H * scale) / 2;
    rect(0, 0, 600, 800, 255);
    int xmap[600], ymap[800];
    for (int x = 0; x < 600; x++)
        xmap[x] = x < left ? -1 : (int)(((x - left + .5) / scale - .5) * 256);
    for (int y = area_y; y < area_y + area_h; y++)
        ymap[y] = y < top ? -1 : (int)(((y - top + .5) / scale - .5) * 256);
    for (int y = area_y; y < area_y + area_h; y++)
        for (int x = 0; x < 600; x++) {
            int u = xmap[x], v = ymap[y];
            if (x < left || y < top || x >= left + W * scale || y >= top + H * scale)
                continue;
            if (u < 0)
                u = 0;
            if (v < 0)
                v = 0;
            int a, b;
            switch (photo_rotation % 4) {
            case 1:
                a = v;
                b = (photo_bh - 1) * 256 - u;
                break;
            case 2:
                a = (photo_bw - 1) * 256 - u;
                b = (photo_bh - 1) * 256 - v;
                break;
            case 3:
                a = (photo_bw - 1) * 256 - v;
                b = u;
                break;
            default:
                a = u;
                b = v;
            }
            int value = image_bilinear(photo_pixels + photo_by * 600 + photo_bx, 600, photo_bw,
                                       photo_bh, a, b);
            ui_pixels[y * 600 + x] = image_tone(value, x, y);
        }
    if (photo_controls || !photo_fill) {
        rect(0, 0, 600, 50, 255);
        header();
        rect(0, 742, 600, 58, 255);
        page_arrow(28, 748, IC_PREV, photo_selected > 0);
        icon_button(148, 748, IC_ROTATE_PHOTO);
        icon_button(278, 748, IC_TRASH);
        icon_button(398, 748, IC_FULLSCREEN);
        page_arrow(528, 748, IC_NEXT, photo_selected + 1 < photo_count);
    }
}
static int polish_photo_input(const Input *in) {
    if (in->kind != INPUT_TAP)
        return 2;
    int x = in->x, y = in->y;
    if (photo_fill && !photo_controls) {
        photo_controls = 1;
        return 1;
    }
    if (y >= 742) {
        if (x < 100 && photo_selected) {
            photo_selected--;
            photo_rotation = 0;
        } else if (x > 500 && photo_selected + 1 < photo_count) {
            photo_selected++;
            photo_rotation = 0;
        } else if (x >= 135 && x < 210)
            photo_rotation = (photo_rotation + 1) % 4;
        else if (x >= 260 && x < 330)
            v1_delete = 1;
        else if (x >= 380 && x < 460) {
            photo_fill ^= 1;
            photo_controls = !photo_fill;
        }
    } else if (photo_fill)
        photo_controls = 0;
    return 1;
}
static int polish_power(const Input *in) {
    if (in->kind == INPUT_POWER && in->x == 2) {
        device_wake();
        sleeping = 0;
        power_hold = 1;
        power_menu_wake_until = extra_millis() + 2500;
        power_menu = 1;
        return 1;
    }
    if (power_hold && in->kind == INPUT_POWER && in->x == 0) {
        power_hold = 0;
        return 1;
    }
    if (!power_menu)
        return 0;
    if (in->kind == INPUT_SLEEP) {
        if (power_hold || extra_millis() < power_menu_wake_until) {
            device_wake();
            sleeping = 0;
            return 1;
        }
        power_menu = 0;
        return 0;
    }
    if (in->kind == INPUT_DOWN) {
        pressed = hit(in->x, in->y);
        if (pressed >= 0)
            press_bounds = widgets[pressed];
        return pressed >= 0 ? 3 : 1;
    }
    if (in->kind == INPUT_TAP) {
        pressed = -1;
        int y = in->y;
        if (in->x < 58 || in->x > 532)
            return 1;
        if (y >= 292 && y < 350) {
            power_menu = 0;
            Input sleep = {.kind = INPUT_SLEEP};
            ui_input(&sleep);
        } else if (y >= 370 && y < 428) {
            power_request = 2;
            power_menu = 0;
            return 2;
        } else if (y >= 448 && y < 506) {
            power_request = 1;
            power_menu = 0;
            return 2;
        } else if (y >= 543 && y < 597)
            power_menu = 0;
        return 1;
    }
    return in->kind == INPUT_DOWN || in->kind == INPUT_MOVE || in->kind == INPUT_SWIPE ||
           in->kind == INPUT_HOLD;
}
static void polish_draw(void) {
    if (!power_menu)
        return;
    rect(27, 209, 558, 425, 175);
    rect(21, 203, 558, 425, 255);
    key_outline(21, 203, 558, 425);
    centered(28, 224, 544, 44, "Kindle", &editorial);
    icon_button(58, 298, IC_LOCK);
    button_h(116, 292, 416, 58, "Sperren", 0);
    icon_button(58, 376, IC_REBOOT);
    button_h(116, 370, 416, 58, "Neustarten", 0);
    icon_button(58, 454, IC_SHUTDOWN);
    button_h(116, 448, 416, 58, "Ausschalten", 0);
    button_h(58, 543, 474, 54, "Abbrechen", 0);
}
static int press_dates[2048], press_dates_count, press_date_page, press_list_page,
    press_reader_page, press_reader_pages, press_offsets[4096], press_initial_y = 145,
                                                                press_has_image;
static uint64_t press_reader_hash;
static char press_query[256];
static int press_loading_kind;
static void press_archive_scan(void) {
    v1_archive_scan();
    press_dates_count = 0;
    for (int i = 0; i < news_date_count; i++)
        press_dates[press_dates_count++] = news_dates[i];
    char p[2048];
    local_path(p, sizeof(p), "press");
    DIR *d = opendir(p);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) && press_dates_count < 2048) {
            int date;
            char tail;
            if (sscanf(e->d_name, "edition-%8d.bin%c", &date, &tail) == 1 &&
                strlen(e->d_name) == 20) {
                int dup = 0;
                for (int i = 0; i < press_dates_count; i++)
                    if (press_dates[i] == date)
                        dup = 1;
                if (!dup)
                    press_dates[press_dates_count++] = date;
            }
        }
        closedir(d);
    }
    for (int i = 0; i < press_dates_count; i++)
        for (int j = i + 1; j < press_dates_count; j++)
            if (press_dates[j] > press_dates[i]) {
                int t = press_dates[i];
                press_dates[i] = press_dates[j];
                press_dates[j] = t;
            }
}
static void press_legacy(int date) {
    memset(&press_list, 0, sizeof(press_list));
    press_list.magic = 0x50524c31;
    press_list.date = date;
    if (news_load(date))
        for (int i = 0; i < newspaper.count; i++) {
            NewsArticle *a = &newspaper.article[i];
            if (a->category == 5)
                continue;
            PressItem *b = &press_list.item[press_list.count++];
            snprintf(b->title, 256, "%s", a->title);
            snprintf(b->url, 2048, "%s", a->url);
            snprintf(b->summary, 1600, "%s", a->body);
            snprintf(b->publisher, 96, "%s", a->publisher);
            snprintf(b->category, 48, "%s", news_category(a->category));
            snprintf(b->published, 96, "%s", a->published);
        }
}
static void press_date_open(int date) {
    press_list_page = 0;
    press_query[0] = 0;
    if (!press_load_day(date))
        press_legacy(date);
    extra_open(95);
    if (date == news_date() && !press_busy()) {
        press_loading_kind = 1;
        press_start_day(date, options[OPT_LANGUAGE]);
    }
}
static void press_read_open(int i) {
    if (i < 0 || i >= press_list.count)
        return;
    if (press_busy())
        press_cancel();
    press_loading_kind = 2;
    int r = press_open(&press_list.item[i]);
    press_reader_hash = 0;
    press_reader_page = 0;
    extra_open(96);
    if (!r)
        ui_message("Artikel konnte nicht geladen werden.");
    else if (!press_busy())
        article_news_open();
}
static void press_image_draw(int y) {
    char p[2048], m[3];
    press_image_path(p, sizeof(p));
    FILE *f = fopen(p, "rb");
    if (!f)
        return;
    int w, h, max;
    if (fscanf(f, "%2s %d %d %d", m, &w, &h, &max) == 4 && !strcmp(m, "P5") && w == 540 &&
        h == 280 && max == 255) {
        fgetc(f);
        for (int row = 0; row < 280 && y + row < 740; row++)
            if (fread(ui_pixels + (y + row) * 600 + 30, 1, 540, f) != 540)
                break;
    }
    fclose(f);
}
static void press_paginate(void) {
    uint64_t key = press_id(press_article.body) ^ press_id(press_article.title);
    if (key == press_reader_hash)
        return;
    press_reader_hash = key;
    press_reader_pages = 0;
    char path[2048];
    press_image_path(path, sizeof(path));
    press_has_image = !access(path, R_OK);
    int title_lines = 0;
    const char *t = press_article.title;
    while (*t && title_lines < 4) {
        t = md_next(t, 540, &editorial);
        title_lines++;
    }
    press_initial_y = 119 + title_lines * 36 + 47 + (press_has_image ? 292 : 0);
    if (press_initial_y > 650)
        press_initial_y = 650;
    const char *p = press_article.body;
    while (*p && press_reader_pages < 4096) {
        press_offsets[press_reader_pages] = p - press_article.body;
        int y = press_reader_pages ? 100 : press_initial_y, lines = 0;
        while (*p && y + 30 < 730) {
            if (!press_reader_pages && !lines)
                glyph(&p);
            int inset = !press_reader_pages && lines < 2 ? 48 : 0;
            const char *n = md_next(p, 540 - inset, &paper_text);
            if (n == p)
                break;
            p = n;
            y += 30;
            if (*p == '\n') {
                p++;
                y += 9;
            }
            lines++;
        }
        if (!lines && *p)
            p = md_next(p, 540, &paper_text);
        press_reader_pages++;
    }
    if (!press_reader_pages) {
        press_offsets[0] = 0;
        press_reader_pages = 1;
    }
    if (press_reader_page >= press_reader_pages)
        press_reader_page = press_reader_pages - 1;
}
static void press_reader_draw(void) {
    if (!*press_article.body) {
        icon_button(528, 58, IC_MORE);
        wrapped_raw(30, 140, press_article.title, &editorial, 540, 4);
        wrapped(30, 380, "Artikel wird geladen …", &paper_text, 540, 3);
        return;
    }
    press_paginate();
    label_at(30, 59, 472, 32, press_article.publisher, &caption);
    icon_button(528, 58, IC_MORE);
    rect(28, 97, 544, 2, 0);
    int y = 100;
    if (!press_reader_page) {
        wrapped_raw(30, 119, press_article.title, &editorial, 540, 4);
        int lines = 0;
        const char *t = press_article.title;
        while (*t && lines < 4) {
            t = md_next(t, 540, &editorial);
            lines++;
        }
        int sub = 119 + lines * 36 + 8;
        drawtext(30, sub,
                 press_article.complete ? "Aus der Quelle · offline gespeichert"
                                        : "Nur Kurzfassung verfügbar",
                 &caption, 540);
        if (press_has_image)
            press_image_draw(sub + 37);
        y = press_initial_y;
    }
    const char *p = press_article.body + press_offsets[press_reader_page];
    int row = 0;
    while (*p && y + 30 < 730) {
        const char *start = p;
        char initial[8] = {0};
        int inset = 0;
        if (!press_reader_page && row == 0) {
            glyph(&p);
            snprintf(initial, sizeof(initial), "%.*s", (int)(p - start), start);
            drawtext_raw(30, y - 8, initial, &title, 60);
        }
        if (!press_reader_page && row < 2)
            inset = 48;
        const char *n = md_next(p, 540 - inset, &paper_text);
        char b[1024];
        snprintf(b, sizeof(b), "%.*s", (int)(n - p), p);
        drawtext_raw(30 + inset, y, b, &paper_text, 540 - inset);
        p = n;
        y += 30;
        if (*p == '\n') {
            p++;
            y += 9;
        }
        row++;
    }
    rect(28, 739, 544, 1, 175);
    pager(746, &press_reader_page, press_reader_pages);
}
static void press_list_draw(void) {
    char date[80];
    if (*press_list.query) {
        snprintf(date, sizeof(date), "%s", press_list.query);
    } else
        snprintf(date, sizeof(date), "%02u.%02u.%04u", press_list.date % 100,
                 press_list.date / 100 % 100, press_list.date / 10000);
    heading_actions("News", 1);
    search_action();
    label_at(30, 120, 540, 28, press_busy() ? tr("Nachrichten werden geladen …") : date, &caption);
    for (int r = 0; r < 5 && press_list_page * 5 + r < press_list.count; r++) {
        int i = press_list_page * 5 + r, y = 151 + r * 113;
        PressItem *a = &press_list.item[i];
        char b[180];
        snprintf(b, sizeof(b), "%s · %s", tr(a->category), a->publisher);
        list_row(28, y, 544, 113, -1, 0, a->title, b, &body, 2);
    }
    if (!press_list.count && !press_busy())
        wrapped(30, 180, "Keine Nachrichten verfügbar. Gespeicherte Ausgaben bleiben erhalten.",
                &editorial, 540, 5);
    pager(746, &press_list_page, (press_list.count + 4) / 5);
}
static void polish_news_draw(void) {
    if (screen == 96) {
        press_reader_draw();
        return;
    }
    if (screen == 95) {
        press_list_draw();
        return;
    }
    if (screen == 97) {
        heading("Quelle");
        drawtext_raw(30, 145, press_article.publisher, &body, 540);
        wrapped_raw(30, 207, press_article.title, &editorial, 540, 3);
        wrapped_raw(30, 370, press_article.url, &small, 540, 7);
        uint8_t temp[qrcodegen_BUFFER_LEN_MAX], qr[qrcodegen_BUFFER_LEN_MAX];
        if (qrcodegen_encodeText(press_article.url, temp, qr, qrcodegen_Ecc_LOW, 1, 12,
                                 qrcodegen_Mask_AUTO, true)) {
            int n = qrcodegen_getSize(qr), scale = 180 / (n + 8);
            for (int yy = 0; yy < n; yy++)
                for (int xx = 0; xx < n; xx++)
                    if (qrcodegen_getModule(qr, xx, yy))
                        rect(210 + (xx + 4) * scale, 550 + (yy + 4) * scale, scale, scale, 0);
        }
        return;
    }
    heading_actions("News", 1);
    search_action();
    label_at(30, 132, 540, 35, tr("Kuratierte Ausgaben"), &caption);
    for (int r = 0; r < 7 && press_date_page * 7 + r < press_dates_count; r++) {
        int date = press_dates[press_date_page * 7 + r], y = 181 + r * 75;
        char b[80];
        snprintf(b, sizeof(b), "%02d.%02d.%04d%s", date % 100, date / 100 % 100, date / 10000,
                 date == news_date() ? tr(" · Heute") : "");
        const char *detail =
            tr(date == news_date() ? "Nachrichten des Tages" : "Gespeicherte Ausgabe");
        ListLayout l = list_row(28, y, 544, 75, -1, 62, b, detail, &body, 1);
        icon(528, l.action_y, IC_NEXT);
    }
    pager(746, &press_date_page, (press_dates_count + 6) / 7);
}
static int polish_news_input(const Input *in) {
    if (in->kind == INPUT_SWIPE && screen == 96) {
        if (in->dx < 0 && press_reader_page + 1 < press_reader_pages)
            press_reader_page++;
        else if (in->dx > 0 && press_reader_page)
            press_reader_page--;
        return 1;
    }
    if (in->kind != INPUT_TAP)
        return 2;
    int x = in->x, y = in->y;
    if (screen == 96) {
        if (search_action_hit(x, y))
            extra_open(97);
        else if (y >= 746 && y < 790) {
            if (x < 300 && press_reader_page)
                press_reader_page--;
            else if (x >= 300 && press_reader_page + 1 < press_reader_pages)
                press_reader_page++;
        }
        return 1;
    }
    if (screen == 95) {
        if (search_action_hit(x, y))
            open_keyboard(150, "News suchen", press_query);
        else if (y >= 151 && y < 716) {
            int i = press_list_page * 5 + (y - 151) / 113;
            press_read_open(i);
        } else if (y >= 746 && y < 790) {
            if (x < 300 && press_list_page)
                press_list_page--;
            else if (x >= 300 && (press_list_page + 1) * 5 < press_list.count)
                press_list_page++;
        }
        return 1;
    }
    if (search_action_hit(x, y))
        open_keyboard(150, "News suchen", press_query);
    else if (y >= 181 && y < 706) {
        int i = press_date_page * 7 + (y - 181) / 75;
        if (i < press_dates_count)
            press_date_open(press_dates[i]);
    } else if (y >= 746 && y < 790) {
        if (x < 300 && press_date_page)
            press_date_page--;
        else if (x >= 300 && (press_date_page + 1) * 7 < press_dates_count)
            press_date_page++;
    }
    return 1;
}
#include "braintrain.h"
#include "language_plus.h"
static time_t press_retry_at;
static int press_daily_enabled;
static void polish_open(int route) {
    if (route == 72) {
        photo_fill = photo_rotation = 0;
        photo_controls = 1;
    }
    if (route == 94 || route == 36) {
        polish_wiki_archive();
        wiki_history_scan();
    }
    if (route == 86 || route == 67) {
        press_archive_scan();
        press_daily_enabled = 1;
        extra_write_text("press-daily-enabled.txt", "1");
        news_enabled = 0;
        news_save_settings();
        news_cancel();
    }
    if (route == 99)
        language_book_page = 0;
}
static void polish_view(void) {
    switch (screen) {
    case 94:
        polish_wiki_history();
        break;
    case 95:
    case 96:
    case 97:
        polish_news_draw();
        break;
    case 99:
        language_book();
        break;
    }
}
static int polish_input(const Input *in) {
    if (screen == 94)
        return wiki_history_input(in);
    if (screen == 95 || screen == 96 || screen == 97)
        return polish_news_input(in);
    if (screen == 99 && in->kind == INPUT_TAP && in->y >= 679 && in->y < 723) {
        if (in->x < 300 && language_book_page)
            language_book_page--;
        else if (in->x >= 300 && language_book_page < 3)
            language_book_page++;
        return 1;
    }
    return 2;
}
static int polish_tick(void) {
    int changed = 0, done = press_poll();
    if (done) {
        if (done > 0) {
            press_reader_hash = 0;
            if (screen == 96 && press_loading_kind == 2) {
                article_news_open();
                changed = 1;
            }
            if (screen == 86)
                press_archive_scan();
            if (screen == 95 || screen == 96 || screen == 86)
                changed = 1;
        } else {
            if (screen == 95 || screen == 96) {
                ui_message("Nicht geladen. Gespeicherte Inhalte bleiben verfügbar.");
                changed = 1;
            }
            press_retry_at = time(NULL) + 3600;
        }
    }
    if (screen == 70 && wordle_game.date != news_date()) {
        wordle_game.date = news_date();
        wordle_game.row = wordle_game.over = 0;
        memset(wordle_game.guesses, 0, sizeof(wordle_game.guesses));
        memset(wordle_game.result, 0, sizeof(wordle_game.result));
        snprintf(word_secret, 6, "%s", wordle_answers[v1_word_index()]);
        v1_dirty = 1;
        changed = 1;
    }
    if (news_enabled) {
        news_enabled = 0;
        news_cancel();
        news_save_settings();
        press_daily_enabled = 1;
        extra_write_text("press-daily-enabled.txt", "1");
    }
    static int initialized;
    if (!initialized) {
        initialized = 1;
        char *s = service_read("press-daily-enabled.txt", 8);
        if (s) {
            press_daily_enabled = atoi(s) != 0;
            free(s);
        }
    }
    if (press_daily_enabled && !press_busy() && time(NULL) >= press_retry_at && screen != 95 &&
        screen != 96) {
        press_retry_at = time(NULL) + 60;
        char n[100], p[2048];
        snprintf(n, sizeof(n), "press/edition-%08d.bin", news_date());
        local_path(p, sizeof(p), n);
        if (access(p, R_OK)) {
            char ip[80];
            device_ip(ip, 80);
            if (device_wifi_enabled() && *ip) {
                press_retry_at = time(NULL) + 3600;
                press_start_day(news_date(), options[OPT_LANGUAGE]);
            }
        }
    }
    return changed;
}
static int polish_poll(void) {
    if (press_busy())
        return 250;
    if (press_daily_enabled)
        return 60000;
    return -1;
}
static int polish_commit(int target, const char *text) {
    if (target == 150) {
        if (!*text)
            return 1;
        snprintf(press_query, 256, "%s", text);
        press_cancel();
        memset(&press_list, 0, sizeof(press_list));
        snprintf(press_list.query, 256, "%s", text);
        press_list_page = 0;
        press_loading_kind = 1;
        press_search(text, options[OPT_LANGUAGE]);
        extra_open(95);
        return 1;
    }
    if (target == 158) {
        language_check_written(text);
        return 1;
    }
    return 0;
}
static void polish_close(void) {
    press_cancel();
    polish_wiki_archive();
}

static void polish_wiki_refetched(void) {
    wiki_refresh_id = 0;
    char p[2048];
    wiki_cache_path(p, sizeof(p), wiki_key(), "deleted");
    unlink(p);
    wiki_saved_hash = 0;
    polish_wiki_archive();
}

static int wiki_refresh_failed(void) {
    if (!wiki_refresh_id)
        return 0;
    char path[2048];
    wiki_cache_path(path, sizeof(path), wiki_refresh_id, "bin");
    WikiSaved *a = malloc(sizeof(*a));
    FILE *f = fopen(path, "rb");
    int ok = a && f && fread(a, sizeof(*a), 1, f) == 1 && a->magic == 0x574b3131;
    if (f)
        fclose(f);
    if (ok) {
        a->title[255] = a->text[524287] = a->image[2047] = a->name[511] = 0;
        wiki_article_language = a->language >= 0 && a->language < 4 ? a->language : 0;
        discover_restore_lookup(a->title, a->text, a->image, a->name);
        wiki_saved_hash = press_id(a->text) ^ wiki_key();
        wiki_cache_path(path, sizeof(path), wiki_refresh_id, "pgm");
        char target[2048];
        local_path(target, sizeof(target), "wiki-image.pgm");
        if (copy_file(path, target))
            extra_write_text("wiki-image-url.txt", a->image);
    }
    free(a);
    wiki_refresh_id = 0;
    return ok && article_wiki_open();
}
