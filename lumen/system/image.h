#ifndef LUMEN_IMAGE_H
#define LUMEN_IMAGE_H
#include <stddef.h>
static inline int image_contrast(int value) {
    int adjusted=128+(value-128)*118/100;
    return adjusted<0?0:adjusted>255?255:adjusted;
}

static inline void image_contrast_pixels(unsigned char *pixels, size_t size) {
    for (size_t i = 0; i < size; i++)
        pixels[i] = (unsigned char)image_contrast(pixels[i]);
}
#endif
