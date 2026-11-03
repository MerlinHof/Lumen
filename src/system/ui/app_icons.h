#include "chess_icons.h"
/* Shared app identities, never defined in individual app views. */
static void extra_icon(int x, int y, int which) {
    if (which < 0)
        return;
    int c = x + 22, d = y + 22;
    switch (which) {
    case 17:
        aa_soft_rect(x + 4, y + 4, 36, 36, 5, 0);
        aa_soft_rect(x + 6, y + 6, 32, 32, 3, 255);
        centered(x, y, 44, 44, "2048", &caption);
        break;
    case 18:
        key_outline(x + 5, y + 5, 25, 34);
        key_outline(x + 14, y + 10, 25, 34);
        suit_icon(x + 26, y + 26, 3);
        break;
    case 19:
        for (int i = 0; i < 4; i++) {
            stroke(x + 5 + i * 11, y + 5, x + 5 + i * 11, y + 38, 2);
            stroke(x + 5, y + 5 + i * 11, x + 38, y + 5 + i * 11, 2);
        }
        break;
    case 20:
        icon(x, y, IC_TYPE);
        ring(x + 22, y + 22, 18, 2);
        break;
    case 21:
        key_outline(x + 4, y + 6, 36, 33);
        stroke(x + 9, y + 13, x + 35, y + 13, 3);
        for (int i = 0; i < 3; i++) {
            stroke(x + 9, y + 21 + i * 6, x + 19, y + 21 + i * 6, 2);
            stroke(x + 25, y + 21 + i * 6, x + 35, y + 21 + i * 6, 2);
        }
        break;

    case 16:
        aa_soft_rect(x + 6, y + 3, 32, 38, 4, 0);
        aa_soft_rect(x + 8, y + 5, 28, 34, 2, 255);
        stroke(x + 13, y + 13, x + 24, y + 13, 2);
        stroke(x + 13, y + 22, x + 24, y + 22, 2);
        stroke(x + 13, y + 31, x + 24, y + 31, 2);
        stroke(x + 29, y + 19, x + 33, y + 19, 2);
        stroke(x + 29, y + 24, x + 33, y + 24, 2);
        break;
    case 0:
        aa_soft_rect(x + 8, y + 3, 28, 38, 5, 0);
        aa_soft_rect(x + 10, y + 5, 24, 34, 3, 255);
        stroke(x + 14, y + 13, x + 30, y + 13, 3);
        for (int k = 0; k < 6; k++)
            disc(x + 15 + k % 3 * 7, y + 23 + k / 3 * 9, 2, 0);
        break;
    case 1:
        icon(x, y, IC_CLOCK);
        stroke(c - 8, d - 17, c + 8, d - 17, 2);
        break;
    case 2:
        aa_soft_rect(x + 8, y + 4, 29, 36, 4, 0);
        aa_soft_rect(x + 10, y + 6, 25, 32, 2, 255);
        for (int k = 0; k < 3; k++)
            stroke(x + 15, y + 15 + k * 8, x + 30, y + 15 + k * 8, 2);
        break;
    case 3:
        stroke(x + 11, y + 29, x + 29, y + 10, 2);
        stroke(x + 16, y + 34, x + 34, y + 15, 2);
        stroke(x + 29, y + 10, x + 34, y + 15, 2);
        stroke(x + 11, y + 29, x + 8, y + 37, 2);
        stroke(x + 8, y + 37, x + 16, y + 34, 2);
        stroke(x + 11, y + 29, x + 16, y + 34, 2);
        stroke(x + 26, y + 13, x + 31, y + 18, 2);
        stroke(x + 7, y + 40, x + 34, y + 40, 2);
        break;
    case 4:
        weather_symbol(x - 3, y - 3, 2, 50);
        break;
    case 5:
        stroke(c - 16, d - 1, c, d - 15, 2);
        stroke(c, d - 15, c + 16, d - 1, 2);
        stroke(c - 12, d - 2, c - 12, d + 15, 2);
        stroke(c - 12, d + 15, c + 12, d + 15, 2);
        stroke(c + 12, d + 15, c + 12, d - 2, 2);
        stroke(c, d + 12, c, d + 1, 2);
        stroke(c, d + 5, c - 7, d - 1, 2);
        stroke(c, d + 8, c + 7, d + 2, 2);
        disc(c, d - 2, 2, 0);
        disc(c - 8, d - 2, 2, 0);
        disc(c + 8, d + 1, 2, 0);
        break;
    case 6:
        aa_soft_rect(x + 6, y + 7, 32, 32, 5, 0);
        aa_soft_rect(x + 8, y + 9, 28, 28, 3, 255);
        stroke(x + 7, y + 16, x + 37, y + 16, 2);
        stroke(x + 14, y + 3, x + 14, y + 11, 3);
        stroke(x + 30, y + 3, x + 30, y + 11, 3);
        for (int k = 0; k < 6; k++)
            disc(x + 14 + k % 3 * 8, y + 24 + k / 3 * 8, 1, 0);
        break;
    case 7:
        stroke(c - 14, d + 14, c + 15, d + 14, 2);
        stroke(c - 13, d + 7, c - 5, d - 1, 3);
        stroke(c - 5, d - 1, c + 3, d + 3, 3);
        stroke(c + 3, d + 3, c + 14, d - 12, 3);
        stroke(c + 14, d - 12, c + 14, d - 4, 2);
        stroke(c + 14, d - 12, c + 6, d - 12, 2);
        break;
    case 8:
        for (int k = 0; k < 2; k++) {
            int xx = x + 7 + k * 18;
            aa_soft_rect(xx, y + 10, 12, 13, 3, 0);
            stroke(xx + 10, y + 21, xx + 8, y + 29, 3);
            stroke(xx + 8, y + 29, xx + 2, y + 33, 3);
        }
        break;
    case 9:
        ring(c, d, 16, 2);
        arc(c, d, 16, 0, 180, 2);
        stroke(c - 16, d, c + 16, d, 2);
        arc(c - 6, d, 10, 285, 435, 2);
        arc(c + 6, d, 10, 105, 255, 2);
        break;
    case 10:
        ring(c, d, 10, 3);
        for (int k = 0; k < 8; k++) {
            float a = k * 3.14159265f / 4;
            stroke(c + cosf(a) * 14, d + sinf(a) * 14, c + cosf(a) * 18, d + sinf(a) * 18, 2);
        }
        disc(c - 3, d - 3, 2, 0);
        break;
    case 11:
        stroke(c - 13, d + 15, c - 13, d - 15, 2);
        stroke(c - 13, d - 15, c + 9, d - 15, 2);
        stroke(c + 9, d - 15, c + 9, d - 8, 2);
        ring(c + 9, d - 3, 5, 2);
        stroke(c + 9, d + 2, c + 9, d + 12, 2);
        stroke(c - 17, d + 15, c - 7, d + 15, 2);
        break;
    case 12:
        for (int k = 0; k < 4; k++) {
            int xx = x + 9 + (k ? 12 : 0), yy = y + 6 + (k ? (k - 1) * 12 : 0);
            aa_soft_rect(xx, yy, 10, 10, 2, 0);
        }
        break;
    case 13:
        stroke(c - 13, d + 10, c - 13, d - 8, 8);
        stroke(c - 13, d - 8, c + 11, d - 8, 8);
        stroke(c + 11, d - 8, c + 11, d + 10, 8);
        stroke(c + 11, d + 10, c - 2, d + 10, 8);
        disc(c - 13, d - 8, 4, 255);
        disc(c - 13, d - 8, 2, 0);
        break;
    case 14:
        stroke(c - 13, d + 14, c + 13, d + 14, 3);
        stroke(c - 7, d + 10, c + 6, d - 3, 9);
        stroke(c + 6, d - 3, c + 1, d - 14, 8);
        stroke(c + 1, d - 14, c - 12, d - 5, 7);
        disc(c, d - 10, 2, 255);
        break;
    case 15:
        stroke(c - 5, d - 15, c - 5, d + 15, 2);
        stroke(c + 6, d - 15, c + 6, d + 15, 2);
        stroke(c - 15, d - 5, c + 15, d - 5, 2);
        stroke(c - 15, d + 6, c + 15, d + 6, 2);
        stroke(c - 13, d - 13, c - 8, d - 8, 2);
        stroke(c - 13, d - 8, c - 8, d - 13, 2);
        ring(c + 11, d + 11, 4, 2);
        break;
    }
}

