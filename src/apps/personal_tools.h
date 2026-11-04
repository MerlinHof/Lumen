/* LUMEN 1.0: native, paper-first applications, shared local persistence. */
#include "../learning.h"
#include "../vendor/astronomy.h"
#include "../vendor/ha_icon.h"
#include "../vendor/sky_catalog.h"
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
static int v1_ready, v1_dirty, v1_page, v1_delete;
typedef struct {
    int id, parent;
    char name[96];
} NoteFolder;
typedef struct {
    int id, folder;
} NotePlace;
static struct {
    NoteFolder folder[512];
    NotePlace place[1024];
    int nf, np, current;
} note_tree;
static int folder_page;
static struct {
    char title[200];
    int state;
    int64_t created;
} tasks[256];
static int task_count, task_page, task_tab, task_board;
typedef struct {
    uint64_t id;
    char deck[64], front[512], back[1024];
    MemoryCard memory;
} Flashcard;
static Flashcard flashcards[512];
static int flash_count, flash_selected = -1, flash_reveal, flash_page, flash_edit = -1;
static char flash_deck[64] = "General", flash_front[512];
static struct {
    int date, row, over;
    char guesses[6][6];
    unsigned char result[6][5];
} wordle_game;
static char word_secret[6];
static unsigned char nono_solution[25], nono_cells[25], hex_board[121], hex_previous[121];
static int nono_ready, nono_mark, hex_turn = 1, hex_win, hex_swap, hex_has_undo;
static int lang_pair, lang_index, lang_reveal, lang_correct, lang_total;
static char photo_names[512][256];
static int photo_count, photo_page, photo_selected, photo_cache_page = -1;
static char photo_loaded[256];
static int64_t photo_hour_drawn = -1;
static unsigned char photo_thumbs[6][168 * 224];
static int news_dates[2048], news_date_count, news_archive_page;
static int launcher_order[APP_COUNT], launcher_visible[APP_COUNT], launcher_count,
    launcher_settings_page;
static int markdown_page;
static int timer_total_seconds;
static int64_t timer_flash_started, timer_last_phase;
static int v1_word_index(void);
static void v1_photos_scan(void), v1_archive_scan(void), v1_nono_new(void);
static int v1_folder_for(int id) {
    for (int i = 0; i < note_tree.np; i++)
        if (note_tree.place[i].id == id)
            return note_tree.place[i].folder;
    return 0;
}
static void v1_place(int id, int folder) {
    for (int i = 0; i < note_tree.np; i++)
        if (note_tree.place[i].id == id) {
            note_tree.place[i].folder = folder;
            v1_dirty = 1;
            return;
        }
    if (note_tree.np < 1024) {
        note_tree.place[note_tree.np++] = (NotePlace){id, folder};
        v1_dirty = 1;
    }
}
static void v1_init(void) {
    polish_init();
    u15_init();
    if (v1_ready)
        return;
    v1_ready = 1;
    for (int i = 0; i < APP_COUNT; i++) {
        launcher_order[i] = i;
        launcher_visible[i] = (i >= APP_CALENDAR && i <= APP_HEX) || i >= APP_JOURNAL;
    }
    launcher_count = APP_COUNT;
    int config[APP_COUNT * 2];
    if (load_blob("launcher-16.bin", config, sizeof(config))) {
        int good = 1, seen[APP_COUNT] = {0};
        for (int i = 0; i < APP_COUNT; i++)
            if (config[i] < 0 || config[i] >= APP_COUNT || seen[config[i]]++)
                good = 0;
        if (good)
            for (int i = 0; i < APP_COUNT; i++) {
                launcher_order[i] = config[i];
                launcher_visible[i] = config[i + APP_COUNT] != 0;
            }
    }
    launcher_visible[APP_NUMSCRATCH] = 0;
    for (int i = 0; i < APP_COUNT - 1; i++)
        if (launcher_order[i] == APP_NUMSCRATCH) {
            memmove(&launcher_order[i], &launcher_order[i + 1], (APP_COUNT - i - 1) * sizeof(int));
            launcher_order[APP_COUNT - 1] = APP_NUMSCRATCH;
            break;
        }
    launcher_visible[APP_DRAWING] = 0;
    for (int i = 0; i < APP_COUNT - 2; i++)
        if (launcher_order[i] == APP_DRAWING) {
            memmove(launcher_order + i, launcher_order + i + 1, (APP_COUNT - i - 2) * sizeof(int));
            launcher_order[APP_COUNT - 2] = APP_DRAWING;
            break;
        }
    load_blob("note-tree-1.bin", &note_tree, sizeof(note_tree));
    if (note_tree.nf < 0 || note_tree.nf > 512 || note_tree.np < 0 || note_tree.np > 1024)
        memset(&note_tree, 0, sizeof(note_tree));
    note_tree.current = 0;
    for (int i = 0; i < note_tree.nf; i++) {
        note_tree.folder[i].name[95] = 0;
        if (note_tree.folder[i].parent == note_tree.folder[i].id)
            note_tree.folder[i].parent = 0;
    }
    if (load_blob("tasks-1.bin", tasks, sizeof(tasks)))
        for (int i = 0; i < 256; i++) {
            tasks[i].title[199] = 0;
            if (*tasks[i].title)
                task_count = i + 1;
            tasks[i].state = tasks[i].state >= 0 && tasks[i].state < 3 ? tasks[i].state : 0;
        }
    if (load_blob("flashcards-1.bin", flashcards, sizeof(flashcards)))
        for (int i = 0; i < 512; i++) {
            flashcards[i].front[511] = flashcards[i].back[1023] = flashcards[i].deck[63] = 0;
            if (*flashcards[i].front)
                flash_count = i + 1;
            MemoryCard *m = &flashcards[i].memory;
            if (!isfinite(m->stability) || !isfinite(m->difficulty) || m->stability < 0 ||
                m->stability > 36500 || m->difficulty < 0 || m->difficulty > 10 || m->due < 0)
                memset(m, 0, sizeof(*m));
        }
    load_blob("wordle-1.bin", &wordle_game, sizeof(wordle_game));
    if (wordle_game.row < 0 || wordle_game.row > 6)
        memset(&wordle_game, 0, sizeof(wordle_game));
    unsigned char hex_saved[123];
    if (load_blob("hex-1.bin", hex_saved, sizeof(hex_saved))) {
        int good = 1;
        for (int i = 0; i < 121; i++)
            if (hex_saved[i] > 2)
                good = 0;
        if (good) {
            memcpy(hex_board, hex_saved, 121);
            hex_turn = hex_saved[121] == 2 ? 2 : 1;
            hex_swap = hex_saved[122] != 0;
            hex_win = hex_winner(hex_board, 11);
        }
    }
    char *duration = service_read("timer-duration.txt", 32);
    if (duration) {
        int n = atoi(duration);
        if (n > 0 && n <= 86400)
            timer_total_seconds = n;
        free(duration);
    }
    unsigned char nono[51];
    if (load_blob("nonogram-1.bin", nono, sizeof(nono))) {
        int valid = nono[50] < 2;
        for (int i = 0; i < 25; i++)
            if (nono[i] > 1 || nono[i + 25] > 2)
                valid = 0;
        if (valid) {
            memcpy(nono_solution, nono, 25);
            memcpy(nono_cells, nono + 25, 25);
            nono_mark = nono[50];
            nono_ready = 1;
        }
    }
    int lp[5];
    if (load_blob("language-1.bin", lp, sizeof(lp)) && lp[0] >= 0 && lp[0] < 3 && lp[1] >= 0 &&
        lp[1] < 24 && lp[2] >= 0 && lp[3] >= lp[2]) {
        lang_pair = lp[0];
        lang_index = lp[1];
        lang_correct = lp[2];
        lang_total = lp[3];
    }
    v1_photos_scan();
}
static void v1_save(void) {
    if (!v1_ready || !v1_dirty)
        return;
    int cfg[APP_COUNT * 2];
    for (int i = 0; i < APP_COUNT; i++) {
        cfg[i] = launcher_order[i];
        cfg[i + APP_COUNT] = launcher_visible[i];
    }
    save_blob("launcher-16.bin", cfg, sizeof(cfg));
    save_blob("note-tree-1.bin", &note_tree, sizeof(note_tree));
    save_blob("tasks-1.bin", tasks, sizeof(tasks));
    save_blob("flashcards-1.bin", flashcards, sizeof(flashcards));
    save_blob("wordle-1.bin", &wordle_game, sizeof(wordle_game));
    unsigned char h[123];
    memcpy(h, hex_board, 121);
    h[121] = hex_turn;
    h[122] = hex_swap;
    save_blob("hex-1.bin", h, sizeof(h));
    if (nono_ready) {
        unsigned char n[51];
        memcpy(n, nono_solution, 25);
        memcpy(n + 25, nono_cells, 25);
        n[50] = nono_mark;
        save_blob("nonogram-1.bin", n, sizeof(n));
    }
    int lp[5] = {lang_pair, lang_index, lang_correct, lang_total, 0};
    save_blob("language-1.bin", lp, sizeof(lp));
    v1_dirty = 0;
}
#include "../system/ui/app_icons.h"
static int v1_visible(int *out) {
    int n = 0;
    for (int i = 0; i < APP_COUNT; i++) {
        int id = launcher_order[i];
        if (id != APP_NUMSCRATCH && id != APP_DRAWING && id != APP_FILES && id != APP_RPG &&
            launcher_visible[id])
            out[n++] = id;
    }
    return n;
}
static void v1_launcher(void) {
    polish_order();
    heading_actions("Apps", 1);
    icon_button(528, 74, IC_SETTINGS);
    int ids[APP_COUNT], n = v1_visible(ids);
    int pages = (n + 11) / 12;
    if (launcher_page >= pages)
        launcher_page = pages ? pages - 1 : 0;
    for (int r = 0; r < 12 && launcher_page * 12 + r < n; r++) {
        int id = ids[launcher_page * 12 + r], x = 53 + (r % 3) * 172, y = 137 + (r / 3) * 130;
        region(x, y, 150, 122);
        aa_soft_rect(x + 36, y + 2, 78, 78, 19, 0);
        aa_soft_rect(x + 38, y + 4, 74, 74, 17, 255);
        v1_icon(x + 53, y + 17, id);
        const char *name = tr(app_definition(id)->name);
        if (textwidth_raw(name, &small) <= 148)
            centered(x, y + 78, 150, 35, name, &small);
        else
            centered(x, y + 78, 150, 35, name, &caption);
    }
    if (!n)
        wrapped(28, 210, "Apps über das Zahnrad einblenden.", &body, 544, 3);
    pager(679, &launcher_page, pages);
}
static void v1_launcher_input(int x, int y) {
    if (y >= 74 && y < 118 && x >= 516) {
        extra_open(83);
        return;
    }
    int ids[APP_COUNT], n = v1_visible(ids);
    if (y >= 679 && y < 723) {
        if (x < 300 && launcher_page)
            launcher_page--;
        else if (x >= 300 && (launcher_page + 1) * 12 < n)
            launcher_page++;
        return;
    }
    if (y >= 137 && y < 657 && x >= 53 && x < 547) {
        int c = (x - 53) / 172, r = (y - 137) / 130, i = launcher_page * 12 + r * 3 + c;
        if (c < 3 && i < n) {
            AppIntent req = {.app = ids[i], .kind = INTENT_OPEN};
            app_send(&req);
        }
    }
}
#include "../vendor/wordle_words.h"
static const char *wordle_answers[] = {
    "PAPER", "LIGHT", "STONE", "EARTH", "WATER", "DREAM", "BRAIN", "BOOKS", "PLANT", "STARS",
    "NIGHT", "CLOUD", "OCEAN", "GRACE", "STUDY", "LEARN", "MUSIC", "SPACE", "GREEN", "HOUSE",
    "TRAIN", "FRAME", "CHESS", "CLOCK", "SOUND", "HEART", "CLEAR", "FRESH", "SWEET", "QUIET",
    "WORLD", "NORTH", "SOUTH", "RIVER", "FLAME", "SHORE", "BREAD", "PEACH", "APPLE", "TABLE",
    "CHAIR", "STORY", "COVER", "LAYER", "FIELD", "ORBIT", "SOLAR", "LUNAR", "SCALE", "LOGIC",
    "QUEST", "PRIDE", "BRAVE", "SMILE", "WHALE", "EAGLE", "HORSE", "TIGER", "ZEBRA", "BLOOM",
    "GLASS", "METAL", "AMBER", "CORAL"};
