#ifndef LUMEN_TEXT_EDIT_H
#define LUMEN_TEXT_EDIT_H
#include <stddef.h>
/* Byte offsets always point to UTF-8 boundaries. Caller owns storage. */
size_t text_boundary(const char *text, size_t offset);
int text_insert(char *text, size_t capacity, int *cursor, const char *value);
int text_backspace(char *text, int *cursor);
void text_erase(char *text, int *cursor, size_t start, size_t end);
#endif
