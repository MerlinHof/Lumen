/* State owned by the keyboard app in the legacy view host. */
typedef struct {
    int keyboard;
    int keyboard_target;
    int keyboard_shift;
    int keyboard_symbols;
    int cursor_visible;
    int64_t cursor_epoch;
    char edit[8192];
    int note_cursor;
    int note_view_start;
    int note_scroll_lock;
    int keyboard_view18;
    int accent_count18, accent_opening_touch, pressed_key;
    int accent_x18;
    int clip_active;
    int keyboard_background_valid18;
    unsigned char keyboard_background18[WIDTH * HEIGHT];
    char accent_keys18[10][8];
    const char *keyboard_title;
} EditorUi;
static EditorUi editor_ui = {
    .cursor_visible = 1,
    .keyboard_title = "Suchen",
};
