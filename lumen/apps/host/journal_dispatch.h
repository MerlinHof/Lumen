/* App-owned module; assembled by apps/host/compose.h. */
static void u17_more_choice(int target, int choice) {
    if (target == 934 && choice >= 0 && choice < 7) {
        journal_store17();
        journal_step16 = choice;
    }
    if (target == 935 && choice >= 0 && choice < model_count17) {
        snprintf(llm_model16, sizeof(llm_model16), "%s", model_ids17[choice]);
        extra_write_text("llm-model.txt", llm_model16);
    }
}
static int u17_more_commit(int target, const char *text) {
    if (target == 176)
        models_loaded17 = 0;
    if (target == 171 && journal_date17) {
        if (journal_step16 < 6)
            snprintf(journal_answers16[journal_step16], 8192, "%s", text);
        journal_store17();
        return 1;
    }
    return 0;
}
static int u17_more_draw(void) {
    if (calendar_layout17())
        return 1;
    if (ui_route() == 110) {
        static unsigned generation;
        heading("Journal");
        if (generation != runtime.generation) {
            journal_list17();
            generation = runtime.generation;
        }
        for (int r = 0; r < 7 && journal_page17 * 7 + r < journal_count17; r++) {
            int date = journal_dates17[journal_page17 * 7 + r], y = 145 + r * 74;
            char b[100];
            snprintf(b, 100, "%02d.%02d.%04d", date % 100, date / 100 % 100, date / 10000);
            const char *detail =
                tr(date == app_date_key(time(NULL)) ? "Heute · Gedanken festhalten"
                                                    : "Öffnen und weiter schreiben");
            ListLayout l = list_row(28, y, 544, 74, -1, 62, b, detail, ui_font(UI_PRIMARY), 1);
            icon(528, l.action_y, IC_NEXT);
        }
        pager(745, &journal_page17, (journal_count17 + 6) / 7);
        return 1;
    }
    if (ui_route() == 126) {
        char b[100];
        snprintf(b, 100, "%02d.%02d.%04d", journal_date17 % 100, journal_date17 / 100 % 100,
                 journal_date17 / 10000);
        heading(b);
        select_row17(140, "Abschnitt", journal_prompts16[journal_step16]);
        if (journal_step16 < 6) {
            region(28, 218, 544, 480);
            wrapped_raw(30, 225,
                        *journal_answers16[journal_step16] ? journal_answers16[journal_step16]
                                                           : tr("Hier tippen und schreiben …"),
                        ui_font(UI_PRIMARY), 540, 13);
            icon_button(528, 74, IC_EDIT);
        } else {
            centered(28, 245, 544, 48, "Wie geht es dir heute?", ui_font(UI_PRIMARY));
            for (int i = 1; i <= 5; i++) {
                snprintf(b, 100, "%d", i);
                button_h(30 + (i - 1) * 110, 335, 100, 70, b, journal_score16 == i);
            }
            centered(28, 445, 544, 44, "1 · schwer    —    5 · sehr gut", ui_font(UI_BODY));
        }
        ink_color = UI_SECONDARY_INK;
        centered(28, 702, 544, 34, "Änderungen werden gespeichert.", ui_font(UI_SECONDARY));
        ink_color = 0;
        return 1;
    }
    if (ui_route() == 128) {
        chats_draw();
        return 1;
    }
    if (ui_route() == 122) {
        chat_draw();
        return 1;
    }
    if (ui_route() == 123) {
        heading_actions("OpenAI", 1);
        icon_button(528, 74, IC_RESTART);
        if (!models_loaded17) {
            models_parse17();
            models_loaded17 = 1;
            char key[2049] = "";
            config16("llm-key.txt", key, sizeof(key));
            if (*key && job_pid16 < 0)
                job_start16(3, "");
        }
        select_row17(155, "Modell", llm_model16);
        drawtext(30, 280, "API-Schlüssel", ui_font(UI_PRIMARY), 440);
        icon_button(528, 272, IC_EDIT);
        char key[2049] = "";
        config16("llm-key.txt", key, sizeof(key));
        wrapped_raw(30, 335, *key ? key : tr("Noch kein Schlüssel hinterlegt."), ui_font(UI_SECONDARY), 540, 10);
        wrapped(30, 620, "Ein Textmodell wählen. Die Liste stammt aus deinem API-Konto.", ui_font(UI_SECONDARY),
                540, 3);
        if (job_pid16 > 0)
            centered(28, 705, 544, 40, "Modelle werden geladen …", ui_font(UI_SECONDARY));
        return 1;
    }
    return u17_games_draw();
}
static int u17_more_input(const Input *in) {
    if (in->kind != INPUT_TAP)
        return u17_games_input(in);
    int x = in->x, y = in->y;
    if (ui_route() == 21 && calendar_view16 == 1 && y >= 200 && y < 660 && x >= 28 && x < 574) {
        time_t stamp = calendar_time16(calendar_year, calendar_month, calendar_day);
        struct tm t;
        localtime_r(&stamp, &t);
        t.tm_mday -= (t.tm_wday + 6) % 7;
        t.tm_mday += (x - 28) / 78;
        mktime(&t);
        calendar_year = t.tm_year + 1900;
        calendar_month = t.tm_mon + 1;
        calendar_day = t.tm_mday;
        calendar_view16 = 0;
        app_calendar_load();
        return 1;
    }
    if (ui_route() == 110 && y >= 145 && y < 663) {
        int i = journal_page17 * 7 + (y - 145) / 74;
        if (i < journal_count17)
            journal_open17(journal_dates17[i]);
        return 1;
    }
    if (ui_route() == 126) {
        if (y < 48 && x < 62) {
            journal_store17();
            extra_return(110);
            journal_list17();
            return 1;
        }
        if (y >= 140 && y < 188 && x >= 300) {
            open_dropdown(934, journal_prompts16, 7, journal_step16, 172, 194, 400);
            return 1;
        }
        if (journal_step16 < 6 && ((y >= 218 && y < 698) || (y >= 74 && y < 118 && x >= 516))) {
            open_keyboard(171, journal_prompts16[journal_step16],
                          journal_answers16[journal_step16]);
            editor_ui.note_cursor = strlen(editor_ui.edit);
            editor_ui.note_view_start = 0;
            return 1;
        }
        if (journal_step16 == 6 && y >= 335 && y < 405) {
            journal_score16 = 1 + (x - 30) / 110;
            if (journal_score16 < 1)
                journal_score16 = 1;
            if (journal_score16 > 5)
                journal_score16 = 5;
            journal_store17();
            return 1;
        }
        return y < 48 ? -1 : 1;
    }
    if (ui_route() == 128)
        return chats_input(in);
    if (ui_route() == 122)
        return chat_input(in);
    if (ui_route() == 123) {
        if (y >= 74 && y < 118 && x >= 516) {
            job_start16(3, "");
            return 1;
        }
        if (y >= 155 && y < 203 && x >= 300) {
            if (model_count17) {
                int chosen = 0;
                for (int i = 0; i < model_count17; i++)
                    if (!strcmp(model_ids17[i], llm_model16))
                        chosen = i;
                open_dropdown(935, model_labels17, model_count17, chosen, 240, 209, 332);
            } else
                job_start16(3, "");
            return 1;
        }
        if (y >= 272 && y < 316) {
            char key[2049] = "";
            config16("llm-key.txt", key, sizeof(key));
            open_keyboard(176, "API-Schlüssel", key);
            return 1;
        }
        return y < 48 ? -1 : 1;
    }
    return u17_games_input(in);
}
