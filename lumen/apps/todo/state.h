/* App-owned state; stable on-disk schemas are intentionally preserved. */
static struct {
    char title[200];
    int state;
    int64_t created;
} tasks[256];
enum { TASK_DONE = 2, TASK_FLAG = 4 };
static unsigned char task_recent_done[256];
static int task_count, task_page, task_tab, task_board;
static void tasks_save(void) {
    app_state_save("tasks-1.bin", tasks, sizeof(tasks));
}
