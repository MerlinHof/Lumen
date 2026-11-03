/* ToDo rows adapt to their actual wrapped content. Shared flow, checkbox and
 * pager geometry are reused for both list and board views. */
typedef struct {
    int index, x, y, w, h;
} TaskRow18;
static TaskRow18 task_rows18[256];
static int task_rows_count18, task_pages18;
static int task_height18(int index, int width) {
    FlowLine lines[16];
    int n = text_flow(tasks[index].title, &small, width, lines, 14);
    int h = n * 27 + 22;
    return h < 68 ? 68 : h;
}
static void tasks_layout18(void) {
    task_rows_count18 = 0;
    task_pages18 = 1;
    int max_pages = 1;
    for (int col = 0; col < (task_board ? 2 : 1); col++) {
        int y = task_board ? 195 : 201, page18 = 0;
        for (int i = 0; i < task_count; i++) {
            if (!task_matches16(i))
                continue;
            if (task_board ? (tasks[i].state == 2) != col
                           : ((task_tab == 1) != (tasks[i].state == 2)))
                continue;
            int h = task_height18(i, task_board ? 194 : 365);
            if (y + h > 720 && y > (task_board ? 195 : 201)) {
                page18++;
                y = task_board ? 195 : 201;
            }
            if (page18 == task_page && task_rows_count18 < 256)
                task_rows18[task_rows_count18++] =
                    (TaskRow18){i, 28 + col * 282, y, task_board ? 262 : 544, h};
            y += h + 4;
        }
        if (page18 + 1 > max_pages)
            max_pages = page18 + 1;
    }
    task_pages18 = max_pages;
    if (task_page >= task_pages18) {
        task_page = task_pages18 - 1;
        tasks_layout18();
    }
}
static void tasks_draw18(void) {
    heading("ToDo");
    icon_button(528, 74, IC_PLUS);
    icon_button(468, 74, IC_GRID);
    dropdown_button(175, 74, 275, 44, *task_context16 ? task_context16 : "Alle Kontexte");
    for (int i = 0; i < task_count; i++)
        if (tasks[i].state == 1) {
            tasks[i].state = 0;
            v1_dirty = 1;
        }
    const char *tabs[] = {"ToDo", "Erledigt"};
    if (task_tab > 1)
        task_tab = 1;
    if (!task_board)
        segments(28, 137, 544, 44, tabs, 2, task_tab);
    else {
        centered(28, 139, 262, 44, "ToDo", &body);
        centered(310, 139, 262, 44, "Erledigt", &body);
    }
    tasks_layout18();
    if (!task_rows_count18) {
        icon(278, 290, IC_DONE);
        centered(28, 349, 544, 40, "Hier ist alles erledigt.", &body);
        centered(28, 395, 544, 35, "Mit + eine Aufgabe hinzufügen", &caption);
    }
    for (int r = 0; r < task_rows_count18; r++) {
        TaskRow18 row = task_rows18[r];
        int i = row.index, done = tasks[i].state == 2;
        region(row.x, row.y, row.w, row.h);
        if (task_board)
            key_outline(row.x, row.y, row.w, row.h);
        else
            rect(row.x, row.y + row.h, row.w, 1, 205);
        checkbox(row.x + 1, row.y + 9, done);
        text_flow_draw(row.x + 52, row.y + 10, task_board ? 194 : 365, tasks[i].title, &small, 27,
                       14, done);
        if (!task_board) {
            icon_button(458, row.y + 7, IC_FOLDER);
            icon_button(522, row.y + 7, IC_TRASH);
        }
    }
    pager(746, &task_page, task_pages18);
}
static int tasks_input18(const Input *in) {
    if (in->kind != INPUT_TAP)
        return 2;
    int x = in->x, y = in->y;
    if (y >= 74 && y < 118) {
        if (x >= 516)
            open_keyboard(110, "Neue Aufgabe", "");
        else if (x >= 456) {
            task_board ^= 1;
            tasks_fullscreen = 0;
            task_tab = 0;
            task_page = 0;
        }
        return 1;
    }
    if (!task_board && y >= 137 && y < 181 && x >= 28 && x < 572) {
        task_tab = (x - 28) / (544 / 2);
        if (task_tab > 1)
            task_tab = 1;
        task_page = 0;
        return 1;
    }
    tasks_layout18();
    for (int r = 0; r < task_rows_count18; r++) {
        TaskRow18 row = task_rows18[r];
        if (x < row.x || x >= row.x + row.w || y < row.y || y >= row.y + row.h)
            continue;
        int i = row.index;
        if (x < row.x + 48) {
            tasks[i].state = tasks[i].state == 2 ? 0 : 2;
            v1_dirty = 1;
            v1_save();
        } else if (!task_board && x >= 510)
            v1_delete = 1000 + i;
        else if (!task_board && x >= 450) {
            category_task16 = i;
            open_keyboard(170, "Kategorie (leer = keine)", task_categories16[i]);
        } else {
            task_edit17 = i;
            open_keyboard(180, "Aufgabe bearbeiten", tasks[i].title);
        }
        return 1;
    }
    return 1;
}
