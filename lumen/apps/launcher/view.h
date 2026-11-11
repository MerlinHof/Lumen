/* App-owned module; assembled by apps/host/compose.h. */
static int v1_visible(int *out) {
    int n = 0;
    for (int i = 0; i < APP_LIMIT; i++) {
        int id = launcher_order[i];
        if (app_definition(id)->entry >= 0 && launcher_visible[id] && !app_package_deleted(id))
            out[n++] = id;
    }
    return n;
}
enum { ENGRAVED_ICON_SIDE = 76, MANAGE_ICON_SIDE = 44 };
static int engraved_app_icon(int x, int y, int side, int id) {
    if(side!=ENGRAVED_ICON_SIDE && side!=MANAGE_ICON_SIDE)return 0;
    unsigned char source[76*76];
    if(!app_package_icon(id,source))return 0;
    if(side==ENGRAVED_ICON_SIDE) {
        ui_paint_image(x,y,side,side,source);
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
                    sum+=source[sy*ENGRAVED_ICON_SIDE+sx];
            small_icon[row*side+col]=sum/((end_y-start_y)*(end_x-start_x));
        }
    ui_paint_image(x,y,side,side,small_icon);
    return 1;
}
static void app_tile(int x, int y, int width, int id) {
    int tile_x = x + (width-78)/2;
    region(tile_x, y+2, 78, 78);
    widget_shape(tile_x,y+2,78,78,RADIUS_MEDIUM,0,1);
    if(!engraved_app_icon(tile_x+1,y+3,ENGRAVED_ICON_SIDE,id))
        icon_sized(tile_x+19,y+21,IC_BOOK,40);
    const char *name = tr(app_definition(id)->name);
    const Font *font=&small;
    char label[256];
    snprintf(label,sizeof(label),"%s",name);
    if (textwidth_raw(label,font) > width-16) {
        size_t n=strlen(label);
        while (n && textwidth_raw(label,font)+textwidth_raw("...",font)>width-16) {
            do { n--; } while (n && (label[n]&0xc0)==0x80);
            label[n]=0;
        }
        snprintf(label+n,sizeof(label)-n,"...");
    }
    int measured=textwidth_raw(label,font);
    label_at(x+(width-measured)/2,y+79,measured,28,label,font);
}
