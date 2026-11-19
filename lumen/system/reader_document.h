#include "../apps/news/api.h"
/* Shared article-to-reader boundary. Documents live outside the personal library. */
static void document_escape(FILE *f, const char *s, size_t n) {
    for (size_t i = 0; i < n; i++)
        switch (s[i]) {
        case '&':
            fputs("&amp;", f);
            break;
        case '<':
            fputs("&lt;", f);
            break;
        case '>':
            fputs("&gt;", f);
            break;
        case '"':
            fputs("&quot;", f);
            break;
        default:
            fputc((unsigned char)s[i], f);
        }
}
static int document_open(const char *title, const char *author, const char *source,
                         const char *text, const char *image, int origin) {
    if (!text || !*text)
        return 0;
    char dir[2048], path[2048];
    snprintf(dir, sizeof(dir), "%s/reader-documents", data_root);
    mkdir(dir, 0700);
    uint64_t id = press_id(source && *source ? source : title);
    snprintf(path, sizeof(path), "%s/%016llx.html", dir, (unsigned long long)id);
    FILE *f = fopen(path, "w");
    if (!f)
        return 0;
    fputs("<!doctype html><html><head><meta charset=\"utf-8\"><title>", f);
    document_escape(f, title, strlen(title));
    fputs("</title><style>h1{font-size:1.65em;line-height:1.15;margin:0 0 "
          ".65em}h2{font-size:1.3em;margin:1.2em 0 .5em}h3{font-size:1.1em;margin:1em 0 "
          ".4em}img{max-width:100%;height:auto}figure{margin:1em 0}p{margin:0 0 "
          ".6em}</style></head><body><h1>",
          f);
    document_escape(f, title, strlen(title));
    fputs("</h1><p>", f);
    document_escape(f, author, strlen(author));
    fputs("</p>", f);
    char local_image[2048], image_name[80];
    snprintf(image_name, sizeof(image_name), "%016llx.pgm", (unsigned long long)id);
    snprintf(local_image, sizeof(local_image), "%s/%s", dir, image_name);
    if (image && *image && package_copy(image, local_image)) {
        image = image_name;
        fputs("<figure><img src=\"", f);
        document_escape(f, image, strlen(image));
        fputs("\"></figure>", f);
    }
    const char *p = text;
    while (*p) {
        const char *end = strchr(p, '\n');
        if (!end)
            end = p + strlen(p);
        const char *a = p, *b = end;
        while (a < b && (*a == ' ' || *a == '\r'))
            a++;
        while (b > a && (b[-1] == ' ' || b[-1] == '\r'))
            b--;
        const char *tag = "p";
        if (b > a) {
            fprintf(f, "<%s>", tag);
            document_escape(f, a, b - a);
            fprintf(f, "</%s>\n", tag);
        }
        p = *end ? end + 1 : end;
    }
    if (source && *source) {
        fputs("<p><a href=\"", f);
        document_escape(f, source, strlen(source));
        fputs("\">", f);
        document_escape(f, title, strlen(title));
        fputs("</a></p>", f);
    }
    fputs("</body></html>", f);
    int ok = !ferror(f);
    if (fclose(f))
        ok = 0;
    if (!ok)
        return 0;
    if (current >= 0) {
        library_progress(&library[current], page, engine_count());
        save_anchor(&library[current], page, -1);
    }
    engine_close();
    current = selected = -1;
    Book *b = &library[BOOK_LIMIT];
    memset(b, 0, sizeof(*b));
    b->id = id;
    snprintf(b->path, sizeof(b->path), "%s", path);
    struct stat document_stat;
    if (!stat(path,&document_stat)) b->size=document_stat.st_size;
    snprintf(b->title, sizeof(b->title), "%s", title);
    snprintf(b->author, sizeof(b->author), "%s", author);
    snprintf(b->format, sizeof(b->format), "html");
    library_restore_state(b);
    reader_ui.reader_document_active = 1;

    if (history_count < 64 && (!history_count || history[history_count - 1] != origin))
        history[history_count++] = origin;
    if (!ui_open(BOOK_LIMIT)) {
        reader_ui.reader_document_active = 0;
        return 0;
    }
    return 1;
}
static int article_news_open(void) {
    char image[2048];
    press_image_path(image, sizeof(image));
    return document_open(press_article.title, press_article.publisher, press_article.url,
                         press_article.body, image, active_app());
}
