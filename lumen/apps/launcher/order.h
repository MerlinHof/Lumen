/* App-owned module; assembled by apps/host/compose.h. */
/* 1.1 app polish: shared chrome, permanent reading caches, deterministic flows. */
#include "../news/api.h"
static int power_menu, power_hold, power_request;
static int64_t power_menu_wake_until;
static int tree_id[1536], tree_type[1536], tree_count, tree_page;

static const AppId launcher_first[] = {
    APP_LIBRARY, APP_CALCULATOR, APP_NOTES, APP_CALENDAR,
    APP_TIMER, APP_TASKS, APP_SETTINGS, APP_FILE_TRANSFER
};
static const AppId launcher_games[] = {
    APP_MINES, APP_WORDS, APP_BLOCKS, APP_SNAKE, APP_CHESS, APP_TIC_TAC_TOE,
    APP_2048, APP_SOLITAIRE, APP_SUDOKU, APP_WORDLE, APP_NONOGRAMS, APP_HEX,
    APP_BINGO, APP_CONNECT4, APP_KNIGHTS, APP_SOKOBAN, APP_MAZE, APP_CROSSWORD,
    APP_SIGNAL, APP_FROG
};

static void launcher_defaults(void) {
    int n = 0, reserved[APP_LIMIT] = {0};
    for (size_t i = 0; i < sizeof(launcher_first)/sizeof(*launcher_first); i++) {
        AppId id = launcher_first[i];
        launcher_order[n++] = id;
        reserved[id] = 1;
    }
    for (size_t i = 0; i < sizeof(launcher_games)/sizeof(*launcher_games); i++)
        reserved[launcher_games[i]] = 1;
    for (int id = 0; id < APP_LIMIT; id++)
        if (!reserved[id])
            launcher_order[n++] = id;
    for (size_t i = 0; i < sizeof(launcher_games)/sizeof(*launcher_games); i++)
        launcher_order[n++] = launcher_games[i];
}
