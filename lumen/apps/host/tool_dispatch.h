/* App-owned module; assembled by apps/host/compose.h. */
static void extra_draw(void) {
    switch (ui_route()) {
    case 51:
        heading("Bildquelle");
        wrapped_raw(30, 150, discover_lookup_image_name(), ui_font(UI_PRIMARY), 540, 5);
        wrapped(30, 330,
                "Freies Artikelbild von Wikimedia. Die genaue Lizenz und der Urheber stehen auf "
                "der Dateiseite:",
                ui_font(UI_BODY), 540, 4);
        char source[1024];
        snprintf(source, sizeof(source), "https://commons.wikimedia.org/wiki/File:%s",
                 discover_lookup_image_name());
        wrapped_raw(30, 470, source, ui_font(UI_SECONDARY), 540, 10);
        break;
    case 29:
        app_notes();
        break;
    case 30:
        app_note();
        break;
    case 39:
        app_time_settings();
        break;
    case 40:
        system_screen();
        break;
    case 41:
        app_reset();
        break;
    case 47: {
        heading("Datum & Uhrzeit");
        const char *names[] = {"Tag", "Monat", "Jahr", "Stunde", "Minute"};
        int values[] = {time_day, time_month, time_year, time_hour, time_minute};
        char b[40];
        for (int i = 0; i < 5; i++) {
            int y = 170 + i * 86;
            drawtext(30, y + 10, names[i], ui_font(UI_BODY), 170);
            snprintf(b, sizeof(b), i == 2 ? "%04d" : "%02d", values[i]);
            centered(200, y, 170, 54, b, ui_font(UI_PRIMARY));
            button_h(390, y, 72, 54, "−", 0);
            button_h(480, y, 92, 54, "+", 0);
        }
        button_h(28, 650, 544, 56, "Fertig", 0);
        break;
    }
    case 43:
    case 44:
    case 45:
        heading("Löschen?");
        wrapped(30, 180,
                ui_route() == 43   ? "Diese Notiz endgültig löschen?"
                : ui_route() == 45 ? "Diese Tagesnotiz löschen?"
                               : "Dieses Zeichenblatt leeren?",
                ui_font(UI_TITLE), 540, 4);
        button(28, 455, 544, "Löschen", 0);
        button(28, 530, 544, "Abbrechen", 0);
        break;
    }
}
static void extra_return(int target) {
    navigation_rewinds++;
    for (int i = history_count - 1; i >= 0; i--)
        if (history[i] == target) {
            history_count = i;
            break;
        }
    route_to(target);
}
static void extra_commit(int target, const char *text) {
    if(package_ui_commit(target,text))return;
    if (u15_commit(target, text))
        return;
    if (new_apps_commit(target, text))
        return;

    if (target == 30) {
        if (note_selected < 0) {
            int id = (int)time(NULL);
            while (1) {
                int exists = 0;
                for (int i = 0; i < note_count; i++)
                    if (note_ids[i] == id)
                        exists = 1;
                if (!exists)
                    break;
                id++;
            }
            note_selected = id;
            v1_note_created(id);
        }
        snprintf(note_text, sizeof(note_text), "%s", text);
        char path[80];
        snprintf(path, sizeof(path), "notes/%d.txt", note_selected);
        extra_write_text(path, note_text);
        notes_scan();
        if (ui_route() == 29 && history_count < 64 &&
            (!history_count || history[history_count - 1] != 29))
            history[history_count++] = 29;
        extra_return(30);
        extra_page = 0;
    }  else if (target == 35) {
        int y, m, d, h, minute;
        char tail;
        if (sscanf(text, "%d-%d-%d %d:%d%c", &y, &m, &d, &h, &minute, &tail) != 5 ||
            !system_set_time(y, m, d, h, minute))
            ui_message("Ungültiges Datum / Uhrzeit.");
        else {
            status_ui.status_time = 0;
            ui_message("Datum und Uhrzeit gesetzt.");
        }
    } else if (target == 37) {
        int h, m;
        char c;
        if (sscanf(text, "%d:%d%c", &h, &m, &c) == 2 && h >= 0 && h < 24 && m >= 0 && m < 60) {
            alarm_hour = h;
            alarm_minute = m;
        } else
            ui_message("Uhrzeit als HH:MM eingeben.");
        extra_save();
    }
}

