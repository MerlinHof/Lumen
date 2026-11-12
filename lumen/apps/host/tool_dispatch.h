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
        app_process_send("{\"event\":\"refresh\"}");
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


    if (ui_route() == 51) {
        return 1;
    }

    if(ui_route()==43){
        if(y>=455 && y<503){note_delete16(note_selected);notes_scan();extra_return(29);}
        else if(y>=530 && y<578)extra_return(29);
        return 1;
    }

    return 1;
}
