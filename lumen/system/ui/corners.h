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

/* Paint only the perimeter so callers can draw content in either order. */
static void outline_surface(int x, int y, int w, int h, int r, int color) {
    int profile = rounding_profile();
    if (r > w/2) r = w/2;
    if (r > h/2) r = h/2;
    if (!profile || r < 3) {
        rect(x,y,w,2,color);
        rect(x,y+h-2,w,2,color);
        rect(x,y+2,2,h-4,color);
        rect(x+w-2,y+2,2,h-4,color);
        return;
    }
    if (r*2 >= (w < h ? w : h)-1) profile = CORNER_CIRCLE;
    const unsigned char *outer = corner_mask(r, profile);
    const unsigned char *inner = corner_mask(r-2, profile);
    rect(x+r,y,w-2*r,2,color);
    rect(x+r,y+h-2,w-2*r,2,color);
    rect(x,y+r,2,h-2*r,color);
    rect(x+w-2,y+r,2,h-2*r,color);
    y += ui_y_offset;
    color = theme_gray(color);
    for (int j=0; j<r; j++)
        for (int i=0; i<r; i++) {
            int alpha = outer[j*r+i];
            if (i>=2 && j>=2) alpha -= inner[(j-2)*(r-2)+i-2];
            if (alpha <= 0) continue;
            paint_corner_pixel(x+i,y+j,alpha,color);
            paint_corner_pixel(x+w-1-i,y+j,alpha,color);
            paint_corner_pixel(x+i,y+h-1-j,alpha,color);
            paint_corner_pixel(x+w-1-i,y+h-1-j,alpha,color);
        }
}

/* Preserve the background under the shared mask when painting image pixels. */
static unsigned char clip_background[4][RADIUS_LARGE * RADIUS_LARGE];
static void rounded_clip_apply(int x, int y, int w, int h, int background) {
    int profile = rounding_profile();
    if (!profile || w<4 || h<4) return;
    int radius=h<2*RADIUS_LARGE ? RADIUS_MEDIUM : RADIUS_LARGE;
    if (radius>(w-1)/2) radius=(w-1)/2;
    if (radius>(h-1)/2) radius=(h-1)/2;
    const unsigned char *mask = corner_mask(radius, profile);
    for (int j = 0; j < radius; j++)
        for (int i = 0; i < radius; i++) {
            int xx[4] = {x + i, x + w - 1 - i, x + i, x + w - 1 - i},
                yy[4] = {y + j, y + j, y + h - 1 - j, y + h - 1 - j};
            int alpha = mask[j * radius + i];
            for (int c = 0; c < 4; c++)
                if (xx[c] >= 0 && xx[c] < WIDTH && yy[c] >= 0 && yy[c] < HEIGHT) {
                    int k = yy[c] * WIDTH + xx[c], v = ui_pixels[k];
                    ui_pixels[k] =
                        corner_blend(background < 0 ? clip_background[c][j * RADIUS_LARGE + i] : background, v, alpha);
                }
        }
}
static void rounded_clip_end(int x, int y, int w, int h) {
    rounded_clip_apply(x,y,w,h,-1);
}
static void rounded_clip_begin(int x, int y, int w, int h) {
    if (!rounding_profile() || w<4 || h<4) return;
    int radius=h<2*RADIUS_LARGE ? RADIUS_MEDIUM : RADIUS_LARGE;
    if (radius>(w-1)/2) radius=(w-1)/2;
    if (radius>(h-1)/2) radius=(h-1)/2;
    for (int j = 0; j < radius; j++)
        for (int i = 0; i < radius; i++) {
            int xx[4] = {x + i, x + w - 1 - i, x + i, x + w - 1 - i},
                yy[4] = {y + j, y + j, y + h - 1 - j, y + h - 1 - j};
            for (int c = 0; c < 4; c++)
                clip_background[c][j * RADIUS_LARGE + i] =
                    (xx[c] >= 0 && xx[c] < WIDTH && yy[c] >= 0 && yy[c] < HEIGHT)
                        ? ui_pixels[yy[c] * WIDTH + xx[c]]
                        : 255;
        }
}
