#ifndef LUMEN_APP_RUNTIME_H
#define LUMEN_APP_RUNTIME_H
#include "contracts.h"
#include "device.h"
#include <stddef.h>
#include <stdint.h>
/* System contract. Apps own routes and choose their chrome; the shell only
 * manages activation, lifecycle notifications and bounded intent delivery. */
typedef enum {
    APP_LIBRARY,
    APP_READER,
    APP_SETTINGS,
    APP_STORE,
    APP_FILE_TRANSFER,
    APP_CALENDAR,
    APP_INSIGHTS,
    APP_MINES,
    APP_WORDS,
    APP_BLOCKS,
    APP_SNAKE,
    APP_CALCULATOR,
    APP_TIMER,
    APP_NOTES,
    APP_DRAWING,
    APP_WEATHER,
    APP_HOME_ASSISTANT,
    APP_WIKIPEDIA,
    APP_QUOTES,
    APP_CHESS,
    APP_TIC_TAC_TOE,
    APP_NUMSCRATCH,
    APP_2048,
    APP_SOLITAIRE,
    APP_SUDOKU,
    APP_FOCUS,
    APP_NEWS,
    APP_WORDLE,
    APP_PHOTOS,
    APP_LANGUAGE,
    APP_TASKS,
    APP_FLASHCARDS,
    APP_PLANETARIUM,
    APP_NONOGRAMS,
    APP_HEX,
    APP_STATUS_BAR,
    APP_RESERVED_36,
    APP_LOCK_SCREEN,
    APP_KEYBOARD,
    APP_JOURNAL,
    APP_STUDY,
    APP_BINGO,
    APP_CONNECT4,
    APP_RECIPES,
    APP_PERIODIC,
    APP_FILES,
    APP_KNIGHTS,
    APP_SOKOBAN,
    APP_MAZE,
    APP_CROSSWORD,
    APP_RPG,
    APP_LLM,
    APP_SIGNAL,
    APP_FROG,
    APP_HOME,
    APP_COUNT
} AppId;
typedef enum { INTENT_OPEN, INTENT_BOOK, INTENT_SEARCH, INTENT_SETTINGS } IntentKind;
typedef struct {
    AppId app;
    IntentKind kind;
    BookId object_id;
    int has_position;
    ReaderPosition position;
    int section;
    char text[2048];
} AppIntent;
typedef struct {
    AppId id;
    const char *name;
    int entry;
} AppDefinition;
typedef struct {
    int route;
    AppId app;
} AppRoute;
typedef struct {
    int route;
    unsigned generation;
    AppIntent intent;
    void (*leave)(AppId, void *);
    void *context;
} AppRuntime;
const AppDefinition *app_definition(AppId id);
AppId app_for_route(int route);
int app_route_valid(int route);
void app_runtime_init(AppRuntime *rt, void (*leave)(AppId, void *), void *context);
int app_runtime_route(AppRuntime *rt, int route);
int app_runtime_deliver(AppRuntime *rt, const AppIntent *intent);
/* A view can be composed by the shell or opened as a standalone app. */
void app_view_register(AppId id, void (*draw)(void), int (*input)(const Input *));
int app_view_draw(AppId id);
int app_view_input(AppId id, const Input *input);
void app_page_register(AppId id, void (*draw)(void), int (*input)(const Input *),
                       void (*checkpoint)(void));
int app_page_draw(int route);
int app_page_input(int route, const Input *input);
void app_page_checkpoint(AppId id);
void app_pages_checkpoint(void);
/* Named, non-navigating app endpoints (overlays and component apps). */
typedef int (*AppReceiver)(const void *payload,size_t bytes,int64_t now);
int app_endpoint_register(const char *name,AppReceiver receive);
int app_call(const char *name,const void *payload,size_t bytes,int64_t now);
#endif
