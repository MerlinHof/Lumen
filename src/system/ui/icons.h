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
    IC_NOTE
};
static void icon(int x, int y, int kind) {
    int c = x + 22, d = y + 22;
    switch (kind) {
    case IC_NOTE:
        stroke(x + 10, y + 5, x + 27, y + 5, 3);
        stroke(x + 27, y + 5, x + 36, y + 14, 3);
        stroke(x + 36, y + 14, x + 36, y + 39, 3);
        stroke(x + 36, y + 39, x + 10, y + 39, 3);
        stroke(x + 10, y + 39, x + 10, y + 5, 3);
        stroke(x + 27, y + 5, x + 27, y + 14, 2);
        stroke(x + 27, y + 14, x + 36, y + 14, 2);
        for (int i = 0; i < 3; i++)
            stroke(x + 16, y + 21 + i * 6, x + 29, y + 21 + i * 6, 2);
        return;
    case IC_FOLDER_PLUS:
    case IC_NOTE_PLUS:
        icon(x - 3, y, kind == IC_FOLDER_PLUS ? IC_FOLDER : IC_NOTE);
        aa_disc(x + 34, y + 33, 10, 255);
        aa_disc(x + 34, y + 33, 8, 0);
        rect(x + 33, y + 28, 2, 11, 255);
        rect(x + 29, y + 32, 10, 2, 255);
        return;
    case IC_TODAY:
        key_outline(x + 7, y + 9, 30, 29);
        stroke(x + 7, y + 17, x + 37, y + 17, 3);
        stroke(x + 14, y + 5, x + 14, y + 12, 3);
        stroke(x + 30, y + 5, x + 30, y + 12, 3);
        disc(x + 22, y + 27, 4, 0);
        return;
    case IC_LOCK:
        aa_arc(x + 22, y + 18, 9, 180, 360, 4);
        stroke(x + 13, y + 17, x + 13, y + 24, 4);
        stroke(x + 31, y + 17, x + 31, y + 24, 4);
        aa_soft_rect(x + 8, y + 22, 28, 19, 4, 0);
        disc(x + 22, y + 29, 2, 255);
        rect(x + 21, y + 29, 2, 6, 255);
        return;
    case IC_EDIT:
        stroke(x + 12, y + 30, x + 29, y + 13, 2);
        stroke(x + 16, y + 34, x + 33, y + 17, 2);
        stroke(x + 29, y + 13, x + 33, y + 17, 2);
        stroke(x + 12, y + 30, x + 10, y + 36, 2);
        stroke(x + 10, y + 36, x + 16, y + 34, 2);
        stroke(x + 12, y + 30, x + 16, y + 34, 2);
        stroke(x + 31, y + 11, x + 35, y + 15, 3);
        return;
    case IC_FOLDER:
        stroke(x + 7, y + 15, x + 7, y + 34, 3);
        stroke(x + 7, y + 34, x + 37, y + 34, 3);
        stroke(x + 37, y + 34, x + 37, y + 17, 3);
        stroke(x + 37, y + 17, x + 21, y + 17, 3);
        stroke(x + 21, y + 17, x + 17, y + 12, 3);
        stroke(x + 17, y + 12, x + 7, y + 12, 3);
        stroke(x + 7, y + 12, x + 7, y + 15, 3);
        return;
    case IC_MOVE:
        stroke(x + 8, y + 14, x + 8, y + 35, 3);
        stroke(x + 8, y + 35, x + 31, y + 35, 3);
        stroke(x + 19, y + 25, x + 36, y + 8, 3);
        stroke(x + 26, y + 8, x + 36, y + 8, 3);
        stroke(x + 36, y + 8, x + 36, y + 18, 3);
        return;
    case IC_MARKDOWN:
        stroke(x + 3, y + 32, x + 3, y + 12, 3);
        stroke(x + 3, y + 12, x + 12, y + 24, 3);
        stroke(x + 12, y + 24, x + 21, y + 12, 3);
        stroke(x + 21, y + 12, x + 21, y + 32, 3);
        stroke(x + 33, y + 12, x + 33, y + 32, 3);
        stroke(x + 27, y + 26, x + 33, y + 32, 3);
        stroke(x + 39, y + 26, x + 33, y + 32, 3);
        return;
    case IC_FULLSCREEN:
    case IC_FIT:
        for (int i = 0; i < 4; i++) {
            int xx = x + (i % 2 ? 37 : 7), yy = y + (i / 2 ? 37 : 7), dx = i % 2 ? -10 : 10,
                dy = i / 2 ? -10 : 10;
            stroke(xx, yy, xx + dx, yy, 5);
            stroke(xx, yy, xx, yy + dy, 5);
        }
        return;
    case IC_DEVICES:
        for (int i = 0; i < 3; i++) {
            int yy = y + 10 + i * 12;
            stroke(x + 5, yy, x + 12, yy + 6, 2);
            stroke(x + 12, yy + 6, x + 19, yy - 3, 3);
            stroke(x + 26, yy, x + 40, yy, 3);
        }
        return;
    case IC_ROTATE_PHOTO:
        stroke(x + 11, y + 21, x + 11, y + 39, 2);
        stroke(x + 11, y + 39, x + 30, y + 39, 2);
        stroke(x + 30, y + 39, x + 30, y + 21, 2);
        stroke(x + 30, y + 21, x + 11, y + 21, 2);
        arc(x + 22, y + 16, 14, 185, 350, 3);
        stroke(x + 36, y + 14, x + 30, y + 10, 3);
        stroke(x + 36, y + 14, x + 39, y + 7, 3);
        return;
    case IC_REBOOT:
        icon(x, y, IC_RESTART);
        return;
    case IC_SHUTDOWN:
        icon(x, y, IC_POWER);
        return;

    case IC_GRAPH:
        stroke(c - 14, d - 15, c - 14, d + 14, 2);
        stroke(c - 14, d + 14, c + 15, d + 14, 2);
        stroke(c - 11, d + 8, c - 5, d - 4, 2);
        stroke(c - 5, d - 4, c + 2, d + 3, 2);
        stroke(c + 2, d + 3, c + 12, d - 13, 2);
        break;
    case IC_ENTER:
        stroke(c + 11, d - 10, c + 11, d + 7, 2);
        stroke(c + 11, d + 7, c - 12, d + 7, 2);
        stroke(c - 12, d + 7, c - 5, d, 2);
        stroke(c - 12, d + 7, c - 5, d + 14, 2);
        break;
    case IC_CLOSE:
        stroke(c - 8, d - 8, c + 8, d + 8, 2);
        stroke(c + 8, d - 8, c - 8, d + 8, 2);
        break;
    case IC_CLOCK:
        ring(c, d, 12, 2);
        stroke(c, d, c, d - 7, 2);
        stroke(c, d, c + 6, d + 3, 2);
        break;
    case IC_HISTORY:
        arc(c, d, 12, 210, 540, 2);
        stroke(c - 12, d - 6, c - 12, d - 14, 2);
        stroke(c - 12, d - 6, c - 4, d - 6, 2);
        stroke(c, d, c, d - 6, 2);
        stroke(c, d, c + 6, d + 4, 2);
        break;
    case IC_UNDO:
        arc(c + 2, d + 4, 11, 190, 350, 3);
        stroke(c - 9, d, c - 9, d - 9, 3);
        stroke(c - 9, d, c, d, 3);
        break;
    case IC_PLUS:
        stroke(c - 12, d, c + 12, d, 3);
        stroke(c, d - 12, c, d + 12, 3);
        break;
    case IC_HOME:
        stroke(c - 13, d - 2, c, d - 14, 3);
        stroke(c, d - 14, c + 13, d - 2, 3);
        stroke(c - 10, d - 3, c - 10, d + 12, 3);
        stroke(c + 10, d - 3, c + 10, d + 12, 3);
        stroke(c - 10, d + 12, c - 4, d + 12, 3);
        stroke(c + 4, d + 12, c + 10, d + 12, 3);
        stroke(c - 4, d + 12, c - 4, d + 3, 3);
        stroke(c + 4, d + 12, c + 4, d + 3, 3);
        stroke(c - 4, d + 3, c + 4, d + 3, 3);
        break;
    case IC_BACK:
        stroke(c - 13, d, c + 12, d, 3);
        stroke(c - 13, d, c - 3, d - 10, 3);
        stroke(c - 13, d, c - 3, d + 10, 3);
        break;
    case IC_SEARCH:
        ring(c - 3, d - 3, 10, 2);
        stroke(c + 4, d + 4, c + 13, d + 13, 3);
        break;
    case IC_STORE:
        stroke(c - 16, d - 13, c - 11, d - 13, 3);
        stroke(c - 11, d - 13, c - 6, d + 7, 3);
        stroke(c - 9, d - 8, c + 15, d - 8, 3);
        stroke(c + 15, d - 8, c + 11, d + 4, 3);
        stroke(c + 11, d + 4, c - 6, d + 4, 3);
        stroke(c - 6, d + 7, c + 12, d + 7, 3);
        ring(c - 4, d + 13, 3, 2);
        ring(c + 10, d + 13, 3, 2);
        break;
    case IC_UP:
    case IC_DOWN: {
        int a = kind == IC_UP ? -1 : 1;
        stroke(c, d - a * 12, c, d + a * 12, 3);
        stroke(c, d + a * 12, c - 9, d + a * 3, 3);
        stroke(c, d + a * 12, c + 9, d + a * 3, 3);
        break;
    }
    case IC_ROTATE:
    case IC_RESTART:
        arc(c, d, 12, 45, 315, 2);
        stroke(c + 8, d - 8, c + 1, d - 8, 3);
        stroke(c + 8, d - 8, c + 8, d - 15, 3);
        break;
    case IC_FLAG:
        stroke(c - 8, d - 14, c - 8, d + 14, 3);
        stroke(c - 13, d + 14, c - 2, d + 14, 3);
        stroke(c - 7, d - 13, c + 12, d - 6, 3);
        stroke(c + 12, d - 6, c - 7, d + 1, 3);
        break;
    case IC_TRASH:
        stroke(c - 12, d - 9, c + 12, d - 9, 3);
        stroke(c - 5, d - 14, c + 5, d - 14, 3);
        stroke(c - 5, d - 14, c - 5, d - 9, 3);
        stroke(c + 5, d - 14, c + 5, d - 9, 3);
        stroke(c - 9, d - 5, c - 8, d + 13, 3);
        stroke(c + 9, d - 5, c + 8, d + 13, 3);
        stroke(c - 8, d + 13, c + 8, d + 13, 3);
        stroke(c - 3, d - 3, c - 3, d + 8, 3);
        stroke(c + 3, d - 3, c + 3, d + 8, 3);
        break;
    case IC_DELETE:
        stroke(c - 14, d, c - 5, d - 10, 3);
        stroke(c - 14, d, c - 5, d + 10, 3);
        stroke(c - 5, d - 10, c + 14, d - 10, 3);
        stroke(c + 14, d - 10, c + 14, d + 10, 3);
        stroke(c + 14, d + 10, c - 5, d + 10, 3);
        stroke(c, d - 4, c + 8, d + 4, 3);
        stroke(c, d + 4, c + 8, d - 4, 3);
        break;
    case IC_DONE:
        stroke(c - 12, d, c - 4, d + 8, 3);
        stroke(c - 4, d + 8, c + 13, d - 10, 3);
        break;
    case IC_SPACE:
        stroke(c - 14, d - 4, c - 14, d + 4, 3);
        stroke(c - 14, d + 4, c + 14, d + 4, 3);
        stroke(c + 14, d + 4, c + 14, d - 4, 3);
        break;
    case IC_SHIFT:
        stroke(c, d - 13, c - 13, d, 3);
        stroke(c, d - 13, c + 13, d, 3);
        stroke(c - 13, d, c - 5, d, 3);
        stroke(c + 13, d, c + 5, d, 3);
        stroke(c - 5, d, c - 5, d + 12, 3);
        stroke(c + 5, d, c + 5, d + 12, 3);
        stroke(c - 5, d + 12, c + 5, d + 12, 3);
        break;
    case IC_SETTINGS:
        for (int i = 0; i < 3; i++) {
            int yy = d - 10 + i * 10, xx = c + (i == 1 ? 6 : -5);
            stroke(c - 14, yy, c + 14, yy, 3);
            rect(xx - 4, yy - 4, 8, 8, 255);
            ring(xx, yy, 4, 2);
        }
        break;
    case IC_CONNECT:
        stroke(c, d - 13, c, d + 12, 3);
        stroke(c, d - 13, c - 5, d - 7, 3);
        stroke(c, d - 13, c + 5, d - 7, 3);
        stroke(c, d + 5, c - 10, d - 3, 3);
        stroke(c - 10, d - 3, c - 10, d - 9, 3);
        rect(c - 13, d - 13, 6, 6, 0);
        stroke(c, d + 1, c + 10, d - 7, 3);
        ring(c + 10, d - 10, 3, 2);
        ring(c, d + 13, 3, 2);
        break;
    case IC_POWER:
        aa_arc(c, d + 1, 13, 305, 360, 4);
        aa_arc(c, d + 1, 13, 0, 235, 4);
        stroke(c, d - 16, c, d - 2, 3);
        break;
    case IC_PREV:
    case IC_NEXT: {
        int a = kind == IC_NEXT ? 1 : -1;
        stroke(c - a * 4, d - 10, c + a * 5, d, 3);
        stroke(c + a * 5, d, c - a * 4, d + 10, 3);
        break;
    }
    case IC_BOOK:
        stroke(c, d - 9, c, d + 12, 3);
        for (int a = -1; a <= 1; a += 2) {
            stroke(c, d - 9, c + a * 13, d - 12, 3);
            stroke(c + a * 13, d - 12, c + a * 13, d + 9, 3);
            stroke(c + a * 13, d + 9, c, d + 12, 3);
        }
        break;
    case IC_TYPE:
        centered(x, y, 44, 44, "Aa", &small);
        break;
    case IC_CONTENTS:
        for (int i = 0; i < 3; i++) {
            disc(c - 13, d - 10 + i * 10, 2, 0);
            stroke(c - 5, d - 10 + i * 10, c + 14, d - 10 + i * 10, 3);
        }
        break;
    case IC_MORE:
        for (int i = -1; i <= 1; i++)
            rect(c + i * 9 - 2, d - 2, 4, 4, 0);
        break;
    case IC_GRID:
        for (int row = 0; row < 3; row++)
            for (int col = 0; col < 3; col++)
                disc(c - 10 + col * 10, d - 10 + row * 10, 3, 0);
        break;
    }
}
static void icon_button(int x, int y, int kind) {
    region(x, y, 44, 44);
    icon(x, y, kind);
}
