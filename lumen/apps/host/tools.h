/* Focused paper apps. Networking runs in a bounded child, never in the input loop. */
#include <fcntl.h>
static int companion_page16, companion_route16 = -1;
static int calendar_loaded16, calendar_day_page16;
static char files_dir16[4096], explorer_dir16[4096], file_names16[512][256];
static int explorer_page16;
static unsigned char file_dirs16[512];
static int files_count16;
static int journal_step16, journal_score16 = 3;
static char journal_answers16[7][8192];
static int game_board16[144], game_turn16 = 1, game_won16, game_moves16, game_mode16,
                              game_level16 = 1;
static int game_undo16[8], game_undo_valid16;
static int game_previous16[144], game_px16, game_py16, game_hp16 = 12, game_gold16;
static unsigned game_seed16;
static int recipe_search16;
typedef struct {
    char id[40], title[160];
    time_t opened;
} RecipeRecent16;
static RecipeRecent16 recipe_recent16[128];
static int recipe_recent_count16;
static void recipe_archive16(void);
static char llm_model16[100] = "gpt-4.1-mini", llm_prompt16[8192], job_error16[200];
static int job_kind16;

static int study_minutes16 = 25;
static char study_selected16[256];
static unsigned random16(void) {
    if (!game_seed16)
        game_seed16 = (unsigned)time(NULL) ^ 0xa6b47d13;
    game_seed16 ^= game_seed16 << 13;
    game_seed16 ^= game_seed16 >> 17;
    game_seed16 ^= game_seed16 << 5;
    return game_seed16;
}
static void config16(const char *name, char *out, int cap) {
    char *s = service_read(name, cap - 1);
    if (s) {
        snprintf(out, cap, "%s", s);
        free(s);
    }
}
static void reader16(const char *title, const char *body, int origin) {
    document_open(title, "LUMEN", title, body, NULL, origin, 0);
}
static int file_open16(const char *path, int origin) {
    const char *ext = strrchr(path, '.');
    int supported = 0;
    const char *types[] = {".epub", ".pdf",  ".txt", ".html", ".htm",
                           ".cbz",  ".mobi", ".azw", ".azw3"};
    for (unsigned i = 0; ext && i < sizeof(types) / sizeof(*types); i++)
        if (!strcasecmp(ext, types[i]))
            supported = 1;
    if (!supported) {
        ui_message("Dieses Format kann der Reader nicht öffnen.");
        return 0;
    }
    ui_save();
    engine_close();
    current = selected = -1;
    Book *b = &library[BOOK_LIMIT];
    memset(b, 0, sizeof(*b));
    b->id = press_id(path);
    snprintf(b->path, sizeof(b->path), "%s", path);
    struct stat document_stat;
    if (!stat(path,&document_stat)) b->size=document_stat.st_size;
    const char *n = strrchr(path, '/');
    snprintf(b->title, sizeof(b->title), "%s", n ? n + 1 : path);
    library_restore_state(b);
    snprintf(b->format, sizeof(b->format), "%s", ext + 1);
    reader_ui.reader_document_active = 1;
    if (history_count < 64)
        history[history_count++] = origin;
    return ui_open(BOOK_LIMIT);
}
static void files_scan16(const char *path) {
    char canonical[4096];
    if (!realpath(path, canonical))
        return;
    snprintf(files_dir16, sizeof(files_dir16), "%s", canonical);
    if (ui_route() == 116)
        snprintf(explorer_dir16, sizeof(explorer_dir16), "%s", canonical);
    files_count16 = 0;
    DIR *d = opendir(canonical);
    struct dirent *e;
    while (d && (e = readdir(d)) && files_count16 < 512) {
        if (e->d_name[0] == '.')
            continue;
        char p[8192];
        snprintf(p, sizeof(p), "%s/%s", canonical, e->d_name);
        struct stat st;
        if (lstat(p, &st) || (!S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode)))
            continue;
        snprintf(file_names16[files_count16], 256, "%s", e->d_name);
        file_dirs16[files_count16++] = S_ISDIR(st.st_mode);
    }
    if (d)
        closedir(d);
    for (int i = 0; i < files_count16; i++)
        for (int j = i + 1; j < files_count16; j++)
            if (file_dirs16[j] > file_dirs16[i] ||
                (file_dirs16[j] == file_dirs16[i] &&
                 strcasecmp(file_names16[j], file_names16[i]) < 0)) {
                char tmp[256];
                memcpy(tmp, file_names16[i], 256);
                memcpy(file_names16[i], file_names16[j], 256);
                memcpy(file_names16[j], tmp, 256);
                int z = file_dirs16[i];
                file_dirs16[i] = file_dirs16[j];
                file_dirs16[j] = z;
            }
    companion_page16 = 0;
}
static void url_encode(const char *s, char *out, int cap) {
    int n = 0;
    for (; *s && n < cap - 4; s++) {
        unsigned char c = *s;
        if (isalnum(c) || strchr("-._~", c))
            out[n++] = c;
        else {
            snprintf(out + n, 4, "%%%02X", c);
            n += 3;
        }
    }
    out[n] = 0;
}
static int curl16(const char *url, const char *destination, const char *key, const char *post) {
    if (strncmp(url, "https://", 8) || strpbrk(url, "\r\n\"\\"))
        return 0;
    char cfg[4096], bodypath[4096], ca[4096];
    snprintf(cfg, sizeof(cfg), "%s/http17-%016llx.cfg", data_root,
             (unsigned long long)press_id(destination));
    snprintf(bodypath, sizeof(bodypath), "%s/http17-%016llx.body", data_root,
             (unsigned long long)press_id(destination));
    FILE *f = fopen(cfg, "w");
    if (!f)
        return 0;
    chmod(cfg, 0600);
    fprintf(f, "url = \"%s\"\noutput = \"%s\"\n", url, destination);
    if (key && *key) {
        if (strpbrk(key, "\r\n\"\\")) {
            fclose(f);
            unlink(cfg);
            return 0;
        }
        fprintf(f, "header = \"Authorization: Bearer %s\"\n", key);
    }
    if (post) {
        if (!atomic_write(bodypath, post, strlen(post))) {
            fclose(f);
            unlink(cfg);
            return 0;
        }
        fprintf(f, "header = \"Content-Type: application/json\"\ndata-binary = \"@%s\"\n",
                bodypath);
    }
    fclose(f);
    pid_t p = fork();
    if (!p) {
#ifndef LUMEN_HOST
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        if (getppid() == 1)
            _exit(1);
#endif
        snprintf(ca, sizeof(ca), "%s/../certs/ca-bundle.pem", data_root);
        if (!access(ca, R_OK))
            setenv("CURL_CA_BUNDLE", ca, 1);
        int nul = open("/dev/null", O_RDWR);
        dup2(nul, 1);
        dup2(nul, 2);
        close(nul);
        execlp("curl", "curl", "--user-agent", "LUMEN/1.7 (personal Kindle reader)", "--config",
               cfg, "--fail", "--silent", "--location", "--proto", "=https", "--proto-redir",
               "=https", "--max-redirs", "3", "--connect-timeout", "8", "--max-time",
               post ? "120" : "40", "--max-filesize", "4194304", NULL);
        _exit(127);
    }
    int st = 0;
    if (p > 0)
        while (waitpid(p, &st, 0) < 0 && errno == EINTR) {
        }
    unlink(cfg);
    unlink(bodypath);
    return p > 0 && WIFEXITED(st) && !WEXITSTATUS(st);
}
static void json_escape16(FILE *f, const char *p) {
    fputc('"', f);
    for (; *p; p++) {
        unsigned char c = *p;
        if (c == '"' || c == '\\') {
            fputc('\\', f);
            fputc(c, f);
        } else if (c < 32)
            fprintf(f, "\\u%04x", c);
        else
            fputc(c, f);
    }
    fputc('"', f);
}

