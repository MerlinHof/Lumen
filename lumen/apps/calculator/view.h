/* App-owned module; assembled by apps/host/compose.h. */
static void calc_preview(void) {
    calc_result[0] = 0;
    calc_error[0] = 0;
}
static void calc_insert(const char *s) {
    if (!text_insert(calc_expr, sizeof(calc_expr), &calc_cursor, s))
        return;
    calc_preview();
    calc_error[0] = 0;
}
static void calc_backspace(void) {
    text_backspace(calc_expr, &calc_cursor);
    calc_preview();
    calc_error[0] = 0;
}
static void calc_run(void) {
    double v;
    if (calc_evaluate(calc_expr, calc_degrees, calc_answer, calc_memory, &v, calc_error,
                      sizeof(calc_error))) {
        calc_answer = v;
        snprintf(calc_result, sizeof(calc_result), "%.*g", calc_precision16, v);
        if (!calc_history_count || strcmp(calc_history[calc_history_count - 1], calc_expr)) {
            if (calc_history_count == 16) {
                memmove(calc_history, calc_history + 1, 15 * sizeof(calc_history[0]));
                calc_history_count--;
            }
            snprintf(calc_history[calc_history_count++], sizeof(calc_history[0]), "%s", calc_expr);
        }
        char all[18000] = "";
        for (int i = 0; i < calc_history_count; i++) {
            strcat(all, calc_history[i]);
            strcat(all, "\n");
        }
        lumen_storage_write("calculator-history.txt",all,strlen(all));
    } else {
        calc_result[0] = 0;
        snprintf(calc_error, sizeof(calc_error), "%s", tr("Ausdruck oder Wertebereich prüfen."));
    }
}
static const char *calc_all0[] = {"sin", "cos", "tan", "log", "ln", "sqrt", "(",   ")",
                                  "pi",  "e",   "^",   "x",   "y",  "abs",  "ans", "%"};
static const char *calc_all1[] = {"asin",  "acos", "atan", "^2",  "^3",   "!",  "exp", "cbrt",
                                  "floor", "ceil", "1/",   "10^", "^0.5", "M+", "mem", "MC"};
static const char *calc_digits[] = {"7", "8", "9", "/", "4", "5", "6", "*",
                                    "1", "2", "3", "-", "0", ".", "%", "+"};
static const char *calc_label(const char *key) {
    if (!strcmp(key, "pi"))
        return "π";
    if (!strcmp(key, "sqrt"))
        return "√";
    if (!strcmp(key, "^2"))
        return "x²";
    if (!strcmp(key, "^3"))
        return "x³";
    if (!strcmp(key, "^0.5"))
        return "√x";
    if (!strcmp(key, "cbrt"))
        return "∛";
    if (!strcmp(key, "1/"))
        return "1/x";
    if (!strcmp(key, "10^"))
        return "10ˣ";
    if (!strcmp(key, "exp"))
        return "eˣ";
    if (!strcmp(key, "^"))
        return "xʸ";
    if (!strcmp(key, "*"))
        return "·";
    if (!strcmp(key, "/"))
        return "÷";
    if (!strcmp(key, "-"))
        return "−";
    return key;
}
static void calc_key(int x, int y, int w, int h, const char *label) {
    region(x, y, w, h);
    key_outline(x, y, w, h);
    centered(x, y, w, h, label, ui_font(UI_PRIMARY));
}
static int calc_function_left(void) {
    return calc_alt ? 66 : 28;
}
static int calc_function_pitch(void) {
    return 63;
}
static void calc_function_key(int x, int y, const char *key) {
    region(x, y, 57, 44);
    key_outline(x, y, 57, 44);
    if (!strcmp(key, "1/")) {
        centered(x, y + 3, 57, 16, "1", ui_font(UI_SECONDARY));
        rect(x + 18, y + 21, 21, 2, 0);
        centered(x, y + 25, 57, 16, "x", ui_font(UI_SECONDARY));
        return;
    }
    const char *base = NULL, *power = NULL;
    if (!strcmp(key, "10^")) {
        base = "10";
        power = "x";
    } else if (!strcmp(key, "exp")) {
        base = "e";
        power = "x";
    } else if (!strcmp(key, "^2")) {
        base = "x";
        power = "2";
    } else if (!strcmp(key, "^3")) {
        base = "x";
        power = "3";
    } else if (!strcmp(key, "^")) {
        base = "x";
        power = "y";
    }
    if (base) {
        int bw = (int)strlen(base)*12, pw = 12,
            left = x + (57 - bw - pw - 2) / 2;
        label_at(left, y + 14, bw + 2, 24, base, ui_font(UI_BODY));
        label_at(left + bw + 2, y + 4, pw + 2, 20, power, ui_font(UI_SECONDARY));
        return;
    }
    centered(x, y, 57, 44, calc_label(key), ui_font(UI_BODY));
}
static void app_calculator(void) {
    calc_alt = !!calc_alt;
    heading_actions("Rechner", 3);
    icon_button(408, 74, IC_GRAPH);
    icon_button(468, 74, IC_HISTORY);
    icon_button(528, 74, IC_SETTINGS);

    key_outline(28, 132, 544, 216);
    key_outline(28, 361, 498, 52);
    region(536, 365, 36, 44);
    icon(532, 365, IC_DELETE);
    lumen_editor_line(43,361,468,52,calc_expr,calc_cursor);
    if (calc_memory != 0)
        drawtext(525, 145, "M", ui_font(UI_SECONDARY), 30);
    lumen_formula(43,140,514,150,calc_expr);
    if (*calc_error)
        wrapped(43, 285, calc_error, ui_font(UI_BODY), 510, 2);
    else {
        const char *value = *calc_result ? calc_result : "0";
        lumen_text(43,288,514,54,value,PAINT_DISPLAY,0,0);
    }
    const char **top = calc_alt ? calc_all1 : calc_all0;
    icon_button(calc_alt ? 20 : 536, 454, calc_alt ? IC_PREV : IC_NEXT);
    for (int i = 0; i < 16; i++)
        calc_function_key(calc_function_left() + (i % 8) * calc_function_pitch(),
                          429 + (i / 8) * 51, top[i]);
    for (int i = 0; i < 16; i++)
        calc_key(28 + (i % 4) * 108, 540 + (i / 4) * 62, 100, 56, calc_label(calc_digits[i]));
    calc_key(460, 540, 112, 56, "±");
    calc_key(460, 602, 112, 56, "AC");
    filled_button(460, 664, 112, 118, "=", ui_font(UI_PRIMARY));
}
static void app_calc_history(void) {
    heading("Rechenverlauf");
    for (int r = 0; r < 8; r++) {
        int i = calc_history_count - 1 - (extra_page * 8 + r);
        if (i < 0)
            break;
        int y = 145 + r * 64;
        lumen_wrapped(31,y+7,540,54,calc_history[i],PAINT_BODY,2);
        ui_paint_hitbox(28,y,544,64);
        ui_paint_line(28,y+63,572,y+63,1,185);
    }
    lumen_pager(679,extra_page, (calc_history_count + 7) / 8);
}
