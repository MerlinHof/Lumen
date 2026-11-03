/* Shared reader controls, catalog preferences and page-bound ink notes. */
static int64_t ink_refresh_at, ink_save_at;
static int u15_ready, note_ink_mode, ink_eraser, ink_width = 3, ink_note = -1, ink_page = -1,
                                                 ink_last_x, ink_last_y, ink_drag, ink_dirty;
static unsigned char ink_pixels[600 * 688], ink_undo[600 * 688];
static int notes_move_folder, notes_delete_folder = -1;
static struct {
    char name[80], url[2048];
    int enabled;
} source15[10];
static int source15_count, source15_page, sources15_dirty;

static int note_offsets[8193], note_numbers[8193], note_rows;
static void ink_save(void) {
    if (ink_note < 0 || !ink_dirty)
        return;
    char p[2048];
    snprintf(p, sizeof(p), "%s/notes/%d-page-%d.ink", data_root, ink_note, ink_page);
    int empty=1;for(size_t i=0;i<sizeof(ink_pixels);i++)if(ink_pixels[i]!=255){empty=0;break;}
    if(empty){unlink(p);ink_dirty=0;return;}
    if (atomic_write(p, ink_pixels, sizeof(ink_pixels)))
        ink_dirty = 0;
}
static void u15_flush(void) { ink_save(); }
static void ink_load(void) {
    if (ink_note == note_selected && ink_page == extra_page)
        return;
    ink_save();
    ink_note = note_selected;
    ink_page = extra_page;
    memset(ink_pixels, 255, sizeof(ink_pixels));
    char p[2048];
    snprintf(p, sizeof(p), "%s/notes/%d-page-%d.ink", data_root, ink_note, ink_page);
    FILE *f = fopen(p, "rb");
    if (f) {
        if (fread(ink_pixels, 1, sizeof(ink_pixels), f) != sizeof(ink_pixels))
            memset(ink_pixels, 255, sizeof(ink_pixels));
        fclose(f);
    }
    ink_dirty = 0;
    memcpy(ink_undo, ink_pixels, sizeof(ink_pixels));
}
static void sources15_save(void) {
    sources15_dirty = 1;
    char p[2048];
    local_path(p, sizeof(p), "store-sources15.bin");
    atomic_write(p, source15, sizeof(source15));
}
static void u15_init(void) {
    if (u15_ready)
        return;
    u15_ready = 1;
    const char *n[] = {"Project Gutenberg", "Internet Archive", "epubBooks", "BDeBooks",
                       "Standard Ebooks"};
    const char *u[] = {"", "", "https://www.epubbooks.com/search?q={q}",
                       "https://bdebooks.com/en/all-books/?q={q}",
                       "https://standardebooks.org/feeds/opds"};
    for (int i = 0; i < 5; i++) {
        snprintf(source15[i].name, 80, "%s", n[i]);
        snprintf(source15[i].url, 2048, "%s", u[i]);
        source15[i].enabled = 1;
    }
    source15_count = 5;
    char p[2048];
    local_path(p, sizeof(p), "store-sources15.bin");
    FILE *f = fopen(p, "rb");
    if (f) {
        if (fread(source15, sizeof(source15), 1, f) != 1)
            memset(source15, 0, sizeof(source15));
        fclose(f);
        for (int i = 0; i < 10; i++) {
            source15[i].name[79] = source15[i].url[2047] = 0;
        }
        source15_count = 0;
        while (source15_count < 10 && *source15[source15_count].name)
            source15_count++;
    }
    local_path(p, sizeof(p), "reader-progress15.txt");
    f = fopen(p, "r");
    if (f) {
        fscanf(f, "%d", &reader_progress_mode);
        fclose(f);
    }
    if (reader_progress_mode < 0 || reader_progress_mode > 6)
        reader_progress_mode = 0;
}
static void u15_sources_prepare(const char *encoded) {
    u15_init();
    char first[2][2048];
    memcpy(first, store_urls, sizeof(first));
    store_provider_count = 0;
    for (int i = 0; i < source15_count && store_provider_count < 12; i++)
        if (source15[i].enabled) {
            char *dest = store_urls[store_provider_count];
            if (i < 2)
                snprintf(dest, 2048, "%s", first[i]);
            else {
                const char *q = strstr(source15[i].url, "{q}");
                if (q)
                    snprintf(dest, 2048, "%.*s%s%s", (int)(q - source15[i].url), source15[i].url,
                             encoded, q + 3);
                else
                    snprintf(dest, 2048, "%s", source15[i].url);
            }
            store_provider_count++;
        }
}
static int folder15_index(int id) {
    for (int i = 0; i < note_tree.nf; i++)
        if (note_tree.folder[i].id == id)
            return i;
    return -1;
}
static int folder15_inside(int id, int root) {
    for (int i = 0; i < 513 && id; i++) {
        if (id == root)
            return 1;
        int f = folder15_index(id);
        if (f < 0)
            break;
        id = note_tree.folder[f].parent;
    }
    return 0;
}
static void note15_layout(const char *text) {
    const char *p = text;
    note_rows = 0;
    int line = 1;
    while (*p && note_rows < 8192) {
        note_offsets[note_rows] = p - text;
        note_numbers[note_rows] = line;
        const char *q = note_next_line(
            p, keyboard && keyboard_target != 30 ? 544 : (note_rows % 23 < 2 ? 420 : 528));
        if (q <= p)
            break;
        if (q[-1] == '\n')
            line++;
        p = q;
        note_rows++;
    }
    if (note_rows && p > text && p[-1] == '\n' && note_rows < 8192) {
        note_offsets[note_rows] = p - text;
        note_numbers[note_rows++] = line;
    }
    note_offsets[note_rows] = p - text;
    if (!note_rows) {
        note_numbers[0] = 1;
        note_rows = 1;
        note_offsets[1] = 0;
    }
}
static int u15_note_page_count(void) {
    note15_layout(note_text);
    return (note_rows + 22) / 23;
}
static void u15_note_editor_draw(void) {
    const char *text = keyboard && keyboard_multiline() ? edit : note_text;
    note15_layout(text);
    int editing = keyboard && keyboard_multiline(), rows = editing ? 15 : 23, cursor_row = 0;
    for (int i = 0; i < note_rows; i++)
        if (note_cursor >= note_offsets[i])
            cursor_row = i;
    if (editing && !note_scroll_lock) {
        if (cursor_row < note_view_start)
            note_view_start = cursor_row;
        if (cursor_row >= note_view_start + rows)
            note_view_start = cursor_row - rows + 1;
    }
    if (!editing)
        note_view_start = extra_page * 23;
    int left = editing && keyboard_target != 30 ? 28 : 48;
    for (int r = 0; r < rows && note_view_start + r < note_rows; r++) {
        int i = note_view_start + r;
        char b[1024];
        int n = note_offsets[i + 1] - note_offsets[i];
        if (n > 1023)
            n = 1023;
        memcpy(b, text + note_offsets[i], n);
        b[n] = 0;
        char num[20];
        snprintf(num, 20, "%d", note_numbers[i]);
        if (i > 0 && note_numbers[i] == note_numbers[i - 1])
            num[0] = 0;
        ink_color = 135;
        if (left == 48)
            small_number17(24, 54 + r * 29, num, 135);
        ink_color = 0;
        drawtext_raw(left, 54 + r * 29, b, &small,
                     editing && keyboard_target != 30 ? 544 : (i % 23 < 2 ? 420 : 528));
        if (editing && cursor_row == i && cursor_visible) {
            snprintf(b, sizeof(b), "%.*s", note_cursor - note_offsets[i], text + note_offsets[i]);
            text_caret(left + textwidth_raw(b, &small), 54 + r * 29, &small);
        }
    }
}
static void u15_note_editor_touch(int x, int y) {
    note15_layout(edit);
    int row = note_view_start + (y - 54) / 29;
    if (row < 0)
        row = 0;
    if (row >= note_rows)
        row = note_rows - 1;
    const char *p = edit + note_offsets[row], *end = edit + note_offsets[row + 1];
    int px = keyboard && keyboard_target != 30 ? 28 : 48;
    while (p < end && *p != '\n') {
        const char *q = p;
        unsigned c = glyph(&q);
        int w = glyph_advance(&small, c);
        if (x < px + w / 2)
            break;
        px += w;
        p = q;
    }
    note_cursor = p - edit;
}
static void u15_note_tools(void) {
    icon_button(470, 54, IC_EDIT);
    icon_button(532, 54, IC_MARKDOWN);
}
static void u15_export_photo(void);
static void u15_note_overlay(void) {
    ink_load();
    for (int y = 0; y < 688; y++)
        for (int x = 0; x < 600; x++) {
            unsigned char v = ink_pixels[y * 600 + x];
            if (v < ui_pixels[(y + 54) * 600 + x])
                ui_pixels[(y + 54) * 600 + x] = v;
        }
    if (note_ink_mode) {
        rect(0, 54, 600, 54, 255);
        icon_button(10, 58, IC_BACK);
        button_h(70, 58, 100, 44, ink_eraser ? "Radierer" : "Stift", ink_eraser);
        button_h(180, 58, 80, 44, ink_width == 3 ? "Fein" : "Breit", 0);
        icon_button(280, 58, IC_UNDO);
        region(518, 58, 64, 44);
        v1_icon(528, 58, APP_PHOTOS);
    } else
        u15_note_tools();
}
static void ink_stamp(int x, int y) {
    y -= 54;
    int radius = ink_eraser ? 14 : ink_width;
    for (int yy = y - radius; yy <= y + radius; yy++)
        for (int xx = x - radius; xx <= x + radius; xx++)
            if (xx >= 0 && xx < 600 && yy >= 0 && yy < 688) {
                int d2 = (xx - x) * (xx - x) + (yy - y) * (yy - y);
                if (d2 > (radius + 1) * (radius + 1))
                    continue;
                if(ink_eraser){if(d2<=radius*radius)ink_pixels[yy*600+xx]=255;continue;}
                double a = d2 <= (radius - 1) * (radius - 1)
                               ? 1
                               : ((radius + .5) * (radius + .5) - d2) / (2 * radius);
                if (a < 0)
                    continue;
                if (a > 1)
                    a = 1;
                int at = yy * 600 + xx;
                if(ink_eraser) { if(d2<=radius*radius)ink_pixels[at]=255; }
                else ink_pixels[at]=(unsigned char)(ink_pixels[at]*(1-a));
            }
    ink_dirty = 1;
}
static void ink_line(int x, int y) {
    int dx = x - ink_last_x, dy = y - ink_last_y, n = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
    if (!n)
        n = 1;
    for (int i = 0; i <= n; i++)
        ink_stamp(ink_last_x + dx * i / n, ink_last_y + dy * i / n);
    ink_last_x = x;
    ink_last_y = y;
}
static int u15_commit(int target, const char *text) {
    if (u16_commit(target, text))
        return 1;
    if (target == 160) {
        if ((!strncmp(text, "https://", 8) || !strncmp(text, "http://", 7)) &&
            strlen(text) < 2048 && !strpbrk(text, "\r\n\"\\<> ") && source15_count < 10) {
            int i = source15_count++;
            snprintf(source15[i].name, 80, "Katalog %d", i - 4);
            snprintf(source15[i].url, 2048, "%s", text);
            source15[i].enabled = 1;
            sources15_save();
        } else
            ui_message("Gültige Katalog-URL eingeben.");
        return 1;
    }
    if (target == 162) {
        int f = folder15_index(note_tree.current);
        if (f >= 0 && *text) {
            snprintf(note_tree.folder[f].name, 96, "%s", text);
            v1_dirty = 1;
            v1_save();
        }
        return 1;
    }
    return 0;
}
static const char *progress15[] = {"Default",  "Seitenpfeile",  "Prozent",           "Seitenzahl",
                                   "Restzeit", "Keine Anzeige", "Fortschrittsbalken"};
