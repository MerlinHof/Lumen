/* Shared fixed-point resampling and 16-grey-level image presentation. */
static int image_bilinear(const unsigned char *pixels, int stride, int w, int h, int u, int v) {
    if (u < 0)
        u = 0;
    if (v < 0)
        v = 0;
    if (u > (w - 1) * 256)
        u = (w - 1) * 256;
    if (v > (h - 1) * 256)
        v = (h - 1) * 256;
    int x = u >> 8, y = v >> 8, x1 = x + 1 < w ? x + 1 : x, y1 = y + 1 < h ? y + 1 : y,
        fx = u & 255, fy = v & 255;
    int a = pixels[y * stride + x] * (256 - fx) + pixels[y * stride + x1] * fx;
    int b = pixels[y1 * stride + x] * (256 - fx) + pixels[y1 * stride + x1] * fx;
    return (a * (256 - fy) + b * fy + 32768) >> 16;
}
static unsigned char image_tone(int value, int x, int y) {
    static const unsigned char bayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    if (value <= 0)
        return 0;
    if (value >= 255)
        return 255;
    int low = value / 17, rem = value - low * 17;
    return (low + (rem * 16 > bayer[(y & 3) * 4 + (x & 3)] * 17 + 8)) * 17;
}

static int photo_contrast(int value) {
    int adjusted=128+(value-128)*118/100;
    return adjusted<0?0:adjusted>255?255:adjusted;
}
