/* Shared selection preview and action layout for document and editable text. */
enum { EDIT_SELECTION_Y = 246, READER_SELECTION_Y = 515 };
static void selection_menu_draw(int y, const char *text, const char *const actions[5]) {
    raised_surface(19, y, 558, 258);
    wrapped_raw(35, y + 18, text, &small, 526, 3);
    button_h(35, y + 116, 165, 48, actions[0], 0);
    button_h(217, y + 116, 165, 48, actions[1], 0);
    button_h(399, y + 116, 160, 48, actions[2], 0);
    button_h(35, y + 189, 250, 48, actions[3], 0);
    button_h(310, y + 189, 250, 48, actions[4], 0);
}
static int selection_menu_action(int top, int x, int y) {
    y -= top;
    if (y >= 116 && y < 164) {
        if (x >= 35 && x < 200) return 0;
        if (x >= 217 && x < 382) return 1;
        if (x >= 399 && x < 559) return 2;
    }
    if (y >= 189 && y < 237) {
        if (x >= 35 && x < 285) return 3;
        if (x >= 310 && x < 560) return 4;
    }
    return -1;
}
