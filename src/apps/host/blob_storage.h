/* App-owned module; assembled by apps/host/compose.h. */
static int save_blob(const char *name, const void *data, size_t bytes) {
    char p[2048];
    local_path(p, sizeof(p), name);
    unsigned char *all = malloc(bytes + 8);
    if (!all)
        return 0;
    uint32_t h[2] = {0x4c554d16, (uint32_t)bytes};
    memcpy(all, h, 8);
    memcpy(all + 8, data, bytes);
    int ok = atomic_write(p, all, bytes + 8);
    free(all);
    return ok;
}
static int load_blob(const char *name, void *data, size_t bytes) {
    char p[2048];
    local_path(p, sizeof(p), name);
    FILE *f = fopen(p, "rb");
    if (!f)
        return 0;
    uint32_t h[2];
    void *tmp = malloc(bytes);
    int ok = tmp && fread(h, 8, 1, f) == 1 && h[0] == 0x4c554d16 && h[1] == bytes &&
             fread(tmp, bytes, 1, f) == 1 && fgetc(f) == EOF;
    if (ok)
        memcpy(data, tmp, bytes);
    free(tmp);
    fclose(f);
    return ok;
}
static uint32_t pro_random(void) {
    pro_rng = pro_rng * 1664525u + 1013904223u;
    return pro_rng;
}
