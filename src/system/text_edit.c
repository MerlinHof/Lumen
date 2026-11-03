#include "text_edit.h"
#include <string.h>
size_t text_boundary(const char *text, size_t offset) {
    size_t n = strlen(text);
    if (offset > n)
        offset = n;
    while (offset && ((unsigned char)text[offset] & 192) == 128)
        offset--;
    return offset;
}
int text_insert(char *text, size_t capacity, int *cursor, const char *value) {
    size_t n = strlen(text), add = strlen(value);
    if (!capacity || n >= capacity || add >= capacity - n)
        return 0;
    size_t at = text_boundary(text, *cursor < 0 ? n : (size_t)*cursor);
    memmove(text + at + add, text + at, n - at + 1);
    memcpy(text + at, value, add);
    *cursor = (int)(at + add);
    return 1;
}
void text_erase(char *text, int *cursor, size_t start, size_t end) {
    start = text_boundary(text, start);
    end = text_boundary(text, end);
    if (end < start)
        return;
    memmove(text + start, text + end, strlen(text + end) + 1);
    *cursor = (int)start;
}
int text_backspace(char *text, int *cursor) {
    size_t at = text_boundary(text, *cursor < 0 ? 0 : (size_t)*cursor);
    if (!at)
        return 0;
    text_erase(text, cursor, text_boundary(text, at - 1), at);
    return 1;
}
