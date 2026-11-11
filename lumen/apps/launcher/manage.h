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
static void v1_launcher_settings(void) {
    heading("Apps verwalten");
    drawtext(30, 127, "Sichtbarkeit & Reihenfolge", ui_font(UI_SECONDARY), 540);
    int indices[APP_LIMIT], count = launcher_manage_indices(indices);
    for (int r = 0; r < 8 && launcher_settings_page * 8 + r < count; r++) {
        int pos = launcher_settings_page * 8 + r, i = indices[pos], id = launcher_order[i],
            y = 174 + r * 68;
        if(!engraved_app_icon(28,y+10,MANAGE_ICON_SIDE,id))
            icon_sized(30,y+12,IC_BOOK,40);
        label_ellipsis(86,y+4,242,33,tr(app_definition(id)->name),ui_font(UI_BODY),0);
        label_at(86,y+34,242,25,app_package_version(id),ui_font(UI_SECONDARY));
        page_arrow(329,y+10,IC_UP,pos>0);
        page_arrow(375,y+10,IC_DOWN,pos+1<count);
        toggle(427,y+16,launcher_visible[id]);
        page_arrow(526,y+10,IC_TRASH,app_package_can_remove(id));
        divider(28, y + 67, 544);
    }
    pager(745, &launcher_settings_page, (count + 7) / 8);
}
