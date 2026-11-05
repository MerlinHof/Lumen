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

static struct { char name[64];AppReceiver receive; } endpoints[16];
static size_t endpoint_count;
int app_endpoint_register(const char *name,AppReceiver receive) {
    if(!name || !*name || strlen(name)>63 || !receive)return 0;
    for(size_t i=0;i<endpoint_count;i++)if(!strcmp(name,endpoints[i].name)){endpoints[i].receive=receive;return 1;}
    if(endpoint_count==16)return 0;
    memcpy(endpoints[endpoint_count].name,name,strlen(name)+1);endpoints[endpoint_count++].receive=receive;return 1;
}
int app_call(const char *name,const void *payload,size_t bytes,int64_t now) {
    if(!name || !payload || bytes>4096)return 0;
    for(size_t i=0;i<endpoint_count;i++)if(!strcmp(name,endpoints[i].name))return endpoints[i].receive(payload,bytes,now);
    return 0;
}

typedef struct {
    void (*draw)(void);
    int (*input)(const Input *);
    void (*checkpoint)(void);
    int standalone;
} AppView;
static AppView views[APP_COUNT];

void app_view_register(AppId id, void (*draw)(void), int (*input)(const Input *)) {
    if (app_definition(id)) views[id] = (AppView){.draw = draw, .input = input};
}
int app_view_draw(AppId id) {
    if (!app_definition(id) || !views[id].draw) return 0;
    views[id].draw();
    return 1;
}
int app_view_input(AppId id, const Input *input) {
    if (!app_definition(id) || !views[id].input) return -1;
    return views[id].input(input);
}

/* Standalone pages dispatch by catalog entry, without route-specific shell code. */
void app_page_register(AppId id, void (*draw)(void), int (*input)(const Input *),
                       void (*checkpoint)(void)) {
    if (!app_definition(id)) return;
    app_view_register(id, draw, input);
    views[id].standalone = 1;
    views[id].checkpoint = checkpoint;
}
int app_page_draw(int route) {
    AppId id = app_for_route(route);
    return views[id].standalone && app_definition(id)->entry == route && app_view_draw(id);
}
int app_page_input(int route, const Input *input) {
    AppId id = app_for_route(route);
    if (!views[id].standalone || app_definition(id)->entry != route) return -1;
    return app_view_input(id, input);
}
void app_page_checkpoint(AppId id) {
    if (app_definition(id) && views[id].checkpoint) views[id].checkpoint();
}
void app_pages_checkpoint(void) {
    for (int i = 0; i < APP_COUNT; i++) app_page_checkpoint((AppId)i);
}
