/* Semantic type roles: apps select meaning, not their own font/grey/weight. */
enum { RADIUS_SMALL = 7, RADIUS_MEDIUM = 12, RADIUS_LARGE = 18 };
enum {
    UI_INK = 0,
    UI_SECONDARY_INK = 104,
    UI_QUIET_INK = 144,
    UI_PAPER = 255,
    UI_RULE = 205,
    UI_SHADOW = 80
};
typedef enum {
    UI_TITLE,
    UI_HEADING,
    UI_PRIMARY,
    UI_BODY,
    UI_SECONDARY,
    UI_DISPLAY,
    UI_CONTROL,
    UI_INVERSE
} TextRole;
typedef struct {
    const Font *font;
    int color, weight;
} TextStyle;
static TextStyle ui_text_style(TextRole role) {
    static const TextStyle styles[] = {{&editorial, UI_INK, 1},
                                       {&body, UI_INK, 1},
                                       {&body, UI_INK, 0},
                                       {&small, UI_INK, 0},
                                       {&caption, UI_SECONDARY_INK, 0},
                                       {&title, UI_INK, 0},
                                       {&small, UI_INK, 0},
                                       {&body, UI_PAPER, 1}};
    return styles[role >= UI_TITLE && role <= UI_INVERSE ? role : UI_BODY];
}
static const Font *ui_font(TextRole role) {
    return ui_text_style(role).font;
}
