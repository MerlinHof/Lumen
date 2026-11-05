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
static float corner_inside(int r, int x, int y) {
    if (r > MAX_CORNER_RADIUS) r = MAX_CORNER_RADIUS;
    if (r < 1 || x >= r || y >= r) return 1;
    return corner_mask(r, CORNER_CIRCLE)[y*r+x] / 16.f;
}
static void paint_corner_pixel(int x, int y, int coverage, int color) {
    if ((unsigned)x >= WIDTH || (unsigned)y >= HEIGHT || !coverage) return;
    unsigned char *pixel = &ui_pixels[y*WIDTH+x];
    *pixel = (unsigned char)((*pixel * (16-coverage) + color * coverage) / 16);
}
static void rounded_surface(int x, int y, int w, int h, int r, int color, int profile, int sides) {
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
static void aa_soft_rect(int x, int y, int w, int h, int r, int color) {
    rounded_surface(x, y, w, h, r, color, CORNER_CIRCLE, 3);
}
static void control_surface(int x, int y, int w, int h, int r, int color) {
    rounded_surface(x, y, w, h, r, color, control_corner_profile, 3);
}
static void segment_fill(int x, int y, int w, int h, int r, int color, int sides) {
    rounded_surface(x, y, w, h, r, color, control_corner_profile, sides);
}
