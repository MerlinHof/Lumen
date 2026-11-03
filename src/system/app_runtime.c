#include "app_runtime.h"
#include <string.h>
void app_runtime_init(AppRuntime *rt, void (*leave)(AppId, void *), void *context) {
    memset(rt, 0, sizeof(*rt));
    rt->route = 1;
    rt->intent.app = APP_LIBRARY;
    rt->leave = leave;
    rt->context = context;
}
int app_runtime_route(AppRuntime *rt, int route) {
    if (!app_route_valid(route))
        return 0;
    AppId from = app_for_route(rt->route), to = app_for_route(route);
    if (from != to && rt->leave)
        rt->leave(from, rt->context);
    if (route != rt->route)
        rt->generation++;
    rt->route = route;
    return 1;
}
int app_runtime_deliver(AppRuntime *rt, const AppIntent *intent) {
    if (!intent || !app_definition(intent->app) || intent->kind < INTENT_OPEN ||
        intent->kind > INTENT_SETTINGS)
        return 0;
    /* Own the payload. Never retain an app's stack/selection/temporary pointer. */
    if (intent->kind == INTENT_BOOK && intent->app != APP_READER)
        return 0;
    AppIntent copy = *intent;
    copy.position.quote[sizeof(copy.position.quote) - 1] = 0;
    copy.text[sizeof(copy.text) - 1] = 0;
    if (!app_runtime_route(rt, app_definition(copy.app)->entry))
        return 0;
    rt->intent = copy;
    return 1;
}
int app_runtime_tabs(const AppRuntime *rt) { return !app_route_fullscreen(rt->route); }
int app_runtime_tab(const AppRuntime *rt) { return app_definition(app_for_route(rt->route))->tab; }
