/* Deliberate practice, bounded sessions, no medical or general-IQ claims. */
static struct {
    int levels[6], attempts[6], correct[6], sessions, streak, lastday, scores[30], days[30];
} brain_stats;
static int brain_ready, brain_phase, brain_mode = -1, brain_task, brain_round, brain_score,
                                     brain_total, brain_right, brain_moves, brain_grid[24],
                                     brain_target[16], brain_selected[16], brain_goal;
static int brain_attention_target, brain_attention_cells;
static char brain_question[512], brain_answer[64], brain_input[64], brain_explanation[512];
static const char *brain_modes[] = {"Bildgedächtnis", "Arbeitsgedächtnis", "Kopfrechnen",
                                    "Muster & Logik", "Aufmerksamkeit",    "Planen"};
static void brain_init(void) {
    if (brain_ready)
        return;
    brain_ready = 1;
    if (!load_blob("braintrain-1.1.bin", &brain_stats, sizeof(brain_stats))) {
        memset(&brain_stats, 0, sizeof(brain_stats));
        brain_stats.sessions = focus_stats.sessions;
        for (int i = 0; i < 30; i++) {
            brain_stats.scores[i] = focus_stats.scores[i] * 100 / 8;
            brain_stats.days[i] = focus_stats.days[i];
        }
        brain_stats.levels[0] = focus_stats.span > 2 ? focus_stats.span - 2 : 1;
        brain_stats.levels[2] = focus_stats.level;
    }

    for (int i = 0; i < 6; i++)
        if (brain_stats.levels[i] < 1 || brain_stats.levels[i] > 8)
            brain_stats.levels[i] = 1;
}
static void brain_lights(int cell) {
    int r = cell / 3, c = cell % 3;
    for (int i = 0; i < 9; i++)
        if (abs(i / 3 - r) + abs(i % 3 - c) <= 1)
            brain_grid[i] ^= 1;
}
static void brain_new(void) {
    brain_task = brain_mode < 0 ? brain_round % 6 : brain_mode;
    int level = brain_stats.levels[brain_task];
    brain_input[0] = 0;
    brain_moves = 0;
    brain_phase = 2;
    memset(brain_selected, 0, sizeof(brain_selected));
    memset(brain_target, 0, sizeof(brain_target));
    if (brain_task == 0) {
        int n = 2 + level;
        if (n > 9)
            n = 9;
        for (int k = 0; k < n;) {
            int c = pro_random() % 16;
            if (!brain_target[c]) {
                brain_target[c] = 1;
                k++;
            }
        }
        brain_phase = 1;
        snprintf(brain_question, 512, "Merke dir die dunklen Felder.");
        snprintf(brain_explanation, 512,
                 "Ordne die Felder gedanklich zu einer Form. Danach rekonstruierst du das Muster "
                 "ohne Vorlage.");
    }
    if (brain_task == 1) {
        int n = 4 + level;
        if (n > 10)
            n = 10;
        char sequence[64];
        for (int i = 0; i < n; i++)
            sequence[i] = '1' + pro_random() % 9;
        sequence[n] = 0;
        snprintf(brain_question, 512, "%s", sequence);
        snprintf(brain_answer, 64, "%c", sequence[n - 3]);
        brain_phase = 1;
        snprintf(brain_explanation, 512,
                 "Halte die letzten drei Ziffern im Gedächtnis. Gesucht ist die Ziffer zwei "
                 "Stellen vor der letzten.");
    }
    if (brain_task == 2) {
        int a = 3 + pro_random() % (10 + level * 9), b = 2 + pro_random() % (5 + level * 2),
            c = pro_random() % 12;
        int type = pro_random() % 3, value;
        if (type == 0) {
            snprintf(brain_question, 512, "%d + %d × %d", a, b, c);
            value = a + b * c;
            snprintf(brain_explanation, 512,
                     tr("Zuerst die Multiplikation, dann die Addition: %d + %d."), a, b * c);
        } else if (type == 1) {
            snprintf(brain_question, 512, "%d × %d − %d", a, b, c);
            value = a * b - c;
            snprintf(brain_explanation, 512,
                     tr("Zerlege die Multiplikation in einfache Teilprodukte, dann ziehe %d ab."),
                     c);
        } else {
            snprintf(brain_question, 512, "(%d + %d) ÷ %d", a * b, c * b, b);
            value = a + c;
            snprintf(brain_explanation, 512, tr("Beide Summanden sind durch %d teilbar: %d + %d."),
                     b, a, c);
        }
        snprintf(brain_answer, 64, "%d", value);
    }
    if (brain_task == 3) {
        int a = 1 + pro_random() % 9, d = 2 + pro_random() % 5, type = pro_random() % 4, v[5];
        v[0] = a;
        for (int i = 1; i < 5; i++)
            v[i] = type == 0   ? v[i - 1] + d * i
                   : type == 1 ? v[i - 1] * 2
                   : type == 2 ? (a + i) * (a + i)
                               : v[i - 1] + (i % 2 ? d : 1);
        if (type == 2)
            v[0] = a * a;
        snprintf(brain_question, 512, "%d, %d, %d, %d, ?", v[0], v[1], v[2], v[3]);
        snprintf(brain_answer, 64, "%d", v[4]);
        snprintf(brain_explanation, 512, "%s",
                 type == 0   ? tr("Die Abstände wachsen gleichmäßig.")
                 : type == 1 ? tr("Jede Zahl wird verdoppelt.")
                 : type == 2 ? tr("Aufeinanderfolgende Quadratzahlen.")
                             : tr("Zwei verschiedene Abstände wechseln sich ab."));
    }
    if (brain_task == 4) {
        brain_goal = 0;
        brain_attention_target = pro_random() % 4;
        brain_attention_cells = level >= 4 ? 24 : 16;
        for (int i = 0; i < brain_attention_cells; i++) {
            brain_grid[i] = pro_random() % 4;
            if (brain_grid[i] == brain_attention_target)
                brain_goal++;
        }
        snprintf(brain_question, 512, tr("Wie oft siehst du %c?"), "AHND"[brain_attention_target]);
        snprintf(brain_answer, 64, "%d", brain_goal);
        snprintf(
            brain_explanation, 512,
            "Scanne Zeile für Zeile in einer festen Richtung. So lässt du weniger Felder aus.");
    }
    if (brain_task == 5) {
        memset(brain_grid, 0, sizeof(brain_grid));
        for (int i = 0; i < 2 + level; i++)
            brain_lights(pro_random() % 9);
        int any = 0;
        for (int i = 0; i < 9; i++)
            any |= brain_grid[i];
        if (!any)
            brain_lights(4);
        snprintf(brain_question, 512, "Schalte alle Felder aus.");
        snprintf(brain_explanation, 512,
                 "Ein Feld verändert auch seine direkten Nachbarn. Überlege vor dem Tippen, welche "
                 "Felder sich gegenseitig beeinflussen.");
    }
}
static void brain_start(int mode) {
    brain_init();
    brain_mode = mode;
    brain_round = brain_score = 0;
    brain_total = mode < 0 ? 12 : 8;
    brain_new();
}
static void brain_feedback(int correct) {
    brain_right = correct;
    brain_score += correct;
    int t = brain_task;
    brain_stats.attempts[t]++;
    brain_stats.correct[t] += correct;
    if (correct && brain_stats.attempts[t] % 3 == 0 && brain_stats.levels[t] < 8)
        brain_stats.levels[t]++;
    if (!correct && brain_stats.levels[t] > 1)
        brain_stats.levels[t]--;
    brain_phase = 3;
    save_blob("braintrain-1.1.bin", &brain_stats, sizeof(brain_stats));
}
static void brain_check(void) {
    if (brain_task == 0) {
        int ok = 1;
        for (int i = 0; i < 16; i++)
            if (brain_selected[i] != brain_target[i])
                ok = 0;
        brain_feedback(ok);
    } else if (brain_task != 5)
        brain_feedback(*brain_input && !strcmp(brain_input, brain_answer));
}
static void brain_finish(void) {
    int at = brain_stats.sessions % 30;
    brain_stats.scores[at] = brain_score * 100 / brain_total;
    brain_stats.days[at] = news_date();
    brain_stats.sessions++;
    int today = news_date();
    if (brain_stats.lastday != today) {
        time_t yesterday = time(NULL) - 86400;
        struct tm t;
        localtime_r(&yesterday, &t);
        int y = (t.tm_year + 1900) * 10000 + (t.tm_mon + 1) * 100 + t.tm_mday;
        brain_stats.streak = brain_stats.lastday == y ? brain_stats.streak + 1 : 1;
        brain_stats.lastday = today;
    }
    save_blob("braintrain-1.1.bin", &brain_stats, sizeof(brain_stats));
    brain_phase = 4;
}
static void polish_brain(void) {
    brain_init();
    heading_actions("BrainTrain", 1);
    icon_button(528, 74, brain_phase ? IC_CLOSE : IC_HISTORY);
    char b[160];
    if (!brain_phase) {
        button_h(28, 145, 544, 62, "Tagesrunde · 12 Aufgaben", 0);
        for (int i = 0; i < 6; i++) {
            int x = 28 + (i % 2) * 282, y = 234 + (i / 2) * 135;
            key_outline(x, y, 262, 112);
            region(x, y, 262, 112);
            label_ellipsis(x + 14, y + 14, 234, 34, tr(brain_modes[i]), ui_font(UI_PRIMARY), 0);
            snprintf(b, 160, "%s %d", tr("Stufe"), brain_stats.levels[i]);
            label_at(x + 14, y + 63, 234, 24, b, ui_font(UI_SECONDARY));
        }
        snprintf(b, 160, "%d %s · %d %s", brain_stats.streak, tr("Tage in Folge"),
                 brain_stats.sessions, tr("Runden"));
        centered(28, 677, 544, 35, b, ui_font(UI_SECONDARY));
        drawtext(30, 739, "Fortschritt gilt für diese Übungen.", ui_font(UI_SECONDARY), 540);
        return;
    }
    if (brain_phase == 5) {
        for (int i = 0; i < 6; i++) {
            int y = 145 + i * 91;
            drawtext(30, y, brain_modes[i], ui_font(UI_PRIMARY), 420);
            snprintf(b, 160, "%s %d · %d / %d", tr("Stufe"), brain_stats.levels[i],
                     brain_stats.correct[i], brain_stats.attempts[i]);
            drawtext_raw(30, y + 35, b, ui_font(UI_SECONDARY), 540);
            rect(30, y + 65, 540, 6, 215);
            if (brain_stats.attempts[i])
                rect(30, y + 65, 540 * brain_stats.correct[i] / brain_stats.attempts[i], 6, 0);
        }
        return;
    }
    if (brain_phase == 4) {
        snprintf(b, 160, "%d / %d", brain_score, brain_total);
        centered(28, 195, 544, 90, b, ui_font(UI_DISPLAY));
        centered(28, 301, 544, 55, "Runde geschafft!", ui_font(UI_TITLE));
        snprintf(b, 160, "%d %s", brain_stats.streak, tr("Tage in Folge"));
        centered(28, 389, 544, 40, b, ui_font(UI_PRIMARY));
        wrapped(30, 485, "Eine kurze Pause gehört zum Training. Morgen wartet eine neue Runde.",
                ui_font(UI_BODY), 540, 4);
        button_h(28, 657, 544, 60, "Fertig", 0);
        return;
    }
    snprintf(b, 160, "%d / %d · %s · %s %d", brain_round + 1, brain_total,
             tr(brain_modes[brain_task]), tr("Stufe"), brain_stats.levels[brain_task]);
    drawtext_raw(30, 130, b, ui_font(UI_SECONDARY), 540);
    rect(30, 169, 540, 5, 220);
    rect(30, 169, 540 * brain_round / brain_total, 5, 0);
    if (brain_phase == 3) {
        centered(28, 195, 544, 60, brain_right ? "Richtig!" : "Weiter üben", ui_font(UI_TITLE));
        if (brain_task == 0) {
            for (int i = 0; i < 16; i++) {
                int x = 166 + (i % 4) * 68, y = 277 + (i / 4) * 68;
                rect(x, y, 58, 58, brain_target[i] ? 0 : 240);
                if (brain_selected[i] != brain_target[i])
                    key_outline(x, y, 58, 58);
            }
        } else if (brain_task != 5)
            centered(28, 290, 544, 80, brain_answer, ui_font(UI_DISPLAY));
        wrapped(30, 555, brain_explanation, ui_font(UI_BODY), 540, 4);
        button_h(28, 708, 544, 62, "Weiter", 0);
        return;
    }
    if (brain_task == 0) {
        drawtext(30, 203,
                 brain_phase == 1 ? "Merke dir die dunklen Felder."
                                  : "Stelle das Muster wieder her.",
                 ui_font(UI_PRIMARY), 540);
        for (int i = 0; i < 16; i++) {
            int x = 124 + (i % 4) * 90, y = 275 + (i / 4) * 90;
            key_outline(x, y, 80, 80);
            rect(x + 3, y + 3, 74, 74,
                 (brain_phase == 1 ? brain_target[i] : brain_selected[i]) ? 0 : 255);
            region(x, y, 80, 80);
        }
        button_h(28, 705, 544, 62, brain_phase == 1 ? "Bereit" : "Prüfen", 0);
        return;
    }
    if (brain_task == 1 && brain_phase == 1) {
        centered(28, 235, 544, 55, "Merken", ui_font(UI_TITLE));
        centered(28, 325, 544, 90, brain_question, ui_font(UI_DISPLAY));
        wrapped(30, 455, "Gesucht wird gleich die Ziffer zwei Stellen vor der letzten.", ui_font(UI_BODY),
                540, 3);
        button_h(28, 705, 544, 62, "Bereit", 0);
        return;
    }
    if (brain_task == 5) {
        drawtext(30, 211, brain_question, ui_font(UI_PRIMARY), 540);
        for (int i = 0; i < 9; i++) {
            int x = 118 + (i % 3) * 126, y = 280 + (i / 3) * 126;
            key_outline(x, y, 112, 112);
            rect(x + 3, y + 3, 106, 106, brain_grid[i] ? 0 : 255);
            region(x, y, 112, 112);
        }
        snprintf(b, 160, "%d %s", brain_moves, tr("Züge"));
        centered(28, 677, 544, 34, b, ui_font(UI_SECONDARY));
        button_h(28, 725, 544, 50, "Überspringen", 0);
        return;
    }
    if (brain_task == 4) {
        drawtext(30, 201, brain_question, ui_font(UI_PRIMARY), 540);
        int cols = brain_attention_cells > 16 ? 6 : 4;
        for (int i = 0; i < brain_attention_cells; i++) {
            char c[2] = {"AHND"[brain_grid[i]], 0};
            centered((cols == 6 ? 30 : 120) + (i % cols) * 90, 248 + (i / cols) * 55, 80, 45, c,
                     ui_font(UI_PRIMARY));
        }
    } else
        wrapped_raw(30, 215,
                    brain_task == 1 ? tr("Welche Ziffer stand zwei Stellen vor der letzten?")
                                    : brain_question,
                    ui_font(UI_TITLE), 540, 3);
    int yy = brain_task == 4 ? 493 : 343;
    button_h(28, yy, 544, 58, *brain_input ? brain_input : tr("Antwort"), 0);
    icon_button(522, yy + 7, IC_DELETE);
    const char *keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "−", "0", "OK"};
    int kh = brain_task == 4 ? 43 : 65, top = yy + 75;
    for (int i = 0; i < 12; i++)
        button_h(28 + (i % 3) * 185, top + (i / 3) * (kh + 8), 174, kh, keys[i], 0);
}
static int polish_brain_input(const Input *in) {
    if (in->kind != INPUT_TAP)
        return 2;
    brain_init();
    int x = in->x, y = in->y;
    if (x >= 516 && y >= 74 && y < 118) {
        brain_phase = brain_phase ? 0 : 5;
        return 1;
    }
    if (!brain_phase) {
        if (y >= 145 && y < 207)
            brain_start(-1);
        else if (x >= 28 && x < 572 && y >= 234 && y < 639) {
            int i = ((y - 234) / 135) * 2 + (x >= 310);
            if (i < 6)
                brain_start(i);
        }
        return 1;
    }
    if (brain_phase == 5)
        return 1;
    if (brain_phase == 4) {
        if (y >= 657 && y < 717)
            brain_phase = 0;
        return 1;
    }
    if (brain_phase == 3) {
        if (y >= 708 && y < 770) {
            if (++brain_round >= brain_total)
                brain_finish();
            else
                brain_new();
        }
        return 1;
    }
    if (brain_task == 0) {
        if (y >= 705 && y < 767) {
            if (brain_phase == 1)
                brain_phase = 2;
            else
                brain_check();
        } else if (brain_phase == 2 && x >= 124 && x < 474 && y >= 275 && y < 625) {
            int c = (x - 124) / 90, r = (y - 275) / 90;
            if (c < 4 && r < 4)
                brain_selected[r * 4 + c] ^= 1;
        }
        return 1;
    }
    if (brain_task == 1 && brain_phase == 1) {
        if (y >= 705 && y < 767)
            brain_phase = 2;
        return 1;
    }
    if (brain_task == 5) {
        if (x >= 118 && x < 496 && y >= 280 && y < 658) {
            int c = (x - 118) / 126, r = (y - 280) / 126;
            if (c < 3 && r < 3) {
                brain_lights(r * 3 + c);
                brain_moves++;
                int any = 0;
                for (int i = 0; i < 9; i++)
                    any |= brain_grid[i];
                if (!any)
                    brain_feedback(1);
            }
        } else if (y >= 725 && y < 775)
            brain_feedback(0);
        return 1;
    }
    int yy = brain_task == 4 ? 493 : 343, kh = brain_task == 4 ? 43 : 65, top = yy + 75;
    if (x >= 516 && y >= yy && y < yy + 58) {
        size_t n = strlen(brain_input);
        if (n)
            brain_input[n - 1] = 0;
    } else if (y >= top && y < top + 4 * (kh + 8) && x >= 28 && x < 572) {
        int c = (x - 28) / 185, r = (y - top) / (kh + 8), k = r * 3 + c;
        if (c < 3 && k < 12) {
            if (k == 11)
                brain_check();
            else if (k == 9) {
                if (!*brain_input)
                    strcpy(brain_input, "-");
            } else {
                size_t n = strlen(brain_input);
                if (n < 20) {
                    brain_input[n] = k == 10 ? '0' : '1' + k;
                    brain_input[n + 1] = 0;
                }
            }
        }
    }
    return 1;
}
