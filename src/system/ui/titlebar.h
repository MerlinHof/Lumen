/* Titles and actions share a 44 px optical center and 60 px action columns. */
static void heading_width(const char *label, int width) {
    label = tr(label);
    const Font *font = textwidth_raw(label, &editorial) <= width ? &editorial : &body;
    type_label(28,74,width,44,label,font==ui_font(UI_TITLE)?UI_TITLE:UI_HEADING);
}
static void heading_actions(const char *label,int actions){heading_width(label,544-actions*60);}
static void heading(const char *label){heading_width(label,544);}
