#include "../system/app_runtime.h"
/* Keep system navigation for glanceable dashboards and utilities.
 * Reading, editing, drawing, calculation and games retain their working space.
 * Application policy belongs to the apps, never to the status/tab bar. */
static const AppDefinition definitions[APP_COUNT] = {
    {APP_LIBRARY, "Bibliothek", 1, 0, 0},
    {APP_READER, "Lesen", 2, 1, 0},
    {APP_SETTINGS, "Einstellungen", 13, 0, 1},
    {APP_STORE, "Store", 10, 0, 2},
    {APP_LAUNCHER, "Apps", 20, 0, 3},
    {APP_CALENDAR, "Kalender", 21, 0, 3},
    {APP_INSIGHTS, "Lesestatistik", 22, 0, 3},
    {APP_MINES, "Minesweeper", 23, 1, 3},
    {APP_WORDS, "Wortraten", 24, 1, 3},
    {APP_BLOCKS, "Tetris", 25, 1, 3},
    {APP_SNAKE, "Snake", 26, 1, 3},
    {APP_CALCULATOR, "Rechner", 27, 1, 3},
    {APP_TIMER, "Timer & Wecker", 28, 0, 3},
    {APP_NOTES, "Notizen", 29, 1, 3},
    {APP_DRAWING, "Zeichnen", 31, 1, 3},
    {APP_WEATHER, "Wetter", 32, 0, 3},
    {APP_HOME_ASSISTANT, "Home Assistant", 33, 0, 3},
    {APP_WIKIPEDIA, "Wikipedia", 36, 1, 3},
    {APP_QUOTES, "Zitate", 38, 0, 3},
    {APP_CHESS, "Schach", 48, 1, 3},
    {APP_TIC_TAC_TOE, "Tic Tac Toe", 49, 1, 3},
    {APP_NUMSCRATCH, "NumScratch", 52, 1, 3},
    {APP_2048, "2048", 63, 1, 3},
    {APP_SOLITAIRE, "Solitaire", 64, 1, 3},
    {APP_SUDOKU, "Sudoku", 65, 1, 3},
    {APP_FOCUS, "BrainTrain", 66, 1, 3},
    {APP_NEWS, "News", 86, 1, 3},
    {APP_WORDLE, "Wordle", 70, 1, 3},
    {APP_PHOTOS, "Fotos", 71, 1, 3},
    {APP_LANGUAGE, "Sprachen", 73, 0, 3},
    {APP_TASKS, "ToDo", 74, 0, 3},
    {APP_FLASHCARDS, "Karteikarten", 76, 1, 3},
    {APP_PLANETARIUM, "Planetarium", 79, 1, 3},
    {APP_NONOGRAMS, "Nonogramme", 81, 1, 3},
    {APP_HEX, "Hex", 82, 1, 3},
    {APP_STATUS_BAR, "Statusleiste", 90, 0, 3},
    {APP_TAB_BAR, "Tableiste", 91, 0, 3},
    {APP_LOCK_SCREEN, "Sperrbild", 92, 1, 3},
    {APP_KEYBOARD, "Tastatur", 93, 1, 3},
    {APP_JOURNAL, "Journal", 110, 0, 3},
    {APP_STUDY, "Lernen", 111, 0, 3},
    {APP_BINGO, "Bingo", 112, 1, 3},
    {APP_CONNECT4, "Vier gewinnt", 113, 1, 3},
    {APP_RECIPES, "Rezepte", 114, 1, 3},
    {APP_PERIODIC, "Periodensystem", 115, 1, 3},
    {APP_FILES, "Dateien", 116, 1, 3},
    {APP_KNIGHTS, "Springertour", 117, 1, 3},
    {APP_SOKOBAN, "Sokoban", 118, 1, 3},
    {APP_MAZE, "Labyrinth", 119, 1, 3},
    {APP_CROSSWORD, "Kreuzworträtsel", 120, 1, 3},
    {APP_RPG, "Paper Quest", 121, 1, 3},
    {APP_LLM, "OpenAI", 122, 1, 3}};
