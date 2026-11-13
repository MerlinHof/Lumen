#include "collection_store.h"

static int note_editor_folder;

static int note_load(int id) {
    char path[48], text[sizeof(note_text)];
    size_t size;
    snprintf(path, sizeof(path), "notes/%d.txt", id);
    if (!lumen_storage_read_at(data_root, path, text, sizeof(text)-1, &size))
        return 0;
    text[size]=0;
    memcpy(note_text, text, size+1);
    note_selected=id;
    return 1;
}