static int job_start16(int kind, const char *arg) {
    if (job_pid16 > 0) {
        ui_message("Eine Anfrage läuft bereits.");
        return 0;
    }
    job_error16[0] = 0;
    job_kind16 = kind;
    job_pid16 = fork();
    if (job_pid16 < 0) {
        job_error16[0] = '!';
        return 0;
    }
    if (!job_pid16) {
#ifndef LUMEN_HOST
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        if (getppid() == 1)
            _exit(1);
#endif
        char out[4096];
        local_path(out, sizeof(out), "online16.json");
        int ok = 0;
        if (kind == 1) {
            recipes_search17(arg);
            ok = 1;
        }
        if (kind == 3) {
            char key[2049] = "";
            config16("llm-key.txt", key, sizeof(key));
            ok = *key && curl16("https://api.openai.com/v1/models", out, key, NULL);
            memset(key, 0, sizeof(key));
        }
        if (kind == 4)
            ok = recipes_fetch17(arg);
        if (kind == 2) {
            char key[2049] = "", model[100] = "gpt-4.1-mini";
            config16("llm-key.txt", key, sizeof(key));
            config16("llm-model.txt", model, sizeof(model));
            char *body = NULL;
            size_t z = 0;
            FILE *f = open_memstream(&body, &z);
            if (f) {
                chat_write_request(f, model, arg);
                fclose(f);
                if (*key)
                    ok = curl16("https://api.openai.com/v1/responses", out, key, body);
                free(body);
            }
            memset(key, 0, sizeof(key));
        }

        _exit(ok ? 0 : 1);
    }
    return 1;
}

