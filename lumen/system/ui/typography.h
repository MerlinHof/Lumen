/* Visible-ink alignment and role-owned emphasis, shared by all app surfaces. */
static void type_label(int x, int y, int w, int h, const char *text, TextRole role) {
    TextStyle style = ui_text_style(role);
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
    rect(x + 3, y + 4, w, h, UI_SHADOW);
    rect(x, y, w, h, UI_PAPER);
    key_outline(x, y, w, h);
}
static void action_button(int x, int y, int w, int h, const char *text) {
    region(x, y, w, h);
    aa_soft_rect(x, y, w, h, 9, UI_INK);
    type_centered(x, y, w, h, text, UI_INVERSE);
}
