/* App-owned module; assembled by apps/host/compose.h. */
static void app_time_settings(void) {
    heading("Datum & Uhrzeit");
    char b[80];
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    strftime(b, sizeof(b), "%d.%m.%Y  %H:%M", &t);
    key_outline(28, 140, 544, 108);
    centered(28, 140, 544, 108, b, ui_font(UI_TITLE));
    label_at(30, 290, 430, 48, tr("Automatisch über WLAN"), ui_font(UI_BODY));
    toggle(492, 298, options[OPT_AUTO_TIME]);
    if (options[OPT_AUTO_TIME]) {
        wrapped(30, 385, "Datum und Uhrzeit werden täglich über WLAN abgeglichen.", ui_font(UI_BODY), 540, 3);

    } else {
        button_h(28, 387, 544, 54, "Datum und Uhrzeit setzen", 0);
        label_at(30, 480, 220, 52, tr("Zeitzone"), ui_font(UI_BODY));
        dropdown_button(270, 480, 302, 52, zone_names[options[OPT_ZONE] % 5]);
    }
}
static uint64_t directory_bytes(const char *p, int depth) {
    if (depth > 12)
        return 0;
    struct stat st;
    if (lstat(p, &st))
        return 0;
    if (S_ISLNK(st.st_mode))
        return 0;
    if (S_ISREG(st.st_mode))
        return (uint64_t)st.st_size;
    if (!S_ISDIR(st.st_mode))
        return 0;
    DIR *d = opendir(p);
    if (!d)
        return 0;
    uint64_t n = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.')
            continue;
        char child[2048];
        if (snprintf(child, sizeof(child), "%s/%s", p, e->d_name) >= (int)sizeof(child))
            continue;
        n += directory_bytes(child, depth + 1);
    }
    closedir(d);
    return n;
}
static uint64_t storage_books, storage_system, storage_free, storage_total, storage_firmware;
static void storage_scan(void) {
#ifndef LUMEN_HOST
    struct statvfs firmware;
    if (!statvfs("/", &firmware))
        storage_firmware = (uint64_t)(firmware.f_blocks - firmware.f_bfree) * firmware.f_frsize;
#endif
    struct statvfs v;
    if (!statvfs(data_root, &v)) {
        storage_total = (uint64_t)v.f_blocks * v.f_frsize;
        storage_free = (uint64_t)v.f_bavail * v.f_frsize;
    }
    storage_books = directory_bytes(library_root, 0);
    char root[2048];
    snprintf(root, sizeof(root), "%s/..", data_root);
    storage_system = directory_bytes(root, 0);
}
static void storage_size(char *out, int cap, uint64_t n) {
    snprintf(out, cap, "%.1f %s", n >= 1073741824 ? n / 1073741824.0 : n / 1048576.0,
             n >= 1073741824 ? "GB" : "MB");
}
static void system_screen(void) {
    heading("System");
    const char *tabs[] = {"Speicher & Version", "Verwaltung"};
    segments(28, 133, 544, 44, tabs, 2, extra_page);
    if (!extra_page) {
        drawtext(30, 205, "LUMEN 2.1", ui_font(UI_TITLE), 540);
        drawtext(30, 253, "Kindle KT2 · 600 × 800", ui_font(UI_SECONDARY), 540);
        wrapped(30, 289,
                "Diese Software wurde Ende 2026 von GPT-6.1 Sol entwickelt unter Aufsicht und "
                "Prompting und Lizenz von Merlin Hof.",
                ui_font(UI_SECONDARY), 540, 3);
        uint64_t total = storage_total, free = storage_free, books = storage_books,
                 lumen = storage_system;
        if (free > total)
            free = total;
        if (books > total - free)
            books = total - free;
        if (lumen > total - free - books)
            lumen = total - free - books;
        uint64_t sizes[] = {books, lumen, storage_firmware, total - free - books - lumen, free};
        total += storage_firmware;
        const char *labels[] = {"Bücher", "LUMEN & Daten", "Firmware (Systempartition)", "Andere",
                                "Frei"};
        int colors[] = {0, 72, 120, 175, 255};
        char b[100], size[40];
        storage_size(size, sizeof(size), total);
        snprintf(b, sizeof(b), "%s %s", size, tr("Gesamt"));
        drawtext(30, 383, b, ui_font(UI_PRIMARY), 540);
        rect(28, 438, 544, 28, 255);
        int x = 28;
        for (int i = 0; i < 5; i++) {
            int w = total ? (int)(544.0 * sizes[i] / total) : 0;
            if (i == 4)
                w = 572 - x;
            rect(x, 438, w, 28, colors[i]);
            if (i && w > 0)
                rect(x, 438, 2, 28, 255);
            x += w;
        }
        key_outline(28, 438, 544, 28);
        for (int i = 0; i < 5; i++) {
            int xx = 30 + (i % 2) * 282, y = 486 + (i / 2) * 67;
            rect(xx, y + 7, 15, 15, colors[i]);
            if (i == 4)
                key_outline(xx, y + 7, 15, 15);
            label_at(xx + 28, y, 230, 28, tr(labels[i]), i == 2 ? ui_font(UI_SECONDARY) : ui_font(UI_BODY));
            storage_size(b, sizeof(b), sizes[i]);
            label_at(xx + 28, y + 29, 230, 22, b, ui_font(UI_SECONDARY));
        }
        wrapped(30, 695, "USB-Speicher und belegte Systempartition.", ui_font(UI_SECONDARY), 540, 1);
    } else {
        label_at(30, 216, 430, 48, tr("LUMEN nach dem Booten"), ui_font(UI_BODY));
        toggle(492, 224, options[OPT_AUTOSTART]);
        const char *actions[] = {"Automatischer Ruhezustand", "Konfiguration exportieren",
                                 "Konfiguration importieren", "Amazon öffnen",
                                 "Einstellungen zurücksetzen"};
        for (int i = 0; i < 5; i++) {
            if (!i)
                u17_sleep_row(304);
            else
                button_h(28, 304 + i * 84, 544, 60, actions[i], 0);
        }
    }
}
static int system_input(const Input *in) {
    if (in->kind != INPUT_TAP)
        return 2;
    int x = in->x, y = in->y;
    if (y >= 133 && y < 177) {
        extra_page = x >= 300;
        if (!extra_page)
            storage_scan();
        return 1;
    }
    if (!extra_page)
        return 1;
    if (y >= 210 && y < 270) {
        options[OPT_AUTOSTART] ^= 1;
        char p[2048];
        local_path(p, sizeof(p), "../DISABLE_AUTOSTART");
        if (options[OPT_AUTOSTART]) {
#ifndef LUMEN_HOST
            if (access("/etc/upstart/lumen.conf", F_OK) != 0 &&
                system("/bin/sh /mnt/us/lumen/install-autostart.sh") != 0) {
                options[OPT_AUTOSTART] = 0;
                ui_message("Speichern fehlgeschlagen.");
            }
#endif
            if (options[OPT_AUTOSTART])
                unlink(p);
        } else
            atomic_write(p, "", 0);
        library_settings_save();
        return 1;
    }
    if (y >= 304 && y < 700) {
        int i = (y - 304) / 84;
        char p[2048];
        if (i == 0)
            u17_sleep_select(304);
        if (i == 1) {
            local_path(p, sizeof(p), "../LUMEN-Einstellungen.txt");
            ui_message(library_config_export(p) ? "Export im Ordner lumen gespeichert."
                                                : "Export fehlgeschlagen.");
        }
        if (i == 2) {
            local_path(p, sizeof(p), "../LUMEN-Einstellungen.txt");
            ui_message(library_config_import(p)
                           ? "Einstellungen importiert."
                           : "Importdatei fehlt oder enthält ungültige Werte.");
        }
        if (i == 3)
            wifi_exit = 1;
        if (i == 4)
            extra_open(41);
    }
    return 1;
}
static void app_reset(void) {
    heading("Zurücksetzen");
    wrapped(30, 172,
            "Alle Einstellungen auf Standard zurücksetzen? Bücher, Lesestände, Notizen, "
            "Zeichnungen und Zitate bleiben erhalten.",
            ui_font(UI_TITLE), 540, 5);
    button(28, 455, 544, "Einstellungen zurücksetzen", 0);
    button(28, 530, 544, "Abbrechen", 0);
}
