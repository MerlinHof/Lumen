/* Independent, bounded native adaptation of the text-calculator principle.
 * No upstream source is incorporated. */
#include "../scratch.h"
#include "../calc.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    double v;
    int d[4];
    int count;
    double lo, hi;
} Q;
typedef struct {
    const char *name;
    double scale;
    int l, t, m, c;
} Unit;
static const Unit units[] = {{"km", 1000, 1, 0, 0, 0},
                             {"cm", .01, 1, 0, 0, 0},
                             {"mm", .001, 1, 0, 0, 0},
                             {"m", 1, 1, 0, 0, 0},
                             {"miles", 1609.344, 1, 0, 0, 0},
                             {"mile", 1609.344, 1, 0, 0, 0},
                             {"mi", 1609.344, 1, 0, 0, 0},
                             {"ft", .3048, 1, 0, 0, 0},
                             {"in", .0254, 1, 0, 0, 0},
                             {"hours", 3600, 0, 1, 0, 0},
                             {"hour", 3600, 0, 1, 0, 0},
                             {"h", 3600, 0, 1, 0, 0},
                             {"minutes", 60, 0, 1, 0, 0},
                             {"min", 60, 0, 1, 0, 0},
                             {"seconds", 1, 0, 1, 0, 0},
                             {"s", 1, 0, 1, 0, 0},
                             {"days", 86400, 0, 1, 0, 0},
                             {"day", 86400, 0, 1, 0, 0},
                             {"kg", 1, 0, 0, 1, 0},
                             {"g", .001, 0, 0, 1, 0},
                             {"lb", .45359237, 0, 0, 1, 0},
                             {"EUR", 1, 0, 0, 0, 1},
                             {"€", 1, 0, 0, 0, 1},
                             {"mph", .44704, 1, -1, 0, 0}};
