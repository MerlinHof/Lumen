/* App-owned module; assembled by apps/host/compose.h. */
#include "catalog.h"
static unsigned photo_scan_generation;
static void photo_seed_default(void) {
    char marker[2048],source[2048],destination[2048];
    local_path(marker,sizeof(marker),"photo-default-seeded");
    if (!access(marker,F_OK)) return;
    local_path(destination,sizeof(destination),"photos/LUMEN - Alpine Dawn.pgm");
    if (!access(destination,F_OK)) { atomic_write(marker,"1",1);return; }
    snprintf(source,sizeof(source),"%s/../photos/alpine-dawn.pgm",data_root);
    FILE *file=fopen(source,"rb");
#ifdef LUMEN_HOST
    if (!file) file=fopen("lumen/assets/photos/alpine-dawn.pgm","rb");
#endif
    if (!file) return;
    unsigned char *data=malloc(480064);
    size_t n=data ? fread(data,1,480064,file) : 0;
    int complete=!ferror(file) && feof(file);
    fclose(file);
    if (data && complete && n>480000 && !memcmp(data,"P5\n600 800\n255\n",15) &&
        atomic_write(destination,data,n))
        atomic_write(marker,"1",1);
    free(data);
}
static void v1_photos_scan(void) {
    photo_catalog_load();
    photo_scan_generation++;
    photo_cache_page = -1;
    photo_loaded[0] = 0;
    photo_count = 0;
    char path[2048];
    local_path(path, sizeof(path), "photos");
    mkdir(path, 0700);
    photo_seed_default();
    DIR *d = opendir(path);
    if (!d)
        return;
    struct dirent *e;
    while ((e = readdir(d)) && photo_count < 512) {
        size_t n = strlen(e->d_name);
        if (n <= 4 || strcmp(e->d_name + n - 4, ".pgm") || e->d_name[0] == '.') continue;
        if (photo_folder) {
            PhotoMembership *item = photo_membership(photo_id(e->d_name), 0);
            if (!item || !(item->folders & (1u << (photo_folder - 1)))) continue;
        }
        snprintf(photo_names[photo_count++], 256, "%s", e->d_name);
    }
    closedir(d);
    for (int i = 0; i < photo_count; i++)
        for (int j = i + 1; j < photo_count; j++)
            if (strcmp(photo_names[i], photo_names[j]) < 0) {
                char t[256];
                memcpy(t, photo_names[i], 256);
                memcpy(photo_names[i], photo_names[j], 256);
                memcpy(photo_names[j], t, 256);
            }
}
static int v1_pgm(const char *name, unsigned char *out) {
    char path[2048];
    local_path(path,sizeof(path),name);
    return photo_image_read(path,out);
}
static unsigned char *photo_pixels;
static int v1_photo_load(int i) {
    if (!photo_pixels)
        photo_pixels = malloc(480000);
    char n[300];
    if (i < 0 || i >= photo_count || !photo_pixels)
        return 0;
    snprintf(n, sizeof(n), "photos/%s", photo_names[i]);
    if (!strcmp(photo_loaded, photo_names[i]))
        return 1;
    int ok = v1_pgm(n, photo_pixels);
    if (ok)
        snprintf(photo_loaded, sizeof(photo_loaded), "%s", photo_names[i]);
    return ok;
}
static int v1_sleep_photo(void) {
    char path[2048], name[300] = "";
    local_path(path, sizeof(path), "photos");
    DIR *dir = opendir(path);
    if (!dir) return 0;
    uint64_t hour = (uint64_t)time(NULL) / 3600, best = UINT64_MAX;
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        size_t size = strlen(entry->d_name);
        if (size <= 4 || strcmp(entry->d_name + size - 4, ".pgm") || entry->d_name[0] == '.')
            continue;
        uint64_t score = photo_id(entry->d_name) ^ (hour * 11400714819323198485ULL);
        if (score <= best) {
            best = score;
            snprintf(name, sizeof(name), "photos/%s", entry->d_name);
        }
    }
    closedir(dir);
    if (!*name) return 0;
    photo_hour_drawn = hour;
    if (!v1_pgm(name, ui_pixels)) return 0;
    image_contrast_pixels(ui_pixels, sizeof(ui_pixels));
    return 1;
}

static void v1_photos_draw(void) {
    polish_photos_grid();
}
static void v1_photo_draw(void) {
    polish_photo();
}

#include "albums.h"
