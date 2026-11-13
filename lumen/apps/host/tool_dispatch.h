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
    }
}
