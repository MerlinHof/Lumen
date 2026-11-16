#include "../system/app_runtime.h"
#include "../system/app_packages.h"
#include "lumen_builtin_catalog.h"

const char *app_factory_provider(PackageProvider role) {
    return builtin_providers[role];
}
const char *app_factory_key(AppId id) {
    return id >= 0 && id < APP_COUNT && builtin_keys[id] ? builtin_keys[id] : "";
}
const AppDefinition *app_factory_definition(AppId id) {
    return id >= 0 && id < APP_COUNT ? &definitions[id] : 0;
}
const AppDefinition *app_definition(AppId id) {
    const AppDefinition *installed=app_package_definition(id);
    if (installed) return installed;
    if (id==APP_FROG || id==APP_SIGNAL || id==APP_TIC_TAC_TOE || id==APP_2048 || id==APP_SUDOKU ||
        id==APP_BINGO || id==APP_CONNECT4 || id==APP_HEX || id==APP_NONOGRAMS || id==APP_SOLITAIRE || id==APP_WORDLE || id==APP_PERIODIC) {
        static AppDefinition unavailable[APP_COUNT];
        unavailable[id]=(AppDefinition){id,definitions[id].name,-1};
        return &unavailable[id];
    }
    return app_factory_definition(id);
}
int app_route_valid(int route) {
    if(route>=1000 && route<1000+APP_LIMIT-APP_COUNT)return app_package_installed(APP_COUNT+route-1000);
    for (unsigned i = 0; i < sizeof(routes) / sizeof(*routes); i++)
        if (routes[i].route == route)
            return 1;
    return 0;
}
AppId app_for_route(int route) {
    if(route>=1000 && route<1000+APP_LIMIT-APP_COUNT)return APP_COUNT+route-1000;
    for (unsigned i = 0; i < sizeof(routes) / sizeof(*routes); i++)
        if (routes[i].route == route)
            return routes[i].app;
    return APP_LIBRARY;
}
