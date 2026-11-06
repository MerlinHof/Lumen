/* App-owned module; assembled by apps/host/compose.h. */
static void wifi_open(void) {
    wifi_ui.wifi_page = 0;
    wifi_ui.wifi_exit = 0;
    wifi_ui.wifi_enabled = device_wifi_enabled();
    network_status(wifi_ui.wifi_ssid, sizeof(wifi_ui.wifi_ssid));
    if (*wifi_ui.wifi_ssid)
        wifi_ui.wifi_enabled = 1;
    if (wifi_ui.wifi_enabled) {
        wifi_ui.wifi_scanning = network_scan();
        network_results();
    } else
        wifi_count = 0;
}
static void wifi_screen(void) {
    heading("WLAN");
    label_at(30, 137, 430, 48, tr("WLAN"), ui_font(UI_PRIMARY));
    toggle(492, 145, wifi_ui.wifi_enabled);
    divider(28, 209, 544);
    if (!wifi_ui.wifi_enabled) {
        wrapped(28, 258, "WLAN ist ausgeschaltet.", ui_font(UI_TITLE), 544, 2);
        return;
    }
    for (int r = 0; r < 6 && wifi_ui.wifi_page * 6 + r < wifi_count; r++) {
        WifiNetwork *n = &wifi_networks[wifi_ui.wifi_page * 6 + r];
        int y = 228 + r * 72;
        int connected = *wifi_ui.wifi_ssid && !strcmp(n->ssid, wifi_ui.wifi_ssid);
        const char *state = connected                 ? "Verbunden"
                            : n->saved                ? "Gespeichert"
                            : strstr(n->flags, "WPA") ? "Passwort geschützt"
                                                      : "Offenes Netz";
        ListLayout l =
            list_row(28, y, 544, 72, connected ? IC_DONE : (!n->saved && strstr(n->flags, "WPA")) ? IC_LOCK : -2, 76, n->ssid, tr(state), ui_font(UI_PRIMARY), 1);
        if (n->saved)
            icon_button(522, l.action_y, IC_TRASH);
    }
    if (!wifi_count)
        wrapped(28, 258, wifi_ui.wifi_scanning ? "Suche läuft …" : "Keine Netzwerke gefunden.", ui_font(UI_TITLE),
                544, 3);
    pager(745, &wifi_ui.wifi_page, (wifi_count + 5) / 6);
    icon_button(528, 745, IC_RESTART);
}
static void wifi_input(const Input *in) {
    if (in->kind != INPUT_TAP)
        return;
    int x = in->x, y = in->y;
    if (y >= 137 && y < 193) {
        int enabled = !wifi_ui.wifi_enabled;
        if (!device_wifi(enabled)) {
            ui_message("WLAN konnte nicht geändert werden.");
            return;
        }
        wifi_ui.wifi_enabled = enabled;
        if (!enabled) {
            connect_stop();
            discover_cancel();
            network_disconnect();
            wifi_ui.wifi_ssid[0] = 0;
            wifi_count = 0;
            wifi_ui.wifi_scanning = 0;
        } else {
            wifi_ui.wifi_scanning = network_scan();
            network_results();
        }
        return;
    }
    if (!wifi_ui.wifi_enabled)
        return;
    if (y >= 228 && y < 660) {
        int i = wifi_ui.wifi_page * 6 + (y - 228) / 72;
        if (i >= wifi_count)
            return;
        WifiNetwork *n = &wifi_networks[i];
        wifi_ui.wifi_selected = i;
        if (x >= 510 && n->saved) {
            int connected = !strcmp(n->ssid, wifi_ui.wifi_ssid);
            if (network_forget(i)) {
                if (connected) {
                    connect_stop();
                    network_disconnect();
                    wifi_ui.wifi_ssid[0] = 0;
                }
                ui_message("Netzwerk vergessen.");
            } else
                ui_message(network_error());
        } else if (x < 510) {
            if (n->saved || !strstr(n->flags, "WPA"))
                ui_message(network_connect(i, "") ? "Verbindung wird hergestellt …"
                                                  : network_error());
            else
                open_keyboard(5, n->ssid, "");
        }
    } else if (y >= 745 && y < 789) {
        if (x >= 430) {
            wifi_ui.wifi_scanning = network_scan();
            if (!wifi_ui.wifi_scanning)
                ui_message(network_error());
        } else if (x < 300 && wifi_ui.wifi_page)
            wifi_ui.wifi_page--;
        else if (x >= 300 && (wifi_ui.wifi_page + 1) * 6 < wifi_count)
            wifi_ui.wifi_page++;
    }
}