static int v1_word_index(void) {
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    int y = t.tm_year + 1900, m = t.tm_mon + 1, d = t.tm_mday;
    y -= m <= 2;
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = y - era * 400, doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1,
             doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    int total = sizeof(wordle_answers) / sizeof(*wordle_answers),
        base = (era * 146097 + (int)doe) % total, offset = 0;
    char *saved = service_read("wordle-calendar-offset-v11.txt", 32);
    if (saved) {
        offset = atoi(saved);
        if (offset < 0 || offset >= total)
            offset = 0;
        free(saved);
    } else {
        if (wordle_game.date == news_date() && wordle_game.row > 0) {
            int old = (((uint32_t)news_date() * 2654435761u) >> 16) % total;
            offset = (old - base + total) % total;
        }
        char b[24];
        snprintf(b, sizeof(b), "%d", offset);
        extra_write_text("wordle-calendar-offset-v11.txt", b);
    }
    return (base + offset) % total;
}
static int word_valid(const char *w) {
    int l = 0, r = sizeof(wordle_words) / 6;
    while (l < r) {
        int m = (l + r) / 2, c = strcmp(w, wordle_words[m]);
        if (!c)
            return 1;
        if (c > 0)
            l = m + 1;
        else
            r = m;
    }
    return 0;
}
static void v1_word_draw(void) {
    heading("Wordle");
    char b[80];
    snprintf(b, sizeof(b), "%02d.%02d.%04d · %s", wordle_game.date % 100,
             wordle_game.date / 100 % 100, wordle_game.date / 10000, tr("English · 6 Versuche"));
    centered(28, 130, 544, 35, b, &caption);
    for (int r = 0; r < 6; r++)
        for (int c = 0; c < 5; c++) {
            int x = 90 + c * 86, y = 187 + r * 72, v = wordle_game.result[r][c];
            rect(x, y, 76, 62, v == 2 ? 0 : v == 1 ? 175 : 255);
            key_outline(x, y, 76, 62);
            char k[2] = {wordle_game.guesses[r][c], 0};
            int old = ink_color;
            if (v == 2) {
                ink_color = -1; /* draw inverted glyph into dark tile via temporary white tile */
                rect(x + 2, y + 2, 72, 58, 255);
                centered(x, y + 2, 76, 58, k, &editorial);
                for (int yy = y + 2; yy < y + 60; yy++)
                    for (int xx = x + 2; xx < x + 74; xx++)
                        ui_pixels[yy * 600 + xx] = 255 - ui_pixels[yy * 600 + xx];
            } else
                centered(x, y + 2, 76, 58, k, &editorial);
            ink_color = old;
        }
    if (wordle_game.over) {
        snprintf(b, sizeof(b), "%s · %s", tr(wordle_game.over == 1 ? "Gewonnen!" : "Lösung"),
                 word_secret);
        centered(28, 652, 544, 42, b, &body);
        centered(28, 712, 544, 35, "Morgen ein neues Wort", &caption);
    } else
        button_h(90, 663, 420, 60, "Wort eingeben", 0);
}
static int nono_run_patterns(unsigned mask, unsigned *out) {
    unsigned n = 0, len = 0;
    for (int x = 0; x < 5; x++) {
        if (mask & (1u << x))
            len++;
        else if (len) {
            out[n++] = len;
            len = 0;
        }
    }
    if (len)
        out[n++] = len;
    return n;
}
static int nono_match(unsigned a, unsigned b) {
    unsigned x[5] = {0}, y[5] = {0};
    int n = nono_run_patterns(a, x), m = nono_run_patterns(b, y);
    return n == m && !memcmp(x, y, n * sizeof(unsigned));
}
static int nono_solve(int row, unsigned acc[5], unsigned target[5], int limit) {
    if (row == 5) {
        for (int c = 0; c < 5; c++) {
            unsigned a = 0, b = 0;
            for (int r = 0; r < 5; r++) {
                a |= ((acc[r] >> c) & 1) << r;
                b |= ((target[r] >> c) & 1) << r;
            }
            if (!nono_match(a, b))
                return 0;
        }
        return 1;
    }
    int count = 0;
    for (unsigned m = 0; m < 32; m++)
        if (nono_match(m, target[row])) {
            acc[row] = m;
            count += nono_solve(row + 1, acc, target, limit - count);
            if (count >= limit)
                return count;
        }
    return count;
}
static void v1_nono_new(void) {
    unsigned t[5], a[5];
    int found = 0;
    for (int tries = 0; tries < 200; tries++) {
        for (int r = 0; r < 5; r++)
            t[r] = (pro_random() >> 12) & 31;
        if (nono_solve(0, a, t, 2) == 1) {
            found = 1;
            break;
        }
    }
    if (!found)
        for (int i = 0; i < 5; i++)
            t[i] = 31;
    for (int i = 0; i < 25; i++)
        nono_solution[i] = (t[i / 5] >> (i % 5)) & 1;
    memset(nono_cells, 0, 25);
    nono_ready = 1;
    v1_dirty = 1;
}
static void v1_nono_draw(void) {
    heading_actions("Nonogramme", 1);
    icon_button(528, 74, IC_RESTART);
    centered(28, 133, 544, 32, "5 × 5 · Ein Bild aus Logik", &caption);
    for (int r = 0; r < 5; r++) {
        unsigned mask = 0, v[5];
        for (int c = 0; c < 5; c++)
            mask |= nono_solution[r * 5 + c] << c;
        int n = nono_run_patterns(mask, v);
        char b[64] = "";
        for (int i = 0; i < n; i++) {
            char x[8];
            snprintf(x, 8, "%u ", v[i]);
            strcat(b, x);
        }
        if (!n)
            strcpy(b, "0");
        centered(18, 263 + r * 76, 126, 76, b, &body);
    }
    for (int c = 0; c < 5; c++) {
        unsigned mask = 0, v[5];
        for (int r = 0; r < 5; r++)
            mask |= nono_solution[r * 5 + c] << r;
        int n = nono_run_patterns(mask, v);
        for (int j = 0; j < n; j++) {
            char b[8];
            snprintf(b, 8, "%u", v[j]);
            centered(146 + c * 76, 243 - (n - j) * 30, 76, 30, b, &small);
        }
    }
    int win = 1;
    for (int i = 0; i < 25; i++) {
        int x = 146 + (i % 5) * 76, y = 263 + (i / 5) * 76;
        region(x, y, 76, 76);
        rect(x + 2, y + 2, 72, 72, nono_cells[i] == 1 ? 0 : 255);
        key_outline(x, y, 76, 76);
        if (nono_cells[i] == 2) {
            stroke(x + 24, y + 24, x + 52, y + 52, 2);
            stroke(x + 52, y + 24, x + 24, y + 52, 2);
        }
        if ((nono_cells[i] == 1) != nono_solution[i])
            win = 0;
    }
    const char *tabs[] = {"Füllen", "Leer markieren"};
    segments(28, 692, 544, 48, tabs, 2, nono_mark);
    if (win)
        centered(28, 746, 544, 40, "Gelöst!", &body);
}
static void hex_center(int i, int *x, int *y) {
    int r = i / 11, c = i % 11;
    *x = 60 + r * 48;
    *y = 155 + c * 36 + r * 18;
}
static void hex_shape(int x, int y, int fill) {
    for (int yy = -18; yy <= 18; yy++) {
        int w = 32 - abs(yy) * 16 / 18;
        rect(x - w, y + yy, 2 * w + 1, 1, fill);
    }
    int dx[] = {-32, -16, 16, 32, 16, -16}, dy[] = {0, -18, -18, 0, 18, 18};
    for (int i = 0; i < 6; i++)
        stroke(x + dx[i], y + dy[i], x + dx[(i + 1) % 6], y + dy[(i + 1) % 6], 2);
}
static void v1_hex_draw(void) {
    heading_actions("Hex", 1);
    icon_button(528, 74, IC_RESTART);
    centered(28, 128, 544, 38,
             hex_win ? (hex_win == 1 ? "Schwarz gewinnt" : "Grau gewinnt")
                     : (hex_turn == 1 ? "Schwarz · links nach rechts" : "Grau · oben nach unten"),
             &small);
    for (int i = 0; i < 121; i++) {
        int x, y;
        hex_center(i, &x, &y);
        hex_shape(x, y, hex_board[i] == 1 ? 0 : hex_board[i] == 2 ? 161 : 255);
    }
    stroke(26, 155, 26, 515, 4);
    stroke(574, 335, 574, 695, 4);
    stroke(60, 133, 540, 313, 3);
    stroke(60, 537, 540, 717, 3);
    button_h(28, 735, 260, 48, "Zug zurück", 0);
    button_h(312, 735, 260, 48, hex_swap ? "Seiten tauschen" : "Neue Partie", 0);
}
static const char *v1_lessons[][3] = {
    {"Guten Morgen", "Good morning", "Bonjour"},
    {"Danke", "Thank you", "Merci"},
    {"Bitte", "Please", "S'il vous plaît"},
    {"Entschuldigung", "Excuse me", "Excusez-moi"},
    {"Wo ist der Bahnhof?", "Where is the station?", "Où est la gare ?"},
    {"Ich verstehe nicht", "I don't understand", "Je ne comprends pas"},
    {"Wasser", "Water", "Eau"},
    {"Buch", "Book", "Livre"},
    {"Heute", "Today", "Aujourd'hui"},
    {"Morgen", "Tomorrow", "Demain"},
    {"Ich möchte lernen", "I want to learn", "Je veux apprendre"},
    {"Wie viel kostet das?", "How much is it?", "Combien ça coûte ?"},
    {"Links", "Left", "Gauche"},
    {"Rechts", "Right", "Droite"},
    {"Guten Abend", "Good evening", "Bonsoir"},
    {"Auf Wiedersehen", "Goodbye", "Au revoir"},
    {"Ja", "Yes", "Oui"},
    {"Nein", "No", "Non"},
    {"Ich heiße …", "My name is …", "Je m'appelle …"},
    {"Wie geht es dir?", "How are you?", "Comment vas-tu ?"},
    {"Gut", "Good", "Bien"},
    {"Freund", "Friend", "Ami"},
    {"Zeit", "Time", "Temps"},
    {"Haus", "House", "Maison"}};
