/* Shared 8x8 coverage, computed once per radius and curve. */
enum { CORNER_SQUARE, CORNER_CIRCLE, CORNER_SUPERELLIPSE };
enum { MAX_CORNER_RADIUS = 32, CORNER_SAMPLES = 8, CORNER_ALPHA = 64 };
#ifdef LUMEN_HOST
static int control_corner_profile = CORNER_SUPERELLIPSE;
#else
enum { control_corner_profile = CORNER_SUPERELLIPSE };
#endif
static unsigned char corner_masks[2][MAX_CORNER_RADIUS + 1][MAX_CORNER_RADIUS * MAX_CORNER_RADIUS];
static unsigned char corner_ready[2][MAX_CORNER_RADIUS + 1];

static const unsigned char *corner_mask(int r, int profile) {
    unsigned char *mask = corner_masks[profile - 1][r];
    if (corner_ready[profile - 1][r]) return mask;
    unsigned radius = 2 * CORNER_SAMPLES * (unsigned)r;
    unsigned radius2 = radius * radius;
    unsigned limit = profile == CORNER_CIRCLE ? radius2 : radius2 * radius;
    for (int y = 0; y < r; y++) {
        for (int x = 0; x < r; x++) {
            int coverage = 0;
            for (int sy = 1; sy < 2 * CORNER_SAMPLES; sy += 2) {
                for (int sx = 1; sx < 2 * CORNER_SAMPLES; sx += 2) {
                    unsigned dx = 2 * CORNER_SAMPLES * (r - x) - sx;
                    unsigned dy = 2 * CORNER_SAMPLES * (r - y) - sy;
                    unsigned xx = dx * dx, yy = dy * dy;
                    unsigned distance = profile == CORNER_CIRCLE ? xx + yy : xx * dx + yy * dy;
                    coverage += distance <= limit;
                }
            }
            mask[y*r + x] = (unsigned char)coverage;
        }
    }
    corner_ready[profile - 1][r] = 1;
    return mask;
}
static int rounding_profile(void) {
    return options[OPT_ROUNDING] ? control_corner_profile : CORNER_SQUARE;
}
static unsigned char corner_blend(int background, int foreground, int alpha) {
    return (unsigned char)((background * (CORNER_ALPHA - alpha) + foreground * alpha +
                            CORNER_ALPHA / 2) / CORNER_ALPHA);
}
static void paint_corner_pixel(int x, int y, int coverage, int color) {
    if ((unsigned)x >= WIDTH || (unsigned)y >= HEIGHT || !coverage) return;
    unsigned char *pixel = &ui_pixels[y*WIDTH+x];
    *pixel = corner_blend(*pixel, color, coverage);
}
static void rounded_surface(int x, int y, int w, int h, int r, int color, int sides) {
    int profile = rounding_profile();
    if (w <= 0 || h <= 0) return;
    if (r > MAX_CORNER_RADIUS) r = MAX_CORNER_RADIUS;
    if (r > w/2) r = w/2;
    if (r > h/2) r = h/2;
    if (r <= 0 || profile == CORNER_SQUARE || !sides) {
        rect(x, y, w, h, color);
        return;
    }
    /* A fully rounded end is a capsule. Other corners use the softer n=3
       superellipse. Both are rendered through the same mask and blend. */
    if (r * 2 >= (w < h ? w : h) - 1) profile = CORNER_CIRCLE;
    const unsigned char *mask = corner_mask(r, profile);
    int left = sides & 1 ? r : 0, right = sides & 2 ? r : 0;
    rect(x+left, y, w-left-right, h, color);
    if (left) rect(x, y+r, r, h-2*r, color);
    if (right) rect(x+w-r, y+r, r, h-2*r, color);
    y += ui_y_offset;
    color = theme_gray(color);
    for (int j = 0; j < r; j++) {
        for (int i = 0; i < r; i++) {
            int alpha = mask[j*r+i];
            if (left) {
                paint_corner_pixel(x+i, y+j, alpha, color);
                paint_corner_pixel(x+i, y+h-1-j, alpha, color);
            }
            if (right) {
                paint_corner_pixel(x+w-1-i, y+j, alpha, color);
                paint_corner_pixel(x+w-1-i, y+h-1-j, alpha, color);
            }
        }
    }
}
static void control_surface(int x, int y, int w, int h, int r, int color) {
    rounded_surface(x, y, w, h, r, color, 3);
}
static void segment_fill(int x, int y, int w, int h, int r, int color, int sides) {
    rounded_surface(x, y, w, h, r, color, sides);
}

/* Preserve the background under the shared mask when painting image pixels. */
static unsigned char clip_background[4][81];
static void rounded_clip_end(int x, int y, int w, int h) {
    int profile = rounding_profile();
    if (!profile) return;
    const unsigned char *mask = corner_mask(9, profile);
    for (int j = 0; j < 9; j++)
        for (int i = 0; i < 9; i++) {
            int xx[4] = {x + i, x + w - 1 - i, x + i, x + w - 1 - i},
                yy[4] = {y + j, y + j, y + h - 1 - j, y + h - 1 - j};
            int alpha = mask[j * 9 + i];
            for (int c = 0; c < 4; c++)
                if (xx[c] >= 0 && xx[c] < WIDTH && yy[c] >= 0 && yy[c] < HEIGHT) {
                    int k = yy[c] * WIDTH + xx[c], v = ui_pixels[k];
                    ui_pixels[k] =
                        corner_blend(clip_background[c][j * 9 + i], v, alpha);
                }
        }
}
static void rounded_clip_begin(int x, int y, int w, int h) {
    if (!rounding_profile()) return;
    for (int j = 0; j < 9; j++)
        for (int i = 0; i < 9; i++) {
            int xx[4] = {x + i, x + w - 1 - i, x + i, x + w - 1 - i},
                yy[4] = {y + j, y + j, y + h - 1 - j, y + h - 1 - j};
            for (int c = 0; c < 4; c++)
                clip_background[c][j * 9 + i] =
                    (xx[c] >= 0 && xx[c] < WIDTH && yy[c] >= 0 && yy[c] < HEIGHT)
                        ? ui_pixels[yy[c] * WIDTH + xx[c]]
                        : 255;
        }
}
