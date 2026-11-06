/* App-owned module; assembled by apps/host/compose.h. */
static int u15_commit(int target, const char *text) {
    if (u16_commit(target, text))
        return 1;
    if (target == 160) {
        if ((!strncmp(text, "https://", 8) || !strncmp(text, "http://", 7)) &&
            strlen(text) < 2048 && !strpbrk(text, "\r\n\"\\<> ") && source15_count < 10) {
            int i = source15_count++;
            snprintf(source15[i].name, 80, tr("Katalog %d"), i - 4);
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
static void u15_reader_progress(void) {
    if (!options[OPT_PROGRESS] || reader_ui.reader_progress_mode == 5)
        return;
    int total = engine_count();
    char b[80];
    if (reader_ui.reader_progress_mode == 1) {
        pager(746, &page, total);
        pagination_bind(&page, total, 746, 1, turn);
        return;
    }
    if (reader_ui.reader_progress_mode == 2)
        snprintf(b, 80, "%d%%", total > 1 ? page * 100 / (total - 1) : 100);
    else if (reader_ui.reader_progress_mode == 4)
        snprintf(b, 80, "~ %d min", (total - page - 1) * 2);
    else
        snprintf(b, 80, "%d / %d", page + 1, total);
    ink_color = reader_ui.reader_progress_mode == 3 ? 130 : 0;
    if (reader_ui.reader_progress_mode != 6)
        centered(reader_ui.reader_progress_mode == 3 ? 392 : 180, 746, reader_ui.reader_progress_mode == 3 ? 180 : 240,
                 35, b, ui_font(UI_SECONDARY));
    ink_color = 0;
    if (reader_ui.reader_progress_mode == 0 || reader_ui.reader_progress_mode == 6)
        progress(28, 787, 544, page + 1, total);
}
static int u15_draw(void) {
    if (ui_route() == 100) {
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
    if (ui_route() == 101) {
        heading_actions("Store · Quellen", 1);
        icon_button(528, 74, IC_PLUS);
        for (int r = 0; r < 6 && source15_page * 6 + r < source15_count; r++) {
            int i = source15_page * 6 + r, y = 145 + r * 82;
            drawtext_raw(30, y + 8, source15[i].name, ui_font(UI_BODY), 420);
            drawtext_raw(30, y + 39, source15[i].url, ui_font(UI_SECONDARY), 420);
            toggle(492, y + 10, source15[i].enabled);
            divider(28, y + 75, 544);
        }
        pager(745, &source15_page, (source15_count + 5) / 6);
        return 1;
    }
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
    if (ui_route() == 27 && search_action_hit(x, y)) {
        extra_open(100);
        return 1;
    }
    if (ui_route() == 100) {
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
    if (ui_route() == 101 && !editor_ui.keyboard && !dropdown && y < 48 && x < 62) {
        extra_return(10);
#ifndef LUMEN_HOST
        if (sources15_dirty)
            store_search();
#endif
        sources15_dirty = 0;
        return 1;
    }
    if (ui_route() == 101) {
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
    if (ui_route() == 10 && y >= 74 && y < 118 && x >= 460 && x < 516) {
        extra_open(101);
        return 1;
    }
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
