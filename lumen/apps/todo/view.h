/* ToDo rows adapt to their actual wrapped content. Shared flow, checkbox and
 * pager geometry are reused for both list and board views. */
typedef struct {
    int index, x, y, w, h;
} TaskRow18;
static TaskRow18 task_rows18[256];
static int task_rows_count18, task_pages18;
static int task_text_height(int index, int width) {
    FlowLine lines[16];
    int n = text_flow(tasks[index].title, ui_font(UI_BODY), width, lines, 14);
    return n * 27;
}
static void tasks_layout18(void) {
    task_rows_count18 = 0;
    task_pages18 = 1;
    int max_pages = 1;
    for (int col = 0; col < (task_board ? 2 : 1); col++) {
        int y = task_board ? 195 : 201, page18 = 0;
        for (int order = 0; order < task_count*2; order++) {
            int i = task_count - 1 - order % task_count;
            if (!!(tasks[i].state & TASK_FLAG) != (order < task_count)) continue;
            if (!!(tasks[i].state & TASK_DONE) != (task_board ? col : task_tab)) continue;
            int h = task_text_height(i, task_board ? 148 : 365) + 22;
            if (h < 68) h = 68;
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
    heading_actions("ToDo", 2);
    icon_button(528, 74, IC_PLUS);
    icon_button(468, 74, IC_GRID);
    const char *tabs[] = {"ToDo", "Erledigt"};
    if (task_tab > 1)
        task_tab = 1;
    if (!task_board)
        segments(28, 137, 544, 44, tabs, 2, task_tab);
    else {
        centered(28, 139, 262, 44, "ToDo", ui_font(UI_PRIMARY));
        centered(310, 139, 262, 44, "Erledigt", ui_font(UI_PRIMARY));
    }
    tasks_layout18();
    if (!task_rows_count18) {
        icon(278, 290, IC_DONE);
        centered(28, 349, 544, 40, "Hier ist alles erledigt.", ui_font(UI_PRIMARY));
        centered(28, 395, 544, 35, "Mit + eine Aufgabe hinzufügen", ui_font(UI_SECONDARY));
    }
    for (int r = 0; r < task_rows_count18; r++) {
        TaskRow18 row = task_rows18[r];
        int i = row.index, done = !!(tasks[i].state & TASK_DONE);
        region(row.x, row.y, row.w, row.h);
        if (task_board)
            key_outline(row.x, row.y, row.w, row.h);
        else
            rect(row.x, row.y + row.h, row.w, 1, 205);
        int center = row.y + row.h/2;
        int width = task_board ? 148 : 365;
        checkbox(row.x + (task_board ? 8 : 1), center - 15, done);
        text_flow_draw(row.x + (task_board ? 60 : 52), center - task_text_height(i,width)/2,
                       width, tasks[i].title, ui_font(UI_BODY), 27, 14, done);
        icon_button(task_board ? row.x+212 : 458, center - 22,
                    tasks[i].state & TASK_FLAG ? IC_FLAG_FILLED : IC_FLAG);
        if (!task_board) icon_button(522, center - 22, IC_TRASH);
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
        if (x >= row.x + (task_board ? 8 : 1) && x < row.x + (task_board ? 52 : 45) &&
            y >= row.y + row.h/2 - 24 && y < row.y + row.h/2 + 24) {
            tasks[i].state ^= TASK_DONE;
            tasks_save();
        } else if (!task_board && x >= 510)
            v1_delete = 1000 + i;
        else if (x >= (task_board ? row.x+212 : 458) &&
                 x < (task_board ? row.x+256 : 502) &&
                 abs(y - (row.y + row.h/2)) < 22) {
            tasks[i].state ^= TASK_FLAG;
            task_page = 0;
            tasks_save();
        } else {
            todo_ui.task_edit17 = i;
            open_keyboard(180, "Aufgabe bearbeiten", tasks[i].title);
        }
        return 1;
    }
    return 1;
}