static const AppRoute routes[] = {
    {0, APP_LIBRARY, 0},      {1, APP_LIBRARY, 0},         {2, APP_READER, 0},
    {3, APP_SETTINGS, 0},     {4, APP_READER, 0},          {5, APP_READER, 0},
    {6, APP_LIBRARY, 0},      {7, APP_LIBRARY, 0},         {8, APP_SETTINGS, 0},
    {9, APP_SETTINGS, 0},     {10, APP_STORE, 0},          {11, APP_STORE, 0},
    {12, APP_SETTINGS, 0},    {13, APP_SETTINGS, 0},       {14, APP_READER, 0},
    {15, APP_WIKIPEDIA, 0},   {20, APP_LAUNCHER, 0},       {21, APP_CALENDAR, 0},
    {22, APP_INSIGHTS, 0},    {23, APP_MINES, 0},          {24, APP_WORDS, 0},
    {25, APP_BLOCKS, 0},      {26, APP_SNAKE, 0},          {27, APP_CALCULATOR, 0},
    {28, APP_TIMER, 0},       {29, APP_NOTES, 0},          {30, APP_NOTES, 0},
    {32, APP_WEATHER, 0},     {33, APP_HOME_ASSISTANT, 0}, {34, APP_HOME_ASSISTANT, 0},
    {36, APP_WIKIPEDIA, 0},   {38, APP_QUOTES, 0},         {39, APP_SETTINGS, 0},
    {40, APP_SETTINGS, 0},    {41, APP_SETTINGS, 0},       {42, APP_CALCULATOR, 0},
    {43, APP_NOTES, 0},       {45, APP_CALENDAR, 0},       {46, APP_TIMER, 0},
    {47, APP_SETTINGS, 0},    {48, APP_CHESS, 0},          {49, APP_TIC_TAC_TOE, 0},
    {50, APP_CHESS, 0},       {51, APP_WIKIPEDIA, 0},      {60, APP_HOME_ASSISTANT, 1},
    {61, APP_CALCULATOR, 0},  {62, APP_CALCULATOR, 0},     {63, APP_2048, 0},
    {64, APP_SOLITAIRE, 0},   {65, APP_SUDOKU, 0},         {66, APP_FOCUS, 0},
    {67, APP_NEWS, 0},        {68, APP_NEWS, 0},           {69, APP_NEWS, 0},
    {70, APP_WORDLE, 0},      {71, APP_PHOTOS, 0},         {72, APP_PHOTOS, 0},
    {73, APP_LANGUAGE, 0},    {74, APP_TASKS, 0},          {75, APP_TASKS, 0},
    {76, APP_FLASHCARDS, 0},  {77, APP_FLASHCARDS, 0},     {78, APP_FLASHCARDS, 0},
    {79, APP_PLANETARIUM, 0}, {80, APP_PLANETARIUM, 0},    {81, APP_NONOGRAMS, 0},
    {82, APP_HEX, 0},         {83, APP_LAUNCHER, 0},       {84, APP_NOTES, 0},
    {85, APP_NOTES, 1},       {86, APP_NEWS, 0},           {87, APP_TIMER, 1},
    {90, APP_STATUS_BAR, 0},  {91, APP_TAB_BAR, 0},        {92, APP_LOCK_SCREEN, 0},
    {93, APP_KEYBOARD, 0},    {94, APP_WIKIPEDIA, 1},      {95, APP_NEWS, 1},
    {96, APP_NEWS, 1},        {97, APP_NEWS, 1},           {99, APP_LANGUAGE, 0},
    {100, APP_CALCULATOR, 1}, {101, APP_STORE, 0},         {102, APP_NOTES, 1},
    {104, APP_READER, 0},     {105, APP_SETTINGS, 0},      {110, APP_JOURNAL, 0},
    {111, APP_STUDY, 0},      {112, APP_BINGO, 0},         {113, APP_CONNECT4, 0},
    {114, APP_RECIPES, 0},    {115, APP_PERIODIC, 0},      {116, APP_FILES, 0},
    {117, APP_KNIGHTS, 0},    {118, APP_SOKOBAN, 0},       {119, APP_MAZE, 0},
    {120, APP_CROSSWORD, 0},  {121, APP_RPG, 0},           {122, APP_LLM, 0},
    {123, APP_LLM, 1},        {125, APP_PLANETARIUM, 1},   {126, APP_JOURNAL, 1},
    {127, APP_STUDY, 1}};
const AppDefinition *app_definition(AppId id) {
    return id >= 0 && id < APP_COUNT ? &definitions[id] : 0;
}
int app_route_valid(int route) {
    for (unsigned i = 0; i < sizeof(routes) / sizeof(*routes); i++)
        if (routes[i].route == route)
            return 1;
    return 0;
}
AppId app_for_route(int route) {
    for (unsigned i = 0; i < sizeof(routes) / sizeof(*routes); i++)
        if (routes[i].route == route)
            return routes[i].app;
    return APP_LIBRARY;
}

int app_route_fullscreen(int route) {
    for (unsigned i = 0; i < sizeof(routes) / sizeof(*routes); i++)
        if (routes[i].route == route)
            return routes[i].fullscreen ? routes[i].fullscreen > 0
                                        : definitions[routes[i].app].fullscreen;
    return 0;
}
