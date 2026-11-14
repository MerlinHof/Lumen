/* App-owned module; assembled by apps/host/compose.h. */
static int polish_power(const Input *in) {
    if (in->kind == INPUT_POWER && in->x == 2) {
        if (sleeping && !device_wake()) return 1;
        power_hold = 1;
        power_menu_wake_until = extra_millis() + 2500;
        power_menu = 1;
        return 1;
    }
    if (power_hold && in->kind == INPUT_POWER && in->x == 0) {
        power_hold = 0;
        return 1;
    }
    if (!power_menu)
        return 0;
    if (in->kind == INPUT_SLEEP) {
        if (power_hold || extra_millis() < power_menu_wake_until) {
            device_wake();
            return 0;
        }
        power_menu = 0;
        return 0;
    }
    if (in->kind == INPUT_DOWN) {
        pressed = hit(in->x, in->y);
        if (pressed >= 0)
            press_bounds = widgets[pressed];
        return pressed >= 0 ? 3 : 1;
    }
    if (in->kind == INPUT_TAP) {
        pressed = -1;
        int y = in->y;
        if (in->x < 58 || in->x > 532)
            return 1;
        if (y >= 292 && y < 350) {
            power_menu = 0;
            device_request_sleep();
        } else if (y >= 370 && y < 428) {
            power_request = 2;
            power_menu = 0;
            return 2;
        } else if (y >= 448 && y < 506) {
            power_request = 1;
            power_menu = 0;
            return 2;
        } else if (y >= 543 && y < 597)
            power_menu = 0;
        return 1;
    }
    return in->kind == INPUT_DOWN || in->kind == INPUT_MOVE || in->kind == INPUT_SWIPE ||
           in->kind == INPUT_HOLD;
}
static void polish_draw(void) {
    if (!power_menu)
        return;
    rect(27, 209, 558, 425, 175);
    rect(21, 203, 558, 425, 255);
    key_outline(21, 203, 558, 425);
    centered(28, 224, 544, 44, "Kindle", ui_font(UI_TITLE));
    icon_button(58, 298, IC_LOCK);
    button_h(116, 292, 416, 58, "Sperren", 0);
    icon_button(58, 376, IC_REBOOT);
    button_h(116, 370, 416, 58, "Neustarten", 0);
    icon_button(58, 454, IC_SHUTDOWN);
    button_h(116, 448, 416, 58, "Ausschalten", 0);
    button_h(58, 543, 474, 54, "Abbrechen", 0);
}
