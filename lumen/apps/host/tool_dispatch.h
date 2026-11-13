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
            int lock=collection_lock(data_root);
            int id=lock>=0 ? collection_create_note(data_root,note_editor_folder,text) : 0;
            if (lock>=0) close(lock);
            if (!id) {ui_message("Speichern fehlgeschlagen.");return;}
            note_selected=id;
        } else {
            char path[48];
            snprintf(path,sizeof(path),"notes/%d.txt",note_selected);
            if (!lumen_storage_write_at(data_root,path,text,strlen(text))) {
                ui_message("Speichern fehlgeschlagen.");
                return;
            }
        }
        snprintf(note_text,sizeof(note_text),"%s",text);
        if (ui_route() == 29 && history_count < 64 &&
            (!history_count || history[history_count - 1] != 29))
            history[history_count++] = 29;
        extra_return(30);
        extra_page = 0;
        app_process_send("{\"event\":\"refresh\"}");
    }
}
