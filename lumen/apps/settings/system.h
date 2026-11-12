static uint64_t directory_bytes(const char *p, int depth) {
    if (depth > 12)
        return 0;
    struct stat st;
    if (lstat(p, &st))
        return 0;
    if (S_ISLNK(st.st_mode))
        return 0;
    if (S_ISREG(st.st_mode))
        return (uint64_t)st.st_size;
    if (!S_ISDIR(st.st_mode))
        return 0;
    DIR *d = opendir(p);
    if (!d)
        return 0;
    uint64_t n = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.')
            continue;
        char child[2048];
        if (snprintf(child, sizeof(child), "%s/%s", p, e->d_name) >= (int)sizeof(child))
            continue;
        n += directory_bytes(child, depth + 1);
    }
    closedir(d);
    return n;
}
static uint64_t storage_books, storage_system, storage_free, storage_total, storage_firmware;
static void storage_scan(void) {
#ifndef LUMEN_HOST
    struct statvfs firmware;
    if (!statvfs("/", &firmware))
        storage_firmware = (uint64_t)(firmware.f_blocks - firmware.f_bfree) * firmware.f_frsize;
#endif
    struct statvfs v;
    if (!statvfs(data_root, &v)) {
        storage_total = (uint64_t)v.f_blocks * v.f_frsize;
        storage_free = (uint64_t)v.f_bavail * v.f_frsize;
    }
    storage_books = directory_bytes(library_root, 0);
    char root[2048];
    snprintf(root, sizeof(root), "%s/..", data_root);
    storage_system = directory_bytes(root, 0);
}
static void storage_size(char *out, int cap, uint64_t n) {
    snprintf(out, cap, "%.1f %s", n >= 1073741824 ? n / 1073741824.0 : n / 1048576.0,
             n >= 1073741824 ? "GB" : "MB");
}
