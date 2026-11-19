#ifndef LUMEN_PRESS_H
#define LUMEN_PRESS_H
#include <stdint.h>
#define PRESS_LIMIT 96
typedef struct {
    char title[256], url[2048], summary[1600], publisher[96], category[48], published[96];
    int rank;
} PressItem;
typedef struct {
    unsigned magic, date;
    int count;
    char query[256];
    PressItem item[PRESS_LIMIT];
} PressList;
typedef struct {
    unsigned magic;
    int complete;
    char title[256], url[2048], publisher[96], body[131072], image[2048], caption[512];
} PressArticle;
#endif