static void v1_language_draw(void) {
    polish_language();
}
static void flash_next(void) {
    flash_selected = -1;
    flash_reveal = 0;
    int64_t now = time(NULL);
    for (int i = 0; i < flash_count; i++)
        if (flashcards[i].memory.due <= now &&
            (flash_selected < 0 ||
             flashcards[i].memory.due < flashcards[flash_selected].memory.due))
            flash_selected = i;
}
static void v1_flash_draw(void) {
    heading_actions("Karteikarten", 2);
    icon_button(528, 74, IC_PLUS);
    icon_button(468, 74, IC_CONTENTS);
    int due = 0;
    for (int i = 0; i < flash_count; i++)
        due += flashcards[i].memory.due <= time(NULL);
    char b[80];
    snprintf(b, 80, "%d · %d %s · FSRS-6", flash_count, due, tr("Fällig"));
    centered(28, 136, 544, 36, b, &caption);
    if (flash_selected < 0) {
        centered(28, 280, 544, 70, "Für heute geschafft.", &editorial);
        wrapped(40, 385,
                flash_count
                    ? "Die nächste Wiederholung erscheint automatisch, sobald sie fällig ist."
                    : "Mit + eine Karte erstellen. Beliebiges Thema, eigene Frage und Antwort.",
                &body, 520, 4);
        return;
    }
    Flashcard *c = &flashcards[flash_selected];
    drawtext_raw(32, 215, c->deck, &caption, 536);
    wrapped_raw(32, 275, c->front, &editorial, 536, 5);
    if (!flash_reveal)
        button_h(28, 622, 544, 60, "Antwort zeigen", 0);
    else {
        divider(28, 440, 544);
        wrapped_raw(32, 470, c->back, &small, 536, 6);
        const char *ratings[] = {"Nochmal", "Schwer", "Gut", "Leicht"};
        for (int i = 0; i < 4; i++) {
            MemoryCard m = c->memory;
            fsrs_review(&m, i + 1, time(NULL));
            long secs = m.due - time(NULL);
            snprintf(b, 80,
                     secs < 3600    ? "%ld min"
                     : secs < 86400 ? "%ld h"
                                    : "%ld d",
                     secs < 3600    ? secs / 60
                     : secs < 86400 ? secs / 3600
                                    : secs / 86400);
            centered(28 + i * 140, 687, 124, 30, b, &caption);
            button_h(28 + i * 140, 722, 124, 54, ratings[i], 0);
        }
    }
}

