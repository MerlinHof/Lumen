#include "../newspaper.h"
#include "../puzzles.h"
#include "plots.h"
static Tiles tile_game;
static Patience solitaire;
static Sudoku sudoku;
static unsigned new_dirty;
static int new_ready[3], sudoku_selected = -1, sudoku_pencil, pat_source = -99, pat_index,
                         pat_confirm;
typedef struct {
    int span, level, sessions, streak[4], failed[4], scores[30], days[30], times[30];
} FocusStats;
static FocusStats focus_stats;
static uint32_t pro_rng;
static unsigned news_topics = 63;
static int news_enabled, news_page, news_done, news_settings_loaded, news_source_page,
    news_puzzle_revealed;
static time_t news_retry;
static int news_today_cached(void) {
    if (newspaper.date == (unsigned)news_date())
        return 1;
    char n[80], p[2048];
    snprintf(n, 80, "news/%08d.bin", news_date());
    local_path(p, sizeof(p), n);
    return access(p, R_OK) == 0;
}

static int save_blob(const char *name, const void *data, size_t bytes) {
    char p[2048];
    local_path(p, sizeof(p), name);
    unsigned char *all = malloc(bytes + 8);
    if (!all)
        return 0;
    uint32_t h[2] = {0x4c554d16, (uint32_t)bytes};
    memcpy(all, h, 8);
    memcpy(all + 8, data, bytes);
    int ok = atomic_write(p, all, bytes + 8);
    free(all);
    return ok;
}
static int load_blob(const char *name, void *data, size_t bytes) {
    char p[2048];
    local_path(p, sizeof(p), name);
    FILE *f = fopen(p, "rb");
    if (!f)
        return 0;
    uint32_t h[2];
    void *tmp = malloc(bytes);
    int ok = tmp && fread(h, 8, 1, f) == 1 && h[0] == 0x4c554d16 && h[1] == bytes &&
             fread(tmp, bytes, 1, f) == 1 && fgetc(f) == EOF;
    if (ok)
        memcpy(data, tmp, bytes);
    free(tmp);
    fclose(f);
    return ok;
}
static uint32_t pro_random(void) {
    pro_rng = pro_rng * 1664525u + 1013904223u;
    return pro_rng;
}
static void new_apps_init(void) {
    v1_init();
    pro_rng = (uint32_t)time(NULL) ^ (uint32_t)extra_millis();
    if (!load_blob("focus-v1.bin", &focus_stats, sizeof(focus_stats)) || focus_stats.span < 2 ||
        focus_stats.span > 8 || focus_stats.level < 1 || focus_stats.level > 5) {
        memset(&focus_stats, 0, sizeof(focus_stats));
        focus_stats.span = 3;
        focus_stats.level = 1;
    }
    char *s = service_read("news-settings.txt", 80);
    if (s) {
        unsigned m = 63;
        int e = 0;
        if (sscanf(s, "%u %d", &m, &e) == 2) {
            news_topics = m & 63;
            news_enabled = e != 0;
        }
        free(s);
    }
    news_settings_loaded = 1;
    char *last = service_read("news/latest.txt", 32);
    if (last) {
        int date = atoi(last);
        if (date >= 20200101 && date <= 21001231)
            news_load(date);
        free(last);
    }
}
static void news_save_settings(void) {
    char b[64];
    snprintf(b, sizeof(b), "%u %d", news_topics, news_enabled);
    extra_write_text("news-settings.txt", b);
}
static void new_apps_save(void) {
    v1_save();
    if ((new_dirty & 1) && new_ready[0] && save_blob("2048-v1.bin", &tile_game, sizeof(tile_game)))
        new_dirty &= ~1u;
    if ((new_dirty & 2) && new_ready[1] &&
        save_blob("solitaire-v1.bin", &solitaire, sizeof(solitaire)))
        new_dirty &= ~2u;
    if ((new_dirty & 4) && new_ready[2] && save_blob("sudoku-v1.bin", &sudoku, sizeof(sudoku)))
        new_dirty &= ~4u;
    if ((new_dirty & 8) && save_blob("focus-v1.bin", &focus_stats, sizeof(focus_stats)))
        new_dirty &= ~8u;
}
static int valid_patience(void) {
    PatienceState *s = &solitaire.state;
    if (s->ns > 24 || s->nw > 24 || s->ns + s->nw > 24 || solitaire.undos < 0 ||
        solitaire.undos > 24)
        return 0;
    for (int i = 0; i < 7; i++) {
        if (s->count[i] > 52 || s->hidden[i] > s->count[i])
            return 0;
        for (int j = 0; j < s->count[i]; j++)
            if (s->column[i][j] >= 52)
                return 0;
    }
    for (int i = 0; i < 4; i++)
        if (s->foundation[i] > 13)
            return 0;
    return 1;
}
static void new_apps_open(int route) {
    if (route == 67) {
        polish_open(86);
        return;
    }
    if (route >= 70) {
        v1_open(route);
        return;
    }
    if (route == 63 && !new_ready[0]) {
        if (!load_blob("2048-v1.bin", &tile_game, sizeof(tile_game)))
            tiles_new(&tile_game, pro_random());
        for (int i = 0; i < 16; i++)
            if (tile_game.state.cell[i] > 30)
                tiles_new(&tile_game, pro_random());
        new_ready[0] = 1;
        new_dirty |= 1;
    }
    if (route == 64 && !new_ready[1]) {
        if (!load_blob("solitaire-v1.bin", &solitaire, sizeof(solitaire)) || !valid_patience())
            patience_new(&solitaire, pro_random());
        new_ready[1] = 1;
        new_dirty |= 2;
    }
    if (route == 65 && !new_ready[2]) {
        int valid = load_blob("sudoku-v1.bin", &sudoku, sizeof(sudoku));
        for (int i = 0; i < 81; i++)
            if (sudoku.board.cell[i] > 9 || sudoku.solution[i] < 1 || sudoku.solution[i] > 9 ||
                sudoku.given[i] > 9)
                valid = 0;
        if (sudoku.undos < 0 || sudoku.undos > 24)
            valid = 0;
        if (!valid)
            sudoku_new(&sudoku, pro_random(), 0);
        new_ready[2] = 1;
        new_dirty |= 4;
    }
    if (route == 67) {
        news_enabled = 1;
        news_save_settings();
        news_page = 0;
        news_done = 0;
        news_puzzle_revealed = 0;
        char readname[80], readpath[2048];
        snprintf(readname, sizeof(readname), "news/%u-read.txt", newspaper.date);
        local_path(readpath, sizeof(readpath), readname);
        news_done = access(readpath, F_OK) == 0;
        if (!news_today_cached())
            news_retry = 0;
    }
}
static int new_apps_poll(void) {
    if (news_busy())
        return 250;

    if (news_enabled && !news_today_cached()) {
        long wait = news_retry - time(NULL);
        if (wait < 1)
            wait = 1;
        return wait > 60 ? 60000 : wait * 1000;
    }
    return -1;
}
static int new_apps_tick(void) {
    int olddate = newspaper.date, oldpage = news_page;
    int changed = 0, done = news_poll();
    if (done) {
        news_retry = time(NULL) + 3600;
        if (done > 0) {
            if (screen == 67 && olddate && olddate != news_date()) {
                news_load(olddate);
                news_page = oldpage;
            } else
                news_page = 0;
            news_puzzle_revealed = 0;
            if (screen == 67)
                changed = 1;
        } else if (screen == 67) {
            ui_message("Neue Ausgabe nicht geladen. Gespeicherte Ausgabe bleibt verfügbar.");
            changed = 1;
        }
    }
    if (news_enabled && !news_busy() && !news_today_cached() && time(NULL) >= news_retry) {
        news_retry = time(NULL) + 60;
#ifndef LUMEN_HOST
        char ip[80];
        device_ip(ip, sizeof(ip));
        if (device_wifi_enabled() && *ip) {
            news_retry = time(NULL) + 3600;
            int before = newspaper.date;
            news_start(options[OPT_LANGUAGE], news_topics);
            if (screen == 67 && before != (int)newspaper.date)
                changed = 1;
        }
#endif
    }

    return changed;
}
static void app_tiles(void) {
    heading_actions("2048", 3);
    icon_button(468, 74, IC_UNDO);
    icon_button(528, 74, IC_RESTART);
    char b[100];
    snprintf(b, sizeof(b), "%s  %llu", tr("Punkte"), (unsigned long long)tile_game.state.score);
    drawtext_raw(30, 130, b, &body, 540);
    for (int i = 0; i < 16; i++) {
        int x = 56 + i % 4 * 124, y = 184 + i / 4 * 124, v = tile_game.state.cell[i];
        int colors[] = {250, 240, 200, 184, 168, 152, 136, 120, 104, 88, 72, 56};
        int color = colors[v < 12 ? v : 11];
        if (color < 28)
            color = 28;
        rect(x, y, 116, 116, color);
        if (v) {
            snprintf(b, sizeof(b), "%u", 1u << v);
            ink_color = color < 140 ? 255 : 0;
            centered(x, y, 116, 116, b, v >= 20 ? &small : v >= 10 ? &editorial : &title);
            ink_color = 0;
        }
    }
    const int kinds[] = {IC_PREV, IC_UP, IC_DOWN, IC_NEXT};
    for (int i = 0; i < 4; i++) {
        button_h(28 + i * 140, 716, 124, 56, "", 0);
        icon(68 + i * 140, 722, kinds[i]);
    }
    if (tiles_over(&tile_game) || tile_game.state.won)
        drawtext(30, 682,
                 tiles_over(&tile_game) ? "Keine Züge mehr. Undo oder neu starten."
                                        : "2048 erreicht! Du kannst weiterspielen.",
                 &caption, 540);
}
static void suit_icon(int x, int y, int suit) {
    if (suit == 0) {
        disc(x - 4, y - 3, 4, 0);
        disc(x + 4, y - 3, 4, 0);
        stroke(x - 7, y - 2, x, y + 7, 5);
        stroke(x + 7, y - 2, x, y + 7, 5);
    } else if (suit == 1) {
        stroke(x, y - 8, x + 6, y, 3);
        stroke(x + 6, y, x, y + 8, 3);
        stroke(x, y + 8, x - 6, y, 3);
        stroke(x - 6, y, x, y - 8, 3);
    } else if (suit == 2) {
        disc(x, y - 4, 4, 0);
        disc(x - 4, y + 2, 4, 0);
        disc(x + 4, y + 2, 4, 0);
        stroke(x, y + 3, x, y + 9, 3);
    } else {
        stroke(x, y - 8, x - 5, y, 5);
        stroke(x, y - 8, x + 5, y, 5);
        disc(x - 3, y + 1, 4, 0);
        disc(x + 3, y + 1, 4, 0);
        stroke(x, y + 2, x, y + 9, 3);
    }
}
static void card(int x, int y, int id, int hidden, int selected) {
    rect(x, y, 72, 96, 255);
    key_outline(x, y, 72, 96);
    if (hidden) {
        for (int j = 8; j < 88; j += 8)
            for (int i = 8; i < 65; i += 8)
                disc(x + i, y + j, 1, 125);
        return;
    }
    if (id < 0)
        return;
    int rank = id % 13 + 1, suit = id / 13;
    char b[8];
    snprintf(b, sizeof(b),
             rank == 1    ? "A"
             : rank == 11 ? "J"
             : rank == 12 ? "Q"
             : rank == 13 ? "K"
                          : "%d",
             rank);
    ink_color = suit < 2 ? 85 : 0;
    drawtext_raw(x + 6, y + 4, b, &small, 40);
    ink_color = 0;
    suit_icon(x + 54, y + 17, suit);
    suit_icon(x + 36, y + 58, suit);
    if (selected) {
        rect(x, y, 72, 4, 0);
        rect(x, y + 92, 72, 4, 0);
    }
}
static int column_gap(int c) {
    int n = solitaire.state.count[c];
    return n > 1 ? ((350 / (n - 1)) < 27 ? 350 / (n - 1) : 27) : 27;
}
static void app_solitaire(void) {
    heading_actions("Solitaire", 4);
    icon_button(408, 74, IC_UNDO);
    icon_button(468, 74, IC_DONE);
    icon_button(528, 74, IC_RESTART);
    PatienceState *s = &solitaire.state;
    region(21, 132, 72, 96);
    card(21, 132, -1, s->ns > 0, 0);
    if (!s->ns)
        icon(35, 158, IC_RESTART);
    region(101, 132, 72, 96);
    card(101, 132, s->nw ? s->waste[s->nw - 1] : -1, 0, pat_source == -1);
    for (int i = 0; i < 4; i++) {
        region(261 + i * 80, 132, 72, 96);
        card(261 + i * 80, 132, s->foundation[i] ? i * 13 + s->foundation[i] - 1 : -1, 0,
             pat_source == 7 + i);
        if (!s->foundation[i])
            suit_icon(297 + i * 80, 180, i);
    }
    for (int c = 0; c < 7; c++) {
        int x = 21 + c * 80, gap = column_gap(c);
        if (!s->count[c]) {
            key_outline(x, 248, 72, 96);
            region(x, 248, 72, 96);
        }
        for (int i = 0; i < s->count[c]; i++) {
            int y = 248 + i * gap;
            card(x, y, s->column[c][i], i < s->hidden[c], pat_source == c && pat_index == i);
            region(x, y, 72, i == s->count[c] - 1 ? 96 : gap);
        }
    }
    char b[100];
    snprintf(b, sizeof(b), "%s %u · %s %d", tr("Züge"), s->moves, tr("Stapel"), s->ns);
    drawtext_raw(28, 736, b, &caption, 544);
    if (patience_won(&solitaire))
        centered(28, 680, 544, 50, "Geschafft!", &editorial);
    else
        drawtext(28, 768, "Karte wählen, dann Ziel antippen. Haken: automatisch ablegen.", &caption,
                 544);
}
static void app_sudoku(void) {
    heading_actions("Sudoku", 1);
    icon_button(528, 74, IC_RESTART);
    button_h(28, 120, 140, 36, "Notizen", sudoku_pencil);
    icon_button(184, 116, IC_UNDO);
    icon_button(240, 116, IC_DELETE);
    button_h(360, 120, 150, 36, "Hinweis", 0);
    for (int i = 0; i < 81; i++) {
        int x = 39 + i % 9 * 58, y = 170 + i / 9 * 58;
        if (i == sudoku_selected)
            rect(x + 1, y + 1, 56, 56, 225);
        else if (sudoku_selected >= 0 && sudoku.board.cell[sudoku_selected] &&
                 sudoku.board.cell[i] == sudoku.board.cell[sudoku_selected])
            rect(x + 1, y + 1, 56, 56, 242);
        region(x, y, 58, 58);
        char b[8];
        if (sudoku.board.cell[i]) {
            snprintf(b, sizeof(b), "%d", sudoku.board.cell[i]);
            ink_color = sudoku.given[i] ? 0 : 70;
            centered(x, y, 58, 58, b, &body);
            if (sudoku.given[i])
                centered(x + 1, y, 58, 58, b, &body);
            ink_color = 0;
            if (sudoku_conflict(&sudoku, i))
                rect(x + 15, y + 48, 28, 3, 0);
        } else
            for (int k = 0; k < 9; k++)
                if (sudoku.board.notes[i] & (1 << k)) {
                    snprintf(b, sizeof(b), "%d", k + 1);
                    centered(x + k % 3 * 19, y + k / 3 * 19, 20, 20, b, &caption);
                }
    }
    for (int i = 0; i <= 9; i++) {
        rect(39 + i * 58, 170, i % 3 ? 1 : 3, 522, i % 3 ? 185 : 50);
        rect(39, 170 + i * 58, 522, i % 3 ? 1 : 3, i % 3 ? 185 : 50);
    }
    for (int v = 1; v <= 9; v++) {
        char b[8];
        snprintf(b, sizeof(b), "%d", v);
        button_h(28 + (v - 1) * 61, 725, 56, 52, b, 0);
    }
    if (sudoku_won(&sudoku))
        centered(28, 692, 544, 28, "Geschafft!", &small);
}

