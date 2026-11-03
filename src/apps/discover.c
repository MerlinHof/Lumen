#include "../i18n.h"
#define _GNU_SOURCE
#include "../discover.h"
#include "../library.h"
#include "../services.h"
#include <errno.h>
#include <fcntl.h>
#include <mupdf/fitz.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#ifndef LUMEN_HOST
#include <sys/prctl.h>
#endif
OPDSItem opds_items[OPDS_LIMIT];
int opds_count;
static pid_t job = -1;
static char current_url[2048], error[256], target[2048], temp[2048];
static int acquisition, wiki_mode, wiki_fallback_pending;
static char wiki_search_url[2048];
static char wiki_text[524288], wiki_title[256], wiki_image[2048], wiki_image_name[512];
const char *discover_error(void) { return error; }
const char *discover_url(void) { return current_url; }
int discover_busy(void) { return job > 0; }
static const char *localtag(fz_xml *x) {
    const char *t = fz_xml_tag(x);
    if (!t)
        return "";
    const char *c = strchr(t, ':');
    return c ? c + 1 : t;
}
static void content_depth(fz_xml *x, char *out, size_t size, int depth) {
    if (!x || depth > 24)
        return;
    for (fz_xml *c = fz_xml_down(x); c; c = fz_xml_next(c)) {
        const char *text = fz_xml_text(c);
        if (text) {
            size_t n = strlen(out);
            snprintf(out + n, size - n, "%s", text);
        } else
            content_depth(c, out, size, depth + 1);
    }
}
static void content(fz_xml *x, char *out, size_t size) { content_depth(x, out, size, 0); }
static void resolve(const char *base, const char *href, char *out, size_t size) {
    if (!strncmp(href, "https://", 8) || !strncmp(href, "http://", 7)) {
        snprintf(out, size, "%s", href);
        return;
    }
    const char *scheme = strstr(base, "://");
    if (!scheme)
        return;
    const char *hostend = strchr(scheme + 3, '/');
    if (href[0] == '/') {
        size_t n = hostend ? (size_t)(hostend - base) : strlen(base);
        snprintf(out, size, "%.*s%s", (int)n, base, href);
    } else {
        char clean[2048];
        snprintf(clean, sizeof(clean), "%s", base);
        char *q = strchr(clean, '?');
        if (q)
            *q = 0;
        char *slash = strrchr(clean, '/');
        if (slash && slash > clean + 8)
            slash[1] = 0;
        else
            snprintf(clean + strlen(clean), sizeof(clean) - strlen(clean), "/");
        snprintf(out, size, "%s%s", clean, href);
    }
}
static void archive_documents(fz_xml *node, const char *base) {
    if (!node || opds_count >= OPDS_LIMIT)
        return;
    if (!strcmp(localtag(node), "result")) {
        const char *start = fz_xml_att(node, "start"), *count = fz_xml_att(node, "numFound");
        if (start && count && atol(start) + 50 < atol(count) && opds_count < OPDS_LIMIT) {
            OPDSItem *b = &opds_items[opds_count++];
            memset(b, 0, sizeof(*b));
            snprintf(b->title, sizeof(b->title), "Weitere Katalogseite");
            char url[2048];
            snprintf(url, sizeof(url), "%s", base);
            char *page = strstr(url, "&page=");
            if (page)
                *page = 0;
            snprintf(b->url, sizeof(b->url), "%s&page=%ld", url, atol(start) / 50 + 2);
        }
    }
    if (!strcmp(localtag(node), "doc")) {
        OPDSItem b = {0};
        char id[256] = "";
        for (fz_xml *c = fz_xml_down(node); c; c = fz_xml_next(c)) {
            const char *name = fz_xml_att(c, "name");
            if (!name)
                continue;
            if (!strcmp(name, "identifier"))
                content(c, id, sizeof(id));
            if (!strcmp(name, "title"))
                content(c, b.title, sizeof(b.title));
            if (!strcmp(name, "creator"))
                content(c, b.author, sizeof(b.author));
        }
        if (*id &&
            strspn(id, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-") ==
                strlen(id)) {
            snprintf(b.url, sizeof(b.url), "https://archive.org/download/%s/%s_files.xml", id, id);
            opds_items[opds_count++] = b;
        }
        return;
    }
    for (fz_xml *c = fz_xml_down(node); c; c = fz_xml_next(c))
        archive_documents(c, base);
}
static void archive_files(fz_xml *root, const char *base) {
    for (fz_xml *c = fz_xml_down(root); c && opds_count < OPDS_LIMIT; c = fz_xml_next(c)) {
        if (strcmp(localtag(c), "file"))
            continue;
        const char *name = fz_xml_att(c, "name");
        if (!name || strlen(name) > 500 || strstr(name, ".."))
            continue;
        const char *ext = strrchr(name, '.');
        if (!ext || (strcmp(ext, ".pdf") && strcmp(ext, ".epub")))
            continue;
        int hidden = 0;
        for (fz_xml *a = fz_xml_down(c); a; a = fz_xml_next(a))
            if (!strcmp(localtag(a), "private")) {
                char v[32] = "";
                content(a, v, sizeof(v));
                if (!strcmp(v, "true"))
                    hidden = 1;
            }
        if (hidden)
            continue;
        OPDSItem b = {.acquisition = 1};
        char escaped[1600];
        size_t n = 0;
        for (const unsigned char *p = (const unsigned char *)name; *p && n + 4 < sizeof(escaped);
             p++) {
            if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
                *p == '.' || *p == '_' || *p == '-')
                escaped[n++] = *p;
            else {
                snprintf(escaped + n, 4, "%%%02X", *p);
                n += 3;
            }
        }
        escaped[n] = 0;
        snprintf(b.title, sizeof(b.title), "%s", name);
        snprintf(b.author, sizeof(b.author), "Internet Archive");
        snprintf(b.summary, sizeof(b.summary),
                 "Freely downloadable file from Internet Archive. PDF preserves the original page "
                 "layout.");
        snprintf(b.format, sizeof(b.format), "%s", ext + 1);
        resolve(base, escaped, b.url, sizeof(b.url));
        opds_items[opds_count++] = b;
    }
}
static char html_book_title[256];
static void html_title(fz_xml *x, int depth) {
    if (!x || depth > 40 || *html_book_title)
        return;
    for (fz_xml *c = x; c; c = fz_xml_next(c)) {
        if (!strcmp(localtag(c), "h1")) {
            content(c, html_book_title, sizeof(html_book_title));
            return;
        }
        html_title(fz_xml_down(c), depth + 1);
    }
}
static void html_books(fz_xml *node, const char *base, int depth) {
    if (!node || depth > 60 || opds_count >= OPDS_LIMIT)
        return;
    for (fz_xml *c = node; c && opds_count < OPDS_LIMIT; c = fz_xml_next(c)) {
        const char *tag = localtag(c), *href = fz_xml_att(c, "href");
        const char *dl = fz_xml_att(c, "data-dlid");
        if (dl && strstr(base, "www.epubbooks.com/book/") &&
            strspn(dl, "0123456789") == strlen(dl) && strlen(dl) < 12) {
            char details[768] = "";
            content(fz_xml_up(c), details, sizeof(details));
            OPDSItem b = {.acquisition = 1};
            snprintf(b.title, sizeof(b.title), "%s",
                     *html_book_title ? html_book_title : "epubBooks");
            snprintf(b.author, sizeof(b.author), "epubBooks");
            snprintf(b.format, sizeof(b.format), "%s", strstr(details, "EPUB") ? "epub" : "mobi");
            snprintf(b.url, sizeof(b.url), "https://www.epubbooks.com/downloads/%s", dl);
            opds_items[opds_count++] = b;
        }
        if (!href && (!strcmp(tag, "iframe") || !strcmp(tag, "embed")))
            href = fz_xml_att(c, "src");
        if (href && (!strcmp(tag, "a") || !strcmp(tag, "iframe") || !strcmp(tag, "embed"))) {
            const char *type = fz_xml_att(c, "type");
            int epub = strstr(href, ".epub") != NULL || (type && strstr(type, "epub")),
                pdf = strstr(href, ".pdf") != NULL || (type && strstr(type, "pdf"));
            int book = strstr(href, "/book/") || strstr(href, "/books/") ||
                       strstr(href, "/ebook/") || strstr(href, "/ebooks/");
            if (epub || pdf || book) {
                OPDSItem b = {0};
                resolve(base, href, b.url, sizeof(b.url));
                int duplicate = 0;
                for (int i = 0; i < opds_count; i++)
                    if (!strcmp(opds_items[i].url, b.url))
                        duplicate = 1;
                if (!duplicate && href[0] != '#' && !strstr(href, "/authors/") &&
                    !strstr(href, "/latest/")) {
                    content(c, b.title, sizeof(b.title));
                    if (!*b.title)
                        for (fz_xml *im = fz_xml_down(c); im; im = fz_xml_next(im)) {
                            const char *alt = fz_xml_att(im, "alt");
                            if (alt) {
                                snprintf(b.title, sizeof(b.title), "%s", alt);
                                break;
                            }
                        }
                    const char *t = fz_xml_att(c, "title");
                    if (!*b.title && t)
                        snprintf(b.title, sizeof(b.title), "%s", t);
                    if (!*b.title) {
                        const char *name = strrchr(href, '/');
                        snprintf(b.title, sizeof(b.title), "%s",
                                 name && name[1] ? name + 1
                                 : epub          ? "EPUB"
                                                 : "PDF");
                    }
                    b.acquisition = epub || pdf;
                    snprintf(b.format, sizeof(b.format), "%s", pdf ? "pdf" : epub ? "epub" : "");
                    snprintf(b.author, sizeof(b.author), "%s",
                             strstr(base, "epubbooks.com")  ? "epubBooks"
                             : strstr(base, "bdebooks.com") ? "BDeBooks"
                                                            : "Katalog");
                    opds_items[opds_count++] = b;
                }
            }
        }
        html_books(fz_xml_down(c), base, depth + 1);
    }
}
int discover_parse(const char *path, const char *base) {
    fz_context *ctx = fz_new_context(NULL, NULL, 1024 * 1024);
    if (!ctx)
        return 0;
    fz_xml *xml = NULL;
    fz_buffer *buf = NULL;
    int ok = 1;
    fz_var(xml);
    fz_var(buf);
    opds_count = 0;
    fz_try(ctx) {
        buf = fz_read_file(ctx, path);
        fz_terminate_buffer(ctx, buf);
        unsigned char *bytes;
        size_t length = fz_buffer_storage(ctx, buf, &bytes);
        int html = length > 10 && (strstr(base, "epubbooks.com") || strstr(base, "bdebooks.com") ||
                                   strcasestr((const char *)bytes, "<html"));
        xml = html ? fz_parse_xml_from_html5(ctx, buf) : fz_parse_xml(ctx, buf, 0);
        fz_xml *root = fz_xml_root(xml);
        if (html) {
            html_book_title[0] = 0;
            html_title(root, 0);
            html_books(root, base, 0);
        }

        if (!strcmp(localtag(root), "response"))
            archive_documents(root, base);
        else if (!strcmp(localtag(root), "files") &&
                 !strncmp(base, "https://archive.org/download/", 29))
            archive_files(root, base);
        else if (!html && strcmp(localtag(root), "feed"))
            fz_throw(ctx, FZ_ERROR_FORMAT, "Kein OPDS-Atom-Katalog.");
        for (fz_xml *x = fz_xml_down(root); x && opds_count < OPDS_LIMIT; x = fz_xml_next(x)) {
            if (!strcmp(localtag(x), "link")) {
                const char *rel = fz_xml_att(x, "rel"), *href = fz_xml_att(x, "href");
                if (rel && href && !strcmp(rel, "next")) {
                    OPDSItem *b = &opds_items[opds_count++];
                    memset(b, 0, sizeof(*b));
                    snprintf(b->title, sizeof(b->title), "Weitere Katalogseite");
                    resolve(base, href, b->url, sizeof(b->url));
                }
                continue;
            }
            if (strcmp(localtag(x), "entry"))
                continue;
            OPDSItem b = {0};
            int priority = 0;
            for (fz_xml *c = fz_xml_down(x); c; c = fz_xml_next(c)) {
                const char *tag = localtag(c);
                if (!strcmp(tag, "title"))
                    content(c, b.title, sizeof(b.title));
                else if (!strcmp(tag, "author")) {
                    for (fz_xml *a = fz_xml_down(c); a; a = fz_xml_next(a))
                        if (!strcmp(localtag(a), "name"))
                            content(a, b.author, sizeof(b.author));
                } else if (!strcmp(tag, "summary") || !strcmp(tag, "content")) {
                    if (!*b.summary)
                        content(c, b.summary, sizeof(b.summary));
                } else if (!strcmp(tag, "link")) {
                    const char *href = fz_xml_att(c, "href"), *type = fz_xml_att(c, "type"),
                               *rel = fz_xml_att(c, "rel");
                    if (!href || !type)
                        continue;
                    int rank = strstr(type, "epub+zip")          ? 3
                               : strstr(type, "application/pdf") ? 2
                               : strstr(type, "atom+xml")        ? 1
                                                                 : 0;
                    if (rank > priority && (!rel || strstr(rel, "acquisition") || rank == 1)) {
                        priority = rank;
                        resolve(base, href, b.url, sizeof(b.url));
                        b.acquisition = rank > 1;
                        snprintf(b.format, sizeof(b.format), "%s",
                                 rank == 3   ? "epub"
                                 : rank == 2 ? "pdf"
                                             : "");
                    }
                }
            }
            if (*b.url) {
                opds_items[opds_count++] = b;
            }
        }
    }
    fz_always(ctx) {
        fz_drop_xml(ctx, xml);
        fz_drop_buffer(ctx, buf);
    }
    fz_catch(ctx) {
        fprintf(stderr, "catalog_error=%s\n", fz_caught_message(ctx));
        snprintf(error, sizeof(error), "%s", tr("Katalog konnte nicht gelesen werden."));
        ok = 0;
    }
    fz_drop_context(ctx);
    return ok;
}
void discover_cancel(void) {
    if (job > 0) {
        kill(-job, SIGTERM);
        kill(job, SIGTERM);
        waitpid(job, NULL, 0);
        job = -1;
    }
    if (*temp)
        unlink(temp);
}
/* Ordinary public epubBooks download grant, identical to its download button. */
static int epubbooks_grant(const char *url, char *out, size_t cap) {
    const char *prefix = "https://www.epubbooks.com/downloads/";
    const char *id = url + strlen(prefix);
    if (!*id || strspn(id, "0123456789") != strlen(id) || strlen(id) > 11)
        return 0;
    char response[2048], payload[80], ca[2048];
    snprintf(response, sizeof(response), "%s/epubbooks-grant.tmp", data_root);
    snprintf(payload, sizeof(payload), "{\"id\":%s}", id);
    snprintf(ca, sizeof(ca), "%s/../certs/ca-bundle.pem", data_root);
    if (!access(ca, R_OK))
        setenv("CURL_CA_BUNDLE", ca, 1);
    pid_t p = fork();
    if (p < 0)
        return 0;
    if (!p) {
        execlp("curl", "curl", "--fail", "--silent", "--max-time", "20", "--proto", "=https", "-X",
               "POST", "--data", payload, "--output", response,
               "https://www.epubbooks.com/downloads", NULL);
        _exit(127);
    }
    int status = 0;
    while (waitpid(p, &status, 0) < 0 && errno == EINTR) {
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status))
        return 0;
    FILE *f = fopen(response, "rb");
    if (!f)
        return 0;
    char text[2048];
    size_t n = fread(text, 1, sizeof(text) - 1, f);
    fclose(f);
    unlink(response);
    text[n] = 0;
    Json j;
    JsonToken tokens[16];
    char token[128];
    if (!json_parse(&j, text, tokens, 16))
        return 0;
    int t = json_get(&j, 0, "id");
    if (t < 0)
        return 0;
    json_string(&j, t, token, sizeof(token));
    if (!*token ||
        strspn(token, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ ") !=
            strlen(token) ||
        strchr(token, ' '))
        return 0;
    snprintf(out, cap, "https://www.epubbooks.com/downloads/%s/file", token);
    return 1;
}
int discover_start(const char *url, const OPDSItem *book) {
    if (job > 0)
        return 0;
    if (strncmp(url, "https://", 8) && strncmp(url, "http://", 7)) {
        snprintf(error, sizeof(error), "%s",
                 tr("Nur HTTP- und HTTPS-Kataloge werden unterstützt."));
        return 0;
    }
    if (strlen(url) >= sizeof(current_url))
        return 0;
    wiki_mode = 0;
    acquisition = book != NULL;
    snprintf(current_url, sizeof(current_url), "%s", url);
    snprintf(temp, sizeof(temp), "%s/opds-download.tmp", data_root);
    if (book) {
        char name[192];
        size_t n = 0;
        for (const unsigned char *p = (const unsigned char *)book->title; *p && n < 170; p++) {
            unsigned char c = *p;
            if (c < ' ' || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' ||
                c == '<' || c == '>')
                c = '_';
            name[n++] = (char)c;
        }
        while (n && ((unsigned char)book->title[n] & 192) == 128)
            n--;
        if (!n) {
            strcpy(name, "Buch");
            n = 4;
        }
        name[n] = 0;
        snprintf(target, sizeof(target), "%s/%s.%s", library_root, name, book->format);
        if (access(target, F_OK) == 0) {
            snprintf(error, sizeof(error), "%s",
                     tr("Dieses Buch ist bereits als Datei vorhanden."));
            return 0;
        }
    } else
        snprintf(target, sizeof(target), "%s/opds-feed.xml", data_root);
    error[0] = 0;
    pid_t parent = getpid();
    sigset_t blocked, oldmask;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGTERM);
    sigprocmask(SIG_BLOCK, &blocked, &oldmask);
    job = fork();
    if (job < 0) {
        sigprocmask(SIG_SETMASK, &oldmask, NULL);
        return 0;
    }
    if (!job) {
        setsid();
        signal(SIGTERM, SIG_DFL);
#ifndef LUMEN_HOST
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        if (getppid() == 1)
            _exit(1);
#endif
        sigprocmask(SIG_SETMASK, &oldmask, NULL);
        char granted[2048];
        const char *fetch_url = url;
        if (acquisition && !strncmp(url, "https://www.epubbooks.com/downloads/",
                                    strlen("https://www.epubbooks.com/downloads/"))) {
            if (!epubbooks_grant(url, granted, sizeof(granted))) {
                kill(parent, SIGUSR1);
                _exit(1);
            }
            fetch_url = granted;
        }
        pid_t curl = fork();
        if (curl < 0) {
            kill(parent, SIGUSR1);
            _exit(1);
        }
        if (!curl) {
#ifndef LUMEN_HOST
            prctl(PR_SET_PDEATHSIG, SIGTERM);
#endif
            struct rlimit limit = {acquisition ? 128 * 1024 * 1024 : 8 * 1024 * 1024,
                                   acquisition ? 128 * 1024 * 1024 : 8 * 1024 * 1024};
            setrlimit(RLIMIT_FSIZE, &limit);
            char log[2048];
            snprintf(log, sizeof(log), "%s/opds-network.log", data_root);
            int f = open(log, O_CREAT | O_TRUNC | O_WRONLY, 0600);
            if (f >= 0) {
                dup2(f, 1);
                dup2(f, 2);
                close(f);
            }
            char ca[2048];
            const char *override = getenv("LUMEN_CA_BUNDLE");
            snprintf(ca, sizeof(ca), "%s", override ? override : "");
            if (!*ca)
                snprintf(ca, sizeof(ca), "%s/../certs/ca-bundle.pem", data_root);
            if (access(ca, R_OK) == 0)
                setenv("CURL_CA_BUNDLE", ca, 1);
            execlp("curl", "curl", "--user-agent", "LUMEN/0.12 (personal Kindle reader)", "--fail",
                   "--location", "--max-redirs", "5", "--connect-timeout", "10", "--max-time",
                   acquisition ? "180" : "45", "--proto", "=http,https", "--proto-redir",
                   "=http,https", "--output", temp, fetch_url, NULL);
            _exit(127);
        }
        int st = 0;
        while (waitpid(curl, &st, 0) < 0 && errno == EINTR) {
        }
        int ok = WIFEXITED(st) && WEXITSTATUS(st) == 0;
        if (ok) {
            if (acquisition) {
                int reserve = open(target, O_CREAT | O_EXCL | O_WRONLY, 0600);
                if (reserve < 0)
                    ok = 0;
                else {
                    close(reserve);
                    if (rename(temp, target)) {
                        unlink(target);
                        ok = 0;
                    }
                }
            } else if (rename(temp, target))
                ok = 0;
        }
        if (!ok)
            unlink(temp);
        kill(parent, SIGUSR1);
        _exit(ok ? 0 : 1);
    }
    sigprocmask(SIG_SETMASK, &oldmask, NULL);
    return 1;
}
int discover_complete(void) {
    if (job <= 0)
        return 0;
    int status;
    pid_t done;
    do {
        done = waitpid(job, &status, WNOHANG);
    } while (done < 0 && errno == EINTR);
    if (done != job)
        return 0;
    job = -1;
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
        snprintf(error, sizeof(error), "%s",
                 tr("Verbindung oder Download fehlgeschlagen. Details im Netzwerkprotokoll."));
        return -1;
    }
    if (wiki_mode) {
        if (discover_parse_lookup(target))
            return 3;
        if (wiki_fallback_pending) {
            wiki_fallback_pending = 0;
            if (discover_start(wiki_search_url, NULL)) {
                wiki_mode = 1;
                return 0;
            }
        }
        return -1;
    }
    if (acquisition)
        return 2;
    return discover_parse(target, current_url) ? 1 : -1;
}

