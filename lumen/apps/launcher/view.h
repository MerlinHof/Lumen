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
enum { ENGRAVED_ICON_SIDE = 76, MANAGE_ICON_SIDE = 44 };
static int engraved_app_icon(int x, int y, int side, int id) {
    if (side != ENGRAVED_ICON_SIDE && side != MANAGE_ICON_SIDE) return 0;
    static unsigned char sheet[APP_COUNT][ENGRAVED_ICON_SIDE*ENGRAVED_ICON_SIDE];
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
    if (loaded < 0 || id < 0 || id >= APP_COUNT) return 0;
    if (side == ENGRAVED_ICON_SIDE) {
        ui_paint_image(x,y,side,side,sheet[id]);
        return 1;
    }
    /* Average the corresponding source area for the smaller Manage Apps list. */
    unsigned char small_icon[MANAGE_ICON_SIDE*MANAGE_ICON_SIDE];
    for (int row=0; row<side; row++)
        for (int col=0; col<side; col++) {
            int start_y=row*ENGRAVED_ICON_SIDE/side, end_y=(row+1)*ENGRAVED_ICON_SIDE/side;
            int start_x=col*ENGRAVED_ICON_SIDE/side, end_x=(col+1)*ENGRAVED_ICON_SIDE/side;
            unsigned sum=0;
            for (int sy=start_y; sy<end_y; sy++)
                for (int sx=start_x; sx<end_x; sx++)
                    sum+=sheet[id][sy*ENGRAVED_ICON_SIDE+sx];
            small_icon[row*side+col]=sum/((end_y-start_y)*(end_x-start_x));
        }
    ui_paint_image(x,y,side,side,small_icon);
    return 1;
}
static void app_tile(int x, int y, int width, int id) {
    int tile_x = x + (width-78)/2;
    region(tile_x, y+2, 78, 78);
    if (options[OPT_APP_ICON_STYLE] && engraved_app_icon(tile_x+1,y+3,ENGRAVED_ICON_SIDE,id)) {
        widget_shape(tile_x,y+2,78,78,RADIUS_MEDIUM,0,1);
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