static void v1_tasks_draw(void) {
    polish_tasks();
}
static void v1_photos_scan(void) {
    photo_cache_page = -1;
    photo_loaded[0] = 0;
    photo_count = 0;
    char path[2048];
    local_path(path, sizeof(path), "photos");
    mkdir(path, 0700);
    DIR *d = opendir(path);
    if (!d)
        return;
    struct dirent *e;
    while ((e = readdir(d)) && photo_count < 512) {
        size_t n = strlen(e->d_name);
        if (n > 4 && !strcmp(e->d_name + n - 4, ".pgm") && e->d_name[0] != '.')
            snprintf(photo_names[photo_count++], 256, "%s", e->d_name);
    }
    closedir(d);
    for (int i = 0; i < photo_count; i++)
        for (int j = i + 1; j < photo_count; j++)
            if (strcmp(photo_names[i], photo_names[j]) < 0) {
                char t[256];
                memcpy(t, photo_names[i], 256);
                memcpy(photo_names[i], photo_names[j], 256);
                memcpy(photo_names[j], t, 256);
            }
}
static int v1_pgm(const char *name, unsigned char *out) {
    char p[2048], m[3];
    local_path(p, sizeof(p), name);
    FILE *f = fopen(p, "rb");
    if (!f)
        return 0;
    int w, h, max,
        ok = fscanf(f, "%2s %d %d %d", m, &w, &h, &max) == 4 && !strcmp(m, "P5") && w == 600 &&
             h == 800 && max == 255;
    if (ok) {
        fgetc(f);
        ok = fread(out, 1, 480000, f) == 480000;
    }
    fclose(f);
    return ok;
}
static unsigned char *photo_pixels;
static int v1_photo_load(int i) {
    if (!photo_pixels)
        photo_pixels = malloc(480000);
    char n[300];
    if (i < 0 || i >= photo_count || !photo_pixels)
        return 0;
    snprintf(n, sizeof(n), "photos/%s", photo_names[i]);
    if (!strcmp(photo_loaded, photo_names[i]))
        return 1;
    int ok = v1_pgm(n, photo_pixels);
    if (ok)
        snprintf(photo_loaded, sizeof(photo_loaded), "%s", photo_names[i]);
    return ok;
}
static int v1_sleep_photo(void) {
    v1_photos_scan();
    if (!photo_count)
        return 0;
    uint64_t hour = (uint64_t)time(NULL) / 3600;
    photo_hour_drawn = hour;
    int i = (int)((hour * 11400714819323198485ull) >> 32) % photo_count;
    if (i < 0)
        i = -i;
    char n[300];
    snprintf(n, sizeof(n), "photos/%s", photo_names[i]);
    return v1_pgm(n, ui_pixels);
}
static void v1_export_drawing(void) {
    char path[2048];
    local_path(path, sizeof(path), "photos");
    mkdir(path, 0700);
    char name[128];
    snprintf(name, sizeof(name), "photos/drawing-%lld.pgm", (long long)time(NULL));
    local_path(path, sizeof(path), name);
    unsigned char *buf = malloc(480064);
    if (!buf)
        return;
    int n = snprintf((char *)buf, 64, "P5\n600 800\n255\n");
    memset(buf + n, 255, 480000);
    for (int y = 0; y < DRAW_H && y < 800; y++)
        memcpy(buf + n + (y + (800 - DRAW_H) / 2) * 600, drawing + y * DRAW_W, 600);
    int ok = atomic_write(path, buf, n + 480000);
    free(buf);
    ui_message(ok ? "In Fotos gespeichert." : "Speichern fehlgeschlagen.");
    v1_photos_scan();
}
static void v1_photos_draw(void) {
    polish_photos_grid();
}
static void v1_photo_draw(void) {
    polish_photo();
}
static void v1_archive_scan(void) {
    news_date_count = 0;
    news_dates[news_date_count++] = news_date();
    char p[2048];
    local_path(p, sizeof(p), "news");
    DIR *d = opendir(p);
    if (!d)
        return;
    struct dirent *e;
    while ((e = readdir(d)) && news_date_count < 2048) {
        int n;
        char tail;
        if (sscanf(e->d_name, "%8d.bin%c", &n, &tail) == 1 && strlen(e->d_name) == 12 &&
            n != news_date() && n > 20000101 && n <= 120261231)
            news_dates[news_date_count++] = n;
    }
    closedir(d);
    for (int i = 1; i < news_date_count; i++)
        for (int j = i + 1; j < news_date_count; j++)
            if (news_dates[j] > news_dates[i]) {
                int t = news_dates[i];
                news_dates[i] = news_dates[j];
                news_dates[j] = t;
            }
}
static void v1_archive_draw(void) {
    polish_news_draw();
}
static const char *v1_folder_name(int id) {
    for (int i = 0; i < note_tree.nf; i++)
        if (note_tree.folder[i].id == id)
            return note_tree.folder[i].name;
    return tr("Notizen");
}

