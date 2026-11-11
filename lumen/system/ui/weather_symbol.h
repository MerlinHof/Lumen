/* Shared monochrome weather symbols. */
static void weather_symbol(int x, int y, int code, int size) {
    int c = x + size / 2, d = y + size / 2, t = size > 90 ? 3 : 2;
    if (code == -1) {
        aa_disc(c, d, size / 4, 0);
        aa_disc(c + size / 9, d - size / 10, size / 4, 255);
        return;
    }
    int sun = code <= 2;
    if (sun) {
        int sx = code == 2 ? x + size / 3 : c, sy = code == 2 ? y + size / 3 : d;
        ring(sx, sy, size / 7, t);
        for (int i = 0; i < 8; i++) {
            float a = i * 3.14159265f / 4;
            stroke(sx + cosf(a) * size / 4, sy + sinf(a) * size / 4, sx + cosf(a) * size / 3,
                   sy + sinf(a) * size / 3, t);
        }
    }
    if (code >= 2) {
        /* Single outline union, no seams between cloud lobes. */
        for (int yy = y; yy <= y + size; yy++)
            for (int xx = x; xx <= x + size; xx++) {
                float a = hypotf(xx - (x + size * .34f), yy - (y + size * .54f)) - size * .15f;
                float b = hypotf(xx - (x + size * .52f), yy - (y + size * .43f)) - size * .21f;
                float e = hypotf(xx - (x + size * .73f), yy - (y + size * .56f)) - size * .14f;
                float base = (hypotf((xx - (x + size * .54f)) / (size * .30f),
                                     (yy - (y + size * .58f)) / (size * .14f)) - 1) * size * .14f;
                float dist = fminf(fminf(a, b), fminf(e, base));
                if (dist <= .5f)
                    blend_pixel(xx, yy, .5f - dist, 255);
                if (dist >= -t)
                    blend_pixel(xx, yy, fminf(.5f - dist, dist + t + .5f), 0);
            }
    }
    if (code == 45 || code == 48) {
        stroke(x + size / 4, y + size * 4 / 5, x + size * 4 / 5, y + size * 4 / 5, t);
        stroke(x + size / 3, y + size * 9 / 10, x + size * 2 / 3, y + size * 9 / 10, t);
    } else if (code >= 95) {
        stroke(c, y + size * 3 / 4, c - size / 10, y + size * 7 / 8, t + 1);
        stroke(c - size / 10, y + size * 7 / 8, c + size / 10, y + size * 7 / 8, t + 1);
        stroke(c + size / 10, y + size * 7 / 8, c, y + size, t + 1);
    } else if (code >= 50) {
        for (int i = 0; i < 3; i++) {
            int px = x + size / 3 + i * size / 6, py = y + size * 4 / 5;
            if ((code >= 70 && code <= 79) || code == 85 || code == 86) {
                stroke(px - 3, py, px + 3, py, t);
                stroke(px, py - 3, px, py + 3, t);
            } else
                stroke(px, py, px - size / 25, py + size / 12, t);
        }
    }
}
