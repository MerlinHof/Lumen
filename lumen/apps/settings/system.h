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
        select_row17(480,"Zeitzone",zone_names[options[OPT_ZONE] % 5]);
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
    const char *tabs[] = {"Update", "Speicher", "Verwaltung"};
    segments(28, 133, 544, 44, tabs, 3, extra_page);
    if (!extra_page) {
        release_poll();
        type_label(30,215,540,48,"LUMEN " LUMEN_VERSION,UI_TITLE);
        wrapped(30,276,
                "Diese Software wurde Ende 2026 von GPT-6.1 Sol entwickelt unter Aufsicht und "
                "Prompting und Lizenz von Merlin Hof.",ui_font(UI_BODY),540,4);
        char b[128];
        if(release_checked()) {
            struct tm t;time_t stamp=release_checked();localtime_r(&stamp,&t);
            strftime(b,sizeof(b),"%d.%m.%Y · %H:%M",&t);
            label_at(30,416,540,30,tr("Zuletzt geprüft"),ui_font(UI_SECONDARY));
            label_at(30,450,540,38,b,ui_font(UI_BODY));
        }
        int mb;
        int phase=release_progress(&mb);
        const char *state=release_state()==4 ? "Bereit zum Neustart" :
                          phase==1 ? "Update wird geladen …" :
                          phase==2 ? "Prüfsumme wird geprüft …" :
                          phase==3 ? "Installation wird vorbereitet …" :
                          release_state()==1 ? "Wird geprüft …" :
                          release_state()==2 ? "LUMEN ist aktuell." :
                          release_state()==3 ? "Neue Version verfügbar" : "Updates über GitHub";
        label_at(30,512,540,40,tr(state),ui_font(UI_PRIMARY));
        if(phase==1){snprintf(b,sizeof(b),"%d MiB",mb);label_at(30,556,540,34,b,ui_font(UI_BODY));}
        if(release_state()==3 || release_state()==4)label_at(30,556,540,34,release_version(),ui_font(UI_BODY));
        if(release_state()<0)wrapped(30,556,tr(release_error()),ui_font(UI_SECONDARY),540,3);
        button_h(28,672,544,54,release_state()==1 ? "Wird geprüft …" : "Nach Updates suchen",0);
        if(release_state()==3)action_button(28,598,544,54,"Update installieren");
        if(release_state()==4)action_button(28,598,544,54,"Jetzt neu starten");
        if(update_trial())button_h(28,738,544,44,"Update behalten",0);
    } else if (extra_page==1) {
        type_label(30,219,540,48,tr("Speicher"),UI_TITLE);
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
        drawtext(30, 310, b, ui_font(UI_PRIMARY), 540);
        rounded_clip_begin(28,370,544,28);
        rect(28, 370, 544, 28, 255);
        int x = 28;
        for (int i = 0; i < 5; i++) {
            int w = total ? (int)(544.0 * sizes[i] / total) : 0;
            if (i == 4)
                w = 572 - x;
            rect(x, 370, w, 28, colors[i]);
            if (i && w > 0)
                rect(x, 370, 2, 28, 255);
            x += w;
        }
        rounded_clip_end(28,370,544,28);
        key_outline(28, 370, 544, 28);
        for (int i = 0; i < 5; i++) {
            int xx = 30 + (i % 2) * 282, y = 425 + (i / 2) * 67;
            rect(xx, y + 7, 15, 15, colors[i]);
            if (i == 4)
                key_outline(xx, y + 7, 15, 15);
            label_at(xx + 28, y, 230, 28, tr(labels[i]), i == 2 ? ui_font(UI_SECONDARY) : ui_font(UI_BODY));
            storage_size(b, sizeof(b), sizes[i]);
            label_at(xx + 28, y + 29, 230, 22, b, ui_font(UI_SECONDARY));
        }
        wrapped(30, 650, "USB-Speicher und belegte Systempartition.", ui_font(UI_SECONDARY), 540, 1);
    } else {
        label_at(30, 216, 430, 48, tr("LUMEN nach dem Booten"), ui_font(UI_BODY));
        toggle(492, 224, options[OPT_AUTOSTART]);
        const char *actions[] = {"Konfiguration exportieren", "Konfiguration importieren", "Amazon öffnen", "Einstellungen zurücksetzen"};
        for (int i=0;i<4;i++)button_h(28,304+i*84,544,60,actions[i],0);
    }
}
static int system_input(const Input *in) {
    if (in->kind != INPUT_TAP)
        return 2;
    int x = in->x, y = in->y;
    if (y >= 133 && y < 177) {
        extra_page = (x-28)*3/544;
        if(extra_page<0)extra_page=0;if(extra_page>2)extra_page=2;
        if (extra_page==1)
            storage_scan();
        return 1;
    }
    if (!extra_page) {
        if(y>=672 && y<726 && release_state()!=1 && release_state()!=4)release_start(0);
        if(y>=598 && y<652 && release_state()==3)release_start(1);
        if(y>=598 && y<652 && release_activate()){
            ui_loading("Update wird installiert …");
            device_present_async(ui_pixels);
            return 0;
        }
        if(y>=738 && update_trial())update_keep();
        return 1;
    }
    if(extra_page==1)return 1;
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
        int i = 1+(y - 304) / 84;
        char p[2048];
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
            wifi_ui.wifi_exit = 1;
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