static void extra_wiki_cache(void) {
    char *xml = service_read("opds-feed.xml", 2097152);
    if (xml) {
        extra_write_text("wiki-last.xml", xml);
        free(xml);
    }
}

static void v1_icon(int x, int y, int app) {
    /* Distinct, solid silhouettes, antialiased by shared drawing primitives. */

    if (app >= APP_JOURNAL) {
        u16_icon(x, y, app);
        return;
    }
    if (app == APP_CALCULATOR) {
        aa_soft_rect(x + 5, y + 1, 34, 43, 5, 0);
        rect(x + 10, y + 6, 24, 10, 255);
        for (int i = 0; i < 9; i++)
            disc(x + 12 + (i % 3) * 10, y + 23 + (i / 3) * 7, 2, 255);
        return;
    }
    if (app == APP_CALENDAR) {
        aa_soft_rect(x + 2, y + 7, 40, 35, 5, 0);
        rect(x + 6, y + 18, 32, 20, 255);
        for (int i = 0; i < 6; i++)
            disc(x + 12 + (i % 3) * 10, y + 24 + (i / 3) * 9, 2, 0);
        stroke(x + 12, y + 2, x + 12, y + 11, 4);
        stroke(x + 32, y + 2, x + 32, y + 11, 4);
        return;
    }
    if (app == APP_INSIGHTS) {
        for (int i = 0; i < 3; i++)
            aa_soft_rect(x + 4 + i * 13, y + 28 - i * 9, 8, 15 + i * 9, 2, 0);
        stroke(x + 1, y + 19, x + 15, y + 8, 3);
        stroke(x + 15, y + 8, x + 23, y + 10, 3);
        stroke(x + 23, y + 10, x + 39, y + 1, 3);
        return;
    }
    if (app == APP_NOTES) {
        aa_soft_rect(x + 6, y + 1, 32, 42, 4, 0);
        for (int i = 0; i < 4; i++)
            rect(x + 12, y + 11 + i * 7, i == 3 ? 13 : 20, 2, 255);
        return;
    }
    if (app == APP_TIMER) {
        ring(x + 22, y + 26, 16, 4);
        stroke(x + 22, y + 26, x + 22, y + 15, 3);
        stroke(x + 22, y + 26, x + 30, y + 29, 3);
        stroke(x + 16, y + 3, x + 28, y + 3, 4);
        stroke(x + 22, y + 3, x + 22, y + 9, 4);
        stroke(x + 35, y + 11, x + 40, y + 7, 3);
        return;
    }
    if (app == APP_WEATHER) {
        aa_disc(x + 14, y + 14, 8, 0);
        for (int a = 0; a < 360; a += 45) {
            double t = a * .01745329252;
            stroke(x + 14 + 12 * cos(t), y + 14 + 12 * sin(t), x + 14 + 15 * cos(t),
                   y + 14 + 15 * sin(t), 3);
        }
        aa_disc(x + 16, y + 29, 11, 255);
        aa_disc(x + 28, y + 25, 13, 255);
        aa_disc(x + 37, y + 31, 9, 255);
        aa_disc(x + 16, y + 29, 8, 0);
        aa_disc(x + 28, y + 25, 10, 0);
        aa_disc(x + 37, y + 31, 6, 0);
        rect(x + 13, y + 29, 25, 10, 0);
        return;
    }
    if (app == APP_BLOCKS) {
        for (int i = 0; i < 4; i++) {
            int xx = x + 2 + (i < 3 ? i * 14 : 14), yy = y + (i < 3 ? 8 : 22);
            aa_soft_rect(xx, yy, 12, 12, 2, 0);
        }
        return;
    }
    if (app == APP_SNAKE) {
        stroke(x + 5, y + 8, x + 35, y + 8, 9);
        stroke(x + 35, y + 8, x + 35, y + 24, 9);
        stroke(x + 35, y + 24, x + 14, y + 24, 9);
        stroke(x + 14, y + 24, x + 14, y + 38, 9);
        disc(x + 14, y + 38, 6, 0);
        disc(x + 12, y + 39, 1, 255);
        disc(x + 17, y + 39, 1, 255);
        return;
    }
    if (app == APP_SUDOKU) {
        for (int i = 0; i < 4; i++) {
            stroke(x + 2 + i * 13, y + 2, x + 2 + i * 13, y + 41, i == 0 || i == 3 ? 3 : 2);
            stroke(x + 2, y + 2 + i * 13, x + 41, y + 2 + i * 13, i == 0 || i == 3 ? 3 : 2);
        }
        rect(x + 5, y + 5, 7, 7, 0);
        rect(x + 18, y + 18, 7, 7, 0);
        rect(x + 31, y + 31, 7, 7, 0);
        return;
    }
    if (app == APP_TIC_TAC_TOE) {
        for (int i = 1; i < 3; i++) {
            stroke(x + i * 15, y + 1, x + i * 15, y + 43, 2);
            stroke(x + 1, y + i * 15, x + 43, y + i * 15, 2);
        }
        stroke(x + 3, y + 3, x + 11, y + 11, 3);
        stroke(x + 11, y + 3, x + 3, y + 11, 3);
        ring(x + 23, y + 23, 5, 3);
        return;
    }
    if (app == APP_HOME_ASSISTANT) {
        for (int yy = 0; yy < 44; yy++)
            for (int xx = 0; xx < 44; xx++)
                if (x + xx >= 0 && x + xx < 600 && y + yy >= 0 && y + yy < 800)
                    ui_pixels[(y + yy) * 600 + x + xx] = ha_logo_pixels[yy * 44 + xx];
        return;
    }
    if (app == APP_WIKIPEDIA) {
        drawtext_raw(x, y - 2, "W", &editorial, 48);
        return;
    }
    if (app == APP_QUOTES) {
        rect(x + 3, y + 8, 15, 19, 0);
        rect(x + 26, y + 8, 15, 19, 0);
        stroke(x + 16, y + 25, x + 7, y + 36, 5);
        stroke(x + 39, y + 25, x + 30, y + 36, 5);
        return;
    }
    if (app == APP_MINES) {
        disc(x + 22, y + 27, 14, 0);
        stroke(x + 27, y + 14, x + 32, y + 5, 4);
        stroke(x + 32, y + 5, x + 39, y + 9, 3);
        stroke(x + 39, y + 9, x + 42, y + 3, 2);
        disc(x + 17, y + 22, 3, 255);
        return;
    }
    if (app == APP_WORDS) {
        stroke(x + 2, y + 41, x + 39, y + 41, 3);
        stroke(x + 9, y + 41, x + 9, y + 3, 3);
        stroke(x + 9, y + 3, x + 29, y + 3, 3);
        stroke(x + 29, y + 3, x + 29, y + 10, 2);
        ring(x + 29, y + 15, 5, 2);
        stroke(x + 29, y + 20, x + 29, y + 30, 3);
        stroke(x + 29, y + 23, x + 21, y + 28, 2);
        stroke(x + 29, y + 23, x + 37, y + 28, 2);
        stroke(x + 29, y + 30, x + 23, y + 39, 2);
        stroke(x + 29, y + 30, x + 35, y + 39, 2);
        return;
    }
    if (app == APP_CHESS) {
        chess_piece(x - 11, y - 10, -2);
        return;
    }
    if (app == APP_2048) {
        aa_soft_rect(x + 1, y + 1, 42, 42, 5, 0);
        int old = ink_color;
        ink_color = -1;
        rect(x + 4, y + 4, 36, 36, 255);
        drawtext_raw(x + 7, y, "20", &caption, 34);
        drawtext_raw(x + 7, y + 20, "48", &caption, 34);
        ink_color = old;
        return;
    }
    if (app == APP_SOLITAIRE) {
        rect(x + 1, y + 1, 28, 36, 255);
        key_outline(x + 1, y + 1, 28, 36);
        rect(x + 13, y + 8, 28, 36, 255);
        key_outline(x + 13, y + 8, 28, 36);
        disc(x + 23, y + 21, 5, 0);
        disc(x + 31, y + 21, 5, 0);
        stroke(x + 20, y + 24, x + 27, y + 33, 6);
        stroke(x + 34, y + 24, x + 27, y + 33, 6);
        return;
    }
    if (app == APP_FOCUS) {
        disc(x + 15, y + 13, 11, 0);
        disc(x + 29, y + 13, 11, 0);
        disc(x + 11, y + 25, 10, 0);
        disc(x + 33, y + 25, 10, 0);
        disc(x + 17, y + 34, 9, 0);
        disc(x + 27, y + 34, 9, 0);
        rect(x + 21, y + 5, 2, 34, 255);
        for (int i = 0; i < 3; i++) {
            rect(x + 7, y + 13 + i * 8, 9, 2, 255);
            rect(x + 29, y + 13 + i * 8, 9, 2, 255);
        }
        return;
    }
    if (app == APP_NEWS) {
        key_outline(x + 1, y + 3, 42, 38);
        rect(x + 6, y + 8, 32, 5, 0);
        rect(x + 6, y + 18, 13, 16, 0);
        for (int i = 0; i < 3; i++)
            rect(x + 24, y + 18 + i * 7, 14, 2, 0);
        return;
    }
    if (app == APP_PHOTOS) {
        aa_soft_rect(x + 3, y + 5, 38, 34, 4, 0);
        aa_soft_rect(x + 6, y + 8, 32, 28, 2, 255);
        aa_disc(x + 30, y + 15, 4, 0);
        for (int yy = 0; yy < 15; yy++) {
            int w = yy * 12 / 15;
            rect(x + 18 - w, y + 20 + yy, w * 2 + 1, 1, 0);
            int v = yy * 8 / 15;
            rect(x + 31 - v, y + 20 + yy, v * 2 + 1, 1, 0);
        }
        return;
    }
    if (app == APP_TASKS) {
        for (int i = 0; i < 3; i++) {
            key_outline(x + 1, y + 3 + i * 14, 10, 10);
            stroke(x + 3, y + 8 + i * 14, x + 5, y + 11 + i * 14, 2);
            stroke(x + 5, y + 11 + i * 14, x + 10, y + 5 + i * 14, 2);
            rect(x + 17, y + 6 + i * 14, 25, 3, 0);
        }
        return;
    }
    if (app == APP_FLASHCARDS) {
        key_outline(x + 1, y + 1, 31, 33);
        rect(x + 10, y + 10, 33, 32, 255);
        key_outline(x + 10, y + 10, 33, 32);
        drawtext_raw(x + 20, y + 10, "?", &body, 22);
        return;
    }
    if (app == APP_PLANETARIUM) {
        ring(x + 23, y + 22, 11, 4);
        for (int a = 0; a < 360; a += 5) {
            double t = a * .0174533;
            disc(x + 22 + 21 * cos(t), y + 22 + 8 * sin(t), 1, 0);
        }
        disc(x + 5, y + 4, 3, 0);
        disc(x + 39, y + 39, 2, 0);
        return;
    }
    if (app == APP_WORDLE) {
        for (int i = 0; i < 3; i++) {
            aa_soft_rect(x + i * 15, y + 13, 12, 18, 2, 0);
            char c[2] = {(char)('A' + i), 0};
            ink_color = 255;
            centered(x + i * 15, y + 13, 12, 18, c, &caption);
            ink_color = 0;
        }
        return;
    }
    if (app == APP_NONOGRAMS) {
        for (int i = 0; i < 9; i++) {
            int xx = x + (i % 3) * 15, yy = y + (i / 3) * 15;
            if ((i + app) % 3)
                rect(xx, yy, 12, 12, 0);
            else
                key_outline(xx, yy, 12, 12);
        }
        return;
    }
    if (app == APP_LANGUAGE) {
        drawtext_raw(x, y, "Aa", &editorial, 48);
        stroke(x + 2, y + 39, x + 39, y + 39, 3);
        return;
    }
    if (app == APP_HEX) {
        for (int j = 0; j < 3; j++)
            for (int i = 0; i < 3; i++)
                disc(x + 8 + i * 14 + (j % 2) * 5, y + 8 + j * 14, 6, (i + j) % 2 ? 0 : 130);
        return;
    }
    if (app == APP_NUMSCRATCH) {
        stroke(x + 2, y + 10, x + 17, y + 25, 3);
        stroke(x + 17, y + 10, x + 2, y + 25, 3);
        rect(x + 25, y + 13, 17, 4, 0);
        rect(x + 25, y + 23, 17, 4, 0);
        rect(x + 3, y + 36, 38, 3, 0);
        return;
    }
    if (app == APP_DRAWING) {
        const int pencil[] = {-18, 20, -14, 8, 9, -15, 15, -21, 21, -15, 15, -9, -8, 14};
        chess_polygon(x + 22, y + 22, pencil, 7, 0);
        stroke(x + 10, y + 31, x + 15, y + 36, 2);
        stroke(x + 29, y + 7, x + 37, y + 15, 2);
        stroke(x + 11, y + 31, x + 15, y + 35, 2);
        rect(x + 9, y + 32, 3, 3, 255);
        return;
    }
    if (app == APP_STATUS_BAR) {
        rect(x, y + 5, 44, 5, 0);
        rect(x + 2, y + 19, 16, 3, 0);
        rect(x + 27, y + 19, 15, 3, 0);
        return;
    }
    if (app == APP_TAB_BAR) {
        key_outline(x, y + 8, 44, 30);
        for (int i = 0; i < 3; i++)
            disc(x + 8 + 14 * i, y + 23, 4, 0);
        return;
    }
    if (app == APP_LOCK_SCREEN) {
        ring(x + 22, y + 17, 12, 4);
        rect(x + 6, y + 17, 32, 26, 0);
        disc(x + 22, y + 28, 3, 255);
        return;
    }
    if (app == APP_KEYBOARD) {
        key_outline(x, y + 7, 44, 32);
        for (int i = 0; i < 10; i++)
            rect(x + 5 + (i % 5) * 7, y + 12 + (i / 5) * 8, 4, 4, 0);
        rect(x + 9, y + 30, 26, 3, 0);
        return;
    }
    int old = -1;
    static const int ids[] = {
        APP_CALCULATOR,     APP_TIMER,      APP_NOTES,    APP_DRAWING,   APP_WEATHER,
        APP_HOME_ASSISTANT, APP_CALENDAR,   APP_INSIGHTS, APP_QUOTES,    APP_WIKIPEDIA,
        APP_MINES,          APP_WORDS,      APP_BLOCKS,   APP_SNAKE,     APP_CHESS,
        APP_TIC_TAC_TOE,    APP_NUMSCRATCH, APP_2048,     APP_SOLITAIRE, APP_SUDOKU,
        APP_FOCUS,          APP_NEWS};
    for (int i = 0; i < 22; i++)
        if (ids[i] == app)
            old = i;
    if (old >= 0)
        extra_icon(x, y, old);
    else
        icon(x, y,
             app == APP_SETTINGS   ? IC_SETTINGS
             : app == APP_STORE    ? IC_STORE
             : app == APP_LAUNCHER ? IC_GRID
             : app == APP_READER   ? IC_BOOK
                                   : IC_HOME);
}
