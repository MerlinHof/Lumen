/* App-owned module; assembled by apps/host/compose.h. */
static void tree_build(void) {
    tree_count = 0;
    for (int i = 0; i < note_tree.nf; i++)
        if (note_tree.folder[i].parent == note_tree.current) {
            tree_type[tree_count] = 1;
            tree_id[tree_count++] = i;
        }
    for (int i = 0; i < note_count; i++)
        if (v1_folder_for(note_ids[i]) == note_tree.current) {
            tree_type[tree_count] = 0;
            tree_id[tree_count++] = note_ids[i];
        }
    if (tree_page * 7 >= tree_count)
        tree_page = tree_count ? (tree_count - 1) / 7 : 0;
}
static void polish_notes(void) {
    u16_notes_defaults();
    const char *name = note_tree.current ? v1_folder_name(note_tree.current) : tr("Notizen");
    label_ellipsis(28, 74, note_tree.current ? 350 : 410, 44, name, ui_font(UI_TITLE), 0);
    notes_ui.note_rename_x17 = 36 + (int)fmin(350, textwidth_raw(name, ui_font(UI_TITLE)));
    if (note_tree.current) {
        icon_button(notes_ui.note_rename_x17, 74, IC_EDIT);
        u17_gray_icon(notes_ui.note_rename_x17, 74);
    }
    icon_button(468, 74, IC_FOLDER_PLUS);
    icon_button(528, 74, IC_NOTE_PLUS);
    tree_build();
    if (!tree_count) {
        ink_color = UI_SECONDARY_INK;
        drawtext(30, 170, "Dieser Ordner ist leer.", ui_font(UI_BODY), 540);
        ink_color = 0;
    }
    for (int r = 0; r < 7 && tree_page * 7 + r < tree_count; r++) {
        int i = tree_page * 7 + r, y = 151 + r * 78;
        ListLayout l;
        if (tree_type[i]) {
            int folder = note_tree.folder[tree_id[i]].id, children = 0;
            for (int f = 0; f < note_tree.nf; f++)
                children += note_tree.folder[f].parent == folder;
            for (int n = 0; n < note_count; n++)
                children += v1_folder_for(note_ids[n]) == folder;
            char count[100];
            if (children == 1)
                snprintf(count, sizeof(count), "%s", tr("1 direkter Eintrag"));
            else
                snprintf(count, sizeof(count), tr("%d direkte Einträge"), children);
            l = list_row(28, y, 544, 78, -2, 76, note_tree.folder[tree_id[i]].name, count,
                         ui_font(UI_PRIMARY), 1);
        } else {
            time_t stamp = tree_id[i];
            char date[50];
            strftime(date, sizeof(date), "%d.%m.%Y", localtime(&stamp));
            l = list_row(28, y, 544, 78, -2, 76, note_list_title(tree_id[i]), date, ui_font(UI_PRIMARY), 1);
        }
        icon(28, l.icon_y + 5, tree_type[i] ? IC_FOLDER : IC_NOTE);
        icon_button(522, l.action_y, IC_MORE);
    }
    pager(746, &tree_page, (tree_count + 6) / 7);
}
static int polish_notes_input(const Input *in) {
    if (in->kind != INPUT_TAP)
        return 2;
    int x = in->x, y = in->y;
    if (y >= 74 && y < 118) {
        if (x >= 516) {
            note_selected = -1;
            note_text[0] = 0;
            extra_page = 0;
            extra_open(30);
            open_keyboard(30, "Notiz", "");
            editor_ui.note_cursor = editor_ui.note_view_start = 0;
        } else if (x >= 456)
            open_keyboard(120, "Neuer Ordner", "");
        else if (note_tree.current && x >= notes_ui.note_rename_x17 && x < notes_ui.note_rename_x17 + 44)
            open_keyboard(162, "Ordner umbenennen", v1_folder_name(note_tree.current));
        return 1;
    }
    tree_build();
    if (y >= 151 && y < 697) {
        int i = tree_page * 7 + (y - 151) / 78;
        if (i < tree_count) {
            int id = tree_type[i] ? note_tree.folder[tree_id[i]].id : tree_id[i];
            if (x >= 510)
                u17_notes_menu(tree_type[i], id, y);
            else if (tree_type[i]) {
                note_tree.current = id;
                tree_page = 0;
            } else {
                note_load(id);
                extra_page = 0;
                extra_open(30);
            }
        }
    }
    return 1;
}
static void polish_tasks(void) {
    tasks_draw18();
}
static int polish_tasks_input(const Input *in) {
    return tasks_input18(in);
}