static void app_focus(void) {
    polish_brain();
}
static const char *news_category(int c) {
    const char *labels[] = {"Welt",    "Wissenschaft",  "Technologie",
                            "Energie", "An diesem Tag", "Tagesrätsel"};
    return tr(labels[c >= 0 && c < 6 ? c : 0]);
}

static void app_newspaper(void) {
    polish_news_draw();
}
static void app_news_settings(void) {
    heading("Zeitung · Themen");
    for (int i = 0; i < 6; i++) {
        drawtext_raw(30, 150 + i * 74, news_category(i), &body, 425);
        toggle(492, 150 + i * 74, (news_topics >> i) & 1);
    }
    wrapped(
        30, 620,
        "Die Themen gelten für die nächste Ausgabe. Meldungen bleiben in der Sprache der Quelle.",
        &caption, 540, 3);
    button_h(28, 706, 544, 52, "Fertig", 0);
}
static void app_news_source(void) {
    heading("Quelle");
    if (news_source_page < 0 || news_source_page >= newspaper.count)
        return;
    NewsArticle *a = &newspaper.article[news_source_page];
    drawtext_raw(28, 143, a->publisher, &editorial, 544);
    wrapped_raw(28, 210, a->title, &body, 544, 4);
    wrapped_raw(28, 350, a->published, &caption, 544, 2);
    wrapped_raw(28, 412, a->url, &small, 544, 7);
    if (*a->url) {
        uint8_t temp[qrcodegen_BUFFER_LEN_MAX], qr[qrcodegen_BUFFER_LEN_MAX];
        if (qrcodegen_encodeText(a->url, temp, qr, qrcodegen_Ecc_LOW, qrcodegen_VERSION_MIN, 12,
                                 qrcodegen_Mask_AUTO, true)) {
            int n = qrcodegen_getSize(qr), scale = 150 / (n + 8);
            if (scale < 1)
                scale = 1;
            for (int y = 0; y < n; y++)
                for (int x = 0; x < n; x++)
                    if (qrcodegen_getModule(qr, x, y))
                        rect(420 + x * scale, 620 + y * scale, scale, scale, 0);
        }
    }
}
static void new_apps_draw(void) {
    if (screen >= 70) {
        v1_draw();
        return;
    }
    switch (screen) {
    case 60:
        sensor_graph_screen();
        break;
    case 61:
        calculator_graph();
        break;
    case 62:
        graph_range_screen();
        break;
    case 63:
        app_tiles();
        break;
    case 64:
        app_solitaire();
        break;
    case 65:
        app_sudoku();
        break;
    case 66:
        app_focus();
        break;
    case 67:
        app_newspaper();
        break;
    case 68:
        app_news_settings();
        break;
    case 69:
        app_news_source();
        break;
    }
}
static int new_apps_commit(int target, const char *text) {
    if (target >= 100)
        return v1_commit(target, text);
    if (target == 80) {
        snprintf(calc_expr, sizeof(calc_expr), "%s", text);
        calc_cursor = strlen(calc_expr);
        calc_preview();
        return 1;
    }
    if (target >= 81 && target <= 84) {
        char *end;
        double v = strtod(text, &end);
        int i = target - 81;
        if (end == text || *end || !isfinite(v) || fabs(v) > 1e12) {
            ui_message("Ausdruck oder Wertebereich prüfen.");
            return 1;
        }
        double copy[4];
        memcpy(copy, plot_bounds, sizeof(copy));
        copy[i] = v;
        if (copy[0] >= copy[1] || copy[2] >= copy[3])
            ui_message("Minimum muss kleiner als Maximum sein.");
        else
            memcpy(plot_bounds, copy, sizeof(copy));
        return 1;
    }
    return 0;
}
static int new_apps_input(const Input *in) {
    if (screen >= 70)
        return v1_input(in);
    int x = in->x, y = in->y;
    if (screen == 63 && in->kind == INPUT_SWIPE) {
        int dir = abs(in->dx) > abs(in->dy) ? (in->dx < 0 ? 0 : 1) : (in->dy < 0 ? 2 : 3);
        if (tiles_move(&tile_game, dir)) {
            new_dirty |= 1;
            new_apps_save();
        }
        return 1;
    }
    if (screen == 67 && in->kind == INPUT_SWIPE) {
        int total = newspaper.count + 2;
        if (in->dx < 0 && news_page + 1 < total)
            news_page++;
        else if (in->dx > 0 && news_page)
            news_page--;
        return 1;
    }
    if (in->kind != INPUT_TAP)
        return 2;
    if (screen == 60) {
        if (y >= 716 && y < 764) {
            ha_plot_hours = x < 210 ? 1 : x < 390 ? 6 : 24;
        }
        return 1;
    }
    if (screen == 61) {
        if (y >= 128 && y < 178)
            open_keyboard(80, "y = f(x)", calc_expr);
        else if (y >= 220 && y < 647 && x >= 66 && x < 558)
            plot_trace = x - 66 + 1;
        else if (y >= 712) {
            if (x < 178)
                extra_open(62);
            else if (x >= 416)
                graph_auto_y();
            else {
                double f = x < 300 ? 2 : .5, cx = (plot_bounds[0] + plot_bounds[1]) / 2,
                       cy = (plot_bounds[2] + plot_bounds[3]) / 2,
                       dx = (plot_bounds[1] - plot_bounds[0]) * f / 2,
                       dy = (plot_bounds[3] - plot_bounds[2]) * f / 2;
                if (dx > 1e-9 && dx < 1e12 && dy > 1e-9 && dy < 1e12) {
                    plot_bounds[0] = cx - dx;
                    plot_bounds[1] = cx + dx;
                    plot_bounds[2] = cy - dy;
                    plot_bounds[3] = cy + dy;
                }
            }
        }
        return 1;
    }
    if (screen == 62) {
        if (y >= 165 && y < 549) {
            int i = (y - 165) / 108;
            if (i < 4) {
                char b[80];
                snprintf(b, sizeof(b), "%.10g", plot_bounds[i]);
                open_keyboard(81 + i, "Graph-Bereich", b);
            }
        } else if (y >= 650 && y < 706)
            extra_return(61);
        return 1;
    }
    if (screen == 63) {
        new_dirty |= 1;
        if (y >= 74 && y < 118) {
            if (x >= 516)
                tiles_new(&tile_game, pro_random());
            else if (x >= 460 && tile_game.can_undo) {
                tile_game.state = tile_game.undo;
                tile_game.can_undo = 0;
            }
        } else if (y >= 716 && y < 772 && x >= 28) {
            int k = (x - 28) / 140;
            int d[] = {0, 2, 3, 1};
            if (k < 4)
                tiles_move(&tile_game, d[k]);
        }
        new_apps_save();
        return 1;
    }
    if (screen == 64) {
        new_dirty |= 2;
        if (y >= 74 && y < 118) {
            if (x >= 516)
                patience_new(&solitaire, pro_random());
            else if (x >= 460)
                patience_auto(&solitaire);
            else if (x >= 405)
                patience_undo(&solitaire);
            pat_source = -99;
        } else if (y >= 132 && y < 228) {
            if (x >= 21 && x < 93) {
                patience_draw(&solitaire);
                pat_source = -99;
            } else if (x >= 101 && x < 173 && solitaire.state.nw) {
                pat_source = -1;
                pat_index = 0;
            } else if (x >= 261 && x < 573) {
                int to = 7 + (x - 261) / 80;
                if (pat_source != -99 && patience_move(&solitaire, pat_source, pat_index, to))
                    pat_source = -99;
                else if (solitaire.state.foundation[to - 7]) {
                    pat_source = to;
                    pat_index = 0;
                }
            }
        } else if (y >= 248 && y < 710 && x >= 21 && x < 573) {
            int c = (x - 21) / 80;
            if (c < 7) {
                if (pat_source != -99 && patience_move(&solitaire, pat_source, pat_index, c))
                    pat_source = -99;
                else if (solitaire.state.count[c]) {
                    int idx = (y - 248) / column_gap(c);
                    if (idx >= solitaire.state.count[c])
                        idx = solitaire.state.count[c] - 1;
                    if (idx >= solitaire.state.hidden[c]) {
                        pat_source = c;
                        pat_index = idx;
                    }
                }
            }
        }
        new_apps_save();
        return 1;
    }
    if (screen == 65) {
        new_dirty |= 4;
        if (y >= 74 && y < 118 && x >= 516) {
            sudoku_new(&sudoku, pro_random(), sudoku.difficulty);
            sudoku_selected = -1;
        } else if (y >= 116 && y < 160) {
            if (x < 170)
                sudoku_pencil ^= 1;
            else if (x < 230)
                sudoku_undo(&sudoku);
            else if (x < 300)
                sudoku_put(&sudoku, sudoku_selected, 0, 0);
            else if (x >= 360)
                sudoku_hint(&sudoku, sudoku_selected);
        } else if (x >= 39 && x < 561 && y >= 170 && y < 692)
            sudoku_selected = (y - 170) / 58 * 9 + (x - 39) / 58;
        else if (y >= 725 && y < 777 && x >= 28 && x < 573) {
            int v = (x - 28) / 61 + 1;
            if (v <= 9)
                sudoku_put(&sudoku, sudoku_selected, v, sudoku_pencil);
        }
        new_apps_save();
        return 1;
    }
    if (screen == 66) {
        return polish_brain_input(in);
    }
    if (screen == 67) {
        return polish_news_input(in);
    }
    if (screen == 68) {
        if (y >= 142 && y < 142 + 6 * 74 && x >= 460) {
            int i = (y - 142) / 74;
            if (i < 6) {
                news_topics ^= 1u << i;
                if (!(news_topics & 31))
                    news_topics |= 1;
                news_save_settings();
            }
        } else if (y >= 706 && y < 758)
            extra_return(67);
        return 1;
    }
    return 1;
}
