/* App-owned state; stable on-disk schemas are intentionally preserved. */
static int calc_degrees=1,calc_alt,calc_cursor,calc_history_count,calc_history_page,calc_history_mode;
static char calc_expr[1024],calc_result[128],calc_error[128],calc_history[16][1024];
static double calc_answer,calc_memory;
