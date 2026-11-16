enum { PHOTO_FOLDERS = 16, PHOTO_PER_PAGE = 16 };
typedef struct {
    uint64_t id;
    unsigned folders;
} PhotoMembership;
typedef struct {
    char folders[PHOTO_FOLDERS][64];
    PhotoMembership items[512];
    int seconds, fill, landscape;
} PhotoCatalog;
static PhotoCatalog photo_catalog={.seconds=10};
static int photo_catalog_ready;

static void photo_catalog_load(void) {
    if (photo_catalog_ready) return;
    photo_catalog_ready = 1;
    if(!app_state_load("albums.bin",&photo_catalog,sizeof(photo_catalog))) {
        struct { uint32_t magic,size; PhotoCatalog data; } legacy;
        size_t bytes;
        if(lumen_legacy_read("photo-albums-v1.bin",&legacy,sizeof(legacy),&bytes) &&
           bytes==sizeof(legacy) && legacy.magic==0x4c554d16 && legacy.size==sizeof(legacy.data))
            photo_catalog=legacy.data;
    }
    if (photo_catalog.seconds < 5 || photo_catalog.seconds > 120) photo_catalog.seconds = 10;
    photo_catalog.fill = !!photo_catalog.fill;
    photo_catalog.landscape = !!photo_catalog.landscape;
    for (int i = 0; i < PHOTO_FOLDERS; i++) photo_catalog.folders[i][63] = 0;
}
static int photo_catalog_save(void) {
    if (app_state_save("albums.bin", &photo_catalog, sizeof(photo_catalog))) return 1;
    lumen_request("notify","","Speichern fehlgeschlagen.",0);
    return 0;
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
