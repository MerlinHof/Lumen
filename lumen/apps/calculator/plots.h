/* Shared plot frame for sensor history and scientific functions. */
static double plot_bounds[4] = {-10, 10, -10, 10};
static int ha_trace_x=-1;
static int plot_trace, plot_last_error, ha_plot_hours = 24;

static double nice_step(double span) {
    double raw = span / 4, base = pow(10, floor(log10(raw))), q = raw / base;
    return (q <= 1 ? 1 : q <= 2 ? 2 : q <= 5 ? 5 : 10) * base;
}
static void graph_frame(double xmin, double xmax, double ymin, double ymax, int temporal) {
    int x = 66, y = 220, w = 492, h = 426;
    key_outline(x, y, w, h);
    double dx = nice_step(xmax - xmin), dy = nice_step(ymax - ymin);
    char b[48];
    for (double v = ceil(ymin / dy) * dy, k = 0; k < 32 && v <= ymax + dy * .001; v += dy, k++) {
        int py = y + h - 1 - (v - ymin) / (ymax - ymin) * (h - 2);
        if (py <= y || py >= y + h)
            continue;
        rect(x + 2, py, w - 4, 1, 220);
        snprintf(b, sizeof(b), "%.4g", fabs(v) < dy * .001 ? 0 : v);
        drawtext_raw(3, py - 8, b, ui_font(UI_SECONDARY), 59);
    }
    if (temporal) {
        for (int i = 0; i <= 4; i++) {
            time_t t = xmin + (xmax - xmin) * i / 4;
            struct tm tm;
            localtime_r(&t, &tm);
            strftime(b, sizeof(b), "%H:%M", &tm);
            centered(x - 20 + i * (w - 1) / 4, y + h + 6, 64, 28, b, ui_font(UI_SECONDARY));
        }
    } else {
        for (double v = ceil(xmin / dx) * dx, k = 0; k < 32 && v <= xmax + dx * .001;
             v += dx, k++) {
            int px = x + (v - xmin) / (xmax - xmin) * (w - 2);
            if (px <= x || px >= x + w)
                continue;
            rect(px, y + 2, 1, h - 4, 220);
            snprintf(b, sizeof(b), "%.4g", fabs(v) < dx * .001 ? 0 : v);
            centered(px - 35, y + h + 6, 70, 28, b, ui_font(UI_SECONDARY));
        }
    }
    if (ymin < 0 && ymax > 0) {
        int py = y + h - (0 - ymin) / (ymax - ymin) * h;
        rect(x, py, w, 2, 160);
    }
    if (!temporal && xmin < 0 && xmax > 0) {
        int px = x + (0 - xmin) / (xmax - xmin) * w;
        rect(px, y, 2, h, 160);
    }
}
static void graph_segment(double x0, double y0, double x1, double y1, double xmin, double xmax,
                          double ymin, double ymax) {
    // Liang-Barsky clip in data coordinates, before converting to pixel integers.
    double p[] = {x0 - x1, x1 - x0, y0 - y1, y1 - y0},
           q[] = {x0 - xmin, xmax - x0, y0 - ymin, ymax - y0}, a = 0, b = 1;
    for (int i = 0; i < 4; i++) {
        if (fabs(p[i]) < 1e-15) {
            if (q[i] < 0)
                return;
        } else {
            double t = q[i] / p[i];
            if (p[i] < 0) {
                if (t > b)
                    return;
                if (t > a)
                    a = t;
            } else {
                if (t < a)
                    return;
                if (t < b)
                    b = t;
            }
        }
    }
    double ax = x0 + a * (x1 - x0), ay = y0 + a * (y1 - y0), bx = x0 + b * (x1 - x0),
           by = y0 + b * (y1 - y0);
    stroke(67 + (ax - xmin) / (xmax - xmin) * 489, 644 - (ay - ymin) / (ymax - ymin) * 422,
           67 + (bx - xmin) / (xmax - xmin) * 489, 644 - (by - ymin) / (ymax - ymin) * 422, 2);
}
static const char *plot_formula(void) {
    const char *s = calc_expr;
    while (*s == ' ')
        s++;
    const char *eq = strchr(s, '=');
    if (eq) {
        if ((s[0] == 'y' && eq - s <= 2) || !strncmp(s, "f(x)", 4))
            s = eq + 1;
        else
            return NULL;
    }
    return s;
}
static void calculator_graph(void) {
    heading("Funktionsgraph");
    button_h(28, 128, 544, 50, *calc_expr ? calc_expr : "y = sin(x)", 0);
    const char *f = plot_formula();
    double xmin = plot_bounds[0], xmax = plot_bounds[1], ymin = plot_bounds[2],
           ymax = plot_bounds[3];
    graph_frame(xmin, xmax, ymin, ymax, 0);
    int last = 0, any = 0;
    double lx = 0, ly = 0;
    char err[80];
    for (int i = 0; i <= 600; i++) {
        double x = xmin + (xmax - xmin) * i / 600, y;
        int ok =
            f && *f &&
            calc_evaluate_xy(f, calc_degrees, calc_answer, calc_memory, x, 0, &y, err, sizeof(err));
        if (ok) {
            any = 1;
            if (last && fabs(y - ly) < (ymax - ymin) * .85)
                graph_segment(lx, ly, x, y, xmin, xmax, ymin, ymax);
            lx = x;
            ly = y;
        }
        last = ok;
    }
    if (!any)
        centered(80, 370, 464, 70, "Ausdruck oder Wertebereich prüfen.", ui_font(UI_BODY));
    button_h(28, 712, 150, 52, "Bereich", 0);
    button_h(194, 712, 95, 52, "−", 0);
    button_h(305, 712, 95, 52, "+", 0);
    button_h(416, 712, 156, 52, "Auto Y", 0);
    if (plot_trace) {
        double x = xmin + (xmax - xmin) * (plot_trace - 1) / 489, y;
        if (f && calc_evaluate_xy(f, calc_degrees, calc_answer, calc_memory, x, 0, &y, err,
                                  sizeof(err))) {
            char b[100];
            snprintf(b, sizeof(b), "x %.6g  ·  y %.6g", x, y);
            drawtext_raw(30, 182, b, ui_font(UI_SECONDARY), 540);
        }
    }
}
static void graph_auto_y(void) {
    const char *f = plot_formula();
    double lo = INFINITY, hi = -INFINITY;
    char err[80];
    if (!f)
        return;
    for (int i = 0; i <= 300; i++) {
        double x = plot_bounds[0] + (plot_bounds[1] - plot_bounds[0]) * i / 300, y;
        if (calc_evaluate_xy(f, calc_degrees, calc_answer, calc_memory, x, 0, &y, err,
                             sizeof(err)) &&
            fabs(y) < 1e12) {
            if (y < lo)
                lo = y;
            if (y > hi)
                hi = y;
        }
    }
    if (isfinite(lo) && isfinite(hi)) {
        double pad = (hi - lo) * .1;
        if (pad < .5)
            pad = .5;
        plot_bounds[2] = lo - pad;
        plot_bounds[3] = hi + pad;
    }
}
static void graph_range_screen(void) {
    heading("Graph-Bereich");
    const char *names[] = {"X min", "X max", "Y min", "Y max"};
    char b[80];
    for (int i = 0; i < 4; i++) {
        drawtext(30, 175 + i * 108, names[i], ui_font(UI_PRIMARY), 170);
        snprintf(b, sizeof(b), "%.9g", plot_bounds[i]);
        button_h(220, 165 + i * 108, 352, 60, b, 0);
    }
    button_h(28, 650, 544, 56, "Fertig", 0);
}
static void sensor_graph_screen(void) {
    HAEntity *e = NULL;
    for (int i = 0; i < ha_count; i++)
        if (!strcmp(ha_entities[i].id, ha_plot_id))
            e = &ha_entities[i];
    heading("Sensorverlauf");
    if (!e) {
        wrapped(30, 170, "Gerät nicht verfügbar.", ui_font(UI_TITLE), 540, 3);
        return;
    }
    drawtext_raw(30, 128, e->name, ui_font(UI_PRIMARY), 540);
    char b[120];
    snprintf(b, sizeof(b), "%s %s", e->state, e->unit);
    drawtext_raw(30, 171, b, ui_font(UI_TITLE), 540);
    HAHistory *h = NULL;
    for (int i = 0; i < 8; i++)
        if (!strcmp(ha_history[i].id, e->id))
            h = &ha_history[i];
    if (!h || !h->count) {
        centered(28, 310, 544, 80,
                 service_history_busy() ? "Verlauf wird geladen …" : "Kein Verlauf", ui_font(UI_BODY));
        return;
    }
    double end = time(NULL), start = end - ha_plot_hours * 3600, lo = INFINITY, hi = -INFINITY;
    for (int i = 0; i < h->count; i++)
        if (h->time[i] >= start) {
            if (h->value[i] < lo)
                lo = h->value[i];
            if (h->value[i] > hi)
                hi = h->value[i];
        }
    if (!isfinite(lo)) {
        lo = h->value[h->count - 1] - .5;
        hi = lo + 1;
    }
    double pad = (hi - lo) * .08;
    if (pad < .01)
        pad = .01;
    lo -= pad;
    hi += pad;
    graph_frame(start, end, lo, hi, 1);
    drawtext(454, 681, "Uhrzeit", ui_font(UI_SECONDARY), 100);
    drawtext_raw(3, 195, e->unit, ui_font(UI_SECONDARY), 58);
    for (int i = 1; i < h->count; i++)
        graph_segment(h->time[i - 1], h->value[i - 1], h->time[i], h->value[i], start, end, lo, hi);
    if (ha_trace_x>=66 && ha_trace_x<558) {
        time_t target=start+(end-start)*(ha_trace_x-66)/492.;
        int nearest=0;
        for (int i=1;i<h->count;i++)
            if (llabs((long long)h->time[i]-target)<llabs((long long)h->time[nearest]-target))
                nearest=i;
        int px=66+(h->time[nearest]-start)*491/(end-start);
        if (px<66) px=66;
        if (px>557) px=557;
        rect(px,221,2,424,0);
        char stamp[48];
        struct tm t;
        localtime_r(&h->time[nearest],&t);
        strftime(stamp,sizeof(stamp),"%d.%m. %H:%M",&t);
        snprintf(b,sizeof(b),"%s   %.6g %s",stamp,h->value[nearest],e->unit);
        control_surface(86,238,450,52,RADIUS_MEDIUM,255);
        centered(86,238,450,52,b,ui_font(UI_BODY));
    }
    const char *labels[] = {"1 h", "6 h", "24 h"};
    segments(28, 716, 544, 48, labels, 3, ha_plot_hours == 1 ? 0 : ha_plot_hours == 6 ? 1 : 2);
}

static int sensor_graph_input(const Input *in) {
    if (in->kind!=INPUT_TAP) return 2;
    if (in->y>=220 && in->y<646 && in->x>=66 && in->x<558) {
        ha_trace_x=in->x;
        return 1;
    }
    if (in->y>=716 && in->y<764 && in->x>=28 && in->x<572) {
        const int hours[]={1,6,24};
        ha_plot_hours=hours[(in->x-28)*3/544];
        ha_trace_x=-1;
        return 1;
    }
    return 2;
}
