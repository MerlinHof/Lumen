/* App-owned module; assembled by apps/host/compose.h. */
static int folder_ids17[513], folder_count17;
static char folder_names17[513][240];
static const char *folder_labels17[513];
static void folder_menu17(void) {
    folder_count17 = 1;
    folder_ids17[0] = 0;
    snprintf(folder_names17[0], 240, "%s", tr("Notizen"));
    for (int i = 0; i < note_tree.nf; i++) {
        folder_ids17[folder_count17] = note_tree.folder[i].id;
        snprintf(folder_names17[folder_count17], 240, "%s%s%s",
                 note_tree.folder[i].parent ? v1_folder_name(note_tree.folder[i].parent) : "",
                 note_tree.folder[i].parent ? " / " : "", note_tree.folder[i].name);
        folder_count17++;
    }
    for (int i = 0; i < folder_count17; i++)
        folder_labels17[i] = folder_names17[i];
    open_dropdown(936, folder_labels17, folder_count17, -1, 190, 200, 382);
}
