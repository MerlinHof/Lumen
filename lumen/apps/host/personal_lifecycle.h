/* App-owned module; assembled by apps/host/compose.h. */
static int launcher_load(const char *name, int count) {
    int config[APP_LIMIT * 2], seen[APP_LIMIT] = {0};
    if (!app_state_load(name, config, sizeof(int) * count * 2))
        return 0;
    for (int i = 0; i < count; i++)
        if (config[i] < 0 || config[i] >= count || seen[config[i]]++)
            return 0;
    for (int i = 0; i < count; i++) {
        launcher_order[i] = config[i];
        launcher_visible[i] = !!config[i + count];
    }
    for (int i = count; i < APP_LIMIT; i++)
        launcher_order[i] = i;
    return 1;
}
static int launcher_save(void) {
    int config[APP_LIMIT*2];
    memcpy(config,launcher_order,sizeof(launcher_order));
    memcpy(config+APP_LIMIT,launcher_visible,sizeof(launcher_visible));
    return app_state_save("launcher-packages.bin",config,sizeof(config));
}
static void v1_init(void) {
    u15_init();
    if (v1_ready)
        return;
    v1_ready = 1;
    launcher_defaults();
    for (int i = 0; i < APP_LIMIT; i++)
        launcher_visible[i] = i == APP_LIBRARY || i == APP_SETTINGS || i == APP_STORE ||
            i == APP_FILE_TRANSFER || (i >= APP_CALENDAR && i <= APP_HEX) || i >= APP_JOURNAL;
    int launcher_loaded=launcher_load("launcher-packages.bin",APP_LIMIT);
    for (int count=APP_COUNT;count>=APP_HOME+1 && !launcher_loaded;count--)
        launcher_loaded=launcher_load("launcher-36.bin",count);
    if (!launcher_loaded) {
        if (!launcher_load("launcher-31.bin", APP_COUNT)) {
            for (int count = APP_COUNT; count >= APP_SIGNAL; count--)
                if (launcher_load("launcher-16.bin", count))
                    break;
            launcher_visible[APP_LIBRARY] = launcher_visible[APP_SETTINGS] = launcher_visible[APP_STORE] = 1;
        }
        launcher_visible[APP_FILE_TRANSFER] = 1;
    }
    char terminal_order[2048];
    snprintf(terminal_order,sizeof(terminal_order),"%s/terminal-order",data_root);
    if(access(terminal_order,F_OK)) {
        int terminal=-1, language=-1;
        for(int i=0;i<APP_LIMIT;i++) {
            if(!strcmp(app_package_key(launcher_order[i]),"lumen.terminal")) terminal=i;
            if(launcher_order[i]==APP_LANGUAGE) language=i;
        }
        if(terminal>=0 && language>=0) {
            int id=launcher_order[terminal];
            if(terminal>language) {
                memmove(launcher_order+language+1,launcher_order+language,
                        (terminal-language)*sizeof(*launcher_order));
                launcher_order[language]=id;
            }
            if(launcher_save())atomic_write(terminal_order,"1",1);
        }
    }
    int notifications=app_package_find("lumen.notifications");
    if(notifications>=0) {
        char marker[2048];
        snprintf(marker,sizeof(marker),"%s/notifications-launcher-default",data_root);
        if(access(marker,F_OK)) {
            launcher_visible[notifications]=0;
            if(launcher_save())atomic_write(marker,"1",1);
        }
    }
    launcher_visible[APP_NUMSCRATCH] = launcher_visible[APP_DRAWING] = 0;
    app_state_load("note-tree-1.bin", &note_tree, sizeof(note_tree));
    if (note_tree.nf < 0 || note_tree.nf > 512 || note_tree.np < 0 || note_tree.np > 1024)
        memset(&note_tree, 0, sizeof(note_tree));
    note_tree.current = 0;
    for (int i = 0; i < note_tree.nf; i++) {
        note_tree.folder[i].name[95] = 0;
        if (note_tree.folder[i].parent == note_tree.folder[i].id)
            note_tree.folder[i].parent = 0;
    }
    if (app_state_load("tasks-1.bin", tasks, sizeof(tasks)))
        for (int i = 0; i < 256; i++) {
            tasks[i].title[199] = 0;
            if (*tasks[i].title)
                task_count = i + 1;
            tasks[i].state = tasks[i].state >= 0 && tasks[i].state <= (TASK_DONE | TASK_FLAG) ?
                             tasks[i].state & (TASK_DONE | TASK_FLAG) : 0;
        }
    v1_photos_scan();
}
static void v1_save(void) {
    if (!v1_ready || !v1_dirty)
        return;
    launcher_save();
    app_state_save("note-tree-1.bin", &note_tree, sizeof(note_tree));
    v1_dirty = 0;
}
#include "../../system/ui/chess_icons.h"
