static struct {
    MemoryCard cards[3][24];
    int attempts, correct, day, count, lastday, streak;
} language_progress;
static int language_ready, language_tab, language_answered, language_options[4],
    language_choice = -1, language_book_page;
static char language_written[512];
static int language_source(void) { return lang_pair == 2 ? 1 : 0; }
static int language_target(void) { return lang_pair == 0 ? 1 : lang_pair == 1 ? 2 : 0; }
static void language_init(void) {
    if (language_ready)
        return;
    language_ready = 1;
    if (!load_blob("language-plus-1.1.bin", &language_progress, sizeof(language_progress)))
        memset(&language_progress, 0, sizeof(language_progress));
}
static void language_new(void) {
    language_init();
    language_answered = 0;
    language_choice = -1;
    lang_reveal = 0;
    language_written[0] = 0;
    int found = -1;
    for (int i = 0; i < 24; i++)
        if (language_progress.cards[lang_pair][i].due <= time(NULL) &&
            (found < 0 || language_progress.cards[lang_pair][i].due <
                              language_progress.cards[lang_pair][found].due))
            found = i;
    lang_index = found >= 0 ? found : pro_random() % 24;
    int correct = pro_random() % 4;
    for (int i = 0; i < 4; i++) {
        if (i == correct) {
            language_options[i] = lang_index;
            continue;
        }
        int q;
        do {
            q = pro_random() % 24;
            int dup = q == lang_index;
            for (int j = 0; j < i; j++)
                if (language_options[j] == q)
                    dup = 1;
            if (!dup)
                break;
        } while (1);
        language_options[i] = q;
    }
}
static void language_grade(int good) {
    language_progress.attempts++;
    language_progress.correct += good;
    int today = news_date();
    if (language_progress.day != today) {
        language_progress.day = today;
        language_progress.count = 0;
    }
    language_progress.count++;
    if (language_progress.lastday != today) {
        time_t t = time(NULL) - 86400;
        struct tm y;
        localtime_r(&t, &y);
        int yesterday = (y.tm_year + 1900) * 10000 + (y.tm_mon + 1) * 100 + y.tm_mday;
        language_progress.streak =
            language_progress.lastday == yesterday ? language_progress.streak + 1 : 1;
        language_progress.lastday = today;
    }
    fsrs_review(&language_progress.cards[lang_pair][lang_index], good ? 3 : 1, time(NULL));
    save_blob("language-plus-1.1.bin", &language_progress, sizeof(language_progress));
    language_answered = good ? 1 : 2;
}
static void language_normalize(const char *s, char *out, int n) {
    char folded[1024];
    utf8_fold(s, folded, sizeof(folded));
    int k = 0;
    for (const unsigned char *p = (unsigned char *)folded; *p && k < n - 1; p++)
        if (isalnum(*p) || *p >= 128)
            out[k++] = *p;
    out[k] = 0;
}
static void language_check_written(const char *text) {
    snprintf(language_written, sizeof(language_written), "%s", text);
    char a[1024], b[1024];
    language_normalize(text, a, sizeof(a));
    language_normalize(v1_lessons[lang_index][language_target()], b, sizeof(b));
    language_grade(!strcmp(a, b));
}
static void polish_language(void) {
    language_init();
    static int started;
    if (!started) {
        started = 1;
        language_new();
    }
    heading("Sprachen");
    icon_button(528, 74, IC_BOOK);
    const char *pair[] = {"DE → EN", "DE → FR", "EN → DE"};
    segments(28, 132, 544, 39, pair, 3, lang_pair);
    const char *tabs[] = {"Karten", "Quiz", "Schreiben", "Fortschritt"};
    segments(28, 184, 544, 42, tabs, 4, language_tab);
    char b[180];
    if (language_tab == 3) {
        int known = 0, due = 0;
        for (int i = 0; i < 24; i++) {
            MemoryCard *c = &language_progress.cards[lang_pair][i];
            known += c->reviews > 0;
            due += c->due <= time(NULL);
        }
        snprintf(b, 180, "%d / 24", known);
        centered(28, 272, 544, 70, b, &title);
        centered(28, 347, 544, 40, "Ausdrücke geübt", &caption);
        snprintf(b, 180, "%d %s · %d %s", due, tr("Fällig"), language_progress.streak,
                 tr("Tage in Folge"));
        centered(28, 413, 544, 42, b, &body);
        snprintf(b, 180, "%d / %d", language_progress.correct, language_progress.attempts);
        centered(28, 493, 544, 42, b, &body);
        centered(28, 538, 544, 32, "Richtige Antworten", &caption);
        button_h(28, 620, 544, 60, "Fällige Ausdrücke üben", 0);
        return;
    }
    wrapped_raw(30, 258, v1_lessons[lang_index][language_source()], &editorial, 540, 3);
    if (language_tab == 0) {
        if (!lang_reveal) {
            button_h(28, 470, 544, 60, "Antwort zeigen", 0);
        } else {
            wrapped_raw(30, 389, v1_lessons[lang_index][language_target()], &body, 540, 3);
            button_h(28, 548, 260, 58, "Noch üben", 0);
            button_h(312, 548, 260, 58, "Gewusst", 0);
        }
    } else if (language_tab == 1) {
        for (int i = 0; i < 4; i++) {
            int y = 352 + i * 72;
            button_h(28, y, 544, 60, v1_lessons[language_options[i]][language_target()],
                     language_answered && language_options[i] == lang_index);
        }
        if (language_answered) {
            label_at(30, 642, 540, 35,
                     tr(language_answered == 1 ? "Richtig!" : "Die richtige Antwort ist markiert."),
                     &caption);
            button_h(28, 681, 544, 46, "Weiter", 0);
        }
    } else {
        button_h(28, 391, 544, 62,
                 *language_written ? language_written : tr("Übersetzung schreiben"), 0);
        if (language_answered) {
            wrapped_raw(30, 492, v1_lessons[lang_index][language_target()], &body, 540, 3);
            label_at(30, 590, 540, 35,
                     tr(language_answered == 1 ? "Richtig!"
                                               : "Vergleiche deine Antwort mit der Lösung."),
                     &caption);
            button_h(28, 658, 544, 60, "Weiter", 0);
        }
    }
    if (language_tab == 0 || !language_answered) {
        int count = language_progress.day == news_date() ? language_progress.count : 0;
        snprintf(b, 180, "%d / 10 · %s", count,
                 tr(count >= 10 ? "Tagesziel erreicht!" : "Tagesziel"));
        label_at(30, 653, 540, 32, b, &caption);
        rect(30, 701, 540, 5, 215);
        rect(30, 701, 54 * (count > 10 ? 10 : count), 5, 0);
    }
}
static int polish_language_input(const Input *in) {
    if (in->kind != INPUT_TAP)
        return 2;
    int x = in->x, y = in->y;
    if (x >= 516 && y >= 74 && y < 118) {
        extra_open(99);
        return 1;
    }
    if (y >= 132 && y < 171 && x >= 28 && x < 572) {
        lang_pair = (x - 28) / (544 / 3);
        if (lang_pair > 2)
            lang_pair = 2;
        language_new();
        return 1;
    }
    if (y >= 184 && y < 226 && x >= 28 && x < 572) {
        language_tab = (x - 28) / (544 / 4);
        if (language_tab > 3)
            language_tab = 3;
        language_new();
        return 1;
    }
    if (language_tab == 3) {
        if (y >= 620 && y < 680) {
            language_tab = 0;
            language_new();
        }
        return 1;
    }
    if (language_tab == 0) {
        if (!lang_reveal && y >= 470 && y < 530)
            lang_reveal = 1;
        else if (lang_reveal && y >= 548 && y < 606) {
            language_grade(x >= 300);
            language_new();
        }
    } else if (language_tab == 1) {
        if (language_answered && y >= 681 && y < 727)
            language_new();
        else if (!language_answered && y >= 352 && y < 640) {
            int i = (y - 352) / 72;
            if (i < 4 && (y - 352) % 72 < 60) {
                language_choice = i;
                language_grade(language_options[i] == lang_index);
            }
        }
    } else if (y >= 391 && y < 453 && !language_answered)
        open_keyboard(158, "Übersetzung", language_written);
    else if (language_answered && y >= 658 && y < 718)
        language_new();
    return 1;
}
static void language_book(void) {
    heading("Wortschatz");
    for (int r = 0; r < 6 && language_book_page * 6 + r < 24; r++) {
        int i = language_book_page * 6 + r, y = 145 + r * 86;
        drawtext_raw(30, y, v1_lessons[i][language_source()], &body, 540);
        drawtext_raw(30, y + 38, v1_lessons[i][language_target()], &small, 540);
        rect(28, y + 76, 544, 1, 205);
    }
    pager(679, &language_book_page, 4);
}
