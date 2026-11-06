/* App-owned module; assembled by apps/host/compose.h. */
static int v1_visible(int *out) {
    int n = 0;
    for (int i = 0; i < APP_COUNT; i++) {
        int id = launcher_order[i];
        if (app_definition(id)->entry >= 0 && launcher_visible[id])
            out[n++] = id;
    }
    return n;
}
static void app_tile(int x, int y, int width, int id) {
    int tile_x = x + (width-78)/2;
    region(tile_x, y+2, 78, 78);
    widget_shape(tile_x,y+2,78,78,32,3,0);
    control_surface(tile_x,y+2,78,78,32,0);
    control_surface(tile_x+2,y+4,74,74,30,255);
    app_icon(tile_x+17, y+17, id);
    const char *name = tr(app_definition(id)->name);
    const Font *font = textwidth_raw(name,ui_font(UI_BODY)) < width-12 ?
                       ui_font(UI_BODY) : ui_font(UI_SECONDARY);
    centered(x,y+78,width,35,name,font);
}
