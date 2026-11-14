static int web_setup16(int fd, const char *query, uint64_t length, const char *body, size_t bodyn) {
    char key[50];
    if (!arg(query, "key", key, sizeof(key)))
        return 0;
    const char *allowed[] = {"llm-key.txt", "llm-model.txt", NULL};
    int good = 0;
    for (int i = 0; allowed[i]; i++)
        if (!strcmp(key, allowed[i]))
            good = 1;
    if (!good || length > 2048 || !length) {
        response(fd, 400, "text/plain", "Invalid setup");
        return 1;
    }
    char b[2049];
    size_t n = bodyn > length ? length : bodyn;
    memcpy(b, body, n);
    while (n < length) {
        ssize_t z = recv(fd, b + n, length - n, 0);
        if (z <= 0)
            return 1;
        n += z;
    }
    b[n] = 0;
    if (strpbrk(b, "\r\n\"\\")) {
        response(fd, 400, "text/plain", "Check input");
        return 1;
    }
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", data_root, key);
    int ok = atomic_write(path, b, n);
    memset(b, 0, sizeof(b));
    response(fd, ok ? 200 : 500, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
    return 1;
}
