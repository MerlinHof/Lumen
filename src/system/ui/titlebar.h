/* Titles and actions share a 44 px optical center and 60 px action columns. */
static void heading_width(const char *label, int width) {
    label = tr(label);
    const Font *font = textwidth_raw(label, &editorial) <= width ? &editorial : &body;
    label_ellipsis(28, 74, width, 44, label, font, 0);
    label_ellipsis(29, 74, width - 1, 44, label, font, 0);
}
static void heading_actions(const char *label, int actions) {
    heading_width(label, 544 - actions * 60);
}
static void heading(const char *label) {
    heading_width(label, 544);
}
