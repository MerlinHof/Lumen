static void elements_init16(void) {
    if (element_grid16[0][0])
        return;
    element_grid16[0][0] = 1;
    element_grid16[0][17] = 2;
    for (int p = 1; p < 7; p++) {
        int start = p == 1 ? 3 : p == 2 ? 11 : p == 3 ? 19 : p == 4 ? 37 : p == 5 ? 55 : 87;
        for (int c = 0; c < 18; c++) {
            if (p < 3 && c >= 2 && c < 12)
                continue;
            int n = start++;
            if (p >= 5 && c == 2) {
                n = p == 5 ? 57 : 89;
                start = p == 5 ? 72 : 104;
            }
            element_grid16[p][c] = n;
        }
    }
    for (int i = 0; i < 14; i++) {
        element_grid16[7][i + 2] = 58 + i;
        element_grid16[8][i + 2] = 90 + i;
    }
}
static const char *element_name(int n) {
    return tr(n == 80 ? "Quecksilber" : elements16[n-1][1]);
}
static char crossword_answer16[5][16];