static int u16_jobs_tick(void) {

    if (job_pid16 <= 0)
        return 0;
    int st = 0;
    pid_t p = waitpid(job_pid16, &st, WNOHANG);
    if (p <= 0)
        return 0;
    job_pid16 = -1;
    if (!WIFEXITED(st) || WEXITSTATUS(st)) {
        snprintf(job_error16, sizeof(job_error16), "%s",
                 job_kind16 == 2
                     ? "OpenAI-Anfrage fehlgeschlagen. Schlüssel, Modell und Verbindung prüfen."
                     : "Anfrage fehlgeschlagen. Verbindung und Einrichtung prüfen.");
        ui_message(job_error16);
        return 1;
    }
    if (job_kind16 == 1) {
        recipe_search16 = 1;
        recipes_parse17();
        companion_page16 = 0;
    }
    if (job_kind16 == 3) {
        char *s = service_read("online16.json", 4194304);
        if (s) {
            extra_write_text("llm-models17.json", s);
            free(s);
            models_parse17();
        }
    }
    if (job_kind16 == 4)
        recipes_open_ready17();
    if (job_kind16 == 2) {
        char *s = service_read("online16.json", 4194304);
        Json j;
        JsonToken *t = calloc(40000, sizeof(*t));
        if (s && t && json_parse(&j, s, t, 40000)) {
            char *text = NULL;
            size_t n = 0;
            FILE *f = open_memstream(&text, &n);
            int a = json_get(&j, 0, "output");
            for (int i = 0; f; i++) {
                int o = json_at(&j, a, i);
                if (o < 0)
                    break;
                int c = json_get(&j, o, "content");
                for (int k = 0;; k++) {
                    int part = json_at(&j, c, k);
                    if (part < 0)
                        break;
                    char type[40];
                    json_string(&j, json_get(&j, part, "type"), type, 40);
                    if (!strcmp(type, "output_text")) {
                        char *b = calloc(1, 131073);
                        if (b) {
                            json_string(&j, json_get(&j, part, "text"), b, 131073);
                            fprintf(f, "%s\n", b);
                            free(b);
                        }
                    }
                }
            }
            if (f) {
                fclose(f);
                if (n) {
                    chat_complete(text);
                }
                free(text);
            }
        }
        free(t);
        free(s);
    }

    return 1;
}
#include "../calendar/events.h"
#include "../calendar/classic_view.h"

#include "../planetarium/solar_view.h"

