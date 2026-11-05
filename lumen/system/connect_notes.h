/* Authenticated, read-only note export. IDs are strictly numeric. */
static int note_file_id16(const char *name) {
    int id = 0, n = 0;
    if (sscanf(name, "%d.txt%n", &id, &n) != 1 || !n || name[n] || id <= 0)
        return 0;
    char s[40];
    snprintf(s, 40, "%d.txt", id);
    return !strcmp(name, s) ? id : 0;
}
static void send_file16(int fd, const char *path, const char *type, const char *name) {
    int in = open(path, O_RDONLY | O_NOFOLLOW);
    struct stat st;
    if (in < 0 || fstat(in, &st) || !S_ISREG(st.st_mode)) {
        if (in >= 0)
            close(in);
        response(fd, 404, "text/plain", "Not found");
        return;
    }
    char h[512];
    int n = snprintf(h, sizeof(h),
                     "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Disposition: attachment; "
                     "filename=\"%s\"\r\nContent-Length: %llu\r\nCache-Control: "
                     "no-store\r\nConnection: close\r\n\r\n",
                     type, name, (unsigned long long)st.st_size);
    writeall(fd, h, n);
    char buf[32768];
    ssize_t z;
    while ((z = read(in, buf, sizeof(buf))) > 0)
        if (!writeall(fd, buf, z))
            break;
    close(in);
}
static void web_notes16(int fd) {
    char dir[2048];
    snprintf(dir, sizeof(dir), "%s/notes", data_root);
    DIR *d = opendir(dir);
    char *body = NULL;
    size_t size = 0;
    FILE *out = open_memstream(&body, &size);
    if (!out) {
        if (d)
            closedir(d);
        response(fd, 500, "text/plain", "Memory");
        return;
    }
    fputs("{\"notes\":[", out);
    int comma = 0;
    struct dirent *e;
    while (d && (e = readdir(d))) {
        int id = note_file_id16(e->d_name);
        if (!id)
            continue;
        char path[4096], title[240] = "";
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        FILE *f = fopen(path, "r");
        if (f) {
            fgets(title, sizeof(title), f);
            fclose(f);
        }
        title[strcspn(title, "\r\n")] = 0;
        if (comma++)
            fputc(',', out);
        fprintf(out, "{\"id\":%d,\"title\":", id);
        json(out, *title ? title : "Note");
        fprintf(out, ",\"created\":%d}", id);
    }
    if (d)
        closedir(d);
    fputs("]}", out);
    fclose(out);
    response(fd, 200, "application/json; charset=utf-8", body);
    free(body);
}
static void web_note16(int fd, const char *query) {
    char idstr[40], fmt[12] = "txt", name[80], path[4096];
    int id = 0, n = 0;
    if (!arg(query, "id", idstr, sizeof(idstr)) || sscanf(idstr, "%d%n", &id, &n) != 1 || id <= 0 ||
        idstr[n]) {
        response(fd, 400, "text/plain", "Invalid note");
        return;
    }
    snprintf(path, sizeof(path), "%s/notes/%d.txt", data_root, id);
    arg(query, "format", fmt, sizeof(fmt));
    if (strcmp(fmt, "pdf")) {
        snprintf(name, 80, "note-%d.txt", id);
        send_file16(fd, path, "text/plain; charset=utf-8", name);
        return;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        response(fd, 404, "text/plain", "Note not found");
        return;
    }
    char *text = calloc(1, 65537);
    if (!text) {
        fclose(f);
        return;
    }
    fread(text, 1, 65536, f);
    fclose(f);
    char html[4096], pdf[4096], dir[4096];
    snprintf(html, sizeof(html), "%s/.note-export.html", data_root);
    snprintf(pdf, sizeof(pdf), "%s/.note-export.pdf", data_root);
    snprintf(dir, sizeof(dir), "%s/notes", data_root);
    f = fopen(html, "w");
    if (!f) {
        free(text);
        response(fd, 500, "text/plain", "Cannot export");
        return;
    }
    fputs("<!doctype html><meta charset=utf-8><style>body{font-family:serif;margin:54px 40px 58px "
          "60px;line-height:1.45}p{margin:0 0 .5em}h1,h2,h3{margin:.4em "
          "0}pre{white-space:pre-wrap}code{font-family:monospace}</style>",
          f);
    markdown_render17(f, text);
    fclose(f);
    free(text);
    if (engine_note_pdf(html, dir, id, pdf)) {
        snprintf(name, 80, "note-%d.pdf", id);
        send_file16(fd, pdf, "application/pdf", name);
    } else
        response(fd, 500, "text/plain", "PDF export failed");
    unlink(html);
    unlink(pdf);
}
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
