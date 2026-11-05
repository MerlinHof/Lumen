typedef struct {
    int target;
    const char *title, *value;
} KeyboardRequest;
static char keyboard_request_title[256];
static int keyboard_receive(const void *payload, size_t size, int64_t now) {
    if (size != sizeof(KeyboardRequest)) return 0;
    const KeyboardRequest *request = payload;
    int target = request->target;
    const char *title = request->title, *value = request->value;
    keyboard = 1;
    dropdown = 0;
    keyboard_target = target;
    snprintf(keyboard_request_title, sizeof(keyboard_request_title), "%s", title);
    keyboard_title = keyboard_request_title;
    snprintf(edit, sizeof(edit), "%s", value);
    note_cursor = strlen(edit);
    note_view_start = note_scroll_lock = keyboard_view18 = accent_count18 = 0;
    keyboard_shift = keyboard_symbols = 0;
    keyboard_background_valid18 = 0;
    editor_reset18();
    cursor_visible = 1;
    cursor_epoch = now;
    pressed = -1;
    return 1;
}