static void game_new16(int route) {
    memset(game_board16, 0, sizeof(game_board16));
    game_turn16 = 1;
    game_won16 = game_moves16 = 0;
    game_undo_valid16 = 0;
    if (route == 118) { /* A solvable warehouse with room for deliberate planning. */
        static const char *maps[][7] = {
            {"########", "#  .   #", "#  $   #", "# . $  #", "#  @   #", "#      #", "########"},
            {"########", "# . .  #", "# $ $  #", "#  @   #", "#      #", "#      #", "########"},
            {"########", "#   .  #", "#   $  #", "# . $  #", "#  @   #", "#      #", "########"},
            {"########", "# .    #", "# $    #", "#   .  #", "#   $  #", "# @    #", "########"}};
        const char **map = maps[random16() % 4];
        game_px16 = 3;
        game_py16 = 4;
        for (int y = 0; y < 7; y++)
            for (int x = 0; x < 8; x++) {
                char c = map[y][x];
                if (c == '@') {
                    game_px16 = x;
                    game_py16 = y;
                }
                game_board16[y * 8 + x] = c == '#' ? 1 : c == '.' ? 2 : c == '$' ? 3 : 0;
            }
    }
    if (route == 119) { /* Randomized DFS perfect maze: always one solvable path. */
        for (int i = 0; i < 143; i++)
            game_board16[i] = 1;
        int stack[40], top = 0;
        stack[top++] = 12;
        game_board16[12] = 0;
        while (top) {
            int pos = stack[top - 1], x = pos % 11, y = pos / 11, neighbors[4], n = 0;
            const int dx[] = {0, 2, 0, -2}, dy[] = {-2, 0, 2, 0};
            for (int d = 0; d < 4; d++) {
                int xx = x + dx[d], yy = y + dy[d];
                if (xx > 0 && xx < 10 && yy > 0 && yy < 12 && game_board16[yy * 11 + xx])
                    neighbors[n++] = d;
            }
            if (!n) {
                top--;
                continue;
            }
            int d = neighbors[random16() % n], q = (y + dy[d]) * 11 + x + dx[d];
            game_board16[(pos + q) / 2] = game_board16[q] = 0;
            stack[top++] = q;
        }
        game_px16 = game_py16 = 1;
    }
    if (route == 121) {
        game_px16 = 4;
        game_py16 = 5;
        game_hp16 = 12;
        game_gold16 = 0;
        game_level16 = 1;
        for (int i = 0; i < 108; i++)
            game_board16[i] = (i % 9 == 0 || i % 9 == 8 || i < 9 || i >= 99) ? 1 : 0;
        for (int i = 0; i < 8; i++) {
            int at = (1 + random16() % 10) * 9 + 1 + random16() % 7;
            game_board16[at] = i < 4 ? 3 : 2;
        }
        game_board16[14] = 4;
        game_board16[5 * 9 + 4] = 0;
    }
}
static void move_game16(int dx, int dy) {
    int cols = ui_route() == 118   ? 8
               : ui_route() == 119 ? 11
                               : 9,
        rows = ui_route() == 118   ? 7
               : ui_route() == 119 ? 13
                               : 12;
    int nx = game_px16 + dx, ny = game_py16 + dy;
    if (nx < 0 || nx >= cols || ny < 0 || ny >= rows || game_won16)
        return;
    int at = ny * cols + nx, v = game_board16[at];
    if (v == 1)
        return;
    if (ui_route() == 118 && (v == 3 || v == 5)) {
        int bx = nx + dx, by = ny + dy;
        if (bx < 0 || bx >= 8 || by < 0 || by >= 7)
            return;
        int to = by * 8 + bx;
        if (game_board16[to] != 0 && game_board16[to] != 2)
            return;
    }
    memcpy(game_previous16, game_board16, sizeof(game_board16));
    game_undo_valid16 = 1;
    game_undo16[0] = game_px16;
    game_undo16[1] = game_py16;
    game_undo16[2] = game_moves16;
    game_undo16[3] = game_hp16;
    game_undo16[4] = game_gold16;
    game_undo16[5] = game_level16;
    if (ui_route() == 118 && (v == 3 || v == 5)) {
        int bx = nx + dx, by = ny + dy;
        if (bx < 0 || bx >= 8 || by < 0 || by >= 7)
            return;
        int to = by * 8 + bx;
        if (game_board16[to] != 0 && game_board16[to] != 2)
            return;
        game_board16[to] = game_board16[to] == 2 ? 5 : 3;
        game_board16[at] = v == 5 ? 2 : 0;
    }
    if (ui_route() == 121) {
        if (v == 3) {
            game_hp16 -= 2;
            game_board16[at] = 0;
            if (game_hp16 <= 0) {
                game_won16 = -1;
                return;
            }
        }
        if (v == 2) {
            game_gold16++;
            game_board16[at] = 0;
        }
        if (v == 4) {
            game_level16++;
            game_hp16 += 3;
            for (int i = 0; i < 6; i++) {
                int z = (1 + random16() % 10) * 9 + 1 + random16() % 7;
                if (z != at)
                    game_board16[z] = i < 3 ? 3 : 2;
            }
        }
    }
    game_undo_valid16 = 1;
    game_px16 = nx;
    game_py16 = ny;
    game_moves16++;
    if (ui_route() == 119 && nx == 9 && ny == 11)
        game_won16 = 1;
    if (ui_route() == 118) {
        int left = 0;
        for (int i = 0; i < 56; i++)
            if (game_board16[i] == 3)
                left++;
        if (!left)
            game_won16 = 1;
    }
}
static void game_help16(int route) {
    const char *title = app_definition(app_for_route(route))->name;
    const char *text = "";
    switch (route) {
    case 118:
        text =
            "Sokoban\n\nSchiebe alle Kisten auf die Zielkreise. Du kannst Kisten schieben, aber "
            "nicht ziehen und nie zwei gleichzeitig schieben. Tippe neben deine Figur oder nutze "
            "die Richtungstasten. Kisten in Ecken können feststecken: Starte dann das Rätsel neu.";
        break;
    case 119:
        text = "Labyrinth\n\nFinde den Weg vom Start oben links zum Ziel unten rechts. Jedes neue "
               "Labyrinth wird lokal erzeugt und besitzt einen Lösungsweg. Tippe eine Richtung "
               "neben deiner Figur oder nutze die Pfeiltasten. Es gibt kein Zeitlimit.";
        break;
    case 121:
        text = "Paper Quest\n\nEin kleines rundenbasiertes Abenteuer. Bewege dich mit den "
               "Pfeiltasten. Sammle Münzen (kleine Kreise), besiege Monster (Kreuze, kostet zwei "
               "Lebenspunkte) und erreiche die Treppe (Stufen), um tiefer zu steigen. Auf jeder "
               "Ebene erhältst du drei Lebenspunkte und neue Gegner. Bei null Lebenspunkten endet "
               "die Reise.";
        break;
    case 23:
        text = "Minesweeper\n\nDecke sichere Felder auf. Jede Zahl zählt Minen in den acht "
               "benachbarten Feldern. Halte ein Feld gedrückt oder aktiviere die Flagge, um eine "
               "vermutete Mine zu markieren. Eine aufgedeckte Mine beendet das Spiel. Alle "
               "sicheren Felder aufzudecken gewinnt.";
        break;
    case 24:
        text = "Wortraten\n\nErrate das versteckte Wort Buchstabe für Buchstabe. Ein falscher "
               "Buchstabe ergänzt die Galgenzeichnung. Finde das Wort, bevor die Zeichnung "
               "vollständig ist. Bereits benutzte Buchstaben helfen dir bei der Auswahl.";
        break;
    case 25:
        text =
            "Tetris\n\nDrehe und verschiebe die fallenden Steine. Vollständige waagerechte Reihen "
            "verschwinden. Räume möglichst viele Reihen ab, bevor das Spielfeld oben voll wird.";
        break;
    case 26:
        text = "Snake\n\nLenke die Schlange zum Futter. Sie wächst nach jeder Mahlzeit. Vermeide "
               "Wände und den eigenen Körper. Eine direkte Kehrtwende ist nicht möglich.";
        break;
    case 48:
        text = "Schach\n\nSetze den gegnerischen König schachmatt. Tippe eine eigene Figur und "
               "dann ein erlaubtes Zielfeld an. Im Einzelspiel antwortet der Computer, im "
               "Mehrspielermodus spielen zwei Personen abwechselnd. Die Drehfunktion richtet die "
               "Figuren zum gegenüber sitzenden Spieler aus.";
        break;
    case 120:
        text = "Kreuzworträtsel\n\nTippe einen Hinweis an, lies die Frage und trage die Lösung "
               "ein. Groß- und Kleinschreibung sind egal. Sich kreuzende Wörter teilen denselben "
               "Buchstaben. Alle Wörter richtig zu lösen beendet das Rätsel.";
        break;
    }
    if (*text)
        u17_game_manual(route, title, text);
}
static int game_help_x(void) {
    return (ui_route() == 119 || ui_route() == 118 || ui_route() == 120 || ui_route() == 121)
               ? 408
               : 468;
}
static void help_overlay16(void) {
    AppId id = app_for_route(ui_route());
    if ((id >= APP_MINES && id <= APP_SNAKE) || id == APP_CHESS ||
        (id >= APP_KNIGHTS && id <= APP_RPG)) {
        int xx = game_help_x();
        region(xx, 74, 44, 44);
        ring(xx + 22, 96, 15, 2);
        centered(xx, 74, 44, 44, "?", ui_font(UI_BODY));
    }
}

