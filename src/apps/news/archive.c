#define _GNU_SOURCE
#include "archive_api.h"
#include "../../system/engine.h"
#include "../../system/library.h"
#include "../../system/services.h"
#include <ctype.h>
#include <errno.h>
#include <mupdf/fitz.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
Newspaper newspaper;
static pid_t worker = -1;
static int pending_date;
int news_date(void) {
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    return (t.tm_year + 1900) * 10000 + (t.tm_mon + 1) * 100 + t.tm_mday;
}
static void path(char *out, int n, const char *name) {
    snprintf(out, n, "%s/news/%s", data_root, name);
}
int news_load(int date) {
    char p[2048], name[80];
    snprintf(name, sizeof(name), "%08d.bin", date);
    path(p, sizeof(p), name);
    FILE *f = fopen(p, "rb");
    if (!f)
        return 0;
    Newspaper *paper = malloc(sizeof(*paper));
    int ok = paper && fread(paper, sizeof(*paper), 1, f) == 1;
    fclose(f);
    if (ok)
        ok = paper->magic == 0x4c4e4557 && paper->version == 1 && paper->date == (unsigned)date &&
             paper->count > 0 && paper->count <= 10;
    if (ok) {
        for (int i = 0; i < paper->count; i++) {
            NewsArticle *a = &paper->article[i];
            if (a->category < 0 || a->category > 5)
                ok = 0;
            a->title[255] = a->body[2299] = a->url[1023] = a->publisher[79] = a->published[79] =
                a->image[127] = 0;
        }
        if (ok)
            newspaper = *paper;
    }
    free(paper);
    return ok;
}
int news_busy(void) { return worker > 0; }
void news_cancel(void) {
    if (worker > 0) {
        kill(-worker, SIGTERM);
        kill(worker, SIGTERM);
        while (waitpid(worker, NULL, 0) < 0 && errno == EINTR) {
        }
        worker = -1;
    }
}
static int fetch(const char *url, const char *out) {
    if (strncmp(url, "https://", 8) && strncmp(url, "http://", 7))
        return 0;
    pid_t p = fork();
    if (!p) {
        char ca[2048];
        snprintf(ca, sizeof(ca), "%s/../certs/ca-bundle.pem", data_root);
        if (access(ca, R_OK) == 0)
            setenv("CURL_CA_BUNDLE", ca, 1);
        FILE *f = freopen("/dev/null", "w", stderr);
        (void)f;
        execlp("curl", "curl", "--fail", "--silent", "--location", "--max-redirs", "4",
               "--connect-timeout", "6", "--max-time", "18", "--max-filesize", "3145728", "--proto",
               "=http,https", "--proto-redir", "=https", "--user-agent",
               "LUMEN Daily/0.16 (personal reader)", "--output", out, url, NULL);
        _exit(127);
    }
    if (p < 0)
        return 0;
    int st = 0;
    while (waitpid(p, &st, 0) < 0 && errno == EINTR) {
    }
    return WIFEXITED(st) && !WEXITSTATUS(st);
}
static const char *tag(fz_xml *x) {
    const char *s = fz_xml_tag(x);
    if (!s)
        return "";
    const char *c = strchr(s, ':');
    return c ? c + 1 : s;
}
static void content(fz_xml *x, char *out, int cap) {
    for (fz_xml *c = fz_xml_down(x); c; c = fz_xml_next(c)) {
        const char *s = fz_xml_text(c);
        if (s) {
            size_t n = strlen(out);
            snprintf(out + n, cap - n, "%s", s);
        } else
            content(c, out, cap);
    }
}
static void plain(char *s, int max) {
    char out[5000];
    int n = 0, in = 0, space = 0;
    for (int i = 0; s[i] && n < max - 1 && n < 4998; i++) {
        if (s[i] == '<') {
            in = 1;
            space = 1;
            continue;
        }
        if (s[i] == '>') {
            in = 0;
            continue;
        }
        if (in)
            continue;
        if (isspace((unsigned char)s[i])) {
            space = 1;
            continue;
        }
        if (space && n)
            out[n++] = ' ';
        space = 0;
        if (s[i] == '&') {
            if (!strncmp(s + i, "amp;", 4)) {
            }
            if (!strncmp(s + i, "&amp;", 5)) {
                out[n++] = '&';
                i += 4;
                continue;
            }
            if (!strncmp(s + i, "&nbsp;", 6)) {
                out[n++] = ' ';
                i += 5;
                continue;
            }
            if (!strncmp(s + i, "&quot;", 6)) {
                out[n++] = '"';
                i += 5;
                continue;
            }
            if (!strncmp(s + i, "&#8217;", 7)) {
                out[n++] = '\'';
                i += 6;
                continue;
            }
        }
        out[n++] = s[i];
    }
    while (n && ((unsigned char)out[n - 1] & 192) == 128)
        n--;
    if (n && (unsigned char)out[n - 1] >= 192)
        n--;
    out[n] = 0;
    snprintf(s, max, "%s", out);
}
static void items(fz_xml *x, Newspaper *p, int category, const char *publisher, int *left) {
    if (!x || !*left || p->count >= 8)
        return;
    if (!strcmp(tag(x), "item") || !strcmp(tag(x), "entry")) {
        NewsArticle a = {.category = category};
        snprintf(a.publisher, sizeof(a.publisher), "%s", publisher);
        for (fz_xml *c = fz_xml_down(x); c; c = fz_xml_next(c)) {
            const char *t = tag(c);
            if (!strcmp(t, "title"))
                content(c, a.title, sizeof(a.title));
            else if (!strcmp(t, "description") || !strcmp(t, "summary") ||
                     (!strcmp(t, "content") && !*a.body))
                content(c, a.body, sizeof(a.body));
            else if (!strcmp(t, "link")) {
                const char *href = fz_xml_att(c, "href"), *rel = fz_xml_att(c, "rel");
                if (href && (!rel || !strcmp(rel, "alternate")))
                    snprintf(a.url, sizeof(a.url), "%s", href);
                else if (!href)
                    content(c, a.url, sizeof(a.url));
            } else if (!strcmp(t, "pubDate") || !strcmp(t, "published"))
                content(c, a.published, sizeof(a.published));
        }
        plain(a.title, sizeof(a.title));
        plain(a.body, sizeof(a.body));
        int duplicate = 0;
        for (int i = 0; i < p->count; i++)
            if (!strcmp(p->article[i].title, a.title))
                duplicate = 1;
        if (*a.title && *a.body && !duplicate) {
            p->article[p->count++] = a;
            (*left)--;
        }
        return;
    }
    for (fz_xml *c = fz_xml_down(x); c && *left; c = fz_xml_next(c))
        items(c, p, category, publisher, left);
}
static int feed(const char *file, Newspaper *p, int category, const char *publisher) {
    fz_context *ctx = fz_new_context(NULL, NULL, 2 * 1024 * 1024);
    if (!ctx)
        return 0;
    fz_xml *xml = NULL;
    fz_buffer *buf = NULL;
    int count = p->count;
    fz_var(xml);
    fz_var(buf);
    fz_try(ctx) {
        buf = fz_read_file(ctx, file);
        xml = fz_parse_xml(ctx, buf, 0);
        int left = 2;
        items(fz_xml_root(xml), p, category, publisher, &left);
    }
    fz_always(ctx) {
        fz_drop_xml(ctx, xml);
        fz_drop_buffer(ctx, buf);
    }
    fz_catch(ctx) {}
    fz_drop_context(ctx);
    return p->count - count;
}
static int history(const char *file, Newspaper *p) {
    FILE *f = fopen(file, "rb");
    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    if (size < 0 || size > 3145728) {
        fclose(f);
        return 0;
    }
    char *s = malloc(size + 1);
    if (!s) {
        fclose(f);
        return 0;
    }
    fread(s, 1, size, f);
    fclose(f);
    s[size] = 0;
    JsonToken *tok = calloc(150000, sizeof(*tok));
    Json j;
    int ok = 0;
    if (tok && json_parse(&j, s, tok, 150000)) {
        int arr = json_get(&j, 0, "events"), count = arr >= 0 ? j.tokens[arr].count : 0;
        if (count) {
            int item = json_at(&j, arr, p->date % count);
            NewsArticle *a = &p->article[p->count];
            memset(a, 0, sizeof(*a));
            a->category = 4;
            int year = (int)json_number(&j, json_get(&j, item, "year"));
            snprintf(a->title, sizeof(a->title), "%d", year);
            json_string(&j, json_get(&j, item, "text"), a->body, sizeof(a->body));
            snprintf(a->publisher, 80, "Wikipedia · CC BY-SA");
            int pg = json_at(&j, json_get(&j, item, "pages"), 0);
            json_string(
                &j, json_get(&j, json_get(&j, json_get(&j, pg, "content_urls"), "desktop"), "page"),
                a->url, sizeof(a->url));
            if (*a->body) {
                p->count++;
                ok = 1;
            }
        }
    }
    free(tok);
    free(s);
    return ok;
}
int news_start(int language, unsigned topics) {
    int date = news_date();
    if (worker > 0)
        return 0;
    if (news_load(date))
        return 1;
    char folder[2048];
    path(folder, sizeof(folder), "");
    mkdir(folder, 0700);
    pending_date = date;
    worker = fork();
    if (worker < 0)
        return 0;
    if (!worker) {
        setpgid(0, 0);
        signal(SIGTERM, SIG_DFL);
        struct rlimit limit = {4 * 1024 * 1024, 4 * 1024 * 1024};
        setrlimit(RLIMIT_FSIZE, &limit);
        Newspaper *p = calloc(1, sizeof(*p));
        if (!p)
            _exit(1);
        p->magic = 0x4c4e4557;
        p->version = 1;
        p->date = date;
        p->topics = topics;
        const char *urls[] = {language == 0 ? "https://www.tagesschau.de/xml/rss2/"
                                            : "https://feeds.bbci.co.uk/news/world/rss.xml",
                              "https://www.nasa.gov/news-release/feed/",
                              language == 0 ? "https://www.heise.de/rss/heise-atom.xml"
                                            : "https://feeds.arstechnica.com/arstechnica/index",
                              "https://www.carbonbrief.org/feed/"};
        const char *names[] = {language == 0 ? "Tagesschau" : "BBC World", "NASA",
                               language == 0 ? "heise" : "Ars Technica", "Carbon Brief"};
        char temp[2048];
        path(temp, sizeof(temp), "feed.tmp");
        for (int cat = 0; cat < 4; cat++)
            if (topics & (1u << cat)) {
                if (fetch(urls[cat], temp))
                    feed(temp, p, cat, names[cat]);
            }
        if (topics & (1u << 4)) {
            char url[256];
            snprintf(url, sizeof(url),
                     "https://en.wikipedia.org/api/rest_v1/feed/onthisday/events/%02d/%02d",
                     date / 100 % 100, date % 100);
            if (fetch(url, temp))
                history(temp, p);
        }
        if (p->count == 0) {
            unlink(temp);
            free(p);
            _exit(1);
        }
        if (topics & (1u << 5)) {
            NewsArticle *a = &p->article[p->count++];
            a->category = 5;
            snprintf(a->title, 256, "Daily puzzle");
            snprintf(a->publisher, 80, "LUMEN");
            snprintf(a->body, sizeof(a->body), "%u", (unsigned)date);
        }
        char name[80], dest[2048];
        snprintf(name, 80, "%08d.bin", date);
        path(dest, sizeof(dest), name);
        int ok = atomic_write(dest, p, sizeof(*p));
        if (ok) {
            char d[32];
            snprintf(d, sizeof(d), "%d", date);
            path(dest, sizeof(dest), "latest.txt");
            atomic_write(dest, d, strlen(d));
        }
        unlink(temp);
        free(p);
        _exit(ok ? 0 : 1);
    }
    setpgid(worker, worker);
    return 1;
}
int news_poll(void) {
    if (worker <= 0)
        return 0;
    int st;
    pid_t p = waitpid(worker, &st, WNOHANG);
    if (!p)
        return 0;
    worker = -1;
    if (p > 0 && WIFEXITED(st) && !WEXITSTATUS(st) && news_load(pending_date))
        return 1;
    return -1;
}
