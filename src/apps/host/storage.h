/* App-owned module; assembled by apps/host/compose.h. */
static int64_t extra_millis(void) {
    struct timespec t;
#ifndef LUMEN_HOST
    if (clock_gettime(CLOCK_BOOTTIME, &t))
        clock_gettime(CLOCK_MONOTONIC, &t);
#else
    clock_gettime(CLOCK_MONOTONIC, &t);
#endif
    return (int64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
static void local_path(char *p, int cap, const char *suffix) {
    snprintf(p, cap, "%s/%s", data_root, suffix);
}
static void extra_write_text(const char *name, const char *s) {
    char p[2048];
    local_path(p, sizeof(p), name);
    if (!atomic_write(p, s, strlen(s)))
        ui_message("Speichern fehlgeschlagen.");
}
static void extra_load_text(const char *name, char *s, int cap) {
    char *p = service_read(name, cap - 1);
    if (p) {
        snprintf(s, cap, "%s", p);
        free(p);
    }
}
