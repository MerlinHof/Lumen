/* Offline experimental apps. All timers stop off-screen and during sleep. */
#include <stdint.h>
#include <sys/stat.h>
typedef struct {
    int date;
    unsigned seconds, pages;
} ReadingDay;
typedef struct {
    unsigned magic, version, seconds, pages, best_snake, best_blocks, mines_wins, word_wins;
    ReadingDay days[31];
} AppData;
static AppData app_data;
static Mines mine_game;
static Hangman word_game;
static Blocks block_game;
static Snake snake_game;
static int apps_dirty, app_ready[7], mine_flags, mine_release, mine_recorded, word_recorded;
static int calendar_year, calendar_month, calendar_day;
static char calendar_note[512];
static unsigned calendar_note_days;
static int64_t app_last_time, app_next_tick;
static int64_t app_millis(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (int64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
static int app_date_key(time_t now) {
    struct tm t;
    localtime_r(&now, &t);
    return (t.tm_year + 1900) * 10000 + (t.tm_mon + 1) * 100 + t.tm_mday;
}
static ReadingDay *app_day(int key, int create) {
    int oldest = 0;
    for (int i = 0; i < 31; i++) {
        if (app_data.days[i].date == key)
            return &app_data.days[i];
        if (app_data.days[i].date < app_data.days[oldest].date)
            oldest = i;
    }
    if (!create)
        return NULL;
    memset(&app_data.days[oldest], 0, sizeof(ReadingDay));
    app_data.days[oldest].date = key;
    return &app_data.days[oldest];
}
static void app_calendar_load(void) {
    u17_calendar_dirty();
    calendar_note_days = 0;
    for (int day = 1; day <= calendar_days(calendar_year, calendar_month); day++) {
        char p[2048];
        snprintf(p, sizeof(p), "%s/calendar/%04d%02d%02d.txt", data_root, calendar_year,
                 calendar_month, day);
        struct stat st;
        if (!stat(p, &st) && st.st_size > 0)
            calendar_note_days |= 1u << (day - 1);
    }
    char path[2048];
    snprintf(path, sizeof(path), "%s/calendar/%04d%02d%02d.txt", data_root, calendar_year,
             calendar_month, calendar_day);
    calendar_note[0] = 0;
    FILE *f = fopen(path, "rb");
    if (f) {
        size_t n = fread(calendar_note, 1, sizeof(calendar_note) - 1, f);
        calendar_note[n] = 0;
        fclose(f);
    }
}
static void apps_note_commit(const char *text) {
    u17_calendar_dirty();
    char folder[2048], path[2048];
    snprintf(folder, sizeof(folder), "%s/calendar", data_root);
    mkdir(folder, 0700);
    snprintf(path, sizeof(path), "%s/%04d%02d%02d.txt", folder, calendar_year, calendar_month,
             calendar_day);
    if (!atomic_write(path, text, strlen(text)))
        ui_message("Speichern fehlgeschlagen.");
    else {
        snprintf(calendar_note, sizeof(calendar_note), "%s", text);
        if (*text)
            calendar_note_days |= 1u << (calendar_day - 1);
        else
            calendar_note_days &= ~(1u << (calendar_day - 1));
    }
}
static void apps_init(void) {
    char path[2048];
    snprintf(path, sizeof(path), "%s/apps-v1.bin", data_root);
    memset(&app_data, 0, sizeof(app_data));
    FILE *f = fopen(path, "rb");
    if (f) {
        AppData tmp;
        if (fread(&tmp, sizeof(tmp), 1, f) == 1 && tmp.magic == 0x4c415050 && tmp.version == 1) {
            app_data = tmp;
        }
        fclose(f);
    }
    app_data.magic = 0x4c415050;
    app_data.version = 1;
    app_last_time = app_millis();
    apps_seed((uint32_t)time(NULL));
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    calendar_year = t.tm_year + 1900;
    calendar_month = t.tm_mon + 1;
    calendar_day = t.tm_mday;
}
static void apps_checkpoint(void) {
    int64_t now = app_millis(), delta = (now - app_last_time) / 1000;
    if (delta < 0)
        delta = 0;
    if (delta > 300)
        delta = 300;
    if (screen == 2 && !sleeping && delta > 0) {
        ReadingDay *d = app_day(app_date_key(time(NULL)), 1);
        d->seconds += (unsigned)delta;
        app_data.seconds += (unsigned)delta;
        apps_dirty = 1;
        extra_track((unsigned)delta, 0, current >= 0 ? library[current].id : 0);
    }
    if (delta > 0 || screen != 2 || sleeping)
        app_last_time = now;
}
static void apps_page_turned(void) {
    ReadingDay *d = app_day(app_date_key(time(NULL)), 1);
    d->pages++;
    app_data.pages++;
    extra_track(0, 1, current >= 0 ? library[current].id : 0);
    apps_dirty = 1;
}
static void apps_save(void) {
    if (!apps_dirty)
        return;
    char path[2048];
    snprintf(path, sizeof(path), "%s/apps-v1.bin", data_root);
    if (atomic_write(path, &app_data, sizeof(app_data)))
        apps_dirty = 0;
}
static void apps_pause(void) {
    block_game.paused = snake_game.paused = 1;
    apps_save();
    extra_save();
}
static void app_records(void) {
    if ((unsigned)snake_game.score > app_data.best_snake) {
        app_data.best_snake = snake_game.score;
        apps_dirty = 1;
    }
    if ((unsigned)block_game.score > app_data.best_blocks) {
        app_data.best_blocks = block_game.score;
        apps_dirty = 1;
    }
    if (mine_game.won && !mine_recorded) {
        mine_recorded = 1;
        app_data.mines_wins++;
        apps_dirty = 1;
    }
    if (word_game.won && !word_recorded) {
        word_recorded = 1;
        app_data.word_wins++;
        apps_dirty = 1;
    }
}
static void app_new(int target) {
    if (target == 23) {
        mines_new(&mine_game);
        mine_flags = mine_release = mine_recorded = 0;
    }
    if (target == 24) {
        hangman_new(&word_game, options[OPT_LANGUAGE]);
        word_recorded = 0;
    }
    if (target == 25)
        blocks_new(&block_game);
    if (target == 26)
        snake_new(&snake_game);
    app_ready[target - 20] = 1;
    app_next_tick = app_millis() + 650;
}
static void app_open(int target) {
    route_to(target);
    if (target == 21)
        app_calendar_load();
    if (target >= 23 && !app_ready[target - 20])
        app_new(target);
}
static int launcher_page;
static const int launcher_targets[] = {27, 28, 29, 31, 32, 33, 21, 22, 38, 36, 23,
                                       24, 25, 26, 48, 49, 52, 63, 64, 65, 66, 67};
static void app_launcher(void) {
    v1_launcher();
}
static void app_calendar(void) {
    heading_width("Kalender", 194);
    icon_button(468, 74, IC_TODAY);
    icon_button(528, 74, IC_NOTE_PLUS);
    const char *months[] = {"Januar", "Februar", "März",      "April",   "Mai",      "Juni",
                            "Juli",   "August",  "September", "Oktober", "November", "Dezember"};
    char label[80];
    snprintf(label, sizeof(label), "%s %d", tr(months[calendar_month - 1]), calendar_year);
    region(92, 130, 416, 44);
    centered(92, 130, 416, 44, label, ui_font(UI_TITLE));
    stroke(477, 149, 483, 155, 2);
    stroke(483, 155, 489, 149, 2);
    icon_button(28, 130, IC_PREV);
    icon_button(528, 130, IC_NEXT);
    const char *weekdays[] = {"Wochentag.Mo", "Wochentag.Di", "Wochentag.Mi", "Wochentag.Do",
                              "Wochentag.Fr", "Wochentag.Sa", "Wochentag.So"};
    for (int i = 0; i < 7; i++)
        centered(27 + i * 78, 194, 78, 34, weekdays[i], ui_font(UI_SECONDARY));
    divider(28, 227, 544);
    struct tm first = {
        .tm_year = calendar_year - 1900, .tm_mon = calendar_month - 1, .tm_mday = 1, .tm_hour = 12};
    mktime(&first);
    int offset = (first.tm_wday + 6) % 7, days = calendar_days(calendar_year, calendar_month),
        today = app_date_key(time(NULL));
    for (int d = 1; d <= days; d++) {
        int cell = offset + d - 1, x = 27 + (cell % 7) * 78, y = 234 + (cell / 7) * 58;
        region(x, y, 78, 56);
        char b[8];
        snprintf(b, sizeof(b), "%d", d);
        if (d == calendar_day)
            aa_disc(x + 39, y + 24, 22, 0);
        else if (calendar_year * 10000 + calendar_month * 100 + d == today)
            ring(x + 39, y + 24, 22, 2);
        ink_color = d == calendar_day ? 255 : (cell % 7 >= 5 ? 112 : 0);
        centered(x, y, 78, 48, b, ui_font(UI_BODY));
        ink_color = 0;
        int key = calendar_year * 10000 + calendar_month * 100 + d;
        if ((calendar_note_days & (1u << (d - 1))) || event_on16(key))
            disc(x + 61, y + 9, 2, 0);

        ReadingDay *r = app_day(key, 0);
        if (r && r->seconds)
            rect(x + 29, y + 55, 20, 2, 160);
    }
    divider(28, 592, 544);
    snprintf(label, sizeof(label), "%02d.%02d.%04d", calendar_day, calendar_month, calendar_year);
    ReadingDay *activity = app_day(calendar_year * 10000 + calendar_month * 100 + calendar_day, 0);
    if (activity) {
        char activity_label[100];
        snprintf(activity_label, sizeof(activity_label), "%s  ·  %u min", label,
                 activity->seconds / 60);
        drawtext(30, 601, activity_label, ui_font(UI_PRIMARY), 375);
    } else
        drawtext(30, 601, label, ui_font(UI_PRIMARY), 375);
    wrapped_raw(30, 642, *calendar_note ? calendar_note : tr("Keine Tagesnotiz"), ui_font(UI_SECONDARY), 370, 2);

    if (*calendar_note)
        icon_button(28, 681, IC_TRASH);
}
static void stat_card(int x, int y, const char *label, const char *value) {
    key_outline(x, y, 260, 108);
    label_at(x + 15, y + 12, 230, 24, tr(label), ui_font(UI_SECONDARY));
    label_ellipsis(x + 15, y + 44, 230, 44, tr(value), ui_font(UI_TITLE), 0);
}
static void app_restart(void) {
    icon_button(528, 74, IC_RESTART);
}
static void app_mines(void) {
    heading("Minesweeper");
    app_restart();
    char b[80];
    snprintf(b, sizeof(b), "%d / 12", mine_game.flags);
    drawtext(30, 129, b, ui_font(UI_BODY), 300);
    button_h(414, 122, 158, 36, mine_flags ? "Aufdecken" : "Fahne", mine_flags);
    for (int i = 0; i < 90; i++) {
        int x = 30 + (i % 9) * 60, y = 170 + (i / 9) * 58;
        if (!mine_game.open[i]) {
            rect(x, y, 58, 56, 215);
            if (!mine_game.over)
                region(x, y, 58, 56);
        } else {
            rect(x, y, 58, 56, 255);
            rect(x, y + 55, 58, 1, 230);
        }
        if (mine_game.flag[i])
            icon(x + 3, y + 2, IC_FLAG);
        else if (mine_game.bomb[i] && mine_game.over) {
            ring(x + 29, y + 28, 8, 3);
            stroke(x + 12, y + 28, x + 38, y + 28, 2);
            stroke(x + 29, y + 11, x + 29, y + 37, 2);
        } else if (mine_game.open[i]) {
            int n = mines_adjacent(&mine_game, i);
            if (n) {
                snprintf(b, sizeof(b), "%d", n);
                centered(x, y, 58, 56, b, ui_font(UI_BODY));
            }
        }
    }
    centered(28, 755, 544, 42,
             mine_game.over ? (mine_game.won ? "Gewonnen!" : "Leider eine Mine.")
                            : "Tippen: aufdecken · Halten: markieren",
             ui_font(UI_SECONDARY));
}
static void app_hangman(void) {
    heading("Wortraten");
    app_restart();
    stroke(118, 320, 300, 320, 2);
    stroke(154, 320, 154, 151, 2);
    stroke(154, 151, 260, 151, 2);
    stroke(260, 151, 260, 175, 2);
    int n = word_game.misses;
    if (n > 0)
        ring(260, 193, 17, 2);
    if (n > 1)
        stroke(260, 210, 260, 264, 2);
    if (n > 2)
        stroke(260, 224, 230, 245, 2);
    if (n > 3)
        stroke(260, 224, 290, 245, 2);
    if (n > 4)
        stroke(260, 264, 235, 300, 2);
    if (n > 5)
        stroke(260, 264, 285, 300, 2);
    int len = (int)strlen(word_game.word), w = 34, x = (600 - w * len) / 2;
    for (int i = 0; i < len; i++) {
        char ch[2] = {word_game.word[i], 0};
        if (word_game.over || (word_game.guesses & (1u << (ch[0] - 'A'))))
            centered(x + i * w, 365, w, 44, ch, ui_font(UI_PRIMARY));
        rect(x + i * w + 4, 414, w - 8, 2, 0);
    }
    for (int i = 0; i < 26; i++) {
        int row = i / 10, col = i % 10, cols = row == 2 ? 6 : 10,
            xx = (600 - cols * 54) / 2 + col * 54, y = 455 + row * 58;
        char key[2] = {(char)('A' + i), 0};
        if (word_game.guesses & (1u << i)) {
            rect(xx, y, 48, 48, 220);
            centered(xx, y, 48, 48, key, ui_font(UI_BODY));
        } else
            button_h(xx, y, 48, 48, key, 0);
    }
    if (word_game.over && !word_game.won) {
        char solution[80];
        snprintf(solution, sizeof(solution), "%s: %s", tr("Das Wort war"), word_game.word);
        centered(28, 626, 544, 40, solution, ui_font(UI_PRIMARY));
    }
    centered(28, 690, 544, 48,
             word_game.over
                 ? (word_game.won ? "Gewonnen!" : "Neues Spiel über das Neustart-Symbol.")
             : options[OPT_LANGUAGE] == 3 ? "Ein Wort, sechs Versuche (Pinyin)"
                                          : "Ein Wort, sechs Versuche",
             ui_font(UI_BODY));
}
static void board_tile(int x, int y, int size, int color) {
    rect(x + 1, y + 1, size - 2, size - 2, color);
}
static void app_blocks(void) {
    heading("Tetris");
    app_restart();
    button_h(414, 124, 158, 40,
             block_game.over     ? "Neu starten"
             : block_game.paused ? "Start"
                                 : "Pause",
             0);
    int x0 = 28, y0 = 132, cell = 36;
    rect(x0 - 1, y0 - 1, 362, 578, 170);
    rect(x0, y0, 360, 576, 255);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 10; x++)
            if (block_game.board[y][x])
                board_tile(x0 + x * cell, y0 + y * cell, cell, 65);
    if (!block_game.over) {
        int ghost = blocks_landing_y(&block_game);
        for (int y = 0; y < 4; y++)
            for (int x = 0; x < 4; x++)
                if (blocks_cell(block_game.piece, block_game.rotation, x, y)) {
                    int xx = x0 + (block_game.x + x) * cell, yy = y0 + (ghost + y) * cell;
                    if (ghost != block_game.y)
                        key_outline(xx + 2, yy + 2, cell - 4, cell - 4);
                    board_tile(xx, y0 + (block_game.y + y) * cell, cell, 0);
                }
    }
    char b[40];
    drawtext(410, 211, "Punkte", ui_font(UI_SECONDARY), 162);
    snprintf(b, sizeof(b), "%d", block_game.score);
    drawtext(410, 246, b, ui_font(UI_PRIMARY), 162);
    drawtext(410, 309, "Linien", ui_font(UI_SECONDARY), 162);
    snprintf(b, sizeof(b), "%d", block_game.lines);
    drawtext(410, 344, b, ui_font(UI_PRIMARY), 162);
    drawtext(410, 407, "Als Nächstes", ui_font(UI_SECONDARY), 162);
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            if (blocks_cell(block_game.next, 0, x, y))
                board_tile(408 + x * 22, 452 + y * 22, 22, 0);
    if (block_game.paused || block_game.over) {
        rect(75, 334, 238, 100, 255);
        centered(75, 345, 238, 50, block_game.over ? "Spiel beendet" : "Bereit?", ui_font(UI_PRIMARY));
        centered(75, 395, 238, 30, "Start oben antippen", ui_font(UI_SECONDARY));
    }
    int kinds[] = {IC_PREV, IC_ROTATE, IC_NEXT, IC_DOWN};
    for (int i = 0; i < 4; i++) {
        int x = 49 + i * 136;
        region(x, 735, 94, 48);
        key_outline(x, 735, 94, 48);
        icon(x + 25, 737, kinds[i]);
    }
}
static void app_snake(void) {
    heading("Snake");
    app_restart();
    char b[60];
    snprintf(b, sizeof(b), "%d  ·  %s %u", snake_game.score, tr("Rekord"), app_data.best_snake);
    drawtext(30, 132, b, ui_font(UI_BODY), 360);
    button_h(414, 124, 158, 40,
             snake_game.over     ? "Neu starten"
             : snake_game.paused ? "Start"
                                 : "Pause",
             0);
    int x0 = 30, y0 = 180, cell = 30;
    key_outline(x0 - 1, y0 - 1, 542, 482);
    if (snake_game.food >= 0)
        disc(x0 + (snake_game.food % 18) * cell + 15, y0 + (snake_game.food / 18) * cell + 15, 5,
             0);
    for (int i = 0; i < snake_game.length; i++) {
        int c = snake_game.cells[i];
        board_tile(x0 + (c % 18) * cell, y0 + (c / 18) * cell, cell, 0);
    }
    if (snake_game.length) {
        int head = snake_game.cells[0], hx = x0 + (head % 18) * cell + 15,
            hy = y0 + (head / 18) * cell + 15;
        disc(hx + snake_game.dx * 5 - snake_game.dy * 4, hy + snake_game.dy * 5 + snake_game.dx * 4,
             2, 255);
        disc(hx + snake_game.dx * 5 + snake_game.dy * 4, hy + snake_game.dy * 5 - snake_game.dx * 4,
             2, 255);
    }
    if (snake_game.paused || snake_game.over) {
        rect(166, 329, 268, 98, 255);
        centered(166, 337, 268, 44, snake_game.over ? "Spiel beendet" : "Bereit?", ui_font(UI_PRIMARY));
        centered(166, 383, 268, 36, "Wischen oder Pfeile", ui_font(UI_SECONDARY));
    }
    int x[] = {258, 153, 258, 363}, y[] = {677, 735, 735, 735},
        kind[] = {IC_UP, IC_PREV, IC_DOWN, IC_NEXT};
    for (int i = 0; i < 4; i++) {
        region(x[i], y[i], 84, 48);
        key_outline(x[i], y[i], 84, 48);
        icon(x[i] + 20, y[i] + 2, kind[i]);
    }
}
static void apps_draw(void) {
    switch (screen) {
    case 20:
        app_launcher();
        break;
    case 21:
        app_calendar();
        break;
    case 22:
        app_insights();
        break;
    case 23:
        app_mines();
        break;
    case 24:
        app_hangman();
        break;
    case 25:
        app_blocks();
        break;
    case 26:
        app_snake();
        break;
    }
}
static int apps_poll_interval(void) {
    if (sleeping)
        return -1;
    int active = screen == 25   ? !block_game.paused && !block_game.over
                 : screen == 26 ? !snake_game.paused && !snake_game.over
                                : 0;
    if (!active)
        return -1;
    int64_t left = app_next_tick - app_millis();
    return left < 0 ? 0 : left > 650 ? 650 : (int)left;
}
static int apps_input(const Input *in) {
    int x = in->x, y = in->y;
    if (in->kind == INPUT_STATUS) {
        if (screen == 25 && !block_game.paused && !block_game.over) {
            blocks_tick(&block_game);
            app_next_tick = app_millis() + 650;
        }
        if (screen == 26 && !snake_game.paused && !snake_game.over) {
            snake_tick(&snake_game);
            app_next_tick = app_millis() + 550;
        }
        app_records();
        return 1;
    }
    if (screen == 23 && in->kind == INPUT_HOLD && y >= 170 && y < 750 && x >= 30 && x < 570) {
        mines_flag(&mine_game, ((y - 170) / 58) * 9 + (x - 30) / 60);
        mine_release = 1;
        return 1;
    }
    if (screen == 26 && in->kind == INPUT_SWIPE) {
        if (abs(in->dx) > abs(in->dy))
            snake_direction(&snake_game, in->dx > 0 ? 1 : -1, 0);
        else
            snake_direction(&snake_game, 0, in->dy > 0 ? 1 : -1);
        return 1;
    }
    if (screen == 23 && mine_release && in->kind == INPUT_SWIPE) {
        mine_release = 0;
        return 1;
    }
    if (in->kind != INPUT_TAP)
        return 2;
    if (screen == 20) {
        v1_launcher_input(x, y);
        return 1;
    }
    if (screen >= 23 && x >= 516 && y >= 74 && y < 118) {
        app_new(screen);
        return 1;
    }
    if (screen == 21) {
        if (y >= 74 && y < 118 && x >= 468) {
            if (x >= 522) {
                static char date_title[80];
                snprintf(date_title, sizeof(date_title), "%02d.%02d.%04d", calendar_day,
                         calendar_month, calendar_year);
                open_keyboard(8, date_title, calendar_note);
            } else {
                time_t now = time(NULL);
                struct tm t;
                localtime_r(&now, &t);
                calendar_year = t.tm_year + 1900;
                calendar_month = t.tm_mon + 1;
                calendar_day = t.tm_mday;
                app_calendar_load();
            }
            return 1;
        }
        if (y >= 681 && y < 721 && x < 80) {
            extra_open(45);
            return 1;
        }
        if (y >= 130 && y < 174) {
            if (x >= 80 && x <= 520) {
                char b[32];
                snprintf(b, sizeof(b), "%04d-%02d", calendar_year, calendar_month);
                open_keyboard(40, "Monat: YYYY-MM", b);
                return 1;
            }
            if (x < 80) {
                calendar_month--;
                if (calendar_month < 1) {
                    calendar_month = 12;
                    calendar_year--;
                }
            } else if (x > 520) {
                calendar_month++;
                if (calendar_month > 12) {
                    calendar_month = 1;
                    calendar_year++;
                }
            }
            if (calendar_year < 1900)
                calendar_year = 1900;
            if (calendar_year > 2100)
                calendar_year = 2100;
            if (calendar_day > calendar_days(calendar_year, calendar_month))
                calendar_day = calendar_days(calendar_year, calendar_month);
            app_calendar_load();
        } else if (y >= 234 && y < 582 && x >= 27 && x < 573) {
            struct tm first = {.tm_year = calendar_year - 1900,
                               .tm_mon = calendar_month - 1,
                               .tm_mday = 1,
                               .tm_hour = 12};
            mktime(&first);
            int d = ((y - 234) / 58) * 7 + (x - 27) / 78 - (first.tm_wday + 6) % 7 + 1;
            if (d >= 1 && d <= calendar_days(calendar_year, calendar_month)) {
                calendar_day = d;
                app_calendar_load();
            }
        } else if (x >= 421 && y >= 622 && y < 666)
            open_keyboard(8, "Tagesnotiz", calendar_note);
        else if (x >= 421 && y >= 681 && y < 721) {
            time_t now = time(NULL);
            struct tm t;
            localtime_r(&now, &t);
            calendar_year = t.tm_year + 1900;
            calendar_month = t.tm_mon + 1;
            calendar_day = t.tm_mday;
            app_calendar_load();
        }
        return 1;
    }
    if (screen == 23) {
        if (mine_release) {
            mine_release = 0;
            return 1;
        }
        if (y >= 122 && y < 158 && x >= 414)
            mine_flags ^= 1;
        else if (y >= 170 && y < 750 && x >= 30 && x < 570) {
            int c = ((y - 170) / 58) * 9 + (x - 30) / 60;
            if (mine_flags)
                mines_flag(&mine_game, c);
            else
                mines_reveal(&mine_game, c);
        }
        app_records();
        return 1;
    }
    if (screen == 24 && y >= 455 && y < 629) {
        int row = (y - 455) / 58, cols = row == 2 ? 6 : 10, start = (600 - cols * 54) / 2,
            col = (x - start) / 54;
        if (x >= start && col < cols && row < 3 && y < 455 + row * 58 + 48)
            hangman_guess(&word_game, row * 10 + col);
        app_records();
        return 1;
    }
    if (screen == 25) {
        if (x >= 414 && y >= 124 && y < 164) {
            if (block_game.over)
                blocks_new(&block_game);
            block_game.paused = !block_game.paused;
            app_next_tick = app_millis() + 650;
        } else if (y >= 735 && y < 783) {
            int i = (x - 49) / 136;
            if (x >= 49 && i < 4) {
                if (i == 0)
                    blocks_move(&block_game, -1);
                if (i == 1)
                    blocks_rotate(&block_game);
                if (i == 2)
                    blocks_move(&block_game, 1);
                if (i == 3)
                    blocks_drop(&block_game);
            }
        }
        app_records();
        return 1;
    }
    if (screen == 26) {
        if (x >= 414 && y >= 124 && y < 164) {
            if (snake_game.over)
                snake_new(&snake_game);
            snake_game.paused = !snake_game.paused;
            app_next_tick = app_millis() + 550;
        } else if (y >= 677 && y < 725 && x >= 258 && x < 342)
            snake_direction(&snake_game, 0, -1);
        else if (y >= 735 && y < 783) {
            if (x >= 153 && x < 237)
                snake_direction(&snake_game, -1, 0);
            else if (x >= 258 && x < 342)
                snake_direction(&snake_game, 0, 1);
            else if (x >= 363 && x < 447)
                snake_direction(&snake_game, 1, 0);
        }
        return 1;
    }
    return 1;
}
