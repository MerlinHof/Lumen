#ifndef LUMEN_CONTRACTS_H
#define LUMEN_CONTRACTS_H
#include <stdint.h>
typedef uint64_t BookId;
typedef struct {
    int chapter, page, chapter_pages;
    char quote[128];
} ReaderPosition;
#endif