static void v1_notes_draw(void) {
    polish_notes();
}
static int v1_notes_input(const Input *in) {
    return polish_notes_input(in);
}
static void v1_note_tools(void) {
    u15_note_tools();
}
static void v1_folder_picker_draw(void) {
    heading("Notiz verschieben");
    int n = note_tree.nf + 1;
    for (int r = 0; r < 8 && folder_page * 8 + r < n; r++) {
        int i = folder_page * 8 + r, y = 145 + r * 62;
        region(28, y, 544, 58);
        icon(28, y + 4, IC_CONTENTS);
        drawtext_raw(88, y + 12, i ? note_tree.folder[i - 1].name : tr("Notizen"), &body, 480);
        if (i) {
            const char *p = v1_folder_name(note_tree.folder[i - 1].parent);
            drawtext_raw(88, y + 40, p, &caption, 480);
        }
    }
    pager(699, &folder_page, (n + 7) / 8);
}
static int markdown_pages = 1;
static const char *md_next(const char *p, int width, const Font *f) {
    const char *q = p, *space = NULL;
    int n = 0;
    while (*q && *q != '\n') {
        const char *old = q;
        unsigned c = glyph(&q);
        int a = glyph_advance(f, c);
        if (n + a > width) {
            q = space && space > p ? space : old;
            break;
        }
        n += a;
        if (c == ' ')
            space = q;
    }
    if (q == p && *q && *q != '\n')
        glyph(&q);
    if (*q == '\n')
        q++;
    return q;
}
static void md_inline(int x, int y, const char *s, int width, const Font *f, int literal) {
    int bold = 0, code = 0;
    const char *p = s;
    while (*p && *p != '\n' && x < 30 + width) {
        if (!literal && p[0] == '*' && p[1] == '*') {
            bold ^= 1;
            p += 2;
            continue;
        }
        if (!literal && *p == '`') {
            code ^= 1;
            p++;
            continue;
        }
        const char *start = p;
        unsigned c = glyph(&p);
        char b[8] = {0};
        size_t n = p - start;
        if (n > 7)
            n = 7;
        memcpy(b, start, n);
        int a = glyph_advance(f, c);
        if (code)
            rect(x, y + 1, a, f->h - 3, 230);
        drawtext_raw(x, y, b, f, a + 4);
        if (bold)
            drawtext_raw(x + 1, y, b, f, a + 3);
        x += a;
    }
}
static void v1_markdown_draw(void) {
    icon_button(528, 57, IC_TYPE);
    const char *p = note_text;
    int pg = 0, y = 116, code = 0;
    while (*p) {
        int level = 0, quote = 0, list = 0;
        const char *q = p;
        if (!strncmp(q, "```", 3)) {
            code ^= 1;
            p = strchr(q, '\n');
            if (!p)
                break;
            p++;
            continue;
        }
        if (!code) {
            while (*q == '#' && level < 6) {
                level++;
                q++;
            }
            if (level && *q == ' ')
                q++;
            else {
                level = 0;
                q = p;
            }
            if (*q == '>' && q[1] == ' ') {
                quote = 1;
                q += 2;
            }
            if (*q == '-' && q[1] == ' ') {
                list = 1;
                q += 2;
            }
        }
        const Font *f = level ? &editorial : &small;
        int h = level ? 44 : 30;
        if (y + h > 731) {
            pg++;
            y = 116;
        }
        int inset = (list || quote) ? 22 : 0;
        const char *next = md_next(q, 540 - inset, f);
        char b[1024];
        snprintf(b, sizeof(b), "%.*s", (int)(next - q), q);
        if (pg == markdown_page) {
            if (code) {
                rect(27, y, 546, h, 235);
                md_inline(34, y, b, 530, &small, 1);
            } else if (!strncmp(b, "---", 3)) {
                rect(30, y + 14, 540, 1, 180);
            } else {
                if (list)
                    disc(35, y + 15, 3, 0);
                if (quote)
                    rect(30, y + 3, 3, h - 5, 140);
                md_inline(30 + inset, y, b, 540 - inset, f, 0);
            }
        }
        p = next;
        y += h;
    }
    markdown_pages = pg + 1;
    if (markdown_page >= markdown_pages)
        markdown_page = markdown_pages - 1;
    pager(746, &markdown_page, markdown_pages);
}
#include "planetarium.h"
static void v1_timer_ring(int fullscreen) {
    int64_t remain = timer_due      ? timer_due - extra_millis()
                     : timer_paused ? timer_paused
                                    : (int64_t)timer_minutes * 60000;
    if (remain < 0)
        remain = 0;
    int total = timer_total_seconds > 0 ? timer_total_seconds : timer_minutes * 60;
    double f = (timer_due || timer_paused) ? fmin(1, remain / (total * 1000.)) : 1;
    int cy = fullscreen ? 370 : 278,
        r = fullscreen ? 215 : 72; /* Sparse ring, only updated on changed seconds. */
    aa_arc_color(300, cy, r, 0, 360, fullscreen ? 12 : 10, 205);
    int end = 270 + (int)(360 * f);
    if (f >= .9999)
        ring(300, cy, r, fullscreen ? 12 : 10);
    else if (end <= 360)
        aa_arc(300, cy, r, 270, end, fullscreen ? 12 : 10);
    else {
        aa_arc(300, cy, r, 270, 360, fullscreen ? 12 : 10);
        aa_arc(300, cy, r, 0, end - 360, fullscreen ? 12 : 10);
    }
    char b[64];
    long secs = (remain + 999) / 1000;
    snprintf(b, sizeof(b), "%02ld:%02ld", secs / 60, secs % 60);
    centered(28, cy - 45, 544, 90, b, fullscreen ? &title : &editorial);
    if (fullscreen) {
        centered(28, cy + 66, 544, 36, "Verbleibende Zeit", &caption);
        button_h(28, 665, 260, 60,
                 timer_due      ? "Pause"
                 : timer_paused ? "Fortsetzen"
                                : "Timer starten",
                 0);
        button_h(312, 665, 260, 60, "Zurücksetzen", 0);
    }
}
static int launcher_manage_indices(int *indices) {
    int n = 0;
    for (int i = 0; i < APP_COUNT; i++)
        if (app_route_valid(app_definition(launcher_order[i])->entry))
            indices[n++] = i;
    return n;
}
static void v1_launcher_settings(void) {
    heading("Apps verwalten");
    drawtext(30, 127, "Sichtbarkeit & Reihenfolge", &caption, 540);
    int indices[APP_COUNT], count = launcher_manage_indices(indices);
    for (int r = 0; r < 7 && launcher_settings_page * 7 + r < count; r++) {
        int pos = launcher_settings_page * 7 + r, i = indices[pos], id = launcher_order[i],
            y = 174 + r * 68;
        v1_icon(28, y + 10, id);
        label_at(87, y + 10, 252, 44, tr(app_definition(id)->name), &small);
        page_arrow(354, y + 10, IC_UP, pos > 0);
        page_arrow(405, y + 10, IC_DOWN, pos + 1 < count);
        toggle(492, y + 16, launcher_visible[id]);
        divider(28, y + 67, 544);
    }
    pager(687, &launcher_settings_page, (count + 6) / 7);
}
static void v1_open(int target) {
    polish_open(target);
    v1_init();
    v1_page = 0;
    if (target == 70) {
        int date = news_date();
        if (wordle_game.date != date) {
            memset(&wordle_game, 0, sizeof(wordle_game));
            wordle_game.date = date;
            v1_dirty = 1;
        }
        snprintf(word_secret, 6, "%s", wordle_answers[v1_word_index()]);
    }
    if (target == 71)
        v1_photos_scan();
    if (target == 76)
        flash_next();
    if (target == 79) {
        static int ready;
        if (!ready) {
            sky_init();
            ready = 1;
        }
    }
    if (target == 81 && !nono_ready)
        v1_nono_new();
    if (target == 85)
        markdown_page = 0;
    if (target == 87 && !timer_due && !timer_paused)
        timer_total_seconds = timer_minutes * 60;
    if (target == 90 || target == 91) { /* registered shell components open their configuration */
        setting_category = 1;
        setting_page = target == 90 ? 0 : 1;
    }
    if (target == 93)
        open_keyboard(130, "Tastatur", "");
}
static void v1_draw(void) {
    if (screen >= 94) {
        polish_view();
        return;
    }
    switch (screen) {
    case 70:
        v1_word_draw();
        break;
    case 71:
        v1_photos_draw();
        break;
    case 72:
        v1_photo_draw();
        break;
    case 73:
        v1_language_draw();
        break;
    case 74:
        v1_tasks_draw();
        break;
    case 76:
        v1_flash_draw();
        break;
    case 77:
        heading_actions("Kartenbibliothek", 1);
        icon_button(528, 74, IC_PLUS);
        for (int r = 0; r < 7 && flash_page * 7 + r < flash_count; r++) {
            int i = flash_page * 7 + r, y = 151 + r * 72;
            region(28, y, 474, 66);
            drawtext_raw(30, y, flashcards[i].front, &small, 466);
            drawtext_raw(30, y + 33, flashcards[i].deck, &caption, 466);
            icon_button(522, y + 8, IC_TRASH);
            divider(28, y + 66, 544);
        }
        pager(695, &flash_page, (flash_count + 6) / 7);
        break;
    case 78:
        heading("Karte bearbeiten");
        button_h(28, 180, 544, 60, "Sammlung", 0);
        button_h(28, 269, 544, 60, "Frage", 0);
        button_h(28, 358, 544, 60, "Antwort", 0);
        break;
    case 79:
        sky_draw();
        break;
    case 80:
        sky_settings_draw();
        break;
    case 81:
        v1_nono_draw();
        break;
    case 82:
        v1_hex_draw();
        break;
    case 83:
        v1_launcher_settings();
        break;
    case 84:
        v1_folder_picker_draw();
        break;
    case 85:
        v1_markdown_draw();
        break;
    case 86:
        v1_archive_draw();
        break;
    case 87:
        v1_timer_ring(1);
        break;
    case 90:
    case 91:
        more_settings();
        break;
    case 92:
        heading("Sperrbild");
        for (int i = 0; i < 5; i++)
            button_h(28, 160 + i * 100, 544, 65, lockmodes[i], options[OPT_LOCK] == i);
        break;
    case 93:
        heading("Tastatur");
        button_h(28, 180, 544, 60, "Text eingeben", 0);
        break;
    }
    if (v1_delete) {
        rect(21, 267, 564, 236, 190);
        rect(17, 263, 564, 236, 255);
        key_outline(17, 263, 564, 236);
        centered(28, 285, 544, 55, "Wirklich löschen?", &editorial);
        button_h(36, 400, 250, 58, "Abbrechen", 0);
        button_h(310, 400, 250, 58, "Löschen", 0);
    }
}
static int v1_commit(int target, const char *text) {
    if (target >= 150)
        return polish_commit(target, text);
    if (target == 100) {
        char g[6];
        if (strlen(text) != 5) {
            ui_message("Bitte fünf englische Buchstaben eingeben.");
            return 1;
        }
        for (int i = 0; i < 5; i++) {
            if (!isalpha((unsigned char)text[i]) || ((unsigned char)text[i]) > 127) {
                ui_message("Bitte fünf englische Buchstaben eingeben.");
                return 1;
            }
            g[i] = toupper((unsigned char)text[i]);
        }
        g[5] = 0;
        if (!word_valid(g)) {
            ui_message("Wort nicht im Wörterbuch.");
            return 1;
        }
        if (wordle_game.row >= 6 || wordle_game.over)
            return 1;
        int r = wordle_game.row++;
        memcpy(wordle_game.guesses[r], g, 6);
        wordle_score(word_secret, g, wordle_game.result[r]);
        if (!strcmp(g, word_secret))
            wordle_game.over = 1;
        else if (wordle_game.row == 6)
            wordle_game.over = 2;
        v1_dirty = 1;
        return 1;
    }
    if (target == 110) {
        if (*text && task_count < 256) {
            snprintf(tasks[task_count].title, 200, "%s", text);
            snprintf(task_categories16[task_count], 64, "%s", task_context16);
            save_blob("task-categories16.bin", task_categories16, sizeof(task_categories16));
            tasks[task_count].created = time(NULL);
            tasks[task_count++].state = 0;
            v1_dirty = 1;
        }
        return 1;
    }
    if (target == 111) {
        if (!*text)
            return 1;
        snprintf(flash_deck, 64, "%s", text);
        open_keyboard(112, "Frage", flash_edit >= 0 ? flashcards[flash_edit].front : "");
        return 2;
    }
    if (target == 112) {
        if (!*text)
            return 1;
        snprintf(flash_front, 512, "%s", text);
        open_keyboard(113, "Antwort", flash_edit >= 0 ? flashcards[flash_edit].back : "");
        return 2;
    }
    if (target == 113) {
        if (!*text || (!*flash_front && flash_edit < 0))
            return 1;
        int i = flash_edit >= 0 ? flash_edit : flash_count;
        if (i >= 512) {
            ui_message("Kartenbibliothek voll.");
            return 1;
        }
        if (flash_edit < 0)
            memset(&flashcards[i], 0, sizeof(Flashcard));
        if (!flashcards[i].id)
            flashcards[i].id = (uint64_t)time(NULL) * 1000 + i;
        snprintf(flashcards[i].deck, 64, "%s", flash_deck);
        snprintf(flashcards[i].front, 512, "%s", flash_front);
        snprintf(flashcards[i].back, 1024, "%s", text);
        if (i == flash_count)
            flash_count++;
        v1_dirty = 1;
        flash_edit = -1;
        flash_next();
        return 1;
    }
    if (target == 120) {
        if (*text && note_tree.nf < 512) {
            int id = 1;
            for (int i = 0; i < note_tree.nf; i++)
                if (note_tree.folder[i].id >= id)
                    id = note_tree.folder[i].id + 1;
            NoteFolder *f = &note_tree.folder[note_tree.nf++];
            f->id = id;
            f->parent = note_tree.current;
            snprintf(f->name, 96, "%s", text);
            v1_dirty = 1;
        }
        return 1;
    }
    if (target == 121) {
        double lat, lon;
        char extra;
        if (sscanf(text, "%lf , %lf %c", &lat, &lon, &extra) == 2 && isfinite(lat) &&
            isfinite(lon) && fabs(lat) <= 90 && fabs(lon) <= 180) {
            sky_config.lat = lat;
            sky_config.lon = lon;
            double p[2] = {lat, lon};
            save_blob("sky-location-1.bin", p, sizeof(p));
            sky_key[0] = 0;
        } else
            ui_message("Breitengrad, Längengrad prüfen.");
        return 1;
    }
    if (target == 122) {
        int Y, M, D, h, m;
        char extra;
        if (sscanf(text, "%d-%d-%d %d:%d %c", &Y, &M, &D, &h, &m, &extra) == 5 && Y >= 1 &&
            Y <= 12026 && M >= 1 && M <= 12 && D >= 1 && D <= calendar_days(Y, M) && h >= 0 &&
            h < 24 && m >= 0 && m < 60) {
            sky_config.year = Y;
            sky_config.month = M;
            sky_config.day = D;
            sky_config.hour = h;
            sky_config.minute = m;
            sky_key[0] = 0;
        } else
            ui_message("Datum: YYYY-MM-DD HH:MM UTC");
        return 1;
    }
    if (target == 130)
        return 1;
    return 0;
}
static int v1_tick(void) {
    if (polish_tick())
        return 1;
    if (alarm_ring) {
        if (!timer_flash_started) {
            timer_flash_started = extra_millis();
            timer_last_phase = -1;
        }
        int64_t phase = (extra_millis() - timer_flash_started) / 500;
        if (phase < 120 && phase != timer_last_phase) {
            timer_last_phase = phase;
            return 1;
        }
    } else
        timer_flash_started = 0;
    static int lastsecond;
    int sec = timer_due ? (timer_due - extra_millis()) / 1000 : 0;
    if ((screen == 87 || screen == 28) && sec != lastsecond) {
        lastsecond = sec;
        return 1;
    }
    return 0;
}
static int v1_poll(void) {
    if (screen == 48 && chess_computer && chess.turn < 0)
        return 250;
    if (alarm_ring && timer_flash_started && extra_millis() - timer_flash_started < 60000)
        return 500;
    if (screen == 87)
        return 1000;
    return -1;
}
static void v1_effect(void) {
    if (alarm_ring && timer_flash_started && extra_millis() - timer_flash_started < 60000 &&
        (timer_last_phase & 1))
        for (size_t i = 0; i < sizeof(ui_pixels); i++)
            ui_pixels[i] = 255 - ui_pixels[i];
}
static int v1_input(const Input *in) {
    if (screen >= 94)
        return polish_input(in);
    if (screen == 86 || screen == 67)
        return polish_news_input(in);
    if (screen == 72 && !v1_delete)
        return polish_photo_input(in);
    if (screen == 73)
        return polish_language_input(in);
    if (in->kind != INPUT_TAP)
        return 2;
    int x = in->x, y = in->y;
    if (v1_delete) {
        if (y >= 400 && y < 458) {
            if (x >= 300) {
                if (v1_delete == 1 && photo_selected >= 0 && photo_selected < photo_count) {
                    char p[2048], n[300];
                    snprintf(n, sizeof(n), "photos/%s", photo_names[photo_selected]);
                    local_path(p, sizeof(p), n);
                    unlink(p);
                    char cache[2048], thumb[300];
                    snprintf(thumb, sizeof(thumb), "photos/.thumbs/%s",
                             photo_names[photo_selected]);
                    local_path(cache, sizeof(cache), thumb);
                    unlink(cache);
                    v1_photos_scan();
                    extra_return(71);
                }
                if (v1_delete == 2 && flash_edit >= 0 && flash_edit < flash_count) {
                    memmove(&flashcards[flash_edit], &flashcards[flash_edit + 1],
                            (flash_count - flash_edit - 1) * sizeof(Flashcard));
                    memset(&flashcards[--flash_count], 0, sizeof(Flashcard));
                    v1_dirty = 1;
                }
                if (v1_delete >= 1000) {
                    int i = v1_delete - 1000;
                    if (i < task_count) {
                        memmove(task_categories16 + i, task_categories16 + i + 1,
                                (task_count - i - 1) * 64);
                        memset(task_categories16[task_count - 1], 0, 64);
                        save_blob("task-categories16.bin", task_categories16,
                                  sizeof(task_categories16));
                        memmove(&tasks[i], &tasks[i + 1], (task_count - i - 1) * sizeof(tasks[0]));
                        memset(&tasks[--task_count], 0, sizeof(tasks[0]));
                        v1_dirty = 1;
                    }
                }
            }
            v1_delete = 0;
        }
        return 1;
    }
    switch (screen) {
    case 70:
        if (!wordle_game.over && y >= 663 && y < 723)
            open_keyboard(100, "Wordle · English", "");
        break;
    case 71:
        if (y >= 74 && y < 118 && x >= 516) {
            extra_open(8);
            return 1;
        }
        if (y >= 746 && y < 790) {
            if (x < 300 && photo_page)
                photo_page--;
            else if (x >= 300 && (photo_page + 1) * 16 < photo_count)
                photo_page++;
        } else if (y >= 142 && y < 710 && x >= 19 && x < 581) {
            int c = (x - 19) / 142, r = (y - 142) / 142, i = photo_page * 16 + r * 4 + c;
            if (c < 4 && i < photo_count) {
                photo_selected = i;
                extra_open(72);
            }
        }
        return 1;
    case 72:
        if (y >= 742) {
            if (x < 100 && photo_selected)
                photo_selected--;
            else if (x > 500 && photo_selected + 1 < photo_count)
                photo_selected++;
            else if (x >= 270 && x < 330)
                v1_delete = 1;
        }
        break;
    case 73:
        if (y >= 135 && y < 179) {
            lang_pair = (x - 28) / (544 / 3);
            if (lang_pair < 0)
                lang_pair = 0;
            if (lang_pair > 2)
                lang_pair = 2;
            lang_reveal = 0;
        } else if (!lang_reveal && y >= 537 && y < 597)
            lang_reveal = 1;
        else if (lang_reveal && y >= 610 && y < 666) {
            lang_total++;
            v1_dirty = 1;
            if (x >= 300)
                lang_correct++;
            lang_index = (lang_index + 1) % 24;
            lang_reveal = 0;
        }
        break;
    case 74:
        return polish_tasks_input(in);
    case 76:
        if (y >= 74 && y < 118) {
            if (x >= 516) {
                flash_edit = -1;
                open_keyboard(111, "Sammlung", flash_deck);
            } else if (x >= 456)
                extra_open(77);
        } else if (flash_selected >= 0) {
            if (!flash_reveal && y >= 622 && y < 682)
                flash_reveal = 1;
            else if (flash_reveal && y >= 722 && y < 776 && x >= 28 && x < 572) {
                int grade = (x - 28) / 140 + 1;
                if (grade <= 4) {
                    fsrs_review(&flashcards[flash_selected].memory, grade, time(NULL));
                    char path[2048];
                    local_path(path, sizeof(path), "flashcard-reviews.tsv");
                    FILE *log = fopen(path, "a");
                    if (log) {
                        MemoryCard *m = &flashcards[flash_selected].memory;
                        fprintf(log, "%lld\t%llu\t%d\t%.8g\t%.8g\t%lld\n", (long long)m->last,
                                (unsigned long long)flashcards[flash_selected].id, grade,
                                m->stability, m->difficulty, (long long)m->due);
                        fclose(log);
                    }
                    v1_dirty = 1;
                    flash_next();
                }
            }
        }
        break;
    case 77:
        if (y >= 74 && y < 118 && x >= 516) {
            flash_edit = -1;
            open_keyboard(111, "Sammlung", flash_deck);
        } else if (y >= 151 && y < 655) {
            int i = flash_page * 7 + (y - 151) / 72;
            if (i < flash_count) {
                flash_edit = i;
                if (x >= 510)
                    v1_delete = 2;
                else {
                    snprintf(flash_deck, 64, "%s", flashcards[i].deck);
                    snprintf(flash_front, 512, "%s", flashcards[i].front);
                    open_keyboard(111, "Sammlung", flash_deck);
                }
            }
        } else if (y >= 695 && y < 739) {
            if (x < 300 && flash_page)
                flash_page--;
            else if (x >= 300 && (flash_page + 1) * 7 < flash_count)
                flash_page++;
        }
        break;
    case 79:
        if (y >= 74 && y < 118 && x >= 516) {
            extra_open(80);
            break;
        }
        if (y >= 696 && y < 734) {
            if (x < 80)
                sky_shift(-1);
            else if (x >= 520)
                sky_shift(1);
            else {
                sky_step = (x - 83) / (434 / 4);
                if (sky_step > 3)
                    sky_step = 3;
                sky_shift(1);
            }
            if (sky_step < 0)
                sky_step = 0;
            if (sky_step > 3)
                sky_step = 3;
        } else if (y >= 747) {
            if (x < 200)
                sky_now();
            else if (x < 390) {
                sky_panel = 1;
                extra_open(80);
            } else {
                char b[80];
                snprintf(b, 80, "%04d-%02d-%02d %02d:%02d", sky_config.year, sky_config.month,
                         sky_config.day, sky_config.hour, sky_config.minute);
                open_keyboard(122, "Datum · UTC", b);
            }
        } else if (y >= 158 && y < 693) {
            int best = 625;
            sky_selected = -1;
            for (unsigned i = 0; i < sizeof(sky_stars) / sizeof(*sky_stars); i++) {
                int dx = x - sky_px[i], dy = y - sky_py[i], d = dx * dx + dy * dy;
                if (d < best) {
                    best = d;
                    sky_selected = i;
                }
            }
        }
        break;
    case 80:
        if (y >= 136 && y < 180) {
            sky_panel = (x - 28) / (544 / 3);
            if (sky_panel < 0)
                sky_panel = 0;
            if (sky_panel > 2)
                sky_panel = 2;
        } else if (!sky_panel) {
            if (y >= 228 && y < 284) {
                char b[100];
                snprintf(b, 100, "%.4f, %.4f", sky_config.lat, sky_config.lon);
                open_keyboard(121, "Breitengrad, Längengrad", b);
            } else if (y >= 325 && y < 375 && x >= 460) {
                sky_show_lines ^= 1;
                sky_key[0] = 0;
            }
        } else if (sky_panel == 2 && y >= 708 && y < 752) {
            if (x < 300 && v1_page)
                v1_page--;
            else if (x >= 300 && v1_page < 4)
                v1_page++;
        }
        break;
    case 81:
        if (y >= 74 && y < 118 && x >= 516)
            v1_nono_new();
        else if (y >= 692 && y < 740)
            nono_mark = x >= 300;
        else if (x >= 146 && x < 526 && y >= 263 && y < 643) {
            int i = ((y - 263) / 76) * 5 + (x - 146) / 76;
            int val = nono_mark ? 2 : 1;
            nono_cells[i] = nono_cells[i] == val ? 0 : val;
            v1_dirty = 1;
        }
        break;
    case 82:
        if (y >= 74 && y < 118 && x >= 516) {
            memset(hex_board, 0, 121);
            hex_turn = 1;
            hex_win = hex_swap = hex_has_undo = 0;
            v1_dirty = 1;
        } else if (y >= 735) {
            if (x < 300 && hex_has_undo) {
                memcpy(hex_board, hex_previous, 121);
                hex_turn = 3 - hex_turn;
                hex_win = 0;
                hex_has_undo = 0;
                v1_dirty = 1;
            } else if (x >= 300 && hex_swap && !hex_win) {
                for (int i = 0; i < 121; i++)
                    if (hex_board[i])
                        hex_board[i] = 3 - hex_board[i];
                hex_swap = 0;
                hex_turn = 1;
                hex_has_undo = 0;
                v1_dirty = 1;
            } else if (x >= 300) {
                memset(hex_board, 0, 121);
                hex_turn = 1;
                hex_win = hex_swap = hex_has_undo = 0;
                v1_dirty = 1;
            }
        } else if (!hex_win) {
            int best = 1500, idx = -1;
            for (int i = 0; i < 121; i++) {
                int xx, yy;
                hex_center(i, &xx, &yy);
                int d = (xx - x) * (xx - x) + (yy - y) * (yy - y);
                if (abs(yy - y) <= 18 && abs(xx - x) <= 32 - abs(yy - y) * 16 / 18 && d < best) {
                    best = d;
                    idx = i;
                }
            }
            if (idx >= 0 && !hex_board[idx]) {
                memcpy(hex_previous, hex_board, 121);
                hex_has_undo = 1;
                hex_board[idx] = hex_turn;
                hex_win = hex_winner(hex_board, 11);
                int n = 0;
                for (int i = 0; i < 121; i++)
                    n += hex_board[i] != 0;
                hex_swap = n == 1;
                hex_turn = 3 - hex_turn;
                v1_dirty = 1;
            }
        }
        break;
    case 83: {
        int indices[APP_COUNT], count = launcher_manage_indices(indices);
        if (y >= 174 && y < 650) {
            int pos = launcher_settings_page * 7 + (y - 174) / 68;
            if (pos < count) {
                int i = indices[pos], id = launcher_order[i];
                if (x >= 460)
                    launcher_visible[id] ^= 1;
                else if (x >= 354 && x < 400 && pos > 0) {
                    int other = indices[pos - 1];
                    launcher_order[i] = launcher_order[other];
                    launcher_order[other] = id;
                } else if (x >= 405 && x < 455 && pos + 1 < count) {
                    int other = indices[pos + 1];
                    launcher_order[i] = launcher_order[other];
                    launcher_order[other] = id;
                }
                v1_dirty = 1;
            }
        } else if (y >= 687 && y < 731) {
            if (x < 300 && launcher_settings_page)
                launcher_settings_page--;
            else if (x >= 300 && (launcher_settings_page + 1) * 7 < count)
                launcher_settings_page++;
        }
        break;
    }
    case 84:
        if (y >= 145 && y < 641) {
            int i = folder_page * 8 + (y - 145) / 62;
            if (i <= note_tree.nf) {
                v1_place(note_selected, i ? note_tree.folder[i - 1].id : 0);
                extra_return(30);
            }
        } else if (y >= 699 && y < 743) {
            if (x < 300 && folder_page)
                folder_page--;
            else if (x >= 300 && (folder_page + 1) * 8 < note_tree.nf + 1)
                folder_page++;
        }
        break;
    case 85:
        if (y >= 57 && y < 101 && x >= 516)
            extra_return(30);
        else if (y >= 746) {
            if (x < 300 && markdown_page)
                markdown_page--;
            else if (x >= 300 && markdown_page + 1 < markdown_pages)
                markdown_page++;
        }
        break;
    case 86:
        if (y >= 153 && y < 197 && x >= 516)
            extra_open(68);
        else if (y >= 223 && y < 679) {
            int i = news_archive_page * 6 + (y - 223) / 76;
            if (i < news_date_count) {
                int date = news_dates[i];
                if (news_load(date)) {
                    extra_open(67);
                } else {
                    news_start(options[OPT_LANGUAGE], news_topics);
                    extra_open(67);
                }
            }
        } else if (y >= 710 && y < 754) {
            if (x < 300 && news_archive_page)
                news_archive_page--;
            else if (x >= 300 && (news_archive_page + 1) * 6 < news_date_count)
                news_archive_page++;
        }
        break;
    case 87:
        if (y >= 665 && y < 725) {
            timer_action(x >= 300);
            extra_save();
        }
        break;
    case 90:
    case 91:
        settings_input(in);
        break;
    case 92:
        if (y >= 160 && y < 625) {
            int i = (y - 160) / 100;
            if (i < 5) {
                options[OPT_LOCK] = i;
                ui_save();
            }
        }
        break;
    case 93:
        if (y >= 180 && y < 240)
            open_keyboard(130, "Tastatur", "");
        break;
    }
    return 1;
}
static pid_t chess_worker = -1;
static ChessPosition chess_job;
static unsigned chess_nodes;
static int chess_eval(const ChessPosition *p) {
    static const int val[] = {0, 100, 320, 330, 500, 900, 20000};
    int n = 0;
    for (int i = 0; i < 64; i++) {
        int v = p->b[i];
        if (v) {
            int center = 7 - abs(i % 8 * 2 - 7) / 2 - abs(i / 8 * 2 - 7) / 2;
            n += side(v) * (val[abs(v)] + (abs(v) == 1  ? (v > 0 ? 6 - i / 8 : i / 8 - 1) * 5
                                           : abs(v) < 4 ? center * 5
                                                        : 0));
        }
    }
    return n * p->turn;
}
static int chess_search(const ChessPosition *p, int depth, int alpha, int beta) {
    if (!depth || ++chess_nodes > 3500)
        return chess_eval(p);
    int best = -30000, moves = 0;
    for (int captures = 1; captures >= 0; captures--)
        for (int f = 0; f < 64; f++)
            if (side(p->b[f]) == p->turn)
                for (int t = 0; t < 64; t++) {
                    if ((p->b[t] != 0) != captures || !chess_pseudo(p, f, t) ||
                        !chess_legal(p, f, t))
                        continue;
                    moves++;
                    ChessPosition q = *p;
                    chess_apply(&q, f, t, 5);
                    int score = -chess_search(&q, depth - 1, -beta, -alpha);
                    if (score > best)
                        best = score;
                    if (score > alpha)
                        alpha = score;
                    if (alpha >= beta)
                        return alpha;
                }
    return moves ? best : chess_check(p, p->turn) ? -29000 - depth : 0;
}
static int v1_chess_tick(void) {
    if (chess_worker > 0) {
        int status;
        pid_t done = waitpid(chess_worker, &status, WNOHANG);
        if (done == 0)
            return 0;
        chess_worker = -1;
        if (done > 0 && WIFEXITED(status) && !WEXITSTATUS(status) && screen == 48 &&
            chess_computer && !memcmp(&chess, &chess_job, sizeof(chess))) {
            int move[2];
            if (load_blob("chess-ai-result.bin", move, sizeof(move)) &&
                chess_legal(&chess, move[0], move[1])) {
                if (chess_history < 256)
                    chess_undo[chess_history++] = chess;
                chess_apply(&chess, move[0], move[1], 5);
                chess_store();
                return 1;
            }
        }
    }
    if (screen != 48 || !chess_computer || chess.turn != -1 || chess_drawn() || !chess_moves())
        return 0;
    chess_job = chess;
    chess_worker = fork();
    if (chess_worker == 0) {
        chess_nodes = 0;
        int move[2] = {-1, -1}, best = -40000;
        for (int f = 0; f < 64; f++)
            if (side(chess.b[f]) == chess.turn)
                for (int t = 0; t < 64; t++)
                    if (chess_pseudo(&chess, f, t) && chess_legal(&chess, f, t)) {
                        ChessPosition q = chess;
                        chess_apply(&q, f, t, 5);
                        int score = -chess_search(&q, 2, -30000, 30000);
                        if (score > best) {
                            best = score;
                            move[0] = f;
                            move[1] = t;
                        }
                    }
        int ok = save_blob("chess-ai-result.bin", move, sizeof(move));
        _exit(ok ? 0 : 1);
    }
    return 1;
}

static void v1_note_created(int id) {
    v1_place(id, note_tree.current);
}

static void v1_close(void) {
    v1_save();
    if (chess_worker > 0) {
        kill(chess_worker, SIGTERM);
        while (waitpid(chess_worker, NULL, 0) < 0 && errno == EINTR) {
        }
        chess_worker = -1;
    }
    free(photo_pixels);
    photo_pixels = NULL;
}

static void v1_timer_started(void) {
    timer_total_seconds = timer_minutes * 60;
    char b[32];
    snprintf(b, sizeof(b), "%d", timer_total_seconds);
    extra_write_text("timer-duration.txt", b);
}
static int v1_sleep_poll(void) {
    if (options[OPT_LOCK] != 4 || !photo_count)
        return -1;
    return (3601 - time(NULL) % 3600) * 1000;
}
static int v1_sleep_tick(void) {
    return options[OPT_LOCK] == 4 && photo_count && photo_hour_drawn != time(NULL) / 3600;
}
