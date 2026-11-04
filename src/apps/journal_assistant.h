#include "../system/markdown.h"
static char model_ids17[512][160];
static const char *model_labels17[512];
static int model_count17, models_loaded17;
static void models_parse17(void) {
    model_count17 = 0;
    char *s = service_read("llm-models17.json", 4194304);
    if (!s)
        return;
    Json j;
    JsonToken *t = calloc(50000, sizeof(*t));
    if (t && json_parse(&j, s, t, 50000)) {
        int a = json_get(&j, 0, "data");
        for (int i = 0; i < 512; i++) {
            int o = json_at(&j, a, i);
            if (o < 0)
                break;
            json_string(&j, json_get(&j, o, "id"), model_ids17[model_count17], 160);
            if (*model_ids17[model_count17])
                model_count17++;
        }
    }
    free(t);
    free(s);
    for (int i = 0; i < model_count17; i++)
        for (int k = i + 1; k < model_count17; k++)
            if (strcmp(model_ids17[k], model_ids17[i]) < 0) {
                char b[160];
                memcpy(b, model_ids17[i], 160);
                memcpy(model_ids17[i], model_ids17[k], 160);
                memcpy(model_ids17[k], b, 160);
            }
    for (int i = 0; i < model_count17; i++)
        model_labels17[i] = model_ids17[i];
}
static void reader_markdown17(const char *title, const char *text, int origin) {
    char dir[2048], path[4096];
    local_path(dir, sizeof(dir), "reader-documents");
    mkdir(dir, 0700);
    snprintf(path, sizeof(path), "%s/md-%016llx.html", dir,
             (unsigned long long)(press_id(title) ^ press_id(text)));
    FILE *f = fopen(path, "w");
    if (!f)
        return;
    fputs("<!doctype html><meta charset=utf-8><style>body{font-family:serif}p{margin:0 0 "
          ".7em}h1,h2,h3{line-height:1.15;margin:1em 0 "
          ".5em}pre{white-space:pre-wrap;font-size:.85em}blockquote{border-left:2px solid "
          "#888;padding-left:1em}li{margin-bottom:.3em}code{font-family:monospace}</style>",
          f);
    markdown_render17(f, text);
    fclose(f);
    if (file_open16(path, origin)) {
        snprintf(library[BOOK_LIMIT].title, sizeof(library[BOOK_LIMIT].title), "%s", title);
        snprintf(library[BOOK_LIMIT].author, sizeof(library[BOOK_LIMIT].author), "LUMEN");
    }
}
static int journal_date17, journal_dates17[2048], journal_count17, journal_page17;
typedef struct {
    int version, date, score;
    char answers[6][8192];
} Journal17;
static void journal_store17(void) {
    if (!journal_date17)
        return;
    Journal17 *d = calloc(1, sizeof(*d));
    if (!d)
        return;
    d->version = 1;
    d->date = journal_date17;
    d->score = journal_score16;
    memcpy(d->answers, journal_answers16, sizeof(d->answers));
    char dir[2048], p[4096];
    local_path(dir, sizeof(dir), "journals17");
    mkdir(dir, 0700);
    snprintf(p, sizeof(p), "%s/%08d.bin", dir, journal_date17);
    char name[80];
    snprintf(name, 80, "journals17/%08d.bin", journal_date17);
    save_blob(name, d, sizeof(*d));
    free(d);
}
static int journal_legacy17(int date) {
    for (int i = 0; i < note_count; i++) {
        char p[80];
        snprintf(p, 80, "notes/%d.txt", note_ids[i]);
        char *s = service_read(p, 65536);
        if (!s)
            continue;
        int day, month, year;
        int match = sscanf(s, "# Journal · %d.%d.%d", &day, &month, &year) == 3 &&
                    year * 10000 + month * 100 + day == date;
        if (match) {
            memset(journal_answers16, 0, sizeof(journal_answers16));
            char *scan = s;
            for (int k = 0; k < 6; k++) {
                char *h = strstr(scan, "\n## ");
                if (!h)
                    break;
                char *begin = strchr(h + 4, '\n');
                if (!begin)
                    break;
                begin++;
                while (*begin == '\n')
                    begin++;
                char *end = strstr(begin, "\n## ");
                if (!end)
                    end = strstr(begin, "\nStimmung:");
                if (!end)
                    end = begin + strlen(begin);
                int n = end - begin;
                while (n && begin[n - 1] == '\n')
                    n--;
                snprintf(journal_answers16[k], 8192, "%.*s", n > 8191 ? 8191 : n, begin);
                scan = end;
            }
            char *score = strstr(s, "Stimmung:");
            journal_score16 = score ? atoi(score + 9) : 3;
            free(s);
            return 1;
        }
        free(s);
    }
    return 0;
}
static void journal_open17(int date) {
    journal_date17 = date;
    journal_step16 = 0;
    journal_score16 = 3;
    memset(journal_answers16, 0, sizeof(journal_answers16));
    char name[80];
    snprintf(name, 80, "journals17/%08d.bin", date);
    Journal17 *d = malloc(sizeof(*d));
    if (d && load_blob(name, d, sizeof(*d)) && d->version == 1 && d->date == date) {
        memcpy(journal_answers16, d->answers, sizeof(d->answers));
        journal_score16 = d->score;
        for (int i = 0; i < 6; i++)
            journal_answers16[i][8191] = 0;
    } else
        journal_legacy17(date);
    free(d);
    if (journal_score16 < 1 || journal_score16 > 5)
        journal_score16 = 3;
    extra_open(126);
}
static void journal_list17(void) {
    journal_count17 = 1;
    journal_dates17[0] = app_date_key(time(NULL));
    char p[2048];
    local_path(p, sizeof(p), "journals17");
    DIR *d = opendir(p);
    struct dirent *e;
    while (d && (e = readdir(d)) && journal_count17 < 2048) {
        int date, n = 0;
        if (sscanf(e->d_name, "%8d.bin%n", &date, &n) != 1 || n != 12 || e->d_name[n] ||
            date == journal_dates17[0])
            continue;
        journal_dates17[journal_count17++] = date;
    }
    if (d)
        closedir(d);
    for (int i = 0; i < note_count && journal_count17 < 2048; i++) {
        char name[80];
        snprintf(name, 80, "notes/%d.txt", note_ids[i]);
        char *s = service_read(name, 65536);
        if (!s)
            continue;
        int day, m, y;
        if (sscanf(s, "# Journal · %d.%d.%d", &day, &m, &y) == 3) {
            int date = y * 10000 + m * 100 + day, exists = 0;
            for (int k = 0; k < journal_count17; k++)
                if (journal_dates17[k] == date)
                    exists = 1;
            if (!exists)
                journal_dates17[journal_count17++] = date;
        }
        free(s);
    }
    for (int i = 0; i < journal_count17; i++)
        for (int k = i + 1; k < journal_count17; k++)
            if (journal_dates17[k] > journal_dates17[i]) {
                int n = journal_dates17[i];
                journal_dates17[i] = journal_dates17[k];
                journal_dates17[k] = n;
            }
}
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
    if (target == 179)
        return cross_commit17(text);
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
    if (screen == 110) {
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
        pager(679, &journal_page17, (journal_count17 + 6) / 7);
        return 1;
    }
    if (screen == 126) {
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
    if (screen == 128) {
        chats_draw();
        return 1;
    }
    if (screen == 122) {
        chat_draw();
        return 1;
    }
    if (screen == 123) {
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
    if (screen == 21 && calendar_view16 == 1 && y >= 200 && y < 660 && x >= 28 && x < 574) {
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
    if (screen == 110 && y >= 145 && y < 663) {
        int i = journal_page17 * 7 + (y - 145) / 74;
        if (i < journal_count17)
            journal_open17(journal_dates17[i]);
        return 1;
    }
    if (screen == 126) {
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
            note_cursor = strlen(edit);
            note_view_start = 0;
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
    if (screen == 128)
        return chats_input(in);
    if (screen == 122)
        return chat_input(in);
    if (screen == 123) {
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
