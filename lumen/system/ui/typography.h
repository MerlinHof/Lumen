/* Visible-ink alignment and role-owned emphasis, shared by all app surfaces. */
static void type_label(int x, int y, int w, int h, const char *text, TextRole role) {
    TextStyle style = ui_text_style(role);
    text = tr(text);
    int old = ink_color;
    ink_color = style.color;
    label_ellipsis(x, y, w, h, text, style.font, 0);
    if (style.weight)
        label_ellipsis(x + 1, y, w - 1, h, text, style.font, 0);
    ink_color = old;
}
static void type_centered(int x, int y, int w, int h, const char *text, TextRole role) {
    TextStyle style = ui_text_style(role);
    int old = ink_color;
    ink_color = style.color;
    centered(x, y, w, h, text, style.font);
    if (style.weight)
        centered(x + 1, y, w - 1, h, text, style.font);
    ink_color = old;
}
/* Crisp offset edges add depth without gradients or screen animation. */
static void raised_surface(int x, int y, int w, int h) {
    control_surface(x + 3, y + 4, w, h, 15, UI_SHADOW);
    control_surface(x, y, w, h, 15, UI_INK);
    control_surface(x + 2, y + 2, w - 4, h - 4, 13, UI_PAPER);
}
static void filled_button(int x, int y, int w, int h, const char *text, const Font *font) {
    region(x, y, w, h);
    widget_shape(x, y, w, h, RADIUS_MEDIUM, 3, 0);
    control_surface(x, y, w, h, RADIUS_MEDIUM, UI_INK);
    int ink = ink_color;
    ink_color = UI_PAPER;
    centered(x, y, w, h, text, font);
    ink_color = ink;
}
static void action_button(int x, int y, int w, int h, const char *text) {
    filled_button(x, y, w, h, text, ui_font(UI_CONTROL));
}

/* Large, quiet display type shares the native font renderer and one bounded
 * cache. Changing a minute redraws a tile; other ticks reuse its raster. */
static void type_display(int x,int y,int w,int h,const char *text,int family,int size,int color) {
    typedef struct { unsigned char pixels[600*180];char text[64];int w,h,size,family,valid;unsigned used; } DisplayTile;
    static DisplayTile cache[2];static unsigned epoch;
    if(w<1 || w>600 || h<1 || h>180)return;
    DisplayTile *tile=NULL;
    for(int i=0;i<2;i++)if(cache[i].valid && cache[i].w==w && cache[i].h==h && cache[i].size==size &&
                            cache[i].family==family && !strcmp(cache[i].text,text)){tile=&cache[i];break;}
    if(!tile) {
        tile=!cache[0].valid ? &cache[0] : !cache[1].valid ? &cache[1] : cache[0].used<=cache[1].used ? &cache[0] : &cache[1];
        tile->valid=ui_display_text(text,family,size,w,h,tile->pixels);
        snprintf(tile->text,sizeof(tile->text),"%s",text);tile->w=w;tile->h=h;tile->size=size;tile->family=family;
    }
    tile->used=++epoch;
    if(!tile->valid){centered(x,y,w,h,text,ui_font(UI_DISPLAY));return;}
    for(int row=0;row<h;row++)for(int col=0;col<w;col++) {
        int tone=tile->pixels[row*w+col];
        if(tone<255)blend_pixel(x+col,y+row,(255-tone)/255.f,color);
    }
}
