#define _GNU_SOURCE
#include "archive_api.h"
#include "../../system/library.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
Newspaper newspaper;
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
