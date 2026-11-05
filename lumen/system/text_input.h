#ifndef LUMEN_TEXT_INPUT_H
#define LUMEN_TEXT_INPUT_H
#include <stddef.h>
enum { TEXT_INPUT_PHOTO_FOLDER = 240 };
typedef struct {
    size_t capacity;
    int multiline, secret, gutter;
} TextInputSpec;
/* Shared input contracts. Capacity includes the terminating zero byte. */
TextInputSpec text_input_spec(int target);
#endif