static void u15_reader_progress(void) {
    if (!options[OPT_PROGRESS] || reader_progress_mode == 5)
        return;
    int total = engine_count();
    char b[80];
    if (reader_progress_mode == 1) {
        pager(746, &page, total);
        pagination_bind(&page, total, 746, 1, turn);
        return;
    }
    if (reader_progress_mode == 2)
        snprintf(b, 80, "%d%%", total > 1 ? page * 100 / (total - 1) : 100);
    else if (reader_progress_mode == 4)
        snprintf(b, 80, "~ %d min", (total - page - 1) * 2);
    else
        snprintf(b, 80, "%d / %d", page + 1, total);
    ink_color = reader_progress_mode == 3 ? 130 : 0;
    if (reader_progress_mode != 6)
        centered(reader_progress_mode == 3 ? 392 : 180, 746, reader_progress_mode == 3 ? 180 : 240,
                 35, b, &caption);
    ink_color = 0;
    if (reader_progress_mode == 0 || reader_progress_mode == 6)
        progress(28, 787, 544, page + 1, total);
}
static int u15_draw(void) {
    if (screen == 100) {
        heading("Rechner · Einstellungen");
        select_row17(155, "Winkelmaß", calc_degrees ? "DEG" : "RAD");
        select_row17(245, "Ergebnisstellen",
                     calc_precision16 == 6    ? "6"
                     : calc_precision16 == 15 ? "15"
                                              : "10");
        button_h(28, 460, 544, 52, "Verlauf öffnen", 0);
        button_h(28, 530, 544, 52, "Verlauf löschen", 0);
        button_h(28, 600, 544, 52, "Speicher löschen", 0);
        return 1;
    }
    if (screen == 101) {
        heading("Store · Quellen");
        icon_button(528, 74, IC_PLUS);
        for (int r = 0; r < 6 && source15_page * 6 + r < source15_count; r++) {
            int i = source15_page * 6 + r, y = 145 + r * 82;
            drawtext_raw(30, y + 8, source15[i].name, &small, 420);
            drawtext_raw(30, y + 39, source15[i].url, &caption, 420);
            toggle(492, y + 10, source15[i].enabled);
            rect(28, y + 75, 544, 1, 205);
        }
        pager(679, &source15_page, (source15_count + 5) / 6);
        return 1;
    }
    if (screen == 104) {
        heading("Reader · Optionen");
        select_row17(155, "Fortschrittsanzeige", progress15[reader_progress_mode]);
        drawtext(30, 626, "Wischen zum Blättern", &small, 440);
        toggle(492, 622, options[OPT_SWIPE]);
        drawtext(30, 686, "Mehr Platz vorwärts", &small, 440);
        toggle(492, 682, options[OPT_TAP]);
        return 1;
    }
    if (screen == 102) {
        heading("Ordner löschen?");
        wrapped(28, 170, "Alle enthaltenen Notizen und Unterordner werden ebenfalls gelöscht.",
                &body, 540, 4);
        button_h(28, 420, 260, 54, "Abbrechen", 0);
        button_h(312, 420, 260, 54, "Löschen", 0);
        return 1;
    }
    return 0;
}
static int u15_input(const Input *in) {
    if (keyboard || dropdown)
        return -1;
    if (sleeping)
        return -1;
    if (screen != 30 && ink_note >= 0) {
        ink_save();
        note_ink_mode = 0;
    }
    int x = in->x, y = in->y;
    if (in->kind == INPUT_SLEEP) {
        ink_save();
        ink_drag = 0;
    }
    if (screen == 30 && !keyboard) {
        if (in->kind == INPUT_TAP && y >= 54 && y < 108 && x >= 460 && !note_ink_mode) {
            if (x < 524) {
                if (note_selected < 0)
                    extra_commit(30, *note_text ? note_text : "Notiz");
                ink_load();
                note_ink_mode = 1;
                return 1;
            }
            return -1;
        }
        if (note_ink_mode) {
            if (in->kind == INPUT_HOLD)
                return 2;
            if (in->kind == INPUT_TAP && y >= 54 && y < 108) {
                if (x >= 500) {
                    u15_export_photo();
                    return 1;
                }
                if (x < 65) {
                    ink_save();
                    note_ink_mode = 0;
                } else if (x < 175)
                    ink_eraser ^= 1;
                else if (x < 270)
                    ink_width = ink_width == 3 ? 7 : 3;
                else if (x < 340) {
                    memcpy(ink_pixels, ink_undo, sizeof(ink_pixels));
                    ink_dirty = 1;
                }
                return 1;
            }
            if (in->kind == INPUT_DOWN && y >= 112 && y < 742) {
                ink_load();
                memcpy(ink_undo, ink_pixels, sizeof(ink_pixels));
                ink_last_x = x;
                ink_last_y = y;
                ink_drag = 1;
                ink_stamp(x, y);
                return 1;
            }
            if (ink_drag &&
                (in->kind == INPUT_MOVE || in->kind == INPUT_TAP || in->kind == INPUT_SWIPE)) {
                if (y >= 112 && y < 742)
                    ink_line(x, y);
                if (in->kind == INPUT_MOVE) {
                    int64_t now = extra_millis();
                    if (now < ink_refresh_at)
                        return 2;
                    ink_refresh_at = now + 32;
                }
                if (in->kind != INPUT_MOVE) {
                    ink_drag = 0;
                    ink_save_at = extra_millis() + 1500;
                }
                return 1;
            }
        }
    }
    if (screen == 36 && (discover_busy() || wiki_reader_pending) && in->kind == INPUT_TAP &&
        y >= 120)
        return 1;
    if (in->kind != INPUT_TAP)
        return -1;
    if (screen == 27 && search_action_hit(x, y)) {
        extra_open(100);
        return 1;
    }
    if (screen == 100) {
        if (y >= 155 && y < 203 && x >= 300) {
            static const char *t[] = {"DEG", "RAD"};
            open_dropdown(931, t, 2, !calc_degrees, 300, 208, 272);
        } else if (y >= 245 && y < 293 && x >= 300) {
            static const char *p[] = {"6", "10", "15"};
            open_dropdown(919, p, 3,
                          calc_precision16 == 6    ? 0
                          : calc_precision16 == 15 ? 2
                                                   : 1,
                          300, 298, 272);
        } else if (y >= 460 && y < 512) {
            extra_page = 0;
            extra_open(42);
        } else if (y >= 530 && y < 582) {
            calc_history_count = 0;
            extra_write_text("calculator-history.txt", "");
        } else if (y >= 600 && y < 652)
            calc_memory = 0;
        return y >= 120 ? 1 : -1;
    }
    if (screen == 104 && y >= 620 && y < 730) {
        int id = y < 675 ? OPT_SWIPE : OPT_TAP;
        options[id] ^= 1;
        library_options_save();
        return 1;
    }
    if (screen == 104 && x >= 300 && y >= 155 && y < 203) {
        open_dropdown(916, progress15, 7, reader_progress_mode, 300, 208, 272);
        return 1;
    }
    if (screen == 101 && !keyboard && !dropdown && y < 48 && x < 62) {
        extra_return(10);
#ifndef LUMEN_HOST
        if (sources15_dirty)
            store_search();
#endif
        sources15_dirty = 0;
        return 1;
    }
    if (screen == 101) {
        if (y >= 74 && y < 118 && x >= 516) {
            open_keyboard(160, "Katalog-URL ({q} = Suche)", "");
            return 1;
        }
        if (y >= 145 && y < 637 && x >= 460) {
            int i = source15_page * 6 + (y - 145) / 82;
            if (i < source15_count) {
                source15[i].enabled ^= 1;
                sources15_save();
            }
            return 1;
        }
        return -1;
    }
    if (screen == 10 && y >= 74 && y < 118 && x >= 460 && x < 516) {
        extra_open(101);
        return 1;
    }
    if (screen == 102 && y >= 420 && y < 474) {
        if (x >= 300 && notes_delete_folder >= 0) {
            int id = notes_delete_folder;
            for (int i = 0; i < note_count; i++)
                if (folder15_inside(v1_folder_for(note_ids[i]), id)) {
                    char p[2048];
                    snprintf(p, sizeof(p), "%s/notes/%d.txt", data_root, note_ids[i]);
                    note_delete16(note_ids[i]);
                }
            int remove[512] = {0};
            for (int i = 0; i < note_tree.nf; i++)
                remove[i] = folder15_inside(note_tree.folder[i].id, id);
            for (int i = note_tree.nf - 1; i >= 0; i--)
                if (remove[i]) {
                    memmove(note_tree.folder + i, note_tree.folder + i + 1,
                            (note_tree.nf - i - 1) * sizeof(NoteFolder));
                    note_tree.nf--;
                }
            v1_dirty = 1;
            v1_save();
            notes_scan();
        }
        extra_return(29);
        return 1;
    }
    if (screen == 84 && y >= 145 && y < 641) {
        int i = folder_page * 8 + (y - 145) / 62;
        if (i <= note_tree.nf) {
            int dest = i ? note_tree.folder[i - 1].id : 0;
            if (notes_move_folder) {
                if (folder15_inside(dest, notes_move_folder)) {
                    ui_message("Ziel liegt im selben Ordner.");
                    return 1;
                }
                int f = folder15_index(notes_move_folder);
                if (f >= 0)
                    note_tree.folder[f].parent = dest;
                v1_dirty = 1;
            } else
                v1_place(note_selected, dest);
            v1_save();
            notes_move_folder = 0;
            extra_return(29);
        }
        return 1;
    }
    return -1;
}

