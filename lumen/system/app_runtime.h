#ifndef LUMEN_APP_RUNTIME_H
#define LUMEN_APP_RUNTIME_H
#include "contracts.h"
#include "device.h"
#include <stddef.h>
#include <stdint.h>
/* App handles are valid only for the lifetime of a running registry. */
typedef int AppId;
enum { APP_LIMIT=128 };
typedef enum { INTENT_OPEN, INTENT_BOOK, INTENT_SEARCH } IntentKind;
typedef struct {
    AppId app;
    IntentKind kind;
    BookId object_id;
    int has_position;
    ReaderPosition position;
    char text[2048];
} AppIntent;
typedef struct {
    AppId id;
    const char *name;
} AppDefinition;
typedef struct {
    AppId active;
    unsigned generation;
    AppIntent intent;
    void (*leave)(AppId, void *);
    void *context;
} AppRuntime;
const AppDefinition *app_definition(AppId id);
void app_runtime_init(AppRuntime *rt, void (*leave)(AppId, void *), void *context);
void app_runtime_activate(AppRuntime *rt,AppId app);
#endif
