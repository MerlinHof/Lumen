/* App-owned module; assembled by apps/host/compose.h. */
static void tiny_number17(int x, int y, int n) {
    static const unsigned char glyphs[10][7] = {
        {14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14},  {14, 17, 1, 2, 4, 8, 31},
        {30, 1, 1, 14, 1, 1, 30},     {2, 6, 10, 18, 31, 2, 2}, {31, 16, 16, 30, 1, 1, 30},
        {14, 16, 16, 30, 17, 17, 14}, {31, 1, 2, 4, 8, 8, 8},   {14, 17, 17, 14, 17, 17, 14},
        {14, 17, 17, 15, 1, 1, 14}};
    for (int yy = 0; yy < 7; yy++)
        for (int xx = 0; xx < 5; xx++)
            if (glyphs[n % 10][yy] & (1 << (4 - xx)))
                rect(x + xx * 2, y + yy * 2, 2, 2, 0);
}
static int cross_selected17, cross_checked17;
static int cross_commit17(const char *text) {
    if (!*text) {
        crossword_answer16[crossword_pick16][0] = 0;
        save_blob("crossword16.bin", crossword_answer16, sizeof(crossword_answer16));
        ui_message("Antwort gelöscht.");
        return 1;
    }
    char answer[16];
    int n = 0;
    for (; *text && n < 15; text++)
        if (isalpha((unsigned char)*text))
            answer[n++] = toupper((unsigned char)*text);
    answer[n] = 0;
    int word = crossword_pick16;
    if (n != (int)strlen(cross_words16[word])) {
        char b[100];
        snprintf(b, 100, tr("Bitte %d Buchstaben eingeben."), (int)strlen(cross_words16[word]));
        ui_message(b);
        return 1;
    }
    for (int i = 0; i < 5; i++)
        if (i != word)
            for (int k = 0; answer[k]; k++)
                for (int j = 0; crossword_answer16[i][j]; j++) {
                    int x = cross_x16[word] + !cross_down16[word] * k,
                        y = cross_y16[word] + cross_down16[word] * k,
                        xx = cross_x16[i] + !cross_down16[i] * j,
                        yy = cross_y16[i] + cross_down16[i] * j;
                    if (x == xx && y == yy && answer[k] != crossword_answer16[i][j]) {
                        ui_message("Der Buchstabe an der Kreuzung passt nicht.");
                        return 1;
                    }
                }
    snprintf(crossword_answer16[word], 16, "%s", answer);
    cross_selected17 = word;
    cross_checked17 = 0;
    save_blob("crossword16.bin", crossword_answer16, sizeof(crossword_answer16));
    return 1;
}
static void crossword_draw17(void) {
    heading_actions("Kreuzworträtsel", 3);
    icon_button(468, 74, IC_DONE);
    icon_button(528, 74, IC_RESTART);
    if (!cross_seed16) {
        char seed[40] = "";
        config16("crossword-seed16.txt", seed, sizeof(seed));
        char answers[5][16];
        int exists = load_blob("crossword16.bin", answers, sizeof(answers));
        crossword_new16((unsigned)strtoul(seed, NULL, 10));
        if (exists && *seed) {
            memcpy(crossword_answer16, answers, sizeof(answers));
            for (int i = 0; i < 5; i++)
                crossword_answer16[i][15] = 0;
        }
    }
    char grid[8][10] = {{0}}, mask[8][10] = {{0}};
    for (int i = 0; i < 5; i++)
        for (int k = 0; cross_words16[i][k]; k++) {
            int x = cross_x16[i] + !cross_down16[i] * k, y = cross_y16[i] + cross_down16[i] * k;
            if (x < 0 || x >= 10 || y < 0 || y >= 8)
                continue;
            mask[y][x] = 1;
            if (crossword_answer16[i][k])
                grid[y][x] = crossword_answer16[i][k];
        }
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 10; x++)
            if (mask[y][x]) {
                int xx = 30 + x * 54, yy = 132 + y * 46;
                key_outline(xx, yy, 54, 46);
                region(xx, yy, 54, 46);
                if (grid[y][x]) {
                    char b[2] = {grid[y][x], 0};
                    centered(xx, yy + 10, 54, 36, b, ui_font(UI_BODY));
                }
            }
    for (int i = 0; i < 5; i++) {
        int offset = 0;
        for (int k = 0; k < i; k++)
            if (cross_x16[k] == cross_x16[i] && cross_y16[k] == cross_y16[i])
                offset += 12;
        tiny_number17(34 + cross_x16[i] * 54 + offset, 136 + cross_y16[i] * 46, i + 1);
        char b[300];
        snprintf(
            b, sizeof(b), "%d %s  %s%s", i + 1, cross_down16[i] ? "↓" : "→", tr(cross_clues16[i]),
            cross_checked17 ? (!strcasecmp(crossword_answer16[i], cross_words16[i]) ? " ✓" : " ×")
                            : "");
        button_h(28, 520 + i * 49, 544, 43, b, cross_selected17 == i);
    }
}
static void games_save17(void) {
    if (warehouse_ready17) {
        Warehouse17 states[2] = {warehouse17, initial_warehouse17};
        save_blob("sokoban17.bin", states, sizeof(states));
    }
    if (maze_ready17) {
        unsigned char b[33 * 35 + 12];
        memcpy(b, maze17, sizeof(maze17));
        int state[] = {maze_x17, maze_y17, maze_moves17};
        memcpy(b + sizeof(maze17), state, 12);
        save_blob("maze17.bin", b, sizeof(b));
    }
}
static int u17_games_draw(void) {
    if (screen == 118) {
        if (!warehouse_ready17) {
            Warehouse17 states[2];
            int loaded = load_blob("sokoban17.bin", states, sizeof(states));
            if (loaded) {
                warehouse17 = states[0];
                initial_warehouse17 = states[1];
            }
            if (!loaded || warehouse17.w < 5 || warehouse17.w > 19 || warehouse17.h < 5 ||
                warehouse17.h > 19 || warehouse17.x < 1 || warehouse17.x >= warehouse17.w ||
                warehouse17.y < 1 || warehouse17.y >= warehouse17.h)
                warehouse_new17(1);
            else
                warehouse_ready17 = 1;
        }
        heading_actions("Sokoban", 3);
        icon_button(468, 74, IC_UNDO);
        icon_button(528, 74, IC_RESTART);
        Warehouse17 *s = &warehouse17;
        char b[100];
        snprintf(b, 100, tr("Level %d · %d Züge"), s->level, s->moves);
        centered(28, 124, 544, 40, b, ui_font(UI_SECONDARY));
        int cell = (528 / s->h < 544 / s->w ? 528 / s->h : 544 / s->w);
        if (cell > 40)
            cell = 40;
        int ox = 300 - s->w * cell / 2;
        for (int y = 0; y < s->h; y++)
            for (int x = 0; x < s->w; x++) {
                int xx = ox + x * cell, yy = 170 + y * cell, v = s->board[y * s->w + x];
                region(xx, yy, cell, cell);
                paper_tile17(xx, yy, cell, v);
            }
        paper_tile17(ox + s->x * cell, 170 + s->y * cell, cell, warehouse_facing17);
        direction_keys16();
        return 1;
    }
    if (screen == 119) {
        if (!maze_ready17)
            maze_new17();
        heading_actions("Labyrinth", 1);
        icon_button(528, 74, IC_RESTART);
        for (int y = 0; y < 35; y++)
            for (int x = 0; x < 33; x++)
                paper_tile17(36 + x * 16, 140 + y * 16, 16, maze17[y * 33 + x] ? 1 : 0);
        paper_tile17(36 + 31 * 16, 140 + 33 * 16, 16, 9);
        paper_tile17(36 + maze_x17 * 16, 140 + maze_y17 * 16, 16, 4);
        region(36, 140, 528, 560);
        direction_keys16();
        return 1;
    }
    if (screen == 120) {
        crossword_draw17();
        return 1;
    }
    return recipes_draw17();
}
static int u17_games_input(const Input *in) {
    int x = in->x, y = in->y;
    if (screen != 118 && screen != 119 && screen != 120)
        return recipes_input17(in);
    if (in->kind == INPUT_TAP && y >= 74 && y < 118) {
        int helpx = (screen == 118 || screen == 120) ? 408 : 468;
        if (x >= helpx && x < helpx + 44) {
            game_help16(screen);
            return 1;
        }
        if (x >= 516) {
            if (screen == 118) {
                warehouse17 = initial_warehouse17;
                warehouse_undo17 = 0;
            }
            if (screen == 119)
                maze_new17();
            if (screen == 120) {
                crossword_new16(random16());
                cross_checked17 = 0;
            }
            return 1;
        }
        if (x >= 460 && x < 512) {
            if (screen == 118 && warehouse_undo17) {
                warehouse17 = undo_warehouse17;
                warehouse_undo17 = 0;
            }
            if (screen == 120) {
                cross_checked17 = 1;
                int good = 1;
                for (int i = 0; i < 5; i++)
                    if (strcasecmp(crossword_answer16[i], cross_words16[i]))
                        good = 0;
                ui_message(good ? "Alle Wörter stimmen!" : "Lösungen geprüft.");
            }
            return 1;
        }
    }
    if (in->kind == INPUT_TAP && screen == 120) {
        int pick = -1;
        if (y >= 520 && y < 765)
            pick = (y - 520) / 49;
        else if (x >= 30 && x < 570 && y >= 132 && y < 500) {
            int xx = (x - 30) / 54, yy = (y - 132) / 46;
            for (int i = 0; i < 5; i++)
                for (int k = 0; cross_words16[i][k]; k++)
                    if (cross_x16[i] + !cross_down16[i] * k == xx &&
                        cross_y16[i] + cross_down16[i] * k == yy)
                        pick = i;
        }
        if (pick >= 0) {
            crossword_pick16 = cross_selected17 = pick;
            open_keyboard(179, tr(cross_clues16[pick]), crossword_answer16[pick]);
            return 1;
        }
        return y < 48 ? -1 : 1;
    }
    int dx = 0, dy = 0, steps = 1;
    if (in->kind == INPUT_SWIPE) {
        dx = abs(in->dx) > abs(in->dy) ? (in->dx > 0 ? 1 : -1) : 0;
        dy = dx ? 0 : (in->dy > 0 ? 1 : -1);
        steps = screen == 119 ? 4 : 1;
    } else if (in->kind == INPUT_TAP && y >= 728 && y < 772) {
        if (x >= 150 && x < 210)
            dx = -1;
        else if (x >= 229 && x < 289)
            dy = -1;
        else if (x >= 308 && x < 368)
            dy = 1;
        else if (x >= 387 && x < 447)
            dx = 1;
    } else if (in->kind == INPUT_TAP && y >= 140 && y < 710) {
        int xx, yy, px, py;
        if (screen == 118) {
            int cell = 528 / warehouse17.h;
            if (cell > 544 / warehouse17.w)
                cell = 544 / warehouse17.w;
            if (cell > 40)
                cell = 40;
            xx = (x - (300 - warehouse17.w * cell / 2)) / cell;
            yy = (y - 170) / cell;
            px = warehouse17.x;
            py = warehouse17.y;
        } else {
            xx = (x - 36) / 16;
            yy = (y - 140) / 16;
            px = maze_x17;
            py = maze_y17;
        }
        if (abs(xx - px) > abs(yy - py)) {
            dx = xx > px ? 1 : -1;
            if (screen == 119)
                steps = abs(xx - px);
        } else if (yy != py) {
            dy = yy > py ? 1 : -1;
            if (screen == 119)
                steps = abs(yy - py);
        }
    }
    if (dx || dy) {
        if (screen == 118)
            warehouse_step17(dx, dy);
        else
            maze_step17(dx, dy, steps);
        return 1;
    }
    return in->kind == INPUT_TAP && y >= 120 ? 1 : -1;
}
