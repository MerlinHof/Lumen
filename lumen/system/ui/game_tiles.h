/* Small, deterministic monochrome game artwork. Rasterized once per cell size;
 * drawing a board is only tile copies, independent of the number of moves. */
static unsigned char paper_sources17[10][32 * 32], paper_tiles17[10][40 * 40];
static int paper_ready17, paper_size17;
static void paper_sources_init17(void) {
    if (paper_ready17)
        return;
    paper_ready17 = 1;
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++) {
            int at = y * 32 + x, floor = 255;
            for (int t = 0; t < 10; t++)
                paper_sources17[t][at] = floor;
            int row = y / 8, xx = (x + (row % 2) * 8) % 16, yy = y % 8,
                brick = 142 + (row % 2) * 12;
            if (yy >= 6 || xx >= 14)
                brick = 62;
            else if (!yy)
                brick = 202;
            else if (!xx)
                brick = 188;
            else if (yy == 5)
                brick = 102;
            else if ((x * 7 + y * 11) % 29 == 0)
                brick += 14;
            paper_sources17[1][at] = brick;
            int dx = x - 16, dy = y - 16, d = dx * dx + dy * dy;
            if ((d >= 29 && d <= 52) || (abs(dx) <= 1 && abs(dy) <= 1))
                paper_sources17[2][at] = 105;
            for (int t = 3; t <= 5; t += 2) {
                int color = floor;
                if (y >= 28 && y <= 30 && x >= 6 && x <= 29)
                    color = 172;
                if (x >= 4 && x <= 27 && y >= 3 && y <= 27) {
                    color = 35;
                    if (x >= 6 && x <= 25 && y >= 5 && y <= 25) {
                        color = t == 5 ? 145 : 192;
                        if (x == 11 || x == 18)
                            color = t == 5 ? 88 : 132;
                        if ((x * 5 + y * 3) % 43 == 0)
                            color -= 15;
                        if (abs(x - y) <= 1)
                            color = 112;
                        if (y <= 7)
                            color = 221;
                        if (y >= 23)
                            color = 114;
                        if (x <= 8)
                            color = 208;
                        if (x >= 23)
                            color = 108;
                    }
                    if ((x == 8 || x == 23) && (y == 7 || y == 24))
                        color = 5;
                    if ((x == 9 || x == 24) && (y == 7 || y == 24))
                        color = 236;
                }
                paper_sources17[t][at] = color;
            }
            for (int t = 4; t <= 8; t++)
                if (t != 5)
                    paper_sources17[t][at] = 255;
            if (y >= 27 && y <= 29 && ((x - 16) * (x - 16)) / 9 + (y - 28) * (y - 28) <= 6)
                for (int t = 4; t <= 8; t++)
                    if (t != 5)
                        paper_sources17[t][at] = 180;
            int door = floor;
            int arch = (y >= 11 && x >= 7 && x <= 25) ||
                       (y < 11 && y >= 3 && (x - 16) * (x - 16) + (y - 11) * (y - 11) <= 81);
            if (arch) {
                door = 58;
                if (x >= 10 && x <= 22 && y >= 8) {
                    door = 167;
                    if (x % 5 == 0)
                        door = 111;
                    if (x >= 21)
                        door = 96;
                }
                if (x >= 12 && x <= 19 && y >= 14 && y <= 16)
                    door = 239;
                if (x >= 16 && x <= 21 && abs(y - 15) <= 21 - x)
                    door = 239;
                if (x == 20 && y == 22)
                    door = 12;
            }
            paper_sources17[9][at] = door;
        }
    const char *person[] = {
        "      XXXX      ", "     XhhhhX     ", "    XhhhhhhX    ", "   XhhhhhhhhX   ",
        "   XXXXXXXXXXX  ", "    XssssssX    ", "    XsesessX    ", "    XssssssX    ",
        "     XssssX     ", "   XXXsXXsXXX   ", "  XttooooottX   ", " Xtttooooott tX ",
        " Xsttoooo ottsX ", " XsstooooootssX ", "  XsXooooooXsX  ", "    XooooooX    ",
        "    XooooooX    ", "    XooXXooX    ", "    XooXXooX    ", "    XooXXooX    ",
        "   XXbbXXbbXX   ", "   XbbbXXbbbX   ", "   XXXXX XXXXX  ", "                "};
    for (int y = 0; y < 24; y++)
        for (int x = 0; x < 16 && x < (int)strlen(person[y]); x++) {
            char c = person[y][x];
            if (c == ' ')
                continue;
            int color = c == 'X'   ? 15
                        : c == 'h' ? 75
                        : c == 's' ? 226
                        : c == 'e' ? 0
                        : c == 't' ? 168
                        : c == 'o' ? 86
                                   : 28;
            int at = (y + 3) * 32 + x + 8;
            paper_sources17[4][at] = color;
            paper_sources17[6][at] = (y >= 5 && y <= 8) ? (c == 'X' ? 15 : 65) : color;
            paper_sources17[7][at] = (c == 'e' && x > 7) ? 226 : color;
            paper_sources17[8][(y + 3) * 32 + 23 - x] = (c == 'e' && x > 7) ? 226 : color;
        }
    /* A placed crate gets an unmistakable inset check, not just a shade change. */
    for (int k = 0; k < 4; k++)
        for (int thick = 0; thick < 2; thick++)
            paper_sources17[5][(16 + k) * 32 + 11 + k + thick] = 243;
    for (int k = 0; k < 7; k++)
        for (int thick = 0; thick < 2; thick++)
            paper_sources17[5][(19 - k) * 32 + 14 + k + thick] = 243;
}
static void paper_tile17(int x, int y, int size, int kind) {
    if (kind < 0 || kind > 9 || size < 1 || size > 40)
        return;
    paper_sources_init17();
    if (paper_size17 != size) {
        for (int t = 0; t < 10; t++)
            for (int yy = 0; yy < size; yy++)
                for (int xx = 0; xx < size; xx++)
                    paper_tiles17[t][yy * size + xx] =
                        paper_sources17[t][(yy * 32 / size) * 32 + xx * 32 / size];
        paper_size17 = size;
    }
    int transparent = kind == 4 || kind == 6 || kind == 7 || kind == 8;
    for (int yy = 0; yy < size; yy++) {
        int sy = y + yy;
        if (sy < 0 || sy >= HEIGHT)
            continue;
        for (int xx = 0; xx < size; xx++) {
            int sx = x + xx;
            if (sx < 0 || sx >= WIDTH)
                continue;
            unsigned char c = paper_tiles17[kind][yy * size + xx];
            if (!transparent || c != 255)
                ui_pixels[sy * WIDTH + sx] = c;
        }
    }
}
