#include "api.h"
#include "lumen_markup.h"
#include "markup.h"
static const char *const attributes[]={"href","rel","class","itemprop","property","content"};
#define article_tree(path,html) markup_load(path,html,attributes,6)
#define article_attr(node,name) markup_attribute(node,name,attributes,6)
static PressList press_list;
static PressArticle press_article;
static pid_t pw = -1;
static int job;
static char result_path[2048];
static uint64_t press_id(const char *s) {
    uint64_t h = 1469598103934665603ull;
    for (; *s; s++)
        h = (h ^ (unsigned char)*s) * 1099511628211ull;
    return h;
}
static void pp(char *out, int n, const char *name) {
    snprintf(out,n,"%s/press/%s",getenv("LUMEN_APP_DATA"),name);
}
static void redirect_worker_io(void) {
    int fd=open("/dev/null",O_RDWR);
    if(fd<0 || dup2(fd,STDIN_FILENO)<0 || dup2(fd,STDOUT_FILENO)<0)_exit(1);
    if(fd>STDOUT_FILENO)close(fd);
}
static int readbin(const char *p, void *out, size_t n) {
    FILE *f = fopen(p, "rb");
    if(!f) {
        const char *legacy=getenv("LUMEN_LEGACY_DATA"),*leaf=strrchr(p,'/');
        char relative[128];
        if(!legacy || !leaf || snprintf(relative,sizeof(relative),"press%s",leaf)>=(int)sizeof(relative))return 0;
        size_t length;
        return lumen_storage_read_at(legacy,relative,out,n,&length) && length==n;
    }
    int ok = fread(out, n, 1, f) == 1;
    fclose(f);
    return ok;
}
static void sane_article(PressArticle *a) {
    a->title[255] = a->url[2047] = a->publisher[95] = a->body[131071] = a->image[2047] =
        a->caption[511] = 0;
}
static void sane_list(PressList *l) {
    l->query[255] = 0;
    for (int i = 0; i < l->count; i++) {
        PressItem *a = &l->item[i];
        a->title[255] = a->url[2047] = a->summary[1599] = a->publisher[95] = a->category[47] =
            a->published[95] = 0;
    }
}
static int fetch(const char *url, const char *file) {
    if (strncmp(url, "https://", 8) && strncmp(url, "http://", 7))
        return 0;
    pid_t p = fork();
    if (!p) {
        freopen("/dev/null", "w", stderr);
        execlp("curl", "curl", "-q", "--fail", "--silent", "--location", "--compressed",
               "--connect-timeout", "6", "--max-time", "22", "--max-redirs", "5", "--max-filesize",
               "4194304", "--proto", "=http,https", "--proto-redir", "=http,https", "--user-agent",
               "Mozilla/5.0 (compatible; LUMEN/1.1; personal offline reader)", "--output", file,
               "--", url, (char *)NULL);
        _exit(127);
    }
    return wait_success(p);
}
static const char *tag(MarkupNode *x) {
    const char *s = x->tag;
    if (!s)
        return "";
    const char *c = strchr(s, ':');
    return c ? c + 1 : s;
}
static void txt(MarkupNode *x, char *out, size_t cap) {
    for (MarkupNode *c = x->child; c; c = c->next) {
        const char *s = c->text;
        if (s) {
            size_t n = strlen(out);
            if (n + 1 < cap)
                snprintf(out + n, cap - n, "%s", s);
        } else
            txt(c, out, cap);
    }
}
static void clean(char *s) {
    char *q = s;
    int markup = 0, space = 0;
    for (char *p = s; *p; p++) {
        if (*p == '<') {
            markup = 1;
            space = 1;
        } else if (*p == '>')
            markup = 0;
        else if (!markup) {
            if (isspace((unsigned char)*p)) {
                space = 1;
                continue;
            }
            if (space && q > s)
                *q++ = ' ';
            space = 0;
            if (!strncmp(p, "&amp;", 5)) {
                *q++ = '&';
                p += 4;
            } else if (!strncmp(p, "&quot;", 6)) {
                *q++ = '"';
                p += 5;
            } else if (!strncmp(p, "&nbsp;", 6)) {
                *q++ = ' ';
                p += 5;
            } else
                *q++ = *p;
        }
    }
    *q = 0;
}
static void rss(MarkupNode *x, PressList *l, const char *publisher, const char *category, int *left) {
    if (!x || !*left || l->count >= PRESS_LIMIT)
        return;
    if (!strcmp(tag(x), "item") || !strcmp(tag(x), "entry")) {
        PressItem a = {0};
        snprintf(a.publisher, 96, "%s", publisher);
        snprintf(a.category, 48, "%s", category);
        a.rank = 12 - *left;
        for (MarkupNode *c = x->child; c; c = c->next) {
            const char *t = tag(c);
            if (!strcmp(t, "title"))
                txt(c, a.title, 256);
            else if (!strcmp(t, "description") || !strcmp(t, "summary"))
                txt(c, a.summary, 1600);
            else if ((!strcmp(t, "source") || !strcmp(t, "Source")))
                txt(c, a.publisher, 96);
            else if (!strcmp(t, "pubDate") || !strcmp(t, "published"))
                txt(c, a.published, 96);
            else if (!strcmp(t, "link")) {
                const char *h = article_attr(c, "href"), *rel = article_attr(c, "rel");
                if (h && (!rel || !strcmp(rel, "alternate")))
                    snprintf(a.url, 2048, "%s", h);
                else if (!h)
                    txt(c, a.url, 2048);
            }
        }
        clean(a.title);
        clean(a.summary);
        int dupe = 0;
        for (int i = 0; i < l->count; i++)
            if (!strcmp(a.url, l->item[i].url) || !strcmp(a.title, l->item[i].title))
                dupe = 1;
        if (*a.title && *a.url && !dupe) {
            l->item[l->count++] = a;
            --*left;
        }
        return;
    }
    for (MarkupNode *c = x->child; c && *left; c = c->next)
        rss(c, l, publisher, category, left);
}
static int feed(const char *file, PressList *l, const char *pub, const char *cat, int limit) {
    MarkupNode *root=article_tree(file,0);
    int before=l->count;
    for(MarkupNode *node=root;node && limit;node=node->next)rss(node,l,pub,cat,&limit);
    markup_free(root);
    return l->count-before;
}
static int blocked(MarkupNode *x) {
    const char *t = tag(x);
    const char *skip[] = {"script", "style",  "nav",      "header", "footer", "aside",
                          "form",   "button", "noscript", "svg",    NULL};
    for (int i = 0; skip[i]; i++)
        if (!strcmp(t, skip[i]))
            return 1;
    const char *c = article_attr(x, "class");
    return c && (strstr(c, "related") || strstr(c, "newsletter") || strstr(c, "advert") ||
                 strstr(c, "consent") || strstr(c, "paywall"));
}
static void gather(MarkupNode *x, char *out, size_t cap) {
    if (!x || blocked(x))
        return;
    const char *t = tag(x);
    if (!strcmp(t, "p") || !strcmp(t, "h2") || !strcmp(t, "h3") || !strcmp(t, "figcaption") ||
        !strcmp(t, "li")) {
        char line[8192] = "";
        txt(x, line, sizeof(line));
        clean(line);
        size_t n = strlen(out), z = strlen(line);
        if (z > 12 && n + z + 3 < cap) {
            memcpy(out + n, line, z);
            memcpy(out + n + z, "\n\n", 3);
        }
        return;
    }
    for (MarkupNode *c = x->child; c; c = c->next)
        gather(c, out, cap);
}
static void find_article(MarkupNode *x, MarkupNode **best, int *score) {
    if (!x || blocked(x))
        return;
    const char *t = tag(x), *ip = article_attr(x, "itemprop");
    if (!strcmp(t, "article") || !strcmp(t, "main") || (ip && strstr(ip, "articleBody"))) {
        char *text = calloc(1, 131072);
        if (text) {
            gather(x, text, 131072);
            int n = strlen(text);
            if (ip)
                n += 2000;
            if (n > *score) {
                *score = n;
                *best = x;
            }
            free(text);
        }
    }
    for (MarkupNode *c = x->child; c; c = c->next)
        find_article(c, best, score);
}
static void metadata(MarkupNode *x, PressArticle *a) {
    if (!x)
        return;
    if (!strcmp(tag(x), "meta")) {
        const char *p = article_attr(x, "property"), *s = article_attr(x, "content");
        if (p && s) {
            if (!strcmp(p, "og:image"))
                snprintf(a->image, 2048, "%s", s);
            if (!strcmp(p, "og:title"))
                snprintf(a->title, 256, "%s", s);
        }
    }
    if (!strcmp(tag(x), "figcaption") && !*a->caption) {
        txt(x, a->caption, 512);
        clean(a->caption);
    }
    for (MarkupNode *c = x->child; c; c = c->next)
        metadata(c, a);
}
int press_extract_file(const char *file, const PressItem *item, PressArticle *a) {
    memset(a, 0, sizeof(*a));
    a->magic = 0x50524131;
    snprintf(a->title, 256, "%s", item->title);
    snprintf(a->url, 2048, "%s", item->url);
    snprintf(a->publisher, 96, "%s", item->publisher);
    MarkupNode *root=article_tree(file,1),*best=NULL;
    int score=0;
    for(MarkupNode *node=root;node;node=node->next)find_article(node,&best,&score);
    if(best) {
        gather(best,a->body,sizeof(a->body));
        for(MarkupNode *node=root;node;node=node->next)metadata(node,a);
        a->complete=strlen(a->body)>300;
    }
    markup_free(root);
    if (!a->complete)
        snprintf(a->body, sizeof(a->body), "%s", item->summary);
    return *a->body != 0;
}
static void init(void) {
    char p[2048];
    pp(p, sizeof(p), "");
    mkdir(p, 0700);
}
int press_busy(void) { return pw > 0; }
void press_cancel(void) {
    if (pw > 0) {
        kill(pw, SIGTERM);
        while (waitpid(pw, NULL, 0) < 0 && errno == EINTR) {
        }
        pw = -1;
    }
}
int press_load_day(int d) {
    init();
    char name[80], p[2048];
    snprintf(name, 80, "edition-%08d.bin", d);
    pp(p, sizeof(p), name);
    PressList *l = malloc(sizeof(*l));
    int ok = l && readbin(p, l, sizeof(*l)) && l->magic == 0x50524c31 && l->date == (unsigned)d &&
             l->count > 0 && l->count <= PRESS_LIMIT;
    if (ok) {
        sane_list(l);
        press_list = *l;
    }
    free(l);
    return ok;
}
static int list_start(int date, int lang, const char *query) {
    if (press_busy())
        return 0;
    init();
    job = 1;
    char name[80];
    snprintf(name, 80, *query ? "search.bin" : "edition-%08d.bin", date);
    pp(result_path, sizeof(result_path), name);
    pw = fork();
    if (!pw) {
        redirect_worker_io();
        PressList *l = calloc(1, sizeof(*l));
        if (!l)
            _exit(1);
        l->magic = 0x50524c31;
        l->date = date;
        snprintf(l->query, 256, "%s", query);
        char temp[2048];
        snprintf(name,sizeof(name),"feed-%ld.tmp",(long)getpid());
        pp(temp,sizeof(temp),name);
        if (*query) {
            char encoded[1600], url[2048];
            int n = 0;
            for (const unsigned char *p = (const unsigned char *)query; *p && n < 1500; p++) {
                if (isalnum(*p))
                    encoded[n++] = *p;
                else {
                    snprintf(encoded + n, 4, "%%%02X", *p);
                    n += 3;
                }
            }
            encoded[n] = 0;
            snprintf(url, sizeof(url),
                     "https://www.bing.com/news/search?q=%s&format=rss&setlang=%s", encoded,
                     lang == 0   ? "de"
                     : lang == 2 ? "fr"
                     : lang == 3 ? "zh"
                                 : "en");
            if (fetch(url, temp))
                feed(temp, l, "News", "Suche", 40);
        } else {
            const char *urls[] = {lang == 0 ? "https://www.tagesschau.de/xml/rss2/"
                                            : "https://feeds.bbci.co.uk/news/world/rss.xml",
                                  "https://feeds.bbci.co.uk/news/business/rss.xml",
                                  lang == 0 ? "https://www.heise.de/rss/heise-atom.xml"
                                            : "https://feeds.bbci.co.uk/news/technology/rss.xml",
                                  "https://www.nasa.gov/news-release/feed/",
                                  "https://www.theguardian.com/science/rss",
                                  "https://www.carbonbrief.org/feed/",
                                  "https://www.theguardian.com/environment/rss",
                                  "https://www.theguardian.com/culture/rss"};
            const char *names[] = {lang == 0 ? "Tagesschau" : "BBC",
                                   "BBC",
                                   lang == 0 ? "heise" : "BBC",
                                   "NASA",
                                   "The Guardian",
                                   "Carbon Brief",
                                   "The Guardian",
                                   "The Guardian"};
            const char *cats[] = {"Politik",      "Wirtschaft", "Technologie", "Weltraum",
                                  "Wissenschaft", "Energie",    "Umwelt",      "Kultur"};
            for (int i = 0; i < 8; i++)
                if (fetch(urls[i], temp))
                    feed(temp, l, names[i], cats[i], i ? 5 : 9);
            /* Editorial headline position first, then topic diversity; no invented importance
             * score. */
            for (int i = 0; i < l->count; i++)
                for (int j = i + 1; j < l->count; j++)
                    if (l->item[j].rank < l->item[i].rank) {
                        PressItem t = l->item[i];
                        l->item[i] = l->item[j];
                        l->item[j] = t;
                    }
        }
        int ok = l->count && lumen_storage_write(strstr(result_path,"/press/")+1,l,sizeof(*l));
        unlink(temp);
        free(l);
        _exit(ok ? 0 : 1);
    }
    return pw > 0;
}
int press_start_day(int date, int lang) {
    if (press_load_day(date))
        return 2;
    return list_start(date, lang, "");
}
int press_search(const char *q, int lang) { return list_start(0, lang, q); }
void press_image_path(char *out, int n) {
    char name[96];
    snprintf(name, 96, "%016llx.pgm", (unsigned long long)press_id(press_article.url));
    pp(out, n, name);
}
int press_open(const PressItem *item) {
    if (press_busy())
        return 0;
    init();
    char name[96];
    snprintf(name, 96, "%016llx.article", (unsigned long long)press_id(item->url));
    pp(result_path, sizeof(result_path), name);
    PressArticle *a = malloc(sizeof(*a));
    int cached = a && readbin(result_path, a, sizeof(*a)) && a->magic == 0x50524131;
    if (cached) {
        sane_article(a);
        cached = !strcmp(a->url, item->url);
    }
    if (cached) {
        press_article = *a;
        if (a->complete) {
            free(a);
            return 2;
        }
    }
    free(a);
    job = 2;
    PressItem request = *item;
    if (!cached)
        memset(&press_article, 0, sizeof(press_article));
    snprintf(press_article.title, 256, "%s", item->title);
    snprintf(press_article.url, 2048, "%s", item->url);
    pw = fork();
    if (!pw) {
        redirect_worker_io();
        PressArticle *article = calloc(1, sizeof(*article));
        char html[2048], image[2048], raw[2048];
        snprintf(name,sizeof(name),"article-%ld.tmp",(long)getpid());
        pp(html,sizeof(html),name);
        int ok = article && fetch(request.url, html) && press_extract_file(html, &request, article);
        if (!ok && article && *request.summary) {
            article->magic = 0x50524131;
            snprintf(article->body, sizeof(article->body), "%s", request.summary);
            snprintf(article->title, 256, "%s", request.title);
            snprintf(article->url, 2048, "%s", request.url);
            snprintf(article->publisher, 96, "%s", request.publisher);
            ok = 1;
        }
        if (ok) {
            ok = lumen_storage_write(strstr(result_path,"/press/")+1,article,sizeof(*article));
            if (*article->image) {
                snprintf(name, 96, "%016llx.pgm", (unsigned long long)press_id(article->url));
                pp(image, sizeof(image), name);
                snprintf(name,sizeof(name),"image-%ld.tmp",(long)getpid());
                pp(raw,sizeof(raw),name);
                if (fetch(article->image, raw))
                    convert_image(raw,image);
                unlink(raw);
            }
        }
        unlink(html);
        free(article);
        _exit(ok ? 0 : 1);
    }
    return pw > 0;
}
int press_poll(void) {
    if (pw <= 0)
        return 0;
    int s;
    pid_t p = waitpid(pw, &s, WNOHANG);
    if (!p)
        return 0;
    pw = -1;
    if (p < 0 || !WIFEXITED(s) || WEXITSTATUS(s))
        return -job;
    if (job == 1) {
        if (!readbin(result_path, &press_list, sizeof(press_list)) ||
            press_list.magic != 0x50524c31 || press_list.count < 1 ||
            press_list.count > PRESS_LIMIT)
            return -1;
    } else if (!readbin(result_path, &press_article, sizeof(press_article)) ||
               press_article.magic != 0x50524131)
        return -2;
    if (job == 1)
        sane_list(&press_list);
    else
        sane_article(&press_article);
    return job;
}
