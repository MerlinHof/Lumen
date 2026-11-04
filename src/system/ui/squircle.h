/* Continuous superellipse curvature, cached at the two launcher tile sizes. */
static unsigned char squircle_masks[2][78 * 78];
static int squircle_ready[2];
static void squircle(int x, int y, int size, int color) {
    int slot = size == 78 ? 0 : 1;
    if (size != 78 && size != 74)
        return;
    if (!squircle_ready[slot]) {
        float half = size / 2.f;
        for (int j = 0; j < size; j++)
            for (int i = 0; i < size; i++) {
                int count = 0;
                for (int sy = 0; sy < 4; sy++)
                    for (int sx = 0; sx < 4; sx++) {
                        float a = (i + (sx + .5f) / 4 - half) / half,
                              b = (j + (sy + .5f) / 4 - half) / half;
                        count += a * a * a * a + b * b * b * b <= 1;
                    }
                squircle_masks[slot][j * size + i] = count;
            }
        squircle_ready[slot] = 1;
    }
    for (int j = 0; j < size; j++)
        for (int i = 0; i < size; i++) {
            int a = squircle_masks[slot][j * size + i];
            if (a == 16)
                rect(x + i, y + j, 1, 1, color);
            else if (a)
                blend_pixel(x + i, y + j, a / 16.f, color);
        }
}
