/* App-owned module; assembled by apps/host/compose.h. */
static void v1_open(int target) {
    polish_open(target);
    v1_init();
    v1_page = 0;
    if (target == 71)
        v1_photos_scan();
    if (target == 85)
        markdown_page = 0;
    if (target == 87 && !timer_due && !timer_paused)
        timer_total_seconds = timer_minutes * 60;
    if (target == 90 || target == 91) { /* registered shell components open their configuration */
        setting_category = 1;
        settings_ui.setting_page = target == 90 ? 0 : 1;
    }
    if (target == 93)
        open_keyboard(130, "Tastatur", "");
}
static void v1_draw(void) {
    switch (ui_route()) {
    case 71:
        v1_photos_draw();
        break;
    case 72:
        v1_photo_draw();
        break;
    case 83:
        v1_launcher_settings();
        break;
    case 84:
        v1_folder_picker_draw();
        break;
    case 85:
        reader_markdown17("Notizen",note_text,30);
        reader();
        break;
    case 87:
        v1_timer_ring(1);
        break;
    case 90:
        more_settings();
        break;
    case 92:
        heading("Sperrbild");
        for (int i = 0; i < 5; i++)
            button_h(28, 154 + i * 86, 544, 65, lockmodes[i], options[OPT_LOCK] == lock_mode_ids[i]);
        break;
    case 93:
        heading("Tastatur");
        button_h(28, 180, 544, 60, "Text eingeben", 0);
        break;
    }
    if (v1_delete) {
        rect(21, 267, 564, 236, 190);
        rect(17, 263, 564, 236, 255);
        key_outline(17, 263, 564, 236);
        centered(28, 285, 544, 55, "Wirklich löschen?", ui_font(UI_TITLE));
        button_h(36, 400, 250, 58, "Abbrechen", 0);
        button_h(310, 400, 250, 58, "Löschen", 0);
    }
}
static int v1_commit(int target, const char *text) {
    if (target == 120) {
        if (*text && note_tree.nf < 512) {
            int id = 1;
            for (int i = 0; i < note_tree.nf; i++)
                if (note_tree.folder[i].id >= id)
                    id = note_tree.folder[i].id + 1;
            NoteFolder *f = &note_tree.folder[note_tree.nf++];
            f->id = id;
            f->parent = note_tree.current;
            snprintf(f->name, 96, "%s", text);
            v1_dirty = 1;
        }
        return 1;
    }
    if (target == 130)
        return 1;
    return 0;
}
static int v1_tick(void) {
    if (!sleeping && ui_route() == 72 && photo_slideshow && photo_count &&
        extra_millis() >= photo_next_frame) {
        photo_selected = (photo_selected + 1) % photo_count;
        photo_next_frame = extra_millis() + photo_catalog.seconds * 1000;
        return 1;
    }
    if (polish_tick())
        return 1;
    if (alarm_ring) {
        if (!timer_flash_started) {
            timer_flash_started = extra_millis();
            timer_last_phase = -1;
        }
        int64_t phase = (extra_millis() - timer_flash_started) / 500;
        if (phase < 120 && phase != timer_last_phase) {
            timer_last_phase = phase;
            return 1;
        }
    } else
        timer_flash_started = 0;
    static int lastsecond;
    int sec = timer_due ? (timer_due - extra_millis()) / 1000 : 0;
    if ((ui_route() == 87 || ui_route() == 28) && sec != lastsecond) {
        lastsecond = sec;
        return 1;
    }
    return 0;
}
static int v1_poll(void) {
    if (ui_route() == 72 && photo_slideshow) {
        int64_t remaining = photo_next_frame - extra_millis();
        return remaining > 0 ? (int)remaining : 1;
    }
    if (alarm_ring && timer_flash_started && extra_millis() - timer_flash_started < 60000)
        return 500;
    if (ui_route() == 87)
        return 1000;
    return -1;
}
static void v1_effect(void) {
    if (alarm_ring && timer_flash_started && extra_millis() - timer_flash_started < 60000 &&
        (timer_last_phase & 1))
        for (size_t i = 0; i < sizeof(ui_pixels); i++)
            ui_pixels[i] = 255 - ui_pixels[i];
}
static int v1_input(const Input *in) {
    if (ui_route() == 72 && !v1_delete)
        return polish_photo_input(in);
    if (in->kind != INPUT_TAP)
        return 2;
    int x = in->x, y = in->y;
    if (v1_delete) {
        if (y >= 400 && y < 458) {
            if (x >= 300) {
                if (v1_delete == 1 && photo_selected >= 0 && photo_selected < photo_count) {
                    char p[2048], n[300];
                    snprintf(n, sizeof(n), "photos/%s", photo_names[photo_selected]);
                    local_path(p, sizeof(p), n);
                    if (unlink(p)) {
                        ui_message("Löschen fehlgeschlagen.");
                        v1_delete = 0;
                        return 1;
                    }
                    PhotoMembership *member = photo_membership(photo_id(photo_names[photo_selected]), 0);
                    if (member) {
                        *member = (PhotoMembership){0};
                        photo_catalog_save();
                    }
                    char cache[2048], thumb[300];
                    snprintf(thumb, sizeof(thumb), "photos/.thumbs/%s",
                             photo_names[photo_selected]);
                    local_path(cache, sizeof(cache), thumb);
                    unlink(cache);
                    v1_photos_scan();
                    extra_return(71);
                }
            }
            v1_delete = 0;
        }
        return 1;
    }
    switch (ui_route()) {
    case 71:
        photo_grid_input(x, y);
        return 1;
    case 83: {
        int indices[APP_LIMIT], count = launcher_manage_indices(indices);
        if (y >= 174 && y < 650) {
            int pos = launcher_settings_page * 8 + (y - 174) / 68;
            if (pos < count) {
                int i = indices[pos], id = launcher_order[i];
                if(x>=526 && x<570 && app_package_can_remove(id)) {
                    static const char *actions[]={"Abbrechen","App endgültig entfernen"};
                    manage_delete_id=id;
                    open_dropdown(2004,actions,2,-1,315,y+20>675 ? 675 : y+20,258);
                } else if(x>=427 && x<503)
                    launcher_visible[id]^=1;
                else if (x >= 329 && x < 373 && pos > 0) {
                    int other = indices[pos - 1];
                    launcher_order[i] = launcher_order[other];
                    launcher_order[other] = id;
                } else if (x >= 375 && x < 419 && pos + 1 < count) {
                    int other = indices[pos + 1];
                    launcher_order[i] = launcher_order[other];
                    launcher_order[other] = id;
                }
                v1_dirty = 1;
            }
        } else if (y >= 745 && y < 789) {
            if (x < 300 && launcher_settings_page)
                launcher_settings_page--;
            else if (x >= 300 && (launcher_settings_page + 1) * 8 < count)
                launcher_settings_page++;
        }
        break;
    }
    case 84:
        if (y >= 145 && y < 641) {
            int i = folder_page * 8 + (y - 145) / 62;
            if (i <= note_tree.nf) {
                v1_place(note_selected, i ? note_tree.folder[i - 1].id : 0);
                extra_return(30);
            }
        } else if (y >= 699 && y < 743) {
            if (x < 300 && folder_page)
                folder_page--;
            else if (x >= 300 && (folder_page + 1) * 8 < note_tree.nf + 1)
                folder_page++;
        }
        break;
    case 85:
        if (y >= 57 && y < 101 && x >= 516)
            extra_return(30);
        else if (y >= 746) {
            if (x < 300 && markdown_page)
                markdown_page--;
            else if (x >= 300 && markdown_page + 1 < markdown_pages)
                markdown_page++;
        }
        break;
    case 87:
        if (y >= 665 && y < 725) {
            timer_action(x >= 300);
            extra_save();
        }
        break;
    case 90:
        settings_input(in);
        break;
    case 92:
        if (y >= 154 && y < 584) {
            int i = (y - 154) / 86;
            if (i < 5) {
                options[OPT_LOCK] = lock_mode_ids[i];
                ui_save();
            }
        }
        break;
    case 93:
        if (y >= 180 && y < 240)
            open_keyboard(130, "Tastatur", "");
        break;
    }
    return 1;
}
