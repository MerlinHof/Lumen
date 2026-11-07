/* Shared form controls: apps use these rather than drawing their own. */
static void select_row17(int y, const char *label, const char *value) {
    form_label(30, y, 312, 48, label);
    dropdown_button(360, y, 210, 48, value);
}
static void checkbox(int x, int y, int checked) {
    region(x - 9, y - 9, 48, 48);
    widget_shape(x - 9, y - 9, 48, 48, RADIUS_MEDIUM, 3, 0);
    key_outline(x, y, 30, 30);
    if (checked) {
        stroke(x + 6, y + 15, x + 12, y + 21, 3);
        stroke(x + 12, y + 21, x + 24, y + 8, 3);
    }
}

/* Natural font metrics, centered in the gutter and aligned to the text ink. */
static void small_number17(int center, int text_y, const char *number, int color) {
    int nh, nt = ink_top(number, &caption, &nh), th, tt = ink_top("H", &small, &th);
    int old = ink_color;
    ink_color = color;
    drawtext_raw(center - textwidth_raw(number, &caption) / 2, text_y + tt + (th - nh) / 2 - nt,
                 number, &caption, center * 2 - 8);
    ink_color = old;
}
