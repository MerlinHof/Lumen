#include "lumen_markup.h"
enum {OPDS_LIMIT=128};
typedef struct {
    char title[256],author[160],summary[768],url[2048],format[8];
    int acquisition;
} OPDSItem;
static OPDSItem opds_items[OPDS_LIMIT];
static int opds_count;
static const char *const catalog_attributes[]={"alt","data-dlid","href","name","numFound","rel","src","start","title","type"};
#define CATALOG_ATTRIBUTES (sizeof(catalog_attributes)/sizeof(catalog_attributes[0]))
#define catalog_attr(node,name) markup_attribute(node,name,catalog_attributes,CATALOG_ATTRIBUTES)
static const char *localtag(MarkupNode *x) {
    const char *t = x->tag;
    if (!t)
        return "";
    const char *c = strchr(t, ':');
    return c ? c + 1 : t;
}
static void content_depth(MarkupNode *x, char *out, size_t size, int depth) {
    if (!x || depth > 24)
        return;
    for (MarkupNode *c = x->child; c; c = c->next) {
        const char *text = c->text;
        if (text) {
            size_t n = strlen(out);
            snprintf(out + n, size - n, "%s", text);
        } else
            content_depth(c, out, size, depth + 1);
    }
}
static void content(MarkupNode *x, char *out, size_t size) { content_depth(x, out, size, 0); }
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
static void archive_documents(MarkupNode *node, const char *base) {
    if (!node || opds_count >= OPDS_LIMIT)
        return;
    if (!strcmp(localtag(node), "result")) {
        const char *start = catalog_attr(node, "start"), *count = catalog_attr(node, "numFound");
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
        for (MarkupNode *c = node->child; c; c = c->next) {
            const char *name = catalog_attr(c, "name");
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
    for (MarkupNode *c = node->child; c; c = c->next)
        archive_documents(c, base);
}
static void archive_files(MarkupNode *root, const char *base) {
    for (MarkupNode *c = root->child; c && opds_count < OPDS_LIMIT; c = c->next) {
        if (strcmp(localtag(c), "file"))
            continue;
        const char *name = catalog_attr(c, "name");
        if (!name || strlen(name) > 500 || strstr(name, ".."))
            continue;
        const char *ext = strrchr(name, '.');
        if (!ext || (strcmp(ext, ".pdf") && strcmp(ext, ".epub")))
            continue;
        int hidden = 0;
        for (MarkupNode *a = c->child; a; a = a->next)
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
static void html_title(MarkupNode *x, int depth) {
    if (!x || depth > 40 || *html_book_title)
        return;
    for (MarkupNode *c = x; c; c = c->next) {
        if (!strcmp(localtag(c), "h1")) {
            content(c, html_book_title, sizeof(html_book_title));
            return;
        }
        html_title(c->child, depth + 1);
    }
}
static void html_books(MarkupNode *node, const char *base, int depth) {
    if (!node || depth > 60 || opds_count >= OPDS_LIMIT)
        return;
    for (MarkupNode *c = node; c && opds_count < OPDS_LIMIT; c = c->next) {
        const char *tag = localtag(c), *href = catalog_attr(c, "href");
        const char *dl = catalog_attr(c, "data-dlid");
        if (dl && strstr(base, "www.epubbooks.com/book/") &&
            strspn(dl, "0123456789") == strlen(dl) && strlen(dl) < 12) {
            char details[768] = "";
            content(c->parent, details, sizeof(details));
            OPDSItem b = {.acquisition = 1};
            snprintf(b.title, sizeof(b.title), "%s",
                     *html_book_title ? html_book_title : "epubBooks");
            snprintf(b.author, sizeof(b.author), "epubBooks");
            snprintf(b.format, sizeof(b.format), "%s", strstr(details, "EPUB") ? "epub" : "mobi");
            snprintf(b.url, sizeof(b.url), "https://www.epubbooks.com/downloads/%s", dl);
            opds_items[opds_count++] = b;
        }
        if (!href && (!strcmp(tag, "iframe") || !strcmp(tag, "embed")))
            href = catalog_attr(c, "src");
        if (href && (!strcmp(tag, "a") || !strcmp(tag, "iframe") || !strcmp(tag, "embed"))) {
            const char *type = catalog_attr(c, "type");
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
                        for (MarkupNode *im = c->child; im; im = im->next) {
                            const char *alt = catalog_attr(im, "alt");
                            if (alt) {
                                snprintf(b.title, sizeof(b.title), "%s", alt);
                                break;
                            }
                        }
                    const char *t = catalog_attr(c, "title");
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
        html_books(c->child, base, depth + 1);
    }
}
static int discover_parse(const char *path,const char *base) {
    FILE *file=fopen(path,"rb");
    if(!file)return 0;
    char head[4096];
    size_t size=fread(head,1,sizeof(head)-1,file);
    fclose(file);
    head[size]=0;
    int html=strstr(base,"epubbooks.com") || strstr(base,"bdebooks.com") || strcasestr(head,"<html");
    MarkupNode *root=markup_load(path,html,catalog_attributes,CATALOG_ATTRIBUTES);
    opds_count=0;
    if(!root)return 0;
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
        {markup_free(root);return 0;}
    for (MarkupNode *x = root->child; x && opds_count < OPDS_LIMIT; x = x->next) {
        if (!strcmp(localtag(x), "link")) {
            const char *rel = catalog_attr(x, "rel"), *href = catalog_attr(x, "href");
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
        for (MarkupNode *c = x->child; c; c = c->next) {
            const char *tag = localtag(c);
            if (!strcmp(tag, "title"))
                content(c, b.title, sizeof(b.title));
            else if (!strcmp(tag, "author")) {
                for (MarkupNode *a = c->child; a; a = a->next)
                    if (!strcmp(localtag(a), "name"))
                        content(a, b.author, sizeof(b.author));
            } else if (!strcmp(tag, "summary") || !strcmp(tag, "content")) {
                if (!*b.summary)
                    content(c, b.summary, sizeof(b.summary));
            } else if (!strcmp(tag, "link")) {
                const char *href = catalog_attr(c, "href"), *type = catalog_attr(c, "type"),
                           *rel = catalog_attr(c, "rel");
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
    markup_free(root);
    return 1;
}
