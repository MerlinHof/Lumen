/* App-owned module; assembled by apps/host/compose.h. */
static void u16_dropdown(int target, int choice) {
    if (target == 920) {
        u16_calendar_choice(choice);
    }
    if (target == 916) {
        reader_ui.reader_progress_mode = choice;
        options[OPT_PROGRESS] = choice != 5;
        library_options_save();
        char b[16];
        snprintf(b, 16, "%d", choice);
        extra_write_text("reader-progress15.txt", b);
    }
    if (target == 917 && choice >= 0 && choice < 6) {
        auto_sleep_minutes = sleep_minutes16[choice];
        char b[16];
        snprintf(b, 16, "%d", auto_sleep_minutes);
        extra_write_text("auto-sleep16.txt", b);
    }
    if (target == 918 && choice >= 0 && choice < context_count16) {
        snprintf(todo_ui.task_context16, 64, "%s", choice ? context_names16[choice] : "");
        task_page = 0;
    }
    if (target == 919) {
        calc_precision16 = choice == 0 ? 6 : choice == 1 ? 10 : 15;
        char b[20];
        snprintf(b, 20, "%d", calc_precision16);
        extra_write_text("calc-precision16.txt", b);
    }
}
static int u16_commit(int target, const char *text) {
    if (u17_commit(target, text))
        return 1;
    if (u16_apps_commit(target, text))
        return 1;
    if (target == 170) {
        if (todo_ui.category_task16 >= 0 && todo_ui.category_task16 < task_count) {
            snprintf(todo_ui.task_categories16[todo_ui.category_task16], 64, "%s", text);
            app_state_save("task-categories16.bin", todo_ui.task_categories16, sizeof(todo_ui.task_categories16));
        }
        return 1;
    }
    return 0;
}
static int u16_draw(void) {
    u16_load();

    return u16_apps_draw();
}
static int u16_tick(void) {
    u16_load();
    if (!sleeping)
        device_activity();
    if (ink_dirty && !ink_drag && extra_millis() >= ink_save_at)
        ink_save();
    if (!sleeping && auto_sleep_minutes > 0 && !connect_running() &&
        extra_millis() - activity16 >= (int64_t)auto_sleep_minutes * 60000) {
        Input in = {.kind = INPUT_SLEEP};
        activity16 = extra_millis();
        device_request_sleep();
        return ui_input(&in);
    }
    return u16_jobs_tick();
}
static int u16_input(const Input *in) {
    u16_load();
    if (in->kind == INPUT_DOWN || in->kind == INPUT_TAP || in->kind == INPUT_SWIPE ||
        in->kind == INPUT_WAKE)
        activity16 = extra_millis();
    if (sleeping || editor_ui.keyboard || dropdown)
        return -1;
    if (in->kind == INPUT_TAP) {
        int x = in->x, y = in->y;

        if (ui_route() == 9 && setting_category == 3 && y >= 590 && y < 642) {
            u17_sleep_select(590);
            return 1;
        }

        if (ui_route() == 74 && y >= 74 && y < 118 && x >= 175 && x < 450) {
            contexts16();
            int selected = 0;
            for (int i = 1; i < context_count16; i++)
                if (!strcmp(context_names16[i], todo_ui.task_context16))
                    selected = i;
            open_dropdown(918, context_labels16, context_count16, selected, 175, 123, 275);
            return 1;
        }
    }
    return u16_apps_input(in);
}
