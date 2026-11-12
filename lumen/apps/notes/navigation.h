/* App-owned module; assembled by apps/host/compose.h. */
static const char *v1_folder_name(int id) {
    for (int i = 0; i < note_tree.nf; i++)
        if (note_tree.folder[i].id == id)
            return note_tree.folder[i].name;
    return tr("Notizen");
}
