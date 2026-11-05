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
        extra_write_text("calculator-history.txt", all);
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
static void calc_math(char *out, int cap) {
    const char *p = calc_expr;
    int n = 0;
    while (*p && n < cap - 8) {
        const char *replacement = NULL;
        int count = 1;
        if (!strncmp(p, "sqrt", 4)) {
            replacement = "√";
            count = 4;
        } else if (!strncmp(p, "pi", 2)) {
            replacement = "π";
            count = 2;
        } else if (!strncmp(p, "^2", 2) && !isdigit((unsigned char)p[2]) && p[2] != '.') {
            replacement = "²";
            count = 2;
        } else if (*p == '*')
            replacement = "·";
        else if (*p == '/')
            replacement = "÷";
        else if (*p == '-')
            replacement = "−";
        if (replacement) {
            int len = strlen(replacement);
            memcpy(out + n, replacement, len);
            n += len;
            p += count;
        } else
            out[n++] = *p++;
    }
    out[n] = 0;
}
#include "formula.h"
static void primary_key(int x, int y, int w, int h, const char *label) {
    action_button(x,y,w,h,label);
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
        int bw = textwidth_raw(base, ui_font(UI_BODY)), pw = textwidth_raw(power, ui_font(UI_SECONDARY)),
            left = x + (57 - bw - pw - 2) / 2;
        label_at(left, y + 14, bw + 2, 24, base, ui_font(UI_BODY));
        label_at(left + bw + 2, y + 4, pw + 2, 20, power, ui_font(UI_SECONDARY));
        return;
    }
    centered(x, y, 57, 44, calc_label(key), ui_font(UI_BODY));
}
static int calc_input_start(void) {
    calc_cursor = text_boundary(calc_expr, calc_cursor < 0 ? 0 : (size_t)calc_cursor);
    return text_visible_start(calc_expr, calc_cursor, ui_font(UI_BODY), 468, 0);
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
    int start = calc_input_start();
    char shown[1024];
    snprintf(shown, sizeof(shown), "%s", calc_expr + start);
    int ih, it = ink_top(*shown ? shown : "Ag", ui_font(UI_BODY), &ih);
    int input_y = 361 + (52 - ih) / 2 - it;
    drawtext_raw(43, input_y, shown, ui_font(UI_BODY), 468);
    char prefix[1024];
    snprintf(prefix, sizeof(prefix), "%.*s", calc_cursor - start, calc_expr + start);
    int cx = 43 + textwidth(prefix, ui_font(UI_BODY));
    if (cx < 516 && editor_ui.cursor_visible) {
        text_caret(cx, input_y, ui_font(UI_BODY));
    }
    if (calc_memory != 0)
        drawtext(525, 145, "M", ui_font(UI_SECONDARY), 30);
    math_formula(43, 140, 514, 150);
    if (*calc_error)
        wrapped(43, 285, calc_error, ui_font(UI_BODY), 510, 2);
    else {
        const char *value = *calc_result ? calc_result : "0";
        int w = textwidth(value, ui_font(UI_DISPLAY));
        drawtext_raw(w < 510 ? 557 - w : 43, 288, value, ui_font(UI_DISPLAY), 514);
    }
    const char **top = calc_alt ? calc_all1 : calc_all0;
    icon_button(calc_alt ? 20 : 536, 454, calc_alt ? IC_PREV : IC_NEXT);
    for (int i = 0; i < 16; i++)
        calc_function_key(calc_function_left() + (i % 8) * calc_function_pitch(),
                          429 + (i / 8) * 51, top[i]);
    for (int i = 0; i < 16; i++)
        calc_key(28 + (i % 4) * 108, 540 + (i / 4) * 62, 100, 56, calc_label(calc_digits[i]));
    calc_key(468, 540, 104, 56, "±");
    calc_key(468, 602, 104, 56, "AC");
    primary_key(468, 664, 104, 118, "=");
}
static void app_calc_history(void) {
    heading("Rechenverlauf");
    for (int r = 0; r < 8; r++) {
        int i = calc_history_count - 1 - (extra_page * 8 + r);
        if (i < 0)
            break;
        int y = 145 + r * 64;
        list_row(28, y, 544, 64, -1, 0, calc_history[i], NULL, ui_font(UI_BODY), 2);
    }
    pager(679, &extra_page, (calc_history_count + 7) / 8);
}
