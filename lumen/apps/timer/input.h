static int clock_input(int x, int y) {
    if (ui_route() == 28) {
        if (y >= 74 && y < 118 && x >= 28 && x < 572) {
            timer_panel = (x - 28) * 3 / 544;
            return 1;
        }
        if (!timer_panel) {
            if (y >= 137 && y < 181) {
                if (x >= 528) clock_ui.clock_fullscreen = 1;
                else if (x >= 28 && x < 338) {
                    open_dropdown(902, clock_styles, CLOCK_FACE_COUNT, clock_style_row(), 28, 185, 310);
                }
            }
            return 1;
        }
        if (timer_panel == 1 && y >= 185 && y < 229 && x >= 516) {
            extra_open(87);
            return 1;
        }
        if (timer_panel == 1) {
            if (y >= 397 && y < 445 && x >= 28) {
                const int presets[] = {5, 10, 20, 30};
                int i = (x - 28) / 140;
                if (i < 4)
                    timer_minutes = presets[i];
            } else if (y >= 472 && y < 524) {
                if (x < 108) {
                    if (timer_minutes > 1)
                        timer_minutes--;
                } else if (x >= 492) {
                    if (timer_minutes < 1440)
                        timer_minutes++;
                } else {
                    char b[32];
                    snprintf(b, sizeof(b), "%d", timer_minutes);
                    open_keyboard(36, "Timer: Minuten", b);
                }
            } else if (y >= 566 && y < 630) {
                timer_action(x >= 300);
            }
        } else {
            if (y>=258 && y<302) {
                if (x>=204 && x<272) alarm_hour=(alarm_hour+1)%24;
                else if (x>=327 && x<395) alarm_minute=(alarm_minute+1)%60;
            } else if (y>=443 && y<487) {
                if (x>=204 && x<272) alarm_hour=(alarm_hour+23)%24;
                else if (x>=327 && x<395) alarm_minute=(alarm_minute+59)%60;
            } else if (y>=522 && y<588) alarm_enabled^=1;
        }
        extra_save();
        return 1;
    }
    if (ui_route() == 46) {
        if (y >= 245 && y < 305) {
            if (x < 300)
                alarm_hour = (alarm_hour + 1) % 24;
            else
                alarm_minute = (alarm_minute + 1) % 60;
        } else if (y >= 415 && y < 475) {
            if (x < 300)
                alarm_hour = (alarm_hour + 23) % 24;
            else
                alarm_minute = (alarm_minute + 59) % 60;
        } else if (y >= 554 && y < 614) {
            extra_save();
            extra_return(28);
        }
        return 1;
    }
    return 0;
}
