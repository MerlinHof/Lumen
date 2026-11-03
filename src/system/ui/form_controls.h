/* Shared form controls: apps use these rather than drawing their own. */
static void select_row17(int y, const char *label, const char *value) {
    drawtext(30, y + 9, label, &small, 260);
    dropdown_button(300, y, 272, 48, value);
}
static void checkbox(int x, int y, int checked) {
    region(x - 9, y - 9, 48, 48);
    key_outline(x, y, 30, 30);
    if (checked) {
        stroke(x + 6, y + 15, x + 12, y + 21, 3);
        stroke(x + 12, y + 21, x + 24, y + 8, 3);
    }
}

static void small_number17(int right, int y, const char *number, int color) {
    static const unsigned char digits[10][7] = {
        {14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14},  {14, 17, 1, 2, 4, 8, 31},
        {30, 1, 1, 14, 1, 1, 30},     {2, 6, 10, 18, 31, 2, 2}, {31, 16, 16, 30, 1, 1, 30},
        {14, 16, 16, 30, 17, 17, 14}, {31, 1, 2, 4, 8, 8, 8},   {14, 17, 17, 14, 17, 17, 14},
        {14, 17, 17, 15, 1, 1, 14}};
    int x = right - (int)strlen(number) * 9;
    for (; *number; number++, x += 9) {
        if (*number < '0' || *number > '9')
            continue;
        for (int yy = 0; yy < 7; yy++)
            for (int xx = 0; xx < 5; xx++)
                if (digits[*number - '0'][yy] & (1 << (4 - xx)))
                    rect(x + xx, y + yy * 2, 1, 2, color);
    }
}
