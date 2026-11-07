/* App-owned module; assembled by apps/host/compose.h. */
static void apps_draw(void) {
    switch (ui_route()) {
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
    int active = ui_route() == 25   ? !block_game.paused && !block_game.over
                 : ui_route() == 26 ? !snake_game.paused && !snake_game.over
                                : 0;
    if (!active)
        return -1;
    int64_t left = app_next_tick - app_millis();
    return left < 0 ? 0 : left > 650 ? 650 : (int)left;
}
static int apps_input(const Input *in) {
    int x = in->x, y = in->y;
    if (in->kind == INPUT_STATUS) {
        int changed=0;
        if (ui_route() == 25 && !block_game.paused && !block_game.over && app_millis() >= app_next_tick) {
            changed=1;
            blocks_tick(&block_game);
            app_next_tick = app_millis() + 650;
        }
        if (ui_route() == 26 && !snake_game.paused && !snake_game.over && app_millis() >= app_next_tick) {
            changed=1;
            snake_tick(&snake_game);
            app_next_tick = app_millis() + 550;
        }
        if (changed) app_records();
        return changed ? 1 : 2;
    }
    if (ui_route() == 23 && in->kind == INPUT_HOLD && y >= 170 && y < 750 && x >= 30 && x < 570) {
        mines_flag(&mine_game, ((y - 170) / 58) * 9 + (x - 30) / 60);
        mine_release = 1;
        return 1;
    }
    if (ui_route() == 26 && in->kind == INPUT_SWIPE) {
        if (abs(in->dx) > abs(in->dy))
            snake_direction(&snake_game, in->dx > 0 ? 1 : -1, 0);
        else
            snake_direction(&snake_game, 0, in->dy > 0 ? 1 : -1);
        return 1;
    }
    if (ui_route() == 23 && mine_release && in->kind == INPUT_SWIPE) {
        mine_release = 0;
        return 1;
    }
    if (in->kind != INPUT_TAP)
        return 2;
    if (ui_route() >= 23 && x >= 516 && y >= 74 && y < 118) {
        app_new(ui_route());
        return 1;
    }
    if (ui_route() == 21) {
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
    if (ui_route() == 23) {
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
    if (ui_route() == 24 && y >= 455 && y < 629) {
        int row = (y - 455) / 58, cols = row == 2 ? 6 : 10, start = (600 - cols * 54) / 2,
            col = (x - start) / 54;
        if (x >= start && col < cols && row < 3 && y < 455 + row * 58 + 48)
            hangman_guess(&word_game, row * 10 + col);
        app_records();
        return 1;
    }
    if (ui_route() == 25) {
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
    if (ui_route() == 26) {
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