const char *discover_lookup_text(void) { return wiki_text; }
const char *discover_lookup_title(void) { return wiki_title; }
const char *discover_lookup_image(void) { return wiki_image; }
const char *discover_lookup_image_name(void) { return wiki_image_name; }
static void wiki_content(fz_xml *x) {
    if (!x)
        return;
    if (!strcmp(localtag(x), "error")) {
        const char *code = fz_xml_att(x, "code");
        if (code && strstr(code, "busy"))
            snprintf(error, sizeof(error), "%s",
                     tr("Wikipedia ist ausgelastet. Bitte später erneut versuchen."));
    }
    if (!strcmp(localtag(x), "page")) {
        const char *title = fz_xml_att(x, "title");
        if (title)
            snprintf(wiki_title, sizeof(wiki_title), "%s", title);
        const char *file = fz_xml_att(x, "pageimage");
        if (file)
            snprintf(wiki_image_name, sizeof(wiki_image_name), "%s", file);
    }
    if (!strcmp(localtag(x), "original") || !strcmp(localtag(x), "thumbnail")) {
        const char *src = fz_xml_att(x, "source");
        if (src &&
            (!strncmp(src, "https://upload.wikimedia.org/", 29) ||
             !strncmp(src, "https://thumb.wikimedia.org/", 28)) &&
            (!*wiki_image || !strcmp(localtag(x), "thumbnail")))
            snprintf(wiki_image, sizeof(wiki_image), "%s", src);
    }
    if (!strcmp(localtag(x), "extract")) {
        content(x, wiki_text, sizeof(wiki_text));
        size_t n = strlen(wiki_text);
        if (n == sizeof(wiki_text) - 1) {
            while (n && ((unsigned char)wiki_text[n - 1] & 192) == 128)
                n--;
            if (n && (unsigned char)wiki_text[n - 1] >= 192)
                n--;
            wiki_text[n] = 0;
        }
    }
    for (fz_xml *c = fz_xml_down(x); c; c = fz_xml_next(c))
        wiki_content(c);
}
int discover_parse_lookup(const char *path) {
    wiki_text[0] = wiki_title[0] = wiki_image[0] = wiki_image_name[0] = 0;
    error[0] = 0;
    fz_context *ctx = fz_new_context(NULL, NULL, 1024 * 1024);
    if (!ctx)
        return 0;
    fz_xml *xml = NULL;
    fz_buffer *buf = NULL;
    int ok = 0;
    fz_var(xml);
    fz_var(buf);
    fz_var(ok);
    fz_try(ctx) {
        buf = fz_read_file(ctx, path);
        xml = fz_parse_xml(ctx, buf, 0);
        wiki_content(fz_xml_root(xml));
        ok = *wiki_text != 0;
    }
    fz_always(ctx) {
        fz_drop_xml(ctx, xml);
        fz_drop_buffer(ctx, buf);
    }
    fz_catch(ctx) {}
    fz_drop_context(ctx);
    if (!ok && !*error)
        snprintf(error, sizeof(error), "%s", tr("Kein Wikipedia-Eintrag gefunden."));
    return ok;
}
int discover_lookup(const char *query, int english) {
    char encoded[900], url[2048];
    size_t n = 0, count = 0;
    size_t length = strlen(query);
    if (length > 160)
        length = 160;
    while (length && ((unsigned char)query[length] & 192) == 128)
        length--;
    for (const unsigned char *p = (const unsigned char *)query;
         *p && count < length && n + 4 < sizeof(encoded); p++, count++) {
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
            *p == '-' || *p == '_')
            encoded[n++] = *p;
        else {
            snprintf(encoded + n, 4, "%%%02X", *p);
            n += 3;
        }
    }
    encoded[n] = 0;
    wiki_text[0] = wiki_title[0] = wiki_image[0] = wiki_image_name[0] = 0;
    snprintf(url, sizeof(url),
             "https://%s.wikipedia.org/w/"
             "api.php?action=query&generator=search&gsrsearch=%s&gsrnamespace=0&gsrlimit=1&prop="
             "extracts%%7Cpageimages&piprop=thumbnail%%7Coriginal%%7Cname&pithumbsize=500&"
             "pilicense=free&explaintext=1&exsectionformat=wiki&exlimit=1&format=xml",
             english == 2   ? "fr"
             : english == 3 ? "zh"
             : english      ? "en"
                            : "de",
             encoded);
    snprintf(wiki_search_url, sizeof(wiki_search_url), "%s", url);
    snprintf(url, sizeof(url),
             "https://%s.wikipedia.org/w/"
             "api.php?action=query&titles=%s&redirects=1&prop=extracts%%7Cpageimages&piprop="
             "thumbnail%%7Coriginal%%7Cname&pithumbsize=500&pilicense=free&explaintext=1&"
             "exsectionformat=wiki&exlimit=1&format=xml",
             english == 2   ? "fr"
             : english == 3 ? "zh"
             : english      ? "en"
                            : "de",
             encoded);
    discover_cancel();
    wiki_fallback_pending = 1;
    int ok = discover_start(url, NULL);
    if (ok)
        wiki_mode = 1;
    return ok;
}

void discover_restore_lookup(const char *t, const char *s, const char *i, const char *n) {
    snprintf(wiki_title, sizeof(wiki_title), "%s", t);
    snprintf(wiki_text, sizeof(wiki_text), "%s", s);
    snprintf(wiki_image, sizeof(wiki_image), "%s", i);
    snprintf(wiki_image_name, sizeof(wiki_image_name), "%s", n);
    error[0] = 0;
}