static void direction_keys16(void) {
    icon_button(160, 728, IC_PREV);
    icon_button(239, 728, IC_UP);
    icon_button(318, 728, IC_DOWN);
    icon_button(397, 728, IC_NEXT);
}
static void game_draw16(void) {
    heading(app_definition(app_for_route(ui_route()))->name);
    icon_button(528, 74, IC_RESTART);
    char b[100];
    if (ui_route() == 118 || ui_route() == 119 || ui_route() == 121)
        icon_button(468, 74, IC_UNDO);
    int cols = ui_route() == 118   ? 8
               : ui_route() == 119 ? 11
                               : 9,
        rows = ui_route() == 118   ? 7
               : ui_route() == 119 ? 13
                               : 12;
    int cell = ui_route() == 118   ? 68
               : ui_route() == 119 ? 40
                               : 44,
        ox = 300 - cols * cell / 2, oy = ui_route() == 118 ? 205 : 150;
    for (int y = 0; y < rows; y++)
        for (int x = 0; x < cols; x++) {
            int xx = ox + x * cell, yy = oy + y * cell, v = game_board16[y * cols + x];
            region(xx, yy, cell, cell);
            if (v == 1)
                rect(xx + 1, yy + 1, cell - 2, cell - 2, 65);
            else if (ui_route() == 118) {
                if (v == 2 || v == 5)
                    ring(xx + cell / 2, yy + cell / 2, 10, 2);
                if (v == 3 || v == 5) {
                    key_outline(xx + 9, yy + 9, cell - 18, cell - 18);
                    stroke(xx + 9, yy + 9, xx + cell - 9, yy + cell - 9, 2);
                }
            } else if (ui_route() == 121) {
                if (v == 2)
                    ring(xx + cell / 2, yy + cell / 2, 6, 3);
                if (v == 3) {
                    stroke(xx + 12, yy + 12, xx + cell - 12, yy + cell - 12, 3);
                    stroke(xx + cell - 12, yy + 12, xx + 12, yy + cell - 12, 3);
                }
                if (v == 4)
                    for (int i = 0; i < 3; i++)
                        rect(xx + 9 + i * 7, yy + cell - 10 - i * 7, cell - 18 - i * 7, 2, 0);
            }
        }
    aa_disc(ox + game_px16 * cell + cell / 2, oy + game_py16 * cell + cell / 2, cell / 4, 0);
    if (ui_route() == 119)
        ring(ox + 9 * cell + cell / 2, oy + 11 * cell + cell / 2, cell / 3, 2);
    snprintf(b, 100,
             tr(ui_route() == 121 ? "Ebene %d · Leben %d · Gold %d"
                : game_won16  ? "Geschafft · %d Züge"
                              : "%d Züge"),
             ui_route() == 121 ? game_level16 : game_moves16, game_hp16, game_gold16);
    centered(28, 680, 544, 42, b, ui_font(UI_BODY));
    direction_keys16();
}
static int u16_apps_commit(int target, const char *text) {
    if (target == 171) {
        snprintf(journal_answers16[journal_step16], 8192, "%s", text);
        char n[64];
        snprintf(n, 64, "journal-draft16-%d.txt", journal_step16);
        extra_write_text(n, text);
        return 1;
    }
    if (target == 172) {
        job_start16(1, text);
        return 1;
    }
    if (target == 174) {
        snprintf(llm_prompt16, sizeof(llm_prompt16), "%s", text);
        extra_write_text("llm-prompt16.txt", text);
        return 1;
    }
    if (target == 175) {
        if (*text && !strpbrk(text, "\r\n\"\\ ")) {
            extra_write_text("llm-model.txt", text);
            snprintf(llm_model16, sizeof(llm_model16), "%s", text);
        }
        return 1;
    }
    if (target == 176) {
        if (*text && !strpbrk(text, "\r\n\"\\ "))
            extra_write_text("llm-key.txt", text);
        return 1;
    }
    if (target == 177) {
        u16_notes_defaults();
        int folder = 0;
        for (int i = 0; i < note_tree.nf; i++)
            if (!strcmp(note_tree.folder[i].name, "Learning"))
                folder = note_tree.folder[i].id;
        char path[4096], b[16000];
        int id = (int)time(NULL);
        do {
            snprintf(path, sizeof(path), "%s/notes/%d.txt", data_root, id++);
        } while (!access(path, F_OK));
        snprintf(b, sizeof(b), tr("# Abrufübung: %s\n\n%s"),
                 *study_selected16 ? study_selected16 : "Lernen", text);
        if (atomic_write(path, b, strlen(b))) {
            v1_place(id - 1, folder);
            v1_save();
            notes_scan();
            ui_message("In Notizen → Learning gespeichert. Jetzt mit dem Material vergleichen.");
        }
        return 1;
    }

    return 0;
}
static void companion_enter16(void) {
    if (companion_route16 == ui_route())
        return;
    companion_route16 = ui_route();
    companion_page16 = 0;
    if (ui_route() == 116) {
        char p[4096];
        snprintf(p, sizeof(p), "%s", *explorer_dir16 ? explorer_dir16 : library_root);
        files_scan16(p);
        if (!*files_dir16)
            files_scan16(data_root);
        companion_page16 = explorer_page16;
        if (companion_page16 * 8 >= files_count16)
            companion_page16 = 0;
    }
    if (ui_route() == 111) {
        char p[4096];
        local_path(p, sizeof(p), "study");
        mkdir(p, 0700);
        files_scan16(p);
    }
    if (ui_route() == 114) {
        recipe_search16 = 0;
        recipe_archive16();
        recipes_parse17();
    }
    if (ui_route() == 110) {
        for (int i = 0; i < 6; i++) {
            char n[80];
            snprintf(n, 80, "journal-draft16-%d.txt", i);
            config16(n, journal_answers16[i], 8192);
        }
    }
    if (ui_route() == 122) {
        config16("llm-model.txt", llm_model16, sizeof(llm_model16));
        config16("llm-prompt16.txt", llm_prompt16, sizeof(llm_prompt16));
    }
    if (ui_route() == 118 || ui_route() == 119 ||
        ui_route() == 121) {
        static int active_game = -1;
        if (active_game != ui_route()) {
            game_new16(ui_route());
            char n[64];
            snprintf(n, 64, "game16-%d.bin", ui_route());
            int state[152];
            if (app_state_load(n, state, sizeof(state))) {
                int valid = state[144] >= 1 && state[144] <= 2 && state[146] >= 0 &&
                            state[146] <= 100000 && state[147] >= 0 && state[147] < 11 &&
                            state[148] >= 0 && state[148] < 13;
                for (int i = 0; i < 144; i++)
                    if (state[i] < -75 || state[i] > 75)
                        valid = 0;
                if (valid) {
                    memcpy(game_board16, state, sizeof(game_board16));
                    game_turn16 = state[144];
                    game_won16 = state[145];
                    game_moves16 = state[146];
                    game_px16 = state[147];
                    game_py16 = state[148];
                    game_hp16 = state[149];
                    game_gold16 = state[150];
                    game_level16 = state[151];
                }
            }
            active_game = ui_route();
        }
    }
}
static int u16_apps_draw(void) {
    if (ui_route() == 21) {
        companion_route16 = 21;
        return calendar_draw16();
    }
    if (ui_route() < 110 || ui_route() > 127) {
        companion_route16 = -1;
        return 0;
    }
    companion_enter16();

    if (ui_route() == 111) {
        heading_actions("Lernen", 1);
        icon_button(528, 74, IC_CLOCK);
        wrapped(30, 140,
                "Lesen → Material schließen → aus dem Gedächtnis erklären → vergleichen. Lade dein "
                "Material über Connect hoch.",
                ui_font(UI_BODY), 540, 3);
        button_h(28, 255, 260, 48, "25 min Fokus", 0);
        button_h(312, 255, 260, 48, "Abrufübung", 0);
        for (int r = 0; r < 5 && companion_page16 * 5 + r < files_count16; r++) {
            int i = companion_page16 * 5 + r, y = 332 + r * 67;
            region(28, y, 544, 62);
            icon(30, y + 8, IC_BOOK);
            drawtext_raw(90, y + 14, file_names16[i], ui_font(UI_BODY), 470);
            divider(28, y + 61, 544);
        }
        if (!files_count16)
            wrapped(30, 375, "Noch kein Material. Connect → Lernen → Dateien hochladen.", ui_font(UI_PRIMARY),
                    540, 4);
        pager(745, &companion_page16, (files_count16 + 4) / 5);
        return 1;
    }
    if (ui_route() == 118 || ui_route() == 119 ||
        ui_route() == 121) {
        game_draw16();
        return 1;
    }

    if (ui_route() == 116) {
        heading_actions("Dateien", 1);
        icon_button(528, 74, IC_UP);
        ink_color = 120;
        drawtext_raw(30, 136, files_dir16, ui_font(UI_SECONDARY), 540);
        ink_color = 0;
        for (int r = 0; r < 8 && companion_page16 * 8 + r < files_count16; r++) {
            int i = companion_page16 * 8 + r, y = 182 + r * 62;
            region(28, y, 544, 58);
            icon(30, y + 7, file_dirs16[i] ? IC_FOLDER : IC_NOTE);
            drawtext_raw(90, y + 13, file_names16[i], ui_font(UI_BODY), 474);
            divider(28, y + 57, 544);
        }
        pager(715, &companion_page16, (files_count16 + 7) / 8);
        return 1;
    }
    if (ui_route() == 125) {
        solar16();
        return 1;
    }
    return 0;
}
#include "../calendar/navigation.h"

