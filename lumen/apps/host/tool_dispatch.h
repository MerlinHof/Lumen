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
    if (target >= 100 && v1_commit(target, text))
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
