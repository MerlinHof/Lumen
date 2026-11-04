/* App-owned state; stable on-disk schemas are intentionally preserved. */
static struct {
    char title[200];
    int state;
    int64_t created;
} tasks[256];
static int task_count, task_page, task_tab, task_board;