static int u16_apps_input(const Input *in) {
    if (ui_route() == 110 || ui_route() == 126 || ui_route() == 122 || ui_route() == 123 || ui_route() == 114 ||
        ui_route() == 118 || ui_route() == 119 || ui_route() == 120)
        return -1;
    if (in->kind != INPUT_TAP)
        return -1;
    int x = in->x, y = in->y;
    int helpx = game_help_x();
    if (y >= 74 && y < 118 && x >= helpx && x < helpx + 44) {
        AppId id = app_for_route(ui_route());
        if ((id >= APP_MINES && id <= APP_SNAKE) || id == APP_CHESS ||
            (id >= APP_KNIGHTS && id <= APP_RPG)) {
            game_help16(ui_route());
            return 1;
        }
    }
    if (ui_route() == 21) {
        if (y >= 74 && y < 118) {

            if (x >= 28 && x < 438) {
                calendar_view16=(x-28)*4/410;
                calendar_day_page16=0;
                return 1;
            }
        }
        if (calendar_view16 != 2) {
            if (calendar_view16 == 1 && y >= 200 && y < 690) {
                time_t start = calendar_time16(calendar_year, calendar_month, calendar_day);
                struct tm t;
                localtime_r(&start, &t);
                t.tm_mday -= (t.tm_wday + 6) % 7;
                t.tm_mday += (y - 200) / 70;
                mktime(&t);
                calendar_year = t.tm_year + 1900;
                calendar_month = t.tm_mon + 1;
                calendar_day = t.tm_mday;
                calendar_view16 = 0;
                calendar_day_page16 = 0;
                app_calendar_load();
                return 1;
            }
            if (y >= 135 && y < 181 && (x < 80 || x >= 520)) {
                calendar_shift16(x < 300 ? -1 : 1);
                return 1;
            }
            if (calendar_view16 == 3 && y >= 205 && y < 697) {
                int m = (y - 205) / 123 * 3 + (x - 28) / 185 + 1;
                if (m >= 1 && m <= 12) {
                    calendar_month = m;
                    calendar_day = 1;
                    calendar_view16 = 2;
                    app_calendar_load();
                }
                return 1;
            }
            if (y >= 120) {
                if (calendar_view16 == 0 && y >= 745 && y < 789)
                    return -1;
                return 1;
            }
        }
    }
    if (ui_route() == 79 && y >= 74 && y < 118 && x >= 460 && x < 516) {
        extra_open(125);
        return 1;
    }
    if (ui_route() < 110 || ui_route() > 127)
        return -1;
    if (y < 48)
        return -1;

    if (ui_route() == 111) {
        if (y >= 74 && y < 118 && x >= 516) {
            extra_open(28);
            return 1;
        }
        if (y >= 255 && y < 303) {
            if (x < 300) {
                timer_minutes = study_minutes16;
                timer_total_seconds = study_minutes16 * 60;
                timer_due = extra_millis() + timer_total_seconds * 1000LL;
                timer_paused = 0;
                extra_save();
                ui_message("Fokuszeit gestartet.");
            } else {
                open_keyboard(177, "Was weißt du noch?", "");
                editor_ui.note_cursor = editor_ui.note_view_start = 0;
            }
            return 1;
        }
        if (y >= 332 && y < 667) {
            int i = companion_page16 * 5 + (y - 332) / 67;
            if (i < files_count16) {
                char p[8192];
                snprintf(p, sizeof(p), "%s/%s", files_dir16, file_names16[i]);
                if (file_dirs16[i])
                    files_scan16(p);
                else {
                    snprintf(study_selected16, sizeof(study_selected16), "%s", file_names16[i]);
                    file_open16(p, 111);
                }
            }
            return 1;
        }
        return -1;
    }

    if (ui_route() == 116) {
        if (y >= 74 && y < 118 && x >= 516) {
            char p[4096];
            snprintf(p, sizeof(p), "%s", files_dir16);
            char *s = strrchr(p, '/');
            if (s && s != p)
                *s = 0;
            else
                snprintf(p, sizeof(p), "/");
            files_scan16(p);
            return 1;
        }
        if (y >= 182 && y < 678) {
            int i = companion_page16 * 8 + (y - 182) / 62;
            if (i < files_count16) {
                char p[8192];
                snprintf(p, sizeof(p), "%s/%s", files_dir16, file_names16[i]);
                if (file_dirs16[i])
                    files_scan16(p);
                else
                    file_open16(p, 116);
            }
            return 1;
        }
        return -1;
    }

    if (ui_route() == 125) {
        if (y >= 74 && y < 118 && x >= 516) {
            extra_open(80);
            return 1;
        }
        if (y >= 714 && y < 762) {
            double d = x < 160 ? -1 : x < 297 ? 1 : x < 434 ? -365.25 : 365.25;
            astro_time_t t = Astronomy_AddDays(sky_time(), d);
            astro_utc_t u = Astronomy_UtcFromTime(t);
            sky_config.year = u.year;
            sky_config.month = u.month;
            sky_config.day = u.day;
            sky_config.hour = u.hour;
            sky_config.minute = u.minute;
            sky_key[0] = 0;
            return 1;
        }
        return 1;
    }
    if (ui_route() == 118 || ui_route() == 119 ||
        ui_route() == 121) {
        if (y >= 74 && y < 118 && x >= 460 && x < 516 &&
            (ui_route() == 118 || ui_route() == 119 || ui_route() == 121)) {
            if (game_undo_valid16) {
                memcpy(game_board16, game_previous16, sizeof(game_board16));
                game_px16 = game_undo16[0];
                game_py16 = game_undo16[1];
                game_moves16 = game_undo16[2];
                game_hp16 = game_undo16[3];
                game_gold16 = game_undo16[4];
                game_level16 = game_undo16[5];
                game_won16 = 0;
                game_undo_valid16 = 0;
            }
            return 1;
        }
        if (y >= 74 && y < 118 && x >= 516) {
            game_new16(ui_route());
            return 1;
        }
        if (ui_route() == 118 || ui_route() == 119 || ui_route() == 121) {
            int dx = 0, dy = 0;
            if (y >= 728 && y < 772) {
                if (x >= 150 && x < 210)
                    dx = -1;
                else if (x >= 229 && x < 289)
                    dy = -1;
                else if (x >= 308 && x < 368)
                    dy = 1;
                else if (x >= 387 && x < 447)
                    dx = 1;
            } else {
                int cell = ui_route() == 118   ? 68
                           : ui_route() == 119 ? 40
                                           : 44,
                    cols = ui_route() == 118   ? 8
                           : ui_route() == 119 ? 11
                                           : 9,
                    ox = 300 - cols * cell / 2, oy = ui_route() == 118 ? 205 : 150;
                int xx = (x - ox) / cell - game_px16, yy = (y - oy) / cell - game_py16;
                if (abs(xx) > abs(yy))
                    dx = xx > 0 ? 1 : -1;
                else if (yy)
                    dy = yy > 0 ? 1 : -1;
            }
            if (dx || dy)
                move_game16(dx, dy);
            return 1;
        }
        return 1;
    }
    return -1;
}

