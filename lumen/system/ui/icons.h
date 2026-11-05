/* Central system icon library. All UI modules reference semantic IDs. */
enum {
    IC_HOME,
    IC_PLUS,
    IC_BACK,
    IC_SEARCH,
    IC_STORE,
    IC_SETTINGS,
    IC_CONNECT,
    IC_POWER,
    IC_PREV,
    IC_NEXT,
    IC_BOOK,
    IC_TYPE,
    IC_CONTENTS,
    IC_MORE,
    IC_GRID,
    IC_DELETE,
    IC_DONE,
    IC_SPACE,
    IC_SHIFT,
    IC_TRASH,
    IC_UP,
    IC_DOWN,
    IC_ROTATE,
    IC_RESTART,
    IC_FLAG,
    IC_CLOSE,
    IC_HISTORY,
    IC_UNDO,
    IC_CLOCK,
    IC_ENTER,
    IC_GRAPH,
    IC_FOLDER,
    IC_MOVE,
    IC_MARKDOWN,
    IC_FULLSCREEN,
    IC_DEVICES,
    IC_ROTATE_PHOTO,
    IC_FIT,
    IC_REBOOT,
    IC_SHUTDOWN,
    IC_EDIT,
    IC_LOCK,
    IC_FOLDER_PLUS,
    IC_NOTE_PLUS,
    IC_TODAY,
    IC_NOTE,
    IC_PIN,
    IC_PERCENT,
    IC_DIGITAL,
    IC_GLOBE,
    IC_ALIGN,
    IC_BARS,
    IC_LAST = IC_BARS
};
/* One optical grid: 44 px tile, 3 px round-capped ink, soft 5 px corners. */
static void icon_box(int x, int y, int w, int h) {
    aa_soft_rect(x, y, w, h, 5, 0);
    aa_soft_rect(x + 3, y + 3, w - 6, h - 6, 3, 255);
}
static void icon_chevron(int cx, int cy, int direction) {
    stroke(cx - 4 * direction, cy - 9, cx + 5 * direction, cy, 3);
    stroke(cx + 5 * direction, cy, cx - 4 * direction, cy + 9, 3);
}
static void icon_native(int x, int y, int kind) {
    int c = x + 22, d = y + 22;
    switch (kind) {
    case IC_HOME:
        stroke(x + 7, y + 20, c, y + 7, 3);
        stroke(c, y + 7, x + 37, y + 20, 3);
        icon_box(x + 11, y + 18, 22, 22);
        rect(x + 19, y + 28, 6, 12, 255);
        break;
    case IC_PLUS:
        stroke(c, d - 12, c, d + 12, 3);
        stroke(c - 12, d, c + 12, d, 3);
        break;
    case IC_BACK:
        stroke(c - 12, d, c + 12, d, 3);
        stroke(c - 12, d, c - 2, d - 10, 3);
        stroke(c - 12, d, c - 2, d + 10, 3);
        break;
    case IC_PREV:
    case IC_NEXT:
        icon_chevron(c, d, kind == IC_NEXT ? 1 : -1);
        break;
    case IC_SEARCH:
        ring(c - 3, d - 3, 10, 3);
        stroke(c + 5, d + 5, c + 14, d + 14, 3);
        break;
    case IC_STORE:
        stroke(x + 5, y + 10, x + 10, y + 10, 3);
        stroke(x + 10, y + 10, x + 15, y + 30, 3);
        stroke(x + 13, y + 15, x + 38, y + 15, 3);
        stroke(x + 38, y + 15, x + 34, y + 27, 3);
        stroke(x + 34, y + 27, x + 15, y + 27, 3);
        disc(x + 18, y + 36, 3, 0);
        disc(x + 32, y + 36, 3, 0);
        break;
    case IC_SETTINGS:
        for (int i = 0; i < 3; i++) {
            int yy = y + 10 + i * 12, xx = x + (i == 1 ? 27 : 16);
            stroke(x + 6, yy, x + 38, yy, 3);
            disc(xx, yy, 5, 255);
            ring(xx, yy, 4, 3);
        }
        break;
    case IC_CONNECT:
        icon_box(x + 10, y + 4, 24, 36);
        stroke(x + 17, y + 10, x + 27, y + 10, 3);
        disc(c, y + 33, 2, 0);
        break;
    case IC_POWER:
    case IC_SHUTDOWN:
        aa_arc(c, d + 2, 14, 310, 360, 3);
        aa_arc(c, d + 2, 14, 0, 230, 3);
        stroke(c, y + 5, c, y + 22, 3);
        break;
    case IC_BOOK:
        aa_soft_rect(x + 5, y + 6, 34, 33, 5, 0);
        aa_soft_rect(x + 8, y + 9, 28, 26, 3, 255);
        stroke(c, y + 9, c, y + 35, 3);
        stroke(x + 12, y + 14, x + 17, y + 14, 2);
        stroke(x + 27, y + 14, x + 32, y + 14, 2);
        break;
    case IC_TYPE:
        stroke(x + 7, y + 33, x + 16, y + 10, 3);
        stroke(x + 16, y + 10, x + 25, y + 33, 3);
        stroke(x + 11, y + 25, x + 21, y + 25, 3);
        ring(x + 33, y + 28, 5, 3);
        stroke(x + 38, y + 23, x + 38, y + 33, 3);
        break;
    case IC_CONTENTS:
    case IC_DEVICES:
        for (int i = 0; i < 3; i++) {
            int yy = y + 11 + i * 11;
            if (kind == IC_CONTENTS)
                disc(x + 8, yy, 2, 0);
            else {
                stroke(x + 5, yy, x + 8, yy + 3, 2);
                stroke(x + 8, yy + 3, x + 13, yy - 4, 2);
            }
            stroke(x + 20, yy, x + 37, yy, 3);
        }
        break;
    case IC_MORE:
        for (int i = -1; i <= 1; i++)
            disc(c + i * 10, d, 2, 0);
        break;
    case IC_GRID:
        for (int i = 0; i < 9; i++)
            disc(x + 11 + i % 3 * 11, y + 11 + i / 3 * 11, 3, 0);
        break;
    case IC_DELETE:
        stroke(x + 5, d, x + 15, y + 10, 3);
        stroke(x + 15, y + 10, x + 38, y + 10, 3);
        stroke(x + 38, y + 10, x + 38, y + 34, 3);
        stroke(x + 38, y + 34, x + 15, y + 34, 3);
        stroke(x + 15, y + 34, x + 5, d, 3);
        stroke(x + 23, y + 17, x + 31, y + 27, 3);
        stroke(x + 31, y + 17, x + 23, y + 27, 3);
        break;
    case IC_DONE:
        stroke(x + 9, y + 23, x + 18, y + 31, 3);
        stroke(x + 18, y + 31, x + 36, y + 12, 3);
        break;
    case IC_SPACE:
        stroke(x + 7, y + 19, x + 7, y + 28, 3);
        stroke(x + 7, y + 28, x + 37, y + 28, 3);
        stroke(x + 37, y + 28, x + 37, y + 19, 3);
        break;
    case IC_SHIFT:
    case IC_UP:
    case IC_DOWN: {
        int s = kind == IC_DOWN ? 1 : -1;
        stroke(c, d - s * 12, c, d + s * 12, 3);
        stroke(c, d + s * 12, c - 10, d + s * 2, 3);
        stroke(c, d + s * 12, c + 10, d + s * 2, 3);
        if (kind == IC_SHIFT)
            stroke(x + 13, y + 39, x + 31, y + 39, 3);
        break;
    }
    case IC_TRASH:
        stroke(x + 7, y + 11, x + 37, y + 11, 3);
        stroke(x + 17, y + 6, x + 27, y + 6, 3);
        icon_box(x + 11, y + 11, 22, 28);
        stroke(x + 18, y + 19, x + 18, y + 31, 2);
        stroke(x + 26, y + 19, x + 26, y + 31, 2);
        break;
    case IC_ROTATE:
    case IC_ROTATE_PHOTO:
    case IC_RESTART:
    case IC_REBOOT:
    case IC_HISTORY:
    case IC_UNDO:
        if (kind == IC_UNDO || kind == IC_HISTORY) {
            aa_arc(c, d, 14, 210, 360, 3);
            aa_arc(c, d, 14, 0, 160, 3);
            stroke(x + 9, y + 8, x + 9, y + 19, 3);
            stroke(x + 9, y + 19, x + 20, y + 19, 3);
        } else {
            aa_arc(c, d, 14, 45, 315, 3);
            stroke(x + 32, y + 12, x + 35, y + 15, 3);
            stroke(x + 35, y + 5, x + 35, y + 15, 3);
            stroke(x + 25, y + 15, x + 35, y + 15, 3);
        }
        if (kind == IC_HISTORY) {
            stroke(c, d, c, y + 14, 3);
            stroke(c, d, x + 28, y + 26, 3);
        }
        break;
    case IC_FLAG:
        stroke(x + 12, y + 5, x + 12, y + 39, 3);
        aa_soft_rect(x + 12, y + 6, 24, 17, 4, 0);
        break;
    case IC_CLOSE:
        stroke(x + 12, y + 12, x + 32, y + 32, 3);
        stroke(x + 32, y + 12, x + 12, y + 32, 3);
        break;
    case IC_CLOCK:
        ring(c, d, 15, 3);
        stroke(c, d, c, y + 12, 3);
        stroke(c, d, x + 30, y + 26, 3);
        break;
    case IC_ENTER:
        stroke(x + 35, y + 10, x + 35, y + 27, 3);
        stroke(x + 35, y + 27, x + 9, y + 27, 3);
        stroke(x + 9, y + 27, x + 18, y + 18, 3);
        stroke(x + 9, y + 27, x + 18, y + 36, 3);
        break;
    case IC_GRAPH:
        stroke(x + 7, y + 6, x + 7, y + 37, 3);
        stroke(x + 7, y + 37, x + 38, y + 37, 3);
        stroke(x + 11, y + 29, x + 20, y + 15, 3);
        stroke(x + 20, y + 15, x + 29, y + 24, 3);
        stroke(x + 29, y + 24, x + 38, y + 9, 3);
        break;
    case IC_FOLDER:
    case IC_FOLDER_PLUS:
        aa_soft_rect(x + 5, y + 12, 34, 25, 5, 0);
        aa_soft_rect(x + 5, y + 7, 17, 15, 4, 0);
        aa_soft_rect(x + 8, y + 15, 28, 19, 3, 255);
        if (kind == IC_FOLDER_PLUS) {
            disc(x + 34, y + 33, 10, 255);
            disc(x + 34, y + 33, 8, 0);
            rect(x + 33, y + 28, 2, 11, 255);
            rect(x + 29, y + 32, 10, 2, 255);
        }
        break;
    case IC_PIN:
        stroke(c-9, y+12, c+9, y+12, 3);
        stroke(c-5, y+12, c-5, y+24, 3);
        stroke(c+5, y+12, c+5, y+24, 3);
        stroke(c-10, y+25, c+10, y+25, 3);
        stroke(c, y+26, c, y+38, 3);
        break;
    case IC_PERCENT:
        ring(c-7, d-8, 4, 2);
        ring(c+7, d+8, 4, 2);
        stroke(c-10, d+13, c+10, d-13, 3);
        break;
    case IC_DIGITAL:
        icon_box(x+4, y+11, 36, 22);
        stroke(c-7,d-4,c-7,d+4,3);
        stroke(c+7,d-4,c+7,d+4,3);
        disc(c,d-3,2,0); disc(c,d+3,2,0);
        break;
    case IC_GLOBE:
        ring(c,d,15,3);
        stroke(c-14,d,c+14,d,2);
        stroke(c,d-14,c,d+14,2);
        break;
    case IC_ALIGN:
        for(int i=0;i<4;i++)
            stroke(x+8,y+10+8*i,x+(i%2?27:36),y+10+8*i,3);
        break;
    case IC_BARS:
        for(int i=0;i<3;i++)
            stroke(x+11+11*i,y+32,x+11+11*i,y+24-i*7,5);
        break;
    case IC_NOTE:
    case IC_NOTE_PLUS:
        icon_box(x + 9, y + 4, 27, 37);
        for (int i = 0; i < 3; i++)
            stroke(x + 15, y + 15 + i * 7, x + 29 - (i == 2 ? 5 : 0), y + 15 + i * 7, 2);
        if (kind == IC_NOTE_PLUS) {
            disc(x + 34, y + 33, 10, 255);
            disc(x + 34, y + 33, 8, 0);
            rect(x + 33, y + 28, 2, 11, 255);
            rect(x + 29, y + 32, 10, 2, 255);
        }
        break;
    case IC_MOVE:
        icon_native(x, y, IC_FOLDER);
        stroke(x + 18, y + 25, x + 29, y + 25, 3);
        stroke(x + 29, y + 25, x + 24, y + 20, 3);
        stroke(x + 29, y + 25, x + 24, y + 30, 3);
        break;
    case IC_MARKDOWN:
        icon_box(x + 3, y + 9, 38, 27);
        stroke(x + 10, y + 28, x + 10, y + 17, 3);
        stroke(x + 10, y + 17, x + 16, y + 23, 3);
        stroke(x + 16, y + 23, x + 22, y + 17, 3);
        stroke(x + 22, y + 17, x + 22, y + 28, 3);
        stroke(x + 32, y + 17, x + 32, y + 28, 3);
        stroke(x + 28, y + 24, x + 32, y + 28, 3);
        stroke(x + 32, y + 28, x + 36, y + 24, 3);
        break;
    case IC_FULLSCREEN:
    case IC_FIT:
        for (int i = 0; i < 4; i++) {
            int xx = x + (i % 2 ? 36 : 8), yy = y + (i / 2 ? 36 : 8), sx = i % 2 ? -1 : 1,
                sy = i / 2 ? -1 : 1;
            stroke(xx, yy, xx + sx * 9, yy, 3);
            stroke(xx, yy, xx, yy + sy * 9, 3);
        }
        break;
    case IC_EDIT:
        stroke(x + 12, y + 29, x + 29, y + 12, 5);
        stroke(x + 29, y + 12, x + 33, y + 16, 5);
        stroke(x + 33, y + 16, x + 16, y + 33, 5);
        stroke(x + 12, y + 29, x + 10, y + 36, 3);
        stroke(x + 10, y + 36, x + 16, y + 33, 3);
        break;
    case IC_LOCK:
        aa_arc(c, y + 19, 9, 180, 360, 3);
        icon_box(x + 8, y + 19, 28, 21);
        disc(c, y + 28, 2, 0);
        stroke(c, y + 28, c, y + 33, 2);
        break;
    case IC_TODAY:
        icon_box(x + 6, y + 9, 32, 30);
        stroke(x + 6, y + 18, x + 38, y + 18, 3);
        stroke(x + 14, y + 5, x + 14, y + 13, 3);
        stroke(x + 30, y + 5, x + 30, y + 13, 3);
        disc(c, y + 28, 4, 0);
        break;
    }
}
/* Canonical masks are cached once; compact actions retain 44px touch targets. */
static void icon_sized(int x, int y, int kind, int size) {
    static unsigned char tiles[IC_LAST + 1][48 * 48], ready[IC_LAST + 1];
    if (kind < 0 || kind > IC_LAST) return;
    if (!ready[kind]) {
        unsigned char saved[48 * 48];
        int offset = ui_y_offset;
        ui_y_offset = 0;
        for (int j=0;j<48;j++) {
            memcpy(saved+j*48,ui_pixels+j*WIDTH,48);
            memset(ui_pixels+j*WIDTH,255,48);
        }
        icon_native(2,2,kind);
        for (int j=0;j<48;j++) {
            memcpy(tiles[kind]+j*48,ui_pixels+j*WIDTH,48);
            memcpy(ui_pixels+j*WIDTH,saved+j*48,48);
        }
        ui_y_offset = offset;
        ready[kind]=1;
    }
    for (int j=0;j<size;j++) for (int i=0;i<size;i++) {
        int u=i*48/size,v=j*48/size,u1=(i+1)*48/size,v1=(j+1)*48/size;
        int sum=0,n=0;
        for(int yy=v;yy<v1;yy++) for(int xx=u;xx<u1;xx++){sum+=tiles[kind][yy*48+xx];n++;}
        blend_pixel(x+i,y+j,(255-sum/(float)n)/255.f,0);
    }
}
static void icon(int x,int y,int kind) { icon_sized(x+4,y+4,kind,36); }
static void icon_button(int x, int y, int kind) {
    region(x, y, 44, 44);
    icon(x, y, kind);
}
