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
enum { ENGRAVED_ICON_SIDE = 76, ENGRAVED_ICON_COUNT = 8 };
static void engraved_app_icon(int x, int y, int id) {
    int index = -1;
    for (int i=0; i<ENGRAVED_ICON_COUNT; i++)
        if ((int)launcher_first[i] == id) index=i;
    if (index < 0) return;
    static unsigned char sheet[ENGRAVED_ICON_COUNT][ENGRAVED_ICON_SIDE*ENGRAVED_ICON_SIDE];
    static int loaded;
    if (!loaded) {
        char path[2048];
        snprintf(path,sizeof(path),"%s/../home/engraved.gray",data_root);
        FILE *file=fopen(path,"rb");
#ifdef LUMEN_HOST
        if (!file) file=fopen("lumen/assets/home/engraved.gray","rb");
#endif
        loaded=file && fread(sheet,1,sizeof(sheet),file) == sizeof(sheet) && fgetc(file) == EOF ? 1 : -1;
        if (file) fclose(file);
    }
    if (loaded > 0)
        ui_paint_image(x,y,ENGRAVED_ICON_SIDE,ENGRAVED_ICON_SIDE,sheet[index]);
}
static void app_tile(int x, int y, int width, int id) {
    int tile_x = x + (width-78)/2;
    region(tile_x, y+2, 78, 78);
    if (options[OPT_APP_ICON_STYLE]) {
        widget_shape(tile_x,y+2,78,78,RADIUS_MEDIUM,0,1);
        engraved_app_icon(tile_x+1,y+3,id);
    } else {
        widget_shape(tile_x,y+2,78,78,32,3,0);
        control_surface(tile_x,y+2,78,78,32,0);
        control_surface(tile_x+2,y+4,74,74,30,255);
        app_icon(tile_x+17,y+17,id);
    }
    const char *name = tr(app_definition(id)->name);
    const Font *font = textwidth_raw(name,ui_font(UI_BODY)) < width-12 ?
                       ui_font(UI_BODY) : ui_font(UI_SECONDARY);
    centered(x,y+78,width,35,name,font);
}
