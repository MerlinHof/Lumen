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
    editor_ui.keyboard = 1;
    dropdown = 0;
    editor_ui.keyboard_target = target;
    snprintf(keyboard_request_title, sizeof(keyboard_request_title), "%s", title);
    editor_ui.keyboard_title = keyboard_request_title;
    snprintf(editor_ui.edit, sizeof(editor_ui.edit), "%s", value);
    editor_ui.note_cursor = strlen(editor_ui.edit);
    editor_ui.autosave = editor_ui.line_numbers = editor_ui.note_scroll_lock = editor_ui.keyboard_view18 = editor_ui.accent_count18 = 0;
    editor_ui.keyboard_shift = editor_ui.keyboard_symbols = 0;
    editor_ui.pressed_key = -1;
    editor_ui.keyboard_background_valid18 = 0;
    editor_reset18();
    editor_ui.cursor_visible = 1;
    editor_ui.cursor_epoch = now;
    pressed = -1;
    return 1;
}
