/* App-owned module; assembled by apps/host/compose.h. */
static int wiki_image_available(void) {
    if (!*discover_lookup_image())
        return 0;
    char *key = service_read("wiki-image-url.txt", 2047);
    int same = key && !strcmp(key, discover_lookup_image());
    free(key);
    char p[2048];
    struct stat st;
    local_path(p, sizeof(p), "wiki-image.pgm");
    return same && !stat(p, &st) && st.st_size >= 130000;
}
