/* App-owned module; assembled by apps/host/compose.h. */
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
#include "../../system/ui/app_icons.h"
