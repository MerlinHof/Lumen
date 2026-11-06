/* App-owned module; assembled by apps/host/compose.h. */
static const char *sort_modes[] = {"Titel",           "Autor",      "Zuletzt gelesen",
                                   "Neu hinzugefügt", "Dateigröße", "Fortschritt"};
static int appearance_group(int key) {
    if (key == OPT_LOCK || key == OPT_LOCK_HINT || key == OPT_COVER_CROP) return 2;
    if (key == OPT_DATE || key == OPT_CLOCK || key == OPT_WIFI || key == OPT_BATTERY ||
        key == OPT_BATTERY_PERCENT) return 1;
    return 0;
}
static void more_settings(void) {
    heading(categories[setting_category]);
    if (setting_category == 1) {
        const char *labels[] = {"Allgemein","Statusleiste","Sperrbild"};
        segments(28,131,544,46,labels,3,settings_ui.setting_page);
    }
    if (setting_category == 2) {
        char b[100];
        snprintf(b, sizeof(b), tr("%d Bücher in deiner Bibliothek"), library_count);
        drawtext(30, 145, b, ui_font(UI_TITLE), 540);
        wrapped(30, 197, "Lesestände werden automatisch gespeichert.", ui_font(UI_SECONDARY), 540, 2);
        select_row17(279, "Sortierung", sort_modes[settings.sort % 6]);
        divider(28, 411, 544);
        button_h(28, 449, 544, 56, "Bibliothek neu einlesen", 0);
        button_h(28, 527, 544, 56, "Buchcover neu einlesen", 0);
        return;
    }
    if (setting_category == 3)
        u17_sleep_row(590);
    int row = 0;
    for (size_t i = 0; i < sizeof(setting_rows) / sizeof(*setting_rows); i++) {
        SettingRow *r = &setting_rows[i];
        if (r->category != setting_category)
            continue;
        if (setting_category == 1 && appearance_group(r->key) != settings_ui.setting_page)
            continue;
        int index = row++;
        int y = setting_category == 1 ? 207 + index*68 : 235 + index*91;
        form_label(30, y - 4, r->choices ? 312 : 310, 48, r->label);
        char value[80];
        int v = options[r->key];
        if (r->choices) {
            snprintf(value, sizeof(value), tr("%s"), r->choices[v >= 0 && v < r->count ? v : 0]);
            dropdown_button(360, y - 4, 210, 48, value);
        } else if (r->min == 0 && r->max == 1) {
            toggle(492, y + 4, v);
        } else {
            snprintf(value, sizeof(value), tr("%d"), v);
            label_at(350, y - 4, 70, 48, value, ui_font(UI_PRIMARY));
            button(428, y - 4, 62, "-", 0);
            button(505, y - 4, 62, "+", 0);
        }
    }
    if (setting_category == 4)
        wrapped(30, 580, "Änderungen gelten beim nächsten Serverstart.", ui_font(UI_BODY), 540, 2);
}
static void settings_input(const Input *in) {
    if (in->kind != INPUT_TAP)
        return;
    int x = in->x, y = in->y;
    if (setting_category == 1 && y >= 131 && y < 177 && x >= 28 && x < 572) {
        settings_ui.setting_page = (x-28)*3/544;
        return;
    }
    if (setting_category == 5 && y >= 145 && y < 193) {
        open_keyboard(4, "Store-Katalogadresse", store_ui.opds_source);
        return;
    }
    int row = 0;
    for (size_t i = 0; i < sizeof(setting_rows) / sizeof(*setting_rows); i++) {
        SettingRow *r = &setting_rows[i];
        if (r->category != setting_category)
            continue;
        if (setting_category == 1 && appearance_group(r->key) != settings_ui.setting_page)
            continue;
        int index = row++;
        int top = setting_category == 1 ? 203 + index*68 : 231 + index*91;
        if (y < top || y >= top + 48)
            continue;
        if (r->choices) {
            open_dropdown(100 + r->key, r->choices, r->count, options[r->key], 220, top + 50, 350);
            return;
        }
        if (r->max == 1) {
            options[r->key] = !options[r->key];
            if (r->key == OPT_AUTOSTART) {
                char path[2048];
                snprintf(path, sizeof(path), tr("%s/../DISABLE_AUTOSTART"), data_root);
                if (options[r->key])
                    unlink(path);
                else
                    atomic_write(path, "", 0);
            }
        } else {
            options[r->key] += x >= 505 ? r->step : x >= 428 ? -r->step : 0;
            if (options[r->key] < r->min)
                options[r->key] = r->min;
            if (options[r->key] > r->max)
                options[r->key] = r->max;
        }
        library_settings_save();
        engine_options(options[OPT_FONT], options[OPT_ALIGN], options[OPT_LETTERSPACE],
                       options[OPT_PARAGRAPH]);
        return;
    }
    if (setting_category == 2) {
        if (x >= 300 && y >= 279 && y < 327) {
            open_dropdown(900, sort_modes, 6, settings.sort, 300, 332, 272);
            return;
        }
        if (y >= 449 && y < 505) {
            refresh_library();
            ui_message("Bibliothek aktualisiert.");
        } else if (y >= 527 && y < 583) {
            for (int i = 0; i < library_count; i++) {
                char path[2048];
                library_cover_path(&library[i], path, sizeof(path));
                unlink(path);
            }
            refresh_library();
            ui_message("Buchcover neu eingelesen.");
        }
    }
}