static void u15_notes_action(int kind, int id) {
    notes_move_folder = kind == 1 ? id : 0;
    if (kind == 2) {
        notes_delete_folder = id;
        extra_open(102);
    } else {
        if (!kind)
            note_selected = id;
        folder_page = 0;
        extra_open(84);
    }
}

static void u15_export_photo(void) {
    ink_save();
    memset(ui_pixels, 255, sizeof(ui_pixels));
    u15_note_editor_draw();
    for (int i = 0; i < 600 * 688; i++)
        if (ink_pixels[i] < ui_pixels[i + 54 * 600])
            ui_pixels[i + 54 * 600] = ink_pixels[i];
    char p[2048], dir[2048];
    local_path(dir, sizeof(dir), "photos");
    mkdir(dir, 0700);
    snprintf(p, sizeof(p), "%s/note-%d-%d-%lld.pgm", dir, note_selected, extra_page,
             (long long)time(NULL));
    unsigned char *buf = malloc(480064);
    if (buf) {
        int n = snprintf((char *)buf, 64, "P5\n600 800\n255\n");
        memcpy(buf + n, ui_pixels, 480000);
        if (atomic_write(p, buf, n + 480000))
            ui_message("Foto gespeichert.");
        free(buf);
    }
    v1_photos_scan();
}
