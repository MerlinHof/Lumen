#ifndef LUMEN_CHESS_ICONS_H
#define LUMEN_CHESS_ICONS_H
/* Shared Staunton silhouettes and polygon rasterization. */
static void chess_polygon(int x, int y, const int *v, int n, int fill) {
    for (int yy = -24; yy <= 24; yy++) {
        int hits[24], m = 0;
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n, x1 = v[i * 2], y1 = v[i * 2 + 1], x2 = v[j * 2], y2 = v[j * 2 + 1];
            if ((y1 <= yy && y2 > yy) || (y2 <= yy && y1 > yy))
                hits[m++] = x1 + (yy - y1) * (x2 - x1) / (y2 - y1);
        }
        for (int i = 0; i < m; i++)
            for (int j = i + 1; j < m; j++)
                if (hits[j] < hits[i]) {
                    int t = hits[i];
                    hits[i] = hits[j];
                    hits[j] = t;
                }
        for (int i = 0; i + 1 < m; i += 2)
            rect(x + hits[i], y + yy, hits[i + 1] - hits[i] + 1, 1, fill);
    }
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        stroke(x + v[i * 2], y + v[i * 2 + 1], x + v[j * 2], y + v[j * 2 + 1], 2);
    }
}
static void chess_piece(int x, int y, int piece) {
    int c = x + 33, d = y + 32, k = abs(piece), fill = piece > 0 ? 255 : 0;
    static const int pawn[] = {-7, -3, 7, -3, 6, 6, 12, 15, -12, 15, -6, 6};
    static const int rook[] = {-15, -20, -8, -20, -8,  -13, -3,  -13, -3,  -20, 3,   -20,
                               3,   -13, 8,  -13, 8,   -20, 15,  -20, 15,  -7,  10,  -4,
                               10,  12,  15, 17,  -15, 17,  -10, 12,  -10, -4,  -15, -7};
    static const int knight[] = {-16, 17, 15,  17,  12, 6,   13, -8, 6,  -18, 3, -24, -2,
                                 -17, -9, -14, -18, -4, -14, 2,  -3, -1, -2,  5, -12, 10};
    static const int bishop[] = {0,  -23, 10, -12, 7,  -4, 4, 0,  7,  10,  13,
                                 16, -13, 16, -7,  10, -4, 0, -7, -4, -10, -12};
    static const int queen[] = {-17, -17, -12, 7, -7,  12, 7,   12, 12,
                                7,   17,  -17, 7, -10, 0,  -22, -7, -10};
    static const int king[] = {-11, -9, 11, -9, 13, 0, 7, 9, 13, 16, -13, 16, -7, 9, -13, 0};
    if (k == 1) {
        chess_polygon(c, d, pawn, sizeof(pawn) / sizeof(int) / 2, fill);
        disc(c, d - 12, 8, fill);
        ring(c, d - 12, 8, 2);
    }
    if (k == 2) {
        chess_polygon(c, d, knight, sizeof(knight) / sizeof(int) / 2, fill);
        disc(c + 3, d - 12, 2, piece > 0 ? 0 : 255);
    }
    if (k == 3) {
        chess_polygon(c, d, bishop, sizeof(bishop) / sizeof(int) / 2, fill);
        stroke(c + 3, d - 18, c - 3, d - 9, 2);
    }
    if (k == 4)
        chess_polygon(c, d, rook, sizeof(rook) / sizeof(int) / 2, fill);
    if (k == 5) {
        chess_polygon(c, d, queen, sizeof(queen) / sizeof(int) / 2, fill);
        for (int i = -1; i <= 1; i++) {
            disc(c + i * 17, d + (i ? -19 : -23), 3, fill);
            ring(c + i * 17, d + (i ? -19 : -23), 3, 1);
        }
    }
    if (k == 6) {
        chess_polygon(c, d, king, sizeof(king) / sizeof(int) / 2, fill);
        stroke(c, d - 24, c, d - 10, 4);
        stroke(c - 6, d - 19, c + 6, d - 19, 4);
    }
    rect(c - 16, d + 18, 33, 6, fill);
    stroke(c - 16, d + 18, c + 16, d + 18, 2);
    stroke(c - 16, d + 24, c + 16, d + 24, 2);
    stroke(c - 16, d + 18, c - 16, d + 24, 2);
    stroke(c + 16, d + 18, c + 16, d + 24, 2);
}

#endif
