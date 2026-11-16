/* App-owned module; assembled by apps/host/compose.h. */
/* 1.1 app polish: shared chrome, permanent reading caches, deterministic flows. */
#include "../news/api.h"
static int power_menu, power_hold, power_request;
static int64_t power_menu_wake_until;
static int tree_id[1536], tree_type[1536], tree_count, tree_page;

static int launcher_compare(const void *left,const void *right) {
    int a=*(const int *)left,b=*(const int *)right;
    int order=app_package_launcher_order(a)-app_package_launcher_order(b);
    if(order)return order;
    return strcmp(app_package_key(a),app_package_key(b));
}
static void launcher_defaults(void) {
    for(int id=0;id<APP_LIMIT;id++) {
        launcher_order[id]=id;
        launcher_visible[id]=app_package_launcher_visible(id);
    }
    qsort(launcher_order,APP_LIMIT,sizeof(*launcher_order),launcher_compare);
}