typedef struct {
    char name[48];
    Q q;
} Var;
typedef struct {
    const char *p;
    Var *vars;
    int n, bad, depth;
    Q previous;
} P;
static Q scalar(double v) {
    Q q = {.v = v};
    return q;
}
static void ws(P *p) {
    while (isspace((unsigned char)*p->p))
        p->p++;
}
static int same(Q a, Q b) { return !memcmp(a.d, b.d, sizeof(a.d)); }
static int plain(Q q) { return !q.d[0] && !q.d[1] && !q.d[2] && !q.d[3] && !q.count; }
static int namechar(int c) { return isalnum((unsigned char)c) || c == '_'; }
static Q expr(P *p);
static Q factor(P *p) {
    ws(p);
    if (++p->depth > 48) {
        p->bad = 1;
        p->depth--;
        return scalar(0);
    }
    Q q = scalar(0);
    if (*p->p == '+' || *p->p == '-') {
        int neg = *p->p++ == '-';
        q = factor(p);
        if (neg)
            q.v = -q.v;
    } else if (*p->p == '(') {
        p->p++;
        q = expr(p);
        ws(p);
        if (*p->p == ')')
            p->p++;
        else
            p->bad = 1;
    } else if (*p->p == '[') {
        p->p++;
        Q total = scalar(0);
        int n = 0;
        double lo = 0, hi = 0;
        do {
            Q v = expr(p);
            if (v.count || (n && !same(v, total)))
                p->bad = 1;
            if (!n) {
                total = v;
                lo = hi = v.v;
            } else
                total.v += v.v;
            if (v.v < lo)
                lo = v.v;
            if (v.v > hi)
                hi = v.v;
            n++;
            ws(p);
            if (*p->p != ',')
                break;
            p->p++;
        } while (n < 64 && !p->bad);
        if (*p->p == ']')
            p->p++;
        else
            p->bad = 1;
        total.count = n;
        total.lo = lo;
        total.hi = hi;
        q = total;
    } else if (isdigit((unsigned char)*p->p) || *p->p == '.') {
        char *end;
        q.v = strtod(p->p, &end);
        if (end == p->p)
            p->bad = 1;
        p->p = end;
    } else if (isalpha((unsigned char)*p->p) || *p->p == '_') {
        char name[64];
        int n = 0;
        while (namechar(*p->p)) {
            if (n < 63)
                name[n++] = *p->p;
            p->p++;
        }
        name[n] = 0;
        ws(p);
        if (*p->p == '(') {
            p->p++;
            Q v = expr(p);
            ws(p);
            if (*p->p == ')')
                p->p++;
            else
                p->bad = 1;
            if (!strcmp(name, "sum") || !strcmp(name, "avg") || !strcmp(name, "min") ||
                !strcmp(name, "max")) {
                q = v;
                if (!strcmp(name, "avg") && v.count)
                    q.v /= v.count;
                if (!strcmp(name, "min") && v.count)
                    q.v = v.lo;
                if (!strcmp(name, "max") && v.count)
                    q.v = v.hi;
                q.count = 0;
            } else {
                char temp[128], err[80];
                snprintf(temp, sizeof(temp), "%s(%.17g)", name, v.v);
                double result = 0;
                if (!plain(v) ||
                    !calc_evaluate(temp, 0, p->previous.v, 0, &result, err, sizeof(err)))
                    p->bad = 1;
                q = scalar(result);
            }
        } else if (!strcmp(name, "sum") || !strcmp(name, "avg") || !strcmp(name, "min") ||
                   !strcmp(name, "max")) {
            q = factor(p);
            if (!strcmp(name, "avg") && q.count)
                q.v /= q.count;
            if (!strcmp(name, "min") && q.count)
                q.v = q.lo;
            if (!strcmp(name, "max") && q.count)
                q.v = q.hi;
            q.count = 0;
        } else if (!strcmp(name, "pi"))
            q.v = 3.141592653589793;
        else if (!strcmp(name, "e"))
            q.v = 2.718281828459045;
        else if (!strcmp(name, "ans"))
            q = p->previous;
        else {
            int found = 0;
            for (int i = p->n - 1; i >= 0; i--)
                if (!strcmp(name, p->vars[i].name)) {
                    q = p->vars[i].q;
                    found = 1;
                    break;
                }
            if (!found) {
                for (unsigned i = 0; i < sizeof(units) / sizeof(*units); i++)
                    if (!strcmp(name, units[i].name)) {
                        q.v = units[i].scale;
                        q.d[0] = units[i].l;
                        q.d[1] = units[i].t;
                        q.d[2] = units[i].m;
                        q.d[3] = units[i].c;
                        found = 1;
                        break;
                    }
            }
            if (!found)
                p->bad = 1;
        }
    } else
        p->bad = 1;
    ws(p);
    for (unsigned i = 0; i < sizeof(units) / sizeof(*units); i++) {
        const Unit *u = &units[i];
        size_t n = strlen(u->name);
        if (!strncmp(p->p, u->name, n) && !namechar(p->p[n])) {
            q.v *= u->scale;
            q.d[0] += u->l;
            q.d[1] += u->t;
            q.d[2] += u->m;
            q.d[3] += u->c;
            p->p += n;
            break;
        }
    }
    ws(p);
    if (*p->p == '^') {
        p->p++;
        Q b = factor(p);
        if (!plain(b) || q.count || b.v < -16 || b.v > 16) {
            p->bad = 1;
        } else {
            for (int i = 0; i < 4; i++)
                if (q.d[i]) {
                    if (b.v != floor(b.v))
                        p->bad = 1;
                    q.d[i] *= (int)b.v;
                }
            q.v = pow(q.v, b.v);
        }
    }
    ws(p);
    if (*p->p == '%') {
        p->p++;
        q.v /= 100;
    }
    p->depth--;
    return q;
}
static Q product(P *p) {
    Q a = factor(p);
    ws(p);
    while (!p->bad && (*p->p == '*' || *p->p == '/')) {
        int c = *p->p++;
        Q b = factor(p);
        if (a.count || b.count || (c == '/' && b.v == 0)) {
            p->bad = 1;
            break;
        }
        a.v = c == '*' ? a.v * b.v : a.v / b.v;
        for (int i = 0; i < 4; i++)
            a.d[i] += c == '*' ? b.d[i] : -b.d[i];
        ws(p);
    }
    return a;
}
static Q expr(P *p) {
    Q a = product(p);
    ws(p);
    while (!p->bad && (*p->p == '+' || *p->p == '-')) {
        int c = *p->p++;
        Q b = product(p);
        if (!same(a, b) || a.count || b.count) {
            p->bad = 1;
            break;
        }
        a.v += c == '+' ? b.v : -b.v;
        ws(p);
    }
    return a;
}
static void format(Q q, const char *unit, double scale, char *out, int cap) {
    if (q.count) {
        snprintf(out, cap, "[%d]", q.count);
        return;
    }
    if(!unit&&q.d[0]==1&&!q.d[1]&&!q.d[2]&&!q.d[3]&&fabs(q.v)>=1000){unit="km";scale=1000;}
    if(!unit&&!q.d[0]&&q.d[1]==1&&!q.d[2]&&!q.d[3]&&fabs(q.v)>=3600){unit="h";scale=3600;}
    char suffix[48] = "";
    if (unit)
        snprintf(suffix, sizeof(suffix), " %s", unit);
    else {
        const char *names[] = {"m", "s", "kg", "EUR"};
        for (int i = 0; i < 4; i++)
            if (q.d[i]) {
                char b[20];
                snprintf(b, sizeof(b), q.d[i] == 1 ? " %s" : " %s^%d", names[i], q.d[i]);
                strncat(suffix, b, sizeof(suffix) - strlen(suffix) - 1);
            }
    }
    snprintf(out, cap, "%.10g%s", q.v / scale, suffix);
}
int scratch_evaluate(const char *text, ScratchLine *lines, int capacity) {
    Var vars[128];
    int nvars = 0, n = 0;
    Q previous = scalar(0);
    const char *line = text;
    while (n < capacity) {
        const char *end = strchr(line, '\n');
        if (!end)
            end = line + strlen(line);
        ScratchLine *r = &lines[n++];
        memset(r, 0, sizeof(*r));
        r->start = line - text;
        r->length = end - line;
        char buf[1024];
        if (r->length > 1023) {
            r->state = -1;
            snprintf(r->result, sizeof(r->result), "...");
            goto next;
        }
        memcpy(buf, line, r->length);
        buf[r->length] = 0;
        char *s = buf;
        while (isspace((unsigned char)*s))
            s++;
        char *comment = strchr(s, '#');
        if (comment)
            *comment = 0;
        if (!*s)
            goto next;
        char name[48] = "";
        char *eq = strchr(s, '=');
        if (eq) {
            char *tail = eq;
            while (tail > s && isspace((unsigned char)tail[-1]))
                tail--;
            int len = tail - s;
            if (len <= 0 || len >= 48 || !(isalpha((unsigned char)*s) || *s == '_'))
                goto bad;
            for (int i = 0; i < len; i++)
                if (!namechar(s[i]))
                    goto bad;
            memcpy(name, s, len);
            name[len] = 0;
            s = eq + 1;
        }
        char *to = strstr(s, " to ");
        char target[64] = "";
        if (to) {
            *to = 0;
            snprintf(target, sizeof(target), "%s", to + 4);
            size_t k = strlen(target);
            while (k && isspace((unsigned char)target[k - 1]))
                target[--k] = 0;
        }
        char continuation[1100];
        while (isspace((unsigned char)*s))
            s++;
        P p = {.p = s, .vars = vars, .n = nvars, .previous = previous};
        if (!eq && (*s == '*' || *s == '/' || *s == '+' || *s == '-')) {
            snprintf(continuation, sizeof(continuation), "ans %s", s);
            p.p = continuation;
        }
        Q q = expr(&p);
        ws(&p);
        if (p.bad || *p.p || !isfinite(q.v))
            goto bad;
        if (*name) {
            int i;
            for (i = 0; i < nvars; i++)
                if (!strcmp(vars[i].name, name))
                    break;
            if (i == nvars) {
                if (nvars >= 128)
                    goto bad;
                nvars++;
            }
            snprintf(vars[i].name, sizeof(vars[i].name), "%s", name);
            vars[i].q = q;
        }
        if (*target) {
            P u = {.p = target, .vars = vars, .n = nvars};
            Q unit = expr(&u);
            ws(&u);
            if (u.bad || *u.p || unit.count || !same(unit, q) || !unit.v)
                goto bad;
            format(q, target, unit.v, r->result, sizeof(r->result));
        } else
            format(q, NULL, 1, r->result, sizeof(r->result));
        r->state = 1;
        previous = q;
        goto next;
    bad:
        r->state = -1;
        snprintf(r->result, sizeof(r->result), "?");
    next:
        if (!*end)
            break;
        line = end + 1;
    }
    return n;
}
