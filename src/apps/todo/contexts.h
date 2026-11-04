/* App-owned module; assembled by apps/host/compose.h. */
static int task_matches16(int i) {
    return !*task_context16 || !strcmp(task_context16, task_categories16[i]);
}
static void contexts16(void) {
    context_count16 = 1;
    snprintf(context_names16[0], 64, "Alle Kontexte");
    for (int i = 0; i < task_count && context_count16 < 257; i++)
        if (*task_categories16[i]) {
            int found = 0;
            for (int j = 1; j < context_count16; j++)
                if (!strcmp(context_names16[j], task_categories16[i]))
                    found = 1;
            if (!found)
                snprintf(context_names16[context_count16++], 64, "%s", task_categories16[i]);
        }
    for (int i = 0; i < context_count16; i++)
        context_labels16[i] = context_names16[i];
}
