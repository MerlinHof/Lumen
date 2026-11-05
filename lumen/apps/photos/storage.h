/* App-owned module; assembled by apps/host/compose.h. */
#include "catalog.h"
static unsigned photo_scan_generation;
static void v1_photos_scan(void) {
    photo_catalog_load();
    photo_scan_generation++;
    photo_cache_page = -1;
    photo_loaded[0] = 0;
    photo_count = 0;
    char path[2048];
    local_path(path, sizeof(path), "photos");
    mkdir(path, 0700);
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
    char p[2048], m[3];
    local_path(p, sizeof(p), name);
    FILE *f = fopen(p, "rb");
    if (!f)
        return 0;
    int w, h, max,
        ok = fscanf(f, "%2s %d %d %d", m, &w, &h, &max) == 4 && !strcmp(m, "P5") && w == 600 &&
             h == 800 && max == 255;
    if (ok) {
        fgetc(f);
        ok = fread(out, 1, 480000, f) == 480000;
    }
    fclose(f);
    return ok;
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
