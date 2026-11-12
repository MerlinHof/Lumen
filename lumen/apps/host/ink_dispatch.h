/* App-owned module; assembled by apps/host/compose.h. */
static int u15_commit(int target, const char *text) {
    if (u16_commit(target, text))
        return 1;
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
static int u15_draw(void) {
    if (ui_route() == 102) {
        heading("Ordner löschen?");
        wrapped(28, 170, "Alle enthaltenen Notizen und Unterordner werden ebenfalls gelöscht.",
                ui_font(UI_PRIMARY), 540, 4);
        button_h(28, 420, 260, 54, "Abbrechen", 0);
        button_h(312, 420, 260, 54, "Löschen", 0);
        return 1;
    }
    return 0;
}
static int u15_input(const Input *in) {
    if (editor_ui.keyboard || dropdown)
        return -1;
    if (sleeping)
        return -1;
    if (ui_route() != 30 && ink_note >= 0) {
        ink_save();
        note_ink_mode = 0;
    }
    int x = in->x, y = in->y;
    if (in->kind == INPUT_SLEEP) {
        ink_save();
        ink_drag = 0;
    }
    if (ui_route() == 30 && !editor_ui.keyboard) {
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
    if (ui_route() == 36 && (discover_busy() || wiki_reader_pending) && in->kind == INPUT_TAP &&
        y >= 120)
        return 1;
    if (in->kind != INPUT_TAP)
        return -1;
    if (ui_route() == 102 && y >= 420 && y < 474) {
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
    if (ui_route() == 84 && y >= 145 && y < 641) {
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
