/* State owned by the todo app in the legacy view host. */
typedef struct {
    int task_edit17;
    char task_categories16[256][64];
    char task_context16[64];
    int category_task16;
} TodoUi;
static TodoUi todo_ui = {
    .task_edit17 = -1,
    .category_task16 = -1,
};
