#include "chess_icons.h"
/* App identities use the same rounded geometry and ink as system actions. */
static void app_icon(int x, int y, int app) {
    if (app == APP_HOME_ASSISTANT) {
        for (int j = 0; j < 44; j++)
            for (int i = 0; i < 44; i++)
                if (x + i >= 0 && x + i < WIDTH && y + j >= 0 && y + j < HEIGHT)
                    ui_pixels[(y + j) * WIDTH + x + i] = ha_logo_pixels[j * 44 + i];
        return;
    }
    if (app == APP_WIKIPEDIA) {
        drawtext_raw(x, y - 2, "W", ui_font(UI_TITLE), 48);
        return;
    }
    if (app == APP_CHESS || app == APP_KNIGHTS) {
        chess_piece(x - 11, y - 10, -2);
        return;
    }
    switch (app) {
    case APP_LIBRARY:
    case APP_READER:
    case APP_STUDY:
        icon_native(x, y, IC_BOOK);
        break;
    case APP_SETTINGS:
        icon_native(x, y, IC_SETTINGS);
        break;
    case APP_STORE:
        icon_native(x, y, IC_STORE);
        break;
    case APP_TIMER:
        icon_native(x, y, IC_CLOCK);
        stroke(x + 16, y + 4, x + 28, y + 4, 3);
        break;
    case APP_CALENDAR:
        icon_native(x, y, IC_TODAY);
        break;
    case APP_NOTES:
    case APP_JOURNAL:
        control_surface(x + 7, y + 3, 30, 38, 6, 0);
        for (int i = 0; i < 4; i++)
            stroke_color(x + 14, y + 13 + i * 6, x + 30 - (i == 3 ? 7 : 0), y + 13 + i * 6, 2, 255);
        if (app == APP_JOURNAL)
            stroke_color(x + 10, y + 4, x + 10, y + 40, 2, 255);
        break;
    case APP_CALCULATOR:
        control_surface(x + 6, y + 2, 32, 40, 7, 0);
        control_surface(x + 11, y + 7, 22, 10, 3, 255);
        for (int i = 0; i < 9; i++)
            disc(x + 13 + i % 3 * 9, y + 24 + i / 3 * 6, 2, 255);
        break;
    case APP_QUOTES:
        for (int i = 0; i < 2; i++) {
            control_surface(x + 5 + i * 21, y + 10, 14, 17, 4, 0);
            stroke(x + 16 + i * 21, y + 24, x + 8 + i * 21, y + 35, 4);
        }
        break;
    case APP_NEWS:
        icon_box(x + 4, y + 5, 36, 34);
        control_surface(x + 10, y + 11, 24, 5, 2, 0);
        control_surface(x + 10, y + 22, 9, 11, 2, 0);
        for (int i = 0; i < 3; i++)
            stroke(x + 25, y + 22 + i * 5, x + 34, y + 22 + i * 5, 2);
        break;
    case APP_PHOTOS:
        icon_box(x + 4, y + 6, 36, 32);
        disc(x + 30, y + 15, 4, 0);
        stroke(x + 10, y + 30, x + 18, y + 20, 4);
        stroke(x + 18, y + 20, x + 26, y + 30, 4);
        stroke(x + 25, y + 28, x + 30, y + 23, 3);
        stroke(x + 30, y + 23, x + 35, y + 30, 3);
        break;
    case APP_WEATHER:
        disc(x + 15, y + 15, 7, 0);
        for (int i = 0; i < 8; i++) {
            double a = i * .785398;
            stroke(x + 15 + 11 * cos(a), y + 15 + 11 * sin(a), x + 15 + 14 * cos(a),
                   y + 15 + 14 * sin(a), 3);
        }
        disc(x + 17, y + 29, 10, 255);
        disc(x + 28, y + 25, 12, 255);
        disc(x + 37, y + 30, 8, 255);
        disc(x + 17, y + 29, 7, 0);
        disc(x + 28, y + 25, 9, 0);
        disc(x + 37, y + 30, 5, 0);
        control_surface(x + 12, y + 28, 27, 10, 4, 0);
        break;
    case APP_INSIGHTS:
        for (int i = 0; i < 3; i++)
            control_surface(x + 6 + i * 12, y + 24 - i * 8, 7, 15 + i * 8, 3, 0);
        stroke(x + 5, y + 17, x + 16, y + 9, 3);
        stroke(x + 16, y + 9, x + 25, y + 12, 3);
        stroke(x + 25, y + 12, x + 37, y + 4, 3);
        break;
    case APP_TASKS:
        icon_native(x, y, IC_DEVICES);
        break;
    case APP_LLM:
        control_surface(x + 4, y + 6, 36, 29, 8, 0);
        stroke(x + 14, y + 32, x + 10, y + 40, 4);
        for (int i = 0; i < 3; i++)
            disc(x + 13 + i * 9, y + 21, 2, 255);
        break;
    case APP_LANGUAGE:
        icon_native(x, y, IC_TYPE);
        break;
    case APP_FLASHCARDS:
        icon_box(x + 3, y + 3, 29, 32);
        control_surface(x + 12, y + 11, 29, 31, 5, 255);
        icon_box(x + 12, y + 11, 29, 31);
        type_label(x + 21, y + 15, 16, 22, "?", UI_PRIMARY);
        break;
    case APP_WORDLE:
        for (int i = 0; i < 3; i++) {
            control_surface(x + 1 + i * 15, y + 12, 12, 20, 3, 0);
            char b[2] = {(char)('A' + i), 0};
            int old = ink_color;
            ink_color = 255;
            label_at(x + 2 + i * 15, y + 13, 11, 18, b, ui_font(UI_SECONDARY));
            ink_color = old;
        }
        break;
    case APP_NONOGRAMS:
    case APP_SUDOKU:
    case APP_BINGO:
    case APP_CONNECT4:
    case APP_CROSSWORD:
        for (int i = 0; i < 9; i++) {
            int xx = x + 3 + i % 3 * 14, yy = y + 3 + i / 3 * 14;
            if (app == APP_CONNECT4) {
                disc(xx + 5, yy + 5, 5, i % 2 ? 144 : 0);
                continue;
            }
            int filled = app == APP_NONOGRAMS   ? (i == 1 || i == 3 || i == 4 || i == 5 || i == 7)
                         : app == APP_CROSSWORD ? (i % 3 == 1 || i / 3 == 1)
                                                : 0;
            control_surface(xx, yy, 11, 11, 3, 0);
            if (!filled)
                control_surface(xx + 2, yy + 2, 7, 7, 2, 255);
            if (app == APP_BINGO && i % 4 == 0)
                disc(xx + 5, yy + 5, 2, 0);
            if (app == APP_SUDOKU && i % 4 == 0)
                rect(xx + 4, yy + 3, 3, 5, 0);
        }
        break;
    case APP_2048:
        control_surface(x + 3, y + 3, 38, 38, 7, 0);
        int old = ink_color;
        ink_color = 255;
        label_at(x + 10, y + 4, 30, 17, "20", ui_font(UI_SECONDARY));
        label_at(x + 10, y + 21, 30, 17, "48", ui_font(UI_SECONDARY));
        ink_color = old;
        break;
    case APP_SOLITAIRE:
        icon_box(x + 3, y + 3, 27, 33);
        control_surface(x + 13, y + 10, 28, 32, 5, 255);
        icon_box(x + 13, y + 10, 28, 32);
        disc(x + 23, y + 23, 4, 0);
        disc(x + 31, y + 23, 4, 0);
        stroke(x + 20, y + 25, x + 27, y + 33, 4);
        stroke(x + 34, y + 25, x + 27, y + 33, 4);
        break;
    case APP_TIC_TAC_TOE:
        for (int i = 1; i < 3; i++) {
            stroke(x + 2 + i * 13, y + 4, x + 2 + i * 13, y + 40, 3);
            stroke(x + 4, y + 2 + i * 13, x + 40, y + 2 + i * 13, 3);
        }
        ring(x + 23, y + 23, 4, 3);
        stroke(x + 5, y + 5, x + 10, y + 10, 3);
        stroke(x + 10, y + 5, x + 5, y + 10, 3);
        break;
    case APP_BLOCKS:
        for (int i = 0; i < 4; i++)
            control_surface(x + 3 + (i < 3 ? i : 1) * 13, y + 9 + (i < 3 ? 0 : 13), 11, 11, 3, 0);
        break;
    case APP_SNAKE:
        stroke(x + 7, y + 9, x + 35, y + 9, 8);
        stroke(x + 35, y + 9, x + 35, y + 24, 8);
        stroke(x + 35, y + 24, x + 15, y + 24, 8);
        stroke(x + 15, y + 24, x + 15, y + 36, 8);
        disc(x + 15, y + 36, 6, 0);
        disc(x + 13, y + 35, 1, 255);
        disc(x + 18, y + 35, 1, 255);
        break;
    case APP_MINES:
        disc(x + 21, y + 26, 13, 0);
        stroke(x + 27, y + 14, x + 33, y + 6, 3);
        stroke(x + 33, y + 6, x + 39, y + 10, 3);
        disc(x + 17, y + 22, 3, 255);
        break;
    case APP_WORDS:
        stroke(x + 6, y + 39, x + 37, y + 39, 3);
        stroke(x + 10, y + 39, x + 10, y + 5, 3);
        stroke(x + 10, y + 5, x + 29, y + 5, 3);
        stroke(x + 29, y + 5, x + 29, y + 11, 3);
        ring(x + 29, y + 16, 5, 3);
        stroke(x + 29, y + 21, x + 29, y + 30, 3);
        stroke(x + 29, y + 24, x + 21, y + 29, 3);
        stroke(x + 29, y + 24, x + 37, y + 29, 3);
        stroke(x + 29, y + 30, x + 23, y + 38, 3);
        stroke(x + 29, y + 30, x + 35, y + 38, 3);
        break;
    case APP_FOCUS:
        for (int i = 0; i < 6; i++)
            disc(x + (i % 2 ? 29 : 15), y + 11 + i / 2 * 11, 9, 0);
        stroke_color(x + 22, y + 6, x + 22, y + 39, 2, 255);
        stroke_color(x + 8, y + 19, x + 16, y + 22, 2, 255);
        stroke_color(x + 28, y + 27, x + 36, y + 24, 2, 255);
        break;
    case APP_PLANETARIUM:
        disc(x + 22, y + 22, 10, 0);
        for (int i = 0; i < 72; i++) {
            double a = i * .087266;
            disc(x + 22 + 19 * cos(a), y + 22 + 7 * sin(a), 1, 0);
        }
        disc(x + 7, y + 5, 2, 0);
        disc(x + 38, y + 38, 2, 0);
        break;
    case APP_HEX:
        for (int i = 0; i < 7; i++)
            disc(x + 8 + i % 3 * 14 + (i / 3 % 2 ? 6 : 0), y + 7 + i / 3 * 14, 5, i % 2 ? 144 : 0);
        break;
    case APP_RECIPES:
        ring(x + 26, y + 25, 13, 3);
        ring(x + 26, y + 25, 7, 2);
        stroke(x + 5, y + 7, x + 5, y + 37, 3);
        for (int i = 0; i < 3; i++)
            stroke(x + 2 + i * 3, y + 5, x + 2 + i * 3, y + 15, 2);
        break;
    case APP_PERIODIC:
        control_surface(x + 4, y + 4, 36, 36, 6, 0);
        type_centered(x + 4, y + 6, 36, 32, "Fe", UI_INVERSE);
        break;
    case APP_SOKOBAN:
        icon_box(x + 5, y + 5, 34, 34);
        stroke(x + 10, y + 10, x + 34, y + 34, 3);
        stroke(x + 34, y + 10, x + 10, y + 34, 3);
        break;
    case APP_MAZE:
        for (int i = 0; i < 3; i++) {
            stroke(x + 5 + i * 10, y + 5 + i * 6, x + 38 - i * 5, y + 5 + i * 6, 3);
            stroke(x + 5 + i * 10, y + 5 + i * 6, x + 5 + i * 10, y + 38 - i * 5, 3);
        }
        break;
    case APP_LOCK_SCREEN:
        icon_native(x, y, IC_LOCK);
        break;
    case APP_KEYBOARD:
        icon_box(x + 3, y + 7, 38, 30);
        for (int i = 0; i < 10; i++)
            disc(x + 10 + i % 5 * 6, y + 15 + i / 5 * 7, 1, 0);
        stroke(x + 12, y + 30, x + 32, y + 30, 3);
        break;
    case APP_STATUS_BAR:
        icon_box(x + 3, y + 8, 38, 28);
        rect(x + 6, y + 11, 32, 5, 0);
        break;
    case APP_SIGNAL:
        for(int i=0;i<3;i++) icon_box(x+5+(i%2)*18,y+5+(i/2)*18,16,16);
        break;
    case APP_HOME:
        for (int row=0; row<15; row++)
            rect(x+20-row,y+7+row,4+2*row,1,0);
        stroke(x+6,y+22,x+22,y+7,5);
        stroke(x+22,y+7,x+38,y+22,5);
        control_surface(x+9,y+19,26,19,5,0);
        control_surface(x+18,y+27,8,13,3,255);
        break;
    case APP_FROG:
        ui_paint_ellipse(x+22, y+34, 18, 5, 176);
        ui_paint_ellipse(x+22, y+26, 17, 12, 48);
        ui_paint_ellipse(x+22, y+26, 14, 10, 126);
        ui_paint_ellipse(x+22, y+30, 9, 5, 205);
        for (int side = -1; side <= 1; side += 2) {
            ui_paint_ellipse(x+22+side*8, y+15, 6, 6, 39);
            ui_paint_ellipse(x+22+side*8, y+15, 4, 4, 218);
            ui_paint_ellipse(x+22+side*8, y+16, 2, 2, 21);
        }
        stroke_color(x+16, y+22, x+22, y+25, 2, 39);
        stroke_color(x+22, y+25, x+28, y+22, 2, 39);
        break;
    default:
        icon_native(x, y, IC_BOOK);
        break;
    }
}
