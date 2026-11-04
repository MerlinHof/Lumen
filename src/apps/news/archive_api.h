#ifndef LUMEN_NEWSPAPER_H
#define LUMEN_NEWSPAPER_H
#include <stdint.h>
typedef struct {
    int category;
    char title[256], body[2300], url[1024], publisher[80], published[80], image[128];
} NewsArticle;
typedef struct {
    uint32_t magic, version, date, topics;
    int count;
    NewsArticle article[10];
} Newspaper;
extern Newspaper newspaper;
int news_date(void);
int news_load(int date);
int news_start(int language, unsigned topics);
int news_poll(void);
int news_busy(void);
void news_cancel(void);
#endif