static void u16_calendar_choice(int choice) {
    if (choice >= 0 && choice < 4)
        calendar_view16 = choice;
}

static int ink_pending16(void) {
    return ink_dirty;
}
static int job_pending16(void) {
    return job_pid16 > 0;
}

static void recipe_archive16(void) {
    recipe_recent_count16 = 0;
    char dir[4096];
    local_path(dir, sizeof(dir), "recipe-archive16");
    mkdir(dir, 0700);
    DIR *d = opendir(dir);
    struct dirent *e;
    while (d && (e = readdir(d)) && recipe_recent_count16 < 128) {
        char *dot = strrchr(e->d_name, '.');
        if (!dot || strcmp(dot, ".txt") || dot - e->d_name >= 39)
            continue;
        char path[8192];
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        FILE *f = fopen(path, "r");
        if (!f)
            continue;
        RecipeRecent16 *r = &recipe_recent16[recipe_recent_count16++];
        fgets(r->title, sizeof(r->title), f);
        fclose(f);
        r->title[strcspn(r->title, "\r\n")] = 0;
        snprintf(r->id, sizeof(r->id), "%.*s", (int)(dot - e->d_name), e->d_name);
        struct stat st;
        if (!stat(path, &st))
            r->opened = st.st_mtime;
    }
    if (d)
        closedir(d);
    for (int i = 0; i < recipe_recent_count16; i++)
        for (int j = i + 1; j < recipe_recent_count16; j++)
            if (recipe_recent16[j].opened > recipe_recent16[i].opened) {
                RecipeRecent16 r = recipe_recent16[i];
                recipe_recent16[i] = recipe_recent16[j];
                recipe_recent16[j] = r;
            }
}

static void companion_checkpoint16(void) {
    if (companion_route16 == 116)
        explorer_page16 = companion_page16;
    if (companion_route16 == 118 || companion_route16 == 119 || companion_route16 == 121) {
        char name[64];
        snprintf(name, 64, "game16-%d.bin", companion_route16);
        int state[152];
        memcpy(state, game_board16, sizeof(game_board16));
        state[144] = game_turn16;
        state[145] = game_won16;
        state[146] = game_moves16;
        state[147] = game_px16;
        state[148] = game_py16;
        state[149] = game_hp16;
        state[150] = game_gold16;
        state[151] = game_level16;
        app_state_save(name, state, sizeof(state));
    }
}
static void companion_close16(void) {
    companion_checkpoint16();
    if (job_pid16 > 0) {
        kill(job_pid16, SIGTERM);
        while (waitpid(job_pid16, NULL, 0) < 0 && errno == EINTR) {
        }
        job_pid16 = -1;
    }
    const char *files[] = {"private16.curl", "private16.body", NULL};
    for (int i = 0; files[i]; i++) {
        char p[4096];
        local_path(p, sizeof(p), files[i]);
        unlink(p);
    }
}
