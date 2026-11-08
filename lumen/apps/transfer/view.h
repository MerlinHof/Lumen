/* App-owned module; assembled by apps/host/compose.h. */
static void connect_screen(void) {
    heading("Dateiübertragung");
    int active = connect_running();
    drawtext(30, 150, "Transfer-Server", ui_font(UI_PRIMARY), 400);
    toggle(492, 150, active);
    divider(28, 207, 544);
    if (active) {
        const char *address = connect_address();
        char seen[2048];
        snprintf(seen, sizeof(seen), "%s/connect-peer16.txt", data_root);
        drawtext(30, 218,
                 access(seen, F_OK) ? "Bereit · im selben WLAN öffnen"
                                    : "Verbunden · Zugriff bestätigt",
                 ui_font(UI_BODY), 542);
        drawtext(30, 258, "ADRESSE", ui_font(UI_SECONDARY), 542);
        wrapped_raw(30, 291, *address ? address : tr("Keine WLAN-Adresse"), ui_font(UI_PRIMARY), 542, 2);
        if (*address) {
            char url[256];
            snprintf(url, sizeof(url), "%s/#%s", address, connect_code());
            uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(10)],
                tmp[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
            if (qrcodegen_encodeText(url, tmp, qr, qrcodegen_Ecc_MEDIUM, 1, 10, qrcodegen_Mask_AUTO,
                                     true)) {
                int n = qrcodegen_getSize(qr), step = 240 / (n + 8), side = (n + 8) * step,
                    x = 300 - side / 2;
                for (int yy = 0; yy < n; yy++)
                    for (int xx = 0; xx < n; xx++)
                        if (qrcodegen_getModule(qr, xx, yy))
                            rect(x + (xx + 4) * step, 365 + (yy + 4) * step, step, step, 0);
            }
        }
        centered(28, 596, 544, 35, "Verbindungscode", ui_font(UI_SECONDARY));
        char code[80];
        snprintf(code, sizeof(code), "%s", connect_code());
        centered(28, 630, 544, 62, code, ui_font(UI_DISPLAY));
        char timeout[120];
        snprintf(timeout, sizeof(timeout), tr("Automatisches Ende nach %d min ohne Zugriff"),
                 options[OPT_TIMEOUT]);
        centered(28, 696, 544, 30, timeout, ui_font(UI_SECONDARY));
    } else {
        wrapped(30, 244,
                "Bücher und Fotos übertragen, Notizen exportieren und eigene Apps installieren. "
                "Aktiviere den Server und öffne den QR-Code im selben WLAN.",
                ui_font(UI_TITLE), 542, 5);
        wrapped(30, 484,
                "Der Server pausiert im Ruhemodus. Beim nächsten Start wird die aktuelle "
                "WLAN-Adresse angezeigt.",
                ui_font(UI_BODY), 542, 4);
    }
    icon_button(528, 76, IC_SETTINGS);
}
static void connect_input(const Input *in) {
    if (in->kind != INPUT_TAP)
        return;
    if (in->x >= 516 && in->y >= 72 && in->y < 124) {
        if (history_count < 64)
            history[history_count++] = 8;
        route_to(133);
        return;
    }
    if (in->y >= 142 && in->y < 198) {
        if (connect_running()) {
            connect_stop();
            refresh_library();
        } else if (!connect_start(options[OPT_PORT], options[OPT_TIMEOUT]))
            ui_message(connect_error());
    }
}

static void transfer_draw(void) {
    if (ui_route() == 133) {
        setting_category = 4;
        more_settings();
    } else connect_screen();
}
static int transfer_input(const Input *in) {
    if (ui_route() == 133) {
        setting_category = 4;
        settings_input(in);
    } else connect_input(in);
    return 1;
}
