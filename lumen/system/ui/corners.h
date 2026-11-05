/* Integer 4x4 coverage. Both profiles share the same warm raster path. */
enum { CORNER_SQUARE, CORNER_CIRCLE, CORNER_SUPERELLIPSE };
enum { MAX_CORNER_RADIUS = 32 };
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
    unsigned radius2 = (unsigned)(8*r) * (8*r);
    for (int y = 0; y < r; y++) {
        for (int x = 0; x < r; x++) {
            int coverage = 0;
            for (int sy = 1; sy <= 7; sy += 2) {
                for (int sx = 1; sx <= 7; sx += 2) {
                    unsigned dx = 8 * (r - x) - sx, dy = 8 * (r - y) - sy;
                    unsigned xx = dx * dx, yy = dy * dy;
                    coverage += profile == CORNER_CIRCLE ? xx + yy <= radius2 :
                                (uint64_t)xx * xx + (uint64_t)yy * yy <= (uint64_t)radius2 * radius2;
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
static float corner_inside(int r, int x, int y) {
    int profile = rounding_profile();
    if (r > MAX_CORNER_RADIUS) r = MAX_CORNER_RADIUS;
    if (profile == CORNER_SQUARE || r < 1 || x >= r || y >= r) return 1;
    return corner_mask(r, profile)[y*r+x] / 16.f;
}
static void paint_corner_pixel(int x, int y, int coverage, int color) {
    if ((unsigned)x >= WIDTH || (unsigned)y >= HEIGHT || !coverage) return;
    unsigned char *pixel = &ui_pixels[y*WIDTH+x];
    *pixel = (unsigned char)((*pixel * (16-coverage) + color * coverage) / 16);
}
static void rounded_surface(int x, int y, int w, int h, int r, int color, int sides) {
    int profile = rounding_profile();
    if (w <= 0 || h <= 0) return;
    if (r > MAX_CORNER_RADIUS) r = MAX_CORNER_RADIUS;
    if (r > w/2) r = w/2;
    if (r > h/2) r = h/2;
    if (!r || profile == CORNER_SQUARE || !sides) {
        rect(x, y, w, h, color);
        return;
    }
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
    if (!rounding_profile())
        return;
    for (int j = 0; j < 9; j++)
        for (int i = 0; i < 9; i++) {
            int xx[4] = {x + i, x + w - 1 - i, x + i, x + w - 1 - i},
                yy[4] = {y + j, y + j, y + h - 1 - j, y + h - 1 - j};
            int a = (int)((1 - corner_inside(9, i, j)) * 16);
            for (int c = 0; c < 4; c++)
                if (xx[c] >= 0 && xx[c] < WIDTH && yy[c] >= 0 && yy[c] < HEIGHT) {
                    int k = yy[c] * WIDTH + xx[c], v = ui_pixels[k];
                    ui_pixels[k] =
                        (unsigned char)((v * (16 - a) + clip_background[c][j * 9 + i] * a + 8) /
                                        16);
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
