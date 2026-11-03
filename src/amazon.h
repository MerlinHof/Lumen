#ifndef LUMEN_AMAZON_H
#define LUMEN_AMAZON_H
#include "library.h"
int amazon_metadata(const char *path, char *title, size_t nt, char *author, size_t na,
                    int *encrypted);
int amazon_prepare(const Book *book, char *path, size_t size);
int amazon_reader_request(const char *path);
#endif
