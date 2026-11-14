/* Focused paper apps. Networking runs in a bounded child, never in the input loop. */
#include <fcntl.h>
typedef struct {
    char id[40], title[160];
    time_t opened;
} RecipeRecent16;
static RecipeRecent16 recipe_recent16[128];
static int recipe_recent_count16;
static void recipe_archive16(void);
static int job_kind16;

static void reader16(const char *title, const char *body, int origin) {
    document_open(title, "LUMEN", title, body, NULL, origin, 0);
}
static int file_open16(const char *path, int origin) {
    const char *ext = strrchr(path, '.');
    int supported = 0;
    const char *types[] = {".epub", ".pdf",  ".txt", ".html", ".htm",
                           ".cbz",  ".mobi", ".azw", ".azw3"};
    for (unsigned i = 0; ext && i < sizeof(types) / sizeof(*types); i++)
        if (!strcasecmp(ext, types[i]))
            supported = 1;
    if (!supported) {
        ui_message("Dieses Format kann der Reader nicht öffnen.");
        return 0;
    }
    ui_save();
    engine_close();
    current = selected = -1;
    Book *b = &library[BOOK_LIMIT];
    memset(b, 0, sizeof(*b));
    b->id = press_id(path);
    snprintf(b->path, sizeof(b->path), "%s", path);
    struct stat document_stat;
    if (!stat(path,&document_stat)) b->size=document_stat.st_size;
    const char *n = strrchr(path, '/');
    snprintf(b->title, sizeof(b->title), "%s", n ? n + 1 : path);
    library_restore_state(b);
    snprintf(b->format, sizeof(b->format), "%s", ext + 1);
    reader_ui.reader_document_active = 1;
    if (history_count < 64)
        history[history_count++] = origin;
    return ui_open(BOOK_LIMIT);
}
static void url_encode(const char *s, char *out, int cap) {
    int n = 0;
    for (; *s && n < cap - 4; s++) {
        unsigned char c = *s;
        if (isalnum(c) || strchr("-._~", c))
            out[n++] = c;
        else {
            snprintf(out + n, 4, "%%%02X", c);
            n += 3;
        }
    }
    out[n] = 0;
}
static int recipe_download(const char *url, const char *destination) {
    if (strncmp(url, "https://", 8) || strpbrk(url, "\r\n\"\\"))
        return 0;
    char cfg[4096], ca[4096];
    snprintf(cfg, sizeof(cfg), "%s/http17-%016llx.cfg", data_root,
             (unsigned long long)press_id(destination));
    FILE *f = fopen(cfg, "w");
    if (!f)
        return 0;
    chmod(cfg, 0600);
    fprintf(f, "url = \"%s\"\noutput = \"%s\"\n", url, destination);
    fclose(f);
    pid_t p = fork();
    if (!p) {
#ifndef LUMEN_HOST
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        if (getppid() == 1)
            _exit(1);
#endif
        snprintf(ca, sizeof(ca), "%s/../certs/ca-bundle.pem", data_root);
        if (!access(ca, R_OK))
            setenv("CURL_CA_BUNDLE", ca, 1);
        int nul = open("/dev/null", O_RDWR);
        dup2(nul, 1);
        dup2(nul, 2);
        close(nul);
        execlp("curl", "curl", "--user-agent", "LUMEN/1.7 (personal Kindle reader)", "--config",
               cfg, "--fail", "--silent", "--location", "--proto", "=https", "--proto-redir",
               "=https", "--max-redirs", "3", "--connect-timeout", "8", "--max-time",
               "40", "--max-filesize", "4194304", NULL);
        _exit(127);
    }
    int st = 0;
    if (p > 0)
        while (waitpid(p, &st, 0) < 0 && errno == EINTR) {
        }
    unlink(cfg);
    return p > 0 && WIFEXITED(st) && !WEXITSTATUS(st);
}
static void json_escape16(FILE *f, const char *p) {
    fputc('"', f);
    for (; *p; p++) {
        unsigned char c = *p;
        if (c == '"' || c == '\\') {
            fputc('\\', f);
            fputc(c, f);
        } else if (c < 32)
            fprintf(f, "\\u%04x", c);
        else
            fputc(c, f);
    }
    fputc('"', f);
}

