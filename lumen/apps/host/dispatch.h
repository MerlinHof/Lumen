/* App view and event dispatch. Status and tab bars own their views in their app modules. */
static void app_render(void) {
    if (app_page_draw(ui_route())) return;
    if (ui_route() >= 60) {
        new_apps_draw();
        return;
    }
    switch (ui_route()) {
    case 9:
        more_settings();
        break;
    case 12:
        wifi_screen();
        break;
    case 13:
        settings_home();
        break;
    default:
        if (ui_route() >= 27)
            extra_draw();
        break;
    }
}
static int app_handle_input(const Input *in) {
    if(ui_route()==131 && !app_package_installed(APP_HOME) && in->kind==INPUT_TAP &&
       in->x>=28 && in->x<572) {
        if(in->y>=280 && in->y<328) {
            AppIntent intent={.app=APP_SETTINGS,.kind=INTENT_OPEN};
            app_send(&intent);
        } else if(in->y>=345 && in->y<393) {
            AppIntent intent={.app=APP_FILE_TRANSFER,.kind=INTENT_OPEN};
            app_send(&intent);
        }
        return 1;
    }
    int x = in->x, y = in->y, old = ui_route();
    int handled = app_page_input(ui_route(), in);
    if (handled >= 0) return handled;
    if (ui_route() >= 60) return new_apps_input(in);

    if (ui_route() == 40)
        return system_input(in);
    if (ui_route() >= 27 || ui_route() == 22)
        return extra_input(in);
    if (ui_route() == 13 && in->kind == INPUT_TAP && y >= 134 && y < 494) {
        int row = (y - 134) / 72;
        settings_ui.setting_page = 0;
        if (history_count < 64)
            history[history_count++] = 13;
        if (row == 0) {
            apps_pause();
            route_to(12);
            wifi_open();
        } else if (row == 1) {
            setting_category = 1;
            route_to(9);
        } else if (row == 2)
            extra_open(39);
        else if (row == 3)
            extra_open(83);
        else
            extra_open(40);
        return 1;
    }
    if (ui_route() == 12) {
        wifi_input(in);
        return wifi_ui.wifi_exit ? 0 : 1;
    }
    if (ui_route() == 9) {
        settings_input(in);
        return wifi_ui.wifi_exit ? 0 : 1;
    }
    if (ui_route() != old) {
        if (ui_route() == 1)
            history_count = 0;
        else {
            int existing = -1;
            for (int i = 0; i < history_count; i++)
                if (history[i] == ui_route())
                    existing = i;
            if (existing >= 0)
                history_count = existing;
            else if (history_count < 64)
                history[history_count++] = old;
        }
    }
    return 1;
}