static int extra_input(const Input *in) {
    int x = in->x, y = in->y;

    if (in->kind != INPUT_TAP)
        return 2;

    if (ui_route() == 29)
        return v1_notes_input(in);
    if (ui_route() == 30) {
        if (y >= 54 && y < 98 && x >= 524) {
            reader_markdown17("Notizen",note_text,30);
            return 1;
        }
        if (y >= 54 && y < 740) {
            open_keyboard(30, "Notiz", note_text);
            editor_ui.note_cursor = strlen(editor_ui.edit);
            editor_ui.note_view_start = extra_page * 23;
            note_editor_touch(x, y);
        } else if (y >= 748) {
            if (x < 300 && extra_page)
                extra_page--;
            else if (x >= 300) {
                const char *p = note_text;
                for (int i = 0; i < (extra_page + 1) * 23 && *p; i++)
                    p = note_next_line(p, 536);
                if (*p)
                    extra_page++;
            }
        }
        return 1;
    }

    if (ui_route() == 51) {
        return 1;
    }
    if (ui_route() == 39) {
        if (y >= 286 && y < 348) {
            options[OPT_AUTO_TIME] ^= 1;
            library_settings_save();
            if (options[OPT_AUTO_TIME] && !service_busy())
                service_start(SERVICE_TIME, "", "", "");

        } else if (!options[OPT_AUTO_TIME] && y >= 387 && y < 441) {
            extra_open(47);
        } else if (!options[OPT_AUTO_TIME] && y >= 480 && y < 532)
            open_dropdown(100 + OPT_ZONE, zone_names, 5, options[OPT_ZONE], 220, 540, 350);
        return 1;
    }
    if (ui_route() == 47) {
        if (y >= 170 && y < 568) {
            int row = (y - 170) / 86, delta = x >= 480 ? 1 : x >= 390 ? -1 : 0;
            if (row == 0) {
                time_day += delta;
                int max = calendar_days(time_year, time_month);
                if (time_day < 1)
                    time_day = max;
                if (time_day > max)
                    time_day = 1;
            }
            if (row == 1) {
                time_month += delta;
                if (time_month < 1)
                    time_month = 12;
                if (time_month > 12)
                    time_month = 1;
            }
            if (row == 2) {
                time_year += delta;
                if (time_year < 2000)
                    time_year = 2000;
                if (time_year > 2100)
                    time_year = 2100;
            }
            if (row == 3)
                time_hour = (time_hour + delta + 24) % 24;
            if (row == 4)
                time_minute = (time_minute + delta + 60) % 60;
            int max = calendar_days(time_year, time_month);
            if (time_day > max)
                time_day = max;
        } else if (y >= 650 && y < 706) {
            if (system_set_time(time_year, time_month, time_day, time_hour, time_minute)) {
                status_ui.status_time = 0;
                extra_return(39);
            } else
                ui_message("Ungültiges Datum / Uhrzeit.");
        }
        return 1;
    }
    if (ui_route() == 41) {
        if (y >= 455 && y < 503) {
            library_defaults();
            extra_zone();
            apply_reading_settings();
            char p[2048];
            local_path(p, sizeof(p), "../DISABLE_AUTOSTART");
            unlink(p);
            route_to(13);
            history_count = 0;
            ui_message("Standardeinstellungen wiederhergestellt.");
        } else if (y >= 530 && y < 578)
            extra_return(40);
        return 1;
    }

    if(ui_route()==43){
        if(y>=455 && y<503){note_delete16(note_selected);notes_scan();extra_return(29);}
        else if(y>=530 && y<578)extra_return(29);
        return 1;
    }

    return 1;
}
