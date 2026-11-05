static void photo_catalog_load(void) {
    if (photo_catalog_ready) return;
    photo_catalog_ready = 1;
    app_state_load("photo-albums-v1.bin", &photo_catalog, sizeof(photo_catalog));
    if (photo_catalog.seconds < 5 || photo_catalog.seconds > 120) photo_catalog.seconds = 10;
    photo_catalog.fill = !!photo_catalog.fill;
    photo_catalog.landscape = !!photo_catalog.landscape;
    for (int i = 0; i < PHOTO_FOLDERS; i++) photo_catalog.folders[i][63] = 0;
}
static int photo_catalog_save(void) {
    if (app_state_save("photo-albums-v1.bin", &photo_catalog, sizeof(photo_catalog))) return 1;
    ui_message("Speichern fehlgeschlagen.");
    return 0;
}
static uint64_t photo_id(const char *name) {
    uint64_t id = 14695981039346656037ULL;
    for (; *name; name++) id = (id ^ (unsigned char)*name) * 1099511628211ULL;
    return id ? id : 1;
}
static PhotoMembership *photo_membership(uint64_t id, int create) {
    PhotoMembership *empty = NULL;
    for (int i = 0; i < 512; i++) {
        PhotoMembership *item = &photo_catalog.items[i];
        if (item->id == id) return item;
        if (!item->id && !empty) empty = item;
    }
    return create ? empty : NULL;
}
