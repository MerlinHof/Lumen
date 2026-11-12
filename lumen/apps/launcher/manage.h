/* App-owned module; assembled by apps/host/compose.h. */
static int launcher_manage_indices(int *indices) {
    int n = 0;
    for (int i = 0; i < APP_LIMIT; i++)
        if (!app_package_deleted(launcher_order[i]) && app_route_valid(app_definition(launcher_order[i])->entry))
            indices[n++] = i;
    return n;
}
static int manage_delete_id=-1;
static void launcher_delete_confirm(int choice) {
    if(choice==1 && app_package_can_remove(manage_delete_id)) {
        if(!app_package_remove(app_package_key(manage_delete_id)))
            ui_message("Löschen fehlgeschlagen.");
        else app_process_send("{\"event\":\"refresh\"}");
    }
    manage_delete_id=-1;
}
