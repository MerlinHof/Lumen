/* Compact system controls and app refinements, shared by every application. */
static void u17_gray_icon(int x, int y) {
    for (int yy = y; yy < y + 44 && yy < 800; yy++)
        for (int xx = x; xx < x + 44 && xx < 600; xx++)
            if (xx >= 0 && yy >= 0) {
                unsigned char *v = &ui_pixels[yy * 600 + xx];
                if (*v < 255)
                    *v = 135 + *v * 120 / 255;
            }
}
static int u17_notes_depth(void) {
    return note_tree.current != 0;
}
static void u17_notes_up(void) {
    int i = folder15_index(note_tree.current);
    note_tree.current = i >= 0 ? note_tree.folder[i].parent : 0;
    tree_page = 0;
}
static void u17_notes_menu(int kind, int id, int y) {
    notes_kind17 = kind;
    notes_id17 = id;
    static const char *actions[] = {"Verschieben", "Löschen"};
    open_dropdown(932, actions, 2, -1, 328, y + 46, 244);
}
static void u17_sleep_row(int y) {
    int i = 1;
    for (int j = 0; j < 6; j++)
        if (auto_sleep_minutes == sleep_minutes16[j])
            i = j;
    select_row17(y, "Ruhezustand", sleep_choices16[i]);
}
static void u17_sleep_select(int y) {
    int i = 1;
    for (int j = 0; j < 6; j++)
        if (auto_sleep_minutes == sleep_minutes16[j])
            i = j;
    open_dropdown(917, sleep_choices16, 6, i, 300, y + 53, 272);
}
static int folder_ids17[513], folder_count17;
static char folder_names17[513][240];
static const char *folder_labels17[513];
static void folder_menu17(void) {
    folder_count17 = 1;
    folder_ids17[0] = 0;
    snprintf(folder_names17[0], 240, "%s", tr("Notizen"));
    for (int i = 0; i < note_tree.nf; i++) {
        folder_ids17[folder_count17] = note_tree.folder[i].id;
        snprintf(folder_names17[folder_count17], 240, "%s%s%s",
                 note_tree.folder[i].parent ? v1_folder_name(note_tree.folder[i].parent) : "",
                 note_tree.folder[i].parent ? " / " : "", note_tree.folder[i].name);
        folder_count17++;
    }
    for (int i = 0; i < folder_count17; i++)
        folder_labels17[i] = folder_names17[i];
    open_dropdown(936, folder_labels17, folder_count17, -1, 190, 200, 382);
}
static void u17_choice(int target, int choice) {
    if (target == 936 && choice >= 0 && choice < folder_count17) {
        int dest = folder_ids17[choice];
        if (notes_kind17) {
            if (folder15_inside(dest, notes_id17)) {
                ui_message("Ziel liegt im selben Ordner.");
                return;
            }
            int f = folder15_index(notes_id17);
            if (f >= 0)
                note_tree.folder[f].parent = dest;
            v1_dirty = 1;
        } else
            v1_place(notes_id17, dest);
        v1_save();
        return;
    }

    if (target == 931) {
        calc_degrees = choice == 0;
        extra_save();
    }
    if (target == 932) {
        if (!choice)
            folder_menu17();
        else if (notes_kind17)
            u15_notes_action(2, notes_id17);
        else {
            note_load(notes_id17);
            extra_open(43);
        }
    }
    if (target == 933) {
        AppIntent i = {.app = APP_PLANETARIUM, .kind = INTENT_OPEN};
        if (!choice)
            app_send(&i);
        else
            extra_open(125);
    }
    u17_more_choice(target, choice);
}
static int u17_commit(int target, const char *text) {
    if (target == 180) {
        if (task_edit17 >= 0 && task_edit17 < task_count && *text) {
            snprintf(tasks[task_edit17].title, 200, "%s", text);
            v1_dirty = 1;
            v1_save();
        }
        return 1;
    }
    return u17_more_commit(target, text);
}
static int u17_input(const Input *in) {
    if (sleeping || keyboard || dropdown)
        return -1;
    if (in->kind == INPUT_TAP && screen == 40 && extra_page == 1 && in->y >= 304 && in->y < 364 &&
        in->x >= 300) {
        u17_sleep_select(304);
        return 1;
    }
    if (in->kind == INPUT_TAP && (screen == 79 || screen == 125) && in->y >= 74 && in->y < 118 &&
        in->x >= 300 && in->x < 516) {
        static const char *t[] = {"Sternhimmel", "Sonnensystem"};
        open_dropdown(933, t, 2, screen == 125, 300, 123, 216);
        return 1;
    }
    return u17_more_input(in);
}
static int u17_draw(void) {
    return u17_more_draw();
}
static int calendar_layout17(void) {
    if (screen != 21 || calendar_view16 == 2 || calendar_view16 == 3)
        return 0;
    heading_width("Kalender", 194);
    const char *views[] = {"Tag", "Woche", "Monat", "Jahr"};
    dropdown_button(250, 74, 145, 44, views[calendar_view16]);
    icon_button(468, 74, IC_TODAY);
    icon_button(528, 74, IC_NOTE_PLUS);
    icon_button(28, 136, IC_PREV);
    icon_button(528, 136, IC_NEXT);
    time_t stamp = calendar_time16(calendar_year, calendar_month, calendar_day);
    struct tm t;
    localtime_r(&stamp, &t);
    char label[160];
    if (calendar_view16 == 1) {
        char week[12], year[12];
        strftime(week, 12, "%V", &t);
        strftime(year, 12, "%G", &t);
        snprintf(label, sizeof(label), tr("Woche %s · %s"), week, year);
        centered(90, 136, 420, 44, label, &body);
        t.tm_mday -= (t.tm_wday + 6) % 7;
        const char *days[] = {"Wochentag.Mo", "Wochentag.Di", "Wochentag.Mi", "Wochentag.Do",
                              "Wochentag.Fr", "Wochentag.Sa", "Wochentag.So"};
        for (int col = 0; col < 7; col++) {
            struct tm day = t;
            day.tm_mday += col;
            mktime(&day);
            int x = 28 + col * 78;
            centered(x, 200, 78, 35, days[col], &caption);
            snprintf(label, 160, "%02d.%02d.", day.tm_mday, day.tm_mon + 1);
            centered(x, 235, 78, 38, label, &caption);
            if (date16(&day) == app_date_key(time(NULL)))
                rect(x + 14, 274, 50, 3, 0);
            rect(x, 289, 1, 370, 205);
            region(x, 200, 78, 460);
            event_lines16(date16(&day), x + 7, 306, 64, 12);
        }
        rect(572, 289, 1, 370, 205);
        rect(28, 288, 544, 2, 180);
        rect(28, 658, 544, 2, 180);
        centered(28, 683, 544, 36, "Tag antippen zum Öffnen", &caption);
    } else {
        const char *months[] = {"Januar",    "Februar", "März",     "April",
                                "Mai",       "Juni",    "Juli",     "August",
                                "September", "Oktober", "November", "Dezember"};
        const char *weekdays[] = {"Sonntag",    "Montag",  "Dienstag", "Mittwoch",
                                  "Donnerstag", "Freitag", "Samstag"};
        snprintf(label, sizeof(label), "%s %d", tr(months[calendar_month - 1]), calendar_year);
        centered(90, 136, 420, 44, label, &small);
        snprintf(label, sizeof(label), "%02d", calendar_day);
        drawtext_raw(30, 208, label, &title, 94);
        drawtext(132, 208, weekdays[t.tm_wday], &body, 425);
        snprintf(label, sizeof(label), "%02d.%02d.%04d", calendar_day, calendar_month,
                 calendar_year);
        ink_color = 130;
        drawtext_raw(134, 253, label, &caption, 400);
        ink_color = 0;
        divider(28, 294, 544);
        drawtext(30, 315, "Tagesnotiz", &editorial, 480);
        if (*calendar_note) {
            static int date;
            int key = date16(&t);
            if (date != key) {
                calendar_day_page16 = 0;
                date = key;
            }
            const char *p = calendar_note;
            int lines = 0;
            while (*p) {
                p = md_next(p, 540, &body);
                lines++;
            }
            int pages = (lines + 7) / 8;
            if (calendar_day_page16 >= pages)
                calendar_day_page16 = 0;
            p = calendar_note;
            for (int i = 0; i < calendar_day_page16 * 8 && *p; i++)
                p = md_next(p, 540, &body);
            const char *end = p;
            for (int i = 0; i < 8 && *end; i++)
                end = md_next(end, 540, &body);
            char text[2048];
            snprintf(text, sizeof(text), "%.*s", (int)(end - p), p);
            wrapped_raw(30, 372, text, &body, 540, 8);
            pager(679, &calendar_day_page16, pages);
        } else {
            ink_color = 135;
            wrapped(30, 379, "Noch keine Notiz für diesen Tag.", &small, 540, 3);
            drawtext(30, 460, "Mit + eine Notiz hinzufügen.", &caption, 540);
            ink_color = 0;
        }
    }

    return 1;
}
