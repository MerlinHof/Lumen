/* App-owned module; assembled by apps/host/compose.h. */
static int v1_visible(int *out) {
    int n = 0;
    for (int i = 0; i < APP_COUNT; i++) {
        int id = launcher_order[i];
        if (id != APP_NUMSCRATCH && id != APP_DRAWING && id != APP_FILES && id != APP_RPG &&
            launcher_visible[id])
            out[n++] = id;
    }
    return n;
}
static void v1_launcher(void) {
    polish_order();
    heading_actions("Apps", 1);
    icon_button(528, 74, IC_SETTINGS);
    int ids[APP_COUNT], n = v1_visible(ids);
    int pages = (n + 11) / 12;
    if (launcher_page >= pages)
        launcher_page = pages ? pages - 1 : 0;
    for (int r = 0; r < 12 && launcher_page * 12 + r < n; r++) {
        int id = ids[launcher_page * 12 + r], x = 53 + (r % 3) * 172, y = 137 + (r / 3) * 130;
        region(x, y, 150, 122);
        control_surface(x+36,y+2,78,78,32,0);
        control_surface(x+38,y+4,74,74,30,255);
        app_icon(x + 53, y + 17, id);
        const char *name = tr(app_definition(id)->name);
        if (textwidth_raw(name, ui_font(UI_BODY)) <= 148)
            centered(x, y + 78, 150, 35, name, ui_font(UI_BODY));
        else
            centered(x, y + 78, 150, 35, name, ui_font(UI_SECONDARY));
    }
    if (!n)
        wrapped(28, 210, "Apps über das Zahnrad einblenden.", ui_font(UI_PRIMARY), 544, 3);
    pager(679, &launcher_page, pages);
}
static void v1_launcher_input(int x, int y) {
    if (y >= 74 && y < 118 && x >= 516) {
        extra_open(83);
        return;
    }
    int ids[APP_COUNT], n = v1_visible(ids);
    if (y >= 679 && y < 723) {
        if (x < 300 && launcher_page)
            launcher_page--;
        else if (x >= 300 && (launcher_page + 1) * 12 < n)
            launcher_page++;
        return;
    }
    if (y >= 137 && y < 657 && x >= 53 && x < 547) {
        int c = (x - 53) / 172, r = (y - 137) / 130, i = launcher_page * 12 + r * 3 + c;
        if (c < 3 && i < n) {
            AppIntent req = {.app = ids[i], .kind = INTENT_OPEN};
            app_send(&req);
        }
    }
}
#include "../../vendor/wordle_words.h"
static const char *wordle_answers[] = {
    "PAPER", "LIGHT", "STONE", "EARTH", "WATER", "DREAM", "BRAIN", "BOOKS", "PLANT", "STARS",
    "NIGHT", "CLOUD", "OCEAN", "GRACE", "STUDY", "LEARN", "MUSIC", "SPACE", "GREEN", "HOUSE",
    "TRAIN", "FRAME", "CHESS", "CLOCK", "SOUND", "HEART", "CLEAR", "FRESH", "SWEET", "QUIET",
    "WORLD", "NORTH", "SOUTH", "RIVER", "FLAME", "SHORE", "BREAD", "PEACH", "APPLE", "TABLE",
    "CHAIR", "STORY", "COVER", "LAYER", "FIELD", "ORBIT", "SOLAR", "LUNAR", "SCALE", "LOGIC",
    "QUEST", "PRIDE", "BRAVE", "SMILE", "WHALE", "EAGLE", "HORSE", "TIGER", "ZEBRA", "BLOOM",
    "GLASS", "METAL", "AMBER", "CORAL"};