static int job_start16(int kind, const char *arg) {
    if (job_pid16 > 0) {
        ui_message("Eine Anfrage läuft bereits.");
        return 0;
    }
    job_kind16 = kind;
    job_pid16 = fork();
    if (job_pid16 < 0) {
        return 0;
    }
    if (!job_pid16) {
#ifndef LUMEN_HOST
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        if (getppid() == 1)
            _exit(1);
#endif
        char out[4096];
        local_path(out, sizeof(out), "online16.json");
        int ok = 0;
        if (kind == 1) {
            recipes_search17(arg);
            ok = 1;
        }
        if (kind == 4)
            ok = recipes_fetch17(arg);
        _exit(ok ? 0 : 1);
    }
    return 1;
}

static int u16_jobs_tick(void) {

    if (job_pid16 <= 0)
        return 0;
    int st = 0;
    pid_t p = waitpid(job_pid16, &st, WNOHANG);
    if (p <= 0)
        return 0;
    job_pid16 = -1;
    if (!WIFEXITED(st) || WEXITSTATUS(st)) {
        ui_message("Anfrage fehlgeschlagen. Verbindung und Einrichtung prüfen.");
        if ((job_kind16==1 || job_kind16==4) && app_for_route(ui_route())==APP_RECIPES)
            app_process_send("{\"event\":\"refresh\"}");
        return 1;
    }
    if (job_kind16 == 1)recipes_parse17();
    if (job_kind16==1 && app_for_route(ui_route())==APP_RECIPES)
        app_process_send("{\"event\":\"refresh\"}");
    if (job_kind16 == 4)
        recipes_open_ready17();

    return 1;
}


static int ink_pending16(void) {
    return ink_dirty;
}
static int job_pending16(void) {
    return job_pid16 > 0;
}

static void recipe_archive16(void) {
    recipe_recent_count16 = 0;
    char dir[4096];
    local_path(dir, sizeof(dir), "recipe-archive16");
    mkdir(dir, 0700);
    DIR *d = opendir(dir);
    struct dirent *e;
    while (d && (e = readdir(d)) && recipe_recent_count16 < 128) {
        char *dot = strrchr(e->d_name, '.');
        if (!dot || strcmp(dot, ".txt") || dot - e->d_name >= 39)
            continue;
        char path[8192];
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        FILE *f = fopen(path, "r");
        if (!f)
            continue;
        RecipeRecent16 *r = &recipe_recent16[recipe_recent_count16++];
        fgets(r->title, sizeof(r->title), f);
        fclose(f);
        r->title[strcspn(r->title, "\r\n")] = 0;
        snprintf(r->id, sizeof(r->id), "%.*s", (int)(dot - e->d_name), e->d_name);
        struct stat st;
        if (!stat(path, &st))
            r->opened = st.st_mtime;
    }
    if (d)
        closedir(d);
    for (int i = 0; i < recipe_recent_count16; i++)
        for (int j = i + 1; j < recipe_recent_count16; j++)
            if (recipe_recent16[j].opened > recipe_recent16[i].opened) {
                RecipeRecent16 r = recipe_recent16[i];
                recipe_recent16[i] = recipe_recent16[j];
                recipe_recent16[j] = r;
            }
}

static void companion_close16(void) {
    if (job_pid16 > 0) {
        kill(job_pid16, SIGTERM);
        while (waitpid(job_pid16, NULL, 0) < 0 && errno == EINTR) {
        }
        job_pid16 = -1;
    }
    const char *files[] = {"private16.curl", "private16.body", NULL};
    for (int i = 0; files[i]; i++) {
        char p[4096];
        local_path(p, sizeof(p), files[i]);
        unlink(p);
    }
}
