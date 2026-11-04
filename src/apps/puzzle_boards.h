/* Solvable reverse-generated warehouses and larger perfect mazes. */
typedef struct {
    int level, w, h, x, y, moves;
    unsigned char board[361];
} Warehouse17;
static Warehouse17 warehouse17, undo_warehouse17, initial_warehouse17;
static int warehouse_ready17, warehouse_undo17, warehouse_facing17 = 4;
static unsigned char maze17[33 * 35];
static int maze_ready17, maze_x17 = 1, maze_y17 = 1, maze_moves17, maze_win17;
static int box17(int v) {
    return v == 3 || v == 5;
}
static int floor17(int v) {
    return v == 0 || v == 2;
}
static void warehouse_new17(int level) {
    Warehouse17 *s = &warehouse17;
    memset(s, 0, sizeof(*s));
    s->level = level < 1 ? 1 : level;
    int scale = (level - 1) / 3;
    if (scale > 4)
        scale = 4;
    s->w = 11 + scale * 2;
    s->h = 13 + scale * 2;
    if (s->h > 19)
        s->h = 19;
    int boxes = 2 + (level - 1) / 2;
    if (boxes > 8)
        boxes = 8;
    for (int attempt = 0; attempt < 30; attempt++) {
        memset(s->board, 0, sizeof(s->board));
        for (int y = 0; y < s->h; y++)
            for (int x = 0; x < s->w; x++)
                if (!x || !y || x == s->w - 1 || y == s->h - 1)
                    s->board[y * s->w + x] = 1;
        for (int k = 0; k < (s->w * s->h) * (8 + scale * 2) / 100; k++) {
            int x = 1 + random16() % (s->w - 2), y = 1 + random16() % (s->h - 2);
            s->board[y * s->w + x] = 1;
        }
        for (int k = 0; k < boxes; k++) {
            int at;
            do {
                int x = 2 + random16() % (s->w - 4), y = 2 + random16() % (s->h - 4);
                at = y * s->w + x;
            } while (s->board[at]);
            s->board[at] = 5;
        }
        do {
            s->x = 1 + random16() % (s->w - 2);
            s->y = 1 + random16() % (s->h - 2);
        } while (s->board[s->y * s->w + s->x]);
        int pulls = 0;
        for (int k = 0; k < 5000 + scale * 2000; k++) {
            int d = random16() % 4,
                dx = d == 0   ? 1
                     : d == 1 ? -1
                              : 0,
                dy = d == 2   ? 1
                     : d == 3 ? -1
                              : 0,
                nx = s->x + dx, ny = s->y + dy;
            if (nx <= 0 || ny <= 0 || nx >= s->w - 1 || ny >= s->h - 1 ||
                !floor17(s->board[ny * s->w + nx]))
                continue;
            int behind = (s->y - dy) * s->w + s->x - dx;
            if (box17(s->board[behind]) && (random16() & 3)) {
                int here = s->y * s->w + s->x;
                s->board[here] = s->board[here] == 2 ? 5 : 3;
                s->board[behind] = s->board[behind] == 5 ? 2 : 0;
                pulls++;
            }
            s->x = nx;
            s->y = ny;
        }
        int off = 0;
        for (int i = 0; i < s->w * s->h; i++)
            if (s->board[i] == 3)
                off++;
        if (pulls > 8 && off)
            break;
    }
    s->moves = 0;
    initial_warehouse17 = *s;
    warehouse_ready17 = 1;
    warehouse_undo17 = 0;
}
static void warehouse_step17(int dx, int dy) {
    Warehouse17 *s = &warehouse17;
    warehouse_facing17 = dx < 0 ? 7 : dx > 0 ? 8 : dy < 0 ? 6 : 4;
    int nx = s->x + dx, ny = s->y + dy;
    if (nx < 0 || ny < 0 || nx >= s->w || ny >= s->h)
        return;
    int at = ny * s->w + nx, v = s->board[at];
    if (v == 1)
        return;
    int to = (ny + dy) * s->w + nx + dx;
    if (box17(v) && (nx + dx < 0 || nx + dx >= s->w || ny + dy < 0 || ny + dy >= s->h ||
                     !floor17(s->board[to])))
        return;
    undo_warehouse17 = *s;
    warehouse_undo17 = 1;
    if (box17(v)) {
        s->board[to] = s->board[to] == 2 ? 5 : 3;
        s->board[at] = v == 5 ? 2 : 0;
    }
    s->x = nx;
    s->y = ny;
    s->moves++;
    int off = 0;
    for (int i = 0; i < s->w * s->h; i++)
        if (s->board[i] == 3)
            off++;
    if (!off) {
        char b[120];
        snprintf(b, sizeof(b), tr("Level %d geschafft!"), s->level);
        int level = s->level;
        warehouse_new17(level + 1);
        ui_message(b);
    }
}
static void maze_new17(void) {
    memset(maze17, 1, sizeof(maze17));
    int stack[300], top = 0;
    stack[top++] = 34;
    maze17[34] = 0;
    while (top) {
        int at = stack[top - 1], x = at % 33, y = at / 33, choices[4], n = 0;
        const int dx[] = {2, -2, 0, 0}, dy[] = {0, 0, 2, -2};
        for (int d = 0; d < 4; d++) {
            int xx = x + dx[d], yy = y + dy[d];
            if (xx > 0 && xx < 32 && yy > 0 && yy < 34 && maze17[yy * 33 + xx])
                choices[n++] = d;
        }
        if (!n) {
            top--;
            continue;
        }
        int d = choices[random16() % n], next = (y + dy[d]) * 33 + x + dx[d];
        maze17[(at + next) / 2] = maze17[next] = 0;
        stack[top++] = next;
    }
    maze_x17 = maze_y17 = 1;
    maze_moves17 = maze_win17 = 0;
    maze_ready17 = 1;
}
static void maze_step17(int dx, int dy, int steps) {
    while (steps-- && !maze_win17) {
        int x = maze_x17 + dx, y = maze_y17 + dy;
        if (x < 0 || x >= 33 || y < 0 || y >= 35 || maze17[y * 33 + x])
            break;
        maze_x17 = x;
        maze_y17 = y;
        maze_moves17++;
        if (x == 31 && y == 33) {
            maze_win17 = 1;
            ui_message("Ausgang gefunden!");
        }
    }
}
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
