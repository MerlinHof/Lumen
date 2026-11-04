#include "api.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    const char *p;
    int deg, depth, failed;
    double ans, mem, x, y;
    int coordinates;
} Parser;
static void ws(Parser *p) {
    while (isspace((unsigned char)*p->p))
        p->p++;
}
static double expression(Parser *p);
static double unary(Parser *p);
static double primary(Parser *p) {
    ws(p);
    if (++p->depth > 64) {
        p->failed = 1;
        p->depth--;
        return 0;
    }
    double v = 0;
    if (*p->p == '(') {
        p->p++;
        v = expression(p);
        ws(p);
        if (*p->p != ')')
            p->failed = 1;
        else
            p->p++;
    } else if (isdigit((unsigned char)*p->p) || *p->p == '.') {
        char *end;
        v = strtod(p->p, &end);
        if (end == p->p)
            p->failed = 1;
        p->p = end;
    } else if (isalpha((unsigned char)*p->p)) {
        char name[16];
        int n = 0;
        while (isalpha((unsigned char)*p->p)) {
            if (n < 15)
                name[n++] = (char)tolower((unsigned char)*p->p);
            p->p++;
        }
        name[n] = 0;
        if (!strcmp(name, "pi"))
            v = 3.14159265358979323846;
        else if (!strcmp(name, "e"))
            v = 2.7182818284590452354;
        else if (!strcmp(name, "ans"))
            v = p->ans;
        else if (p->coordinates && !strcmp(name, "x"))
            v = p->x;
        else if (p->coordinates && !strcmp(name, "y"))
            v = p->y;
        else if (!strcmp(name, "mem"))
            v = p->mem;
        else {
            ws(p);
            if (*p->p != '(')
                p->failed = 1;
            else {
                p->p++;
                double x = expression(p);
                ws(p);
                if (*p->p != ')')
                    p->failed = 1;
                else
                    p->p++;
                double a = p->deg ? x * 3.14159265358979323846 / 180 : x;
                if (!strcmp(name, "sin"))
                    v = sin(a);
                else if (!strcmp(name, "cos"))
                    v = cos(a);
                else if (!strcmp(name, "tan")) {
                    if (fabs(cos(a)) < 1e-14)
                        p->failed = 1;
                    else
                        v = tan(a);
                } else if (!strcmp(name, "asin"))
                    v = asin(x) * (p->deg ? 180 / 3.14159265358979323846 : 1);
                else if (!strcmp(name, "acos"))
                    v = acos(x) * (p->deg ? 180 / 3.14159265358979323846 : 1);
                else if (!strcmp(name, "atan"))
                    v = atan(x) * (p->deg ? 180 / 3.14159265358979323846 : 1);
                else if (!strcmp(name, "sqrt"))
                    v = sqrt(x);
                else if (!strcmp(name, "cbrt"))
                    v = cbrt(x);
                else if (!strcmp(name, "log"))
                    v = log10(x);
                else if (!strcmp(name, "ln"))
                    v = log(x);
                else if (!strcmp(name, "exp"))
                    v = exp(x);
                else if (!strcmp(name, "abs"))
                    v = fabs(x);
                else if (!strcmp(name, "floor"))
                    v = floor(x);
                else if (!strcmp(name, "ceil"))
                    v = ceil(x);
                else
                    p->failed = 1;
            }
        }
    } else {
        p->failed = 1;
        if (*p->p)
            p->p++;
    }
    p->depth--;
    return v;
}
static double postfix(Parser *p) {
    double v = primary(p);
    ws(p);
    while (*p->p == '!' || *p->p == '%') {
        if (*p->p++ == '%')
            v /= 100;
        else {
            if (v < 0 || v > 170 || v != floor(v)) {
                p->failed = 1;
                return 0;
            }
            double f = 1;
            for (int i = 2; i <= (int)v; i++)
                f *= i;
            v = f;
        }
        ws(p);
    }
    return v;
}
static double power(Parser *p) {
    double v = postfix(p);
    ws(p);
    if (*p->p == '^') {
        p->p++;
        v = pow(v, unary(p));
    }
    return v;
}
static double unary(Parser *p) {
    ws(p);
    if (++p->depth > 64) {
        p->failed = 1;
        p->depth--;
        return 0;
    }
    double v;
    if (*p->p == '+' || *p->p == '-') {
        int neg = *p->p++ == '-';
        v = unary(p);
        if (neg)
            v = -v;
    } else
        v = power(p);
    p->depth--;
    return v;
}
static double product(Parser *p) {
    double v = unary(p);
    ws(p);
    while (!p->failed) {
        int op = *p->p;
        if (op == '*' || op == '/') {
            p->p++;
            double w = unary(p);
            if (op == '/') {
                if (w == 0)
                    p->failed = 1;
                else
                    v /= w;
            } else
                v *= w;
        } else if (*p->p == '(' || isalpha((unsigned char)*p->p)) {
            v *= unary(p);
        } else
            break;
        ws(p);
    }
    return v;
}
static double expression(Parser *p) {
    double v = product(p);
    ws(p);
    while (!p->failed && (*p->p == '+' || *p->p == '-')) {
        int op = *p->p++;
        double w = product(p);
        v = op == '+' ? v + w : v - w;
        ws(p);
    }
    return v;
}
static int evaluate(const char *s, int deg, double ans, double mem, double x, double y,
                    int coordinates, double *out, char *error, int cap) {
    if (!s || strlen(s) > 1023) {
        snprintf(error, cap, "Expression too long");
        return 0;
    }
    Parser p = {
        .p = s, .deg = deg, .ans = ans, .mem = mem, .x = x, .y = y, .coordinates = coordinates};
    double v = expression(&p);
    ws(&p);
    if (p.failed || *p.p || !isfinite(v)) {
        snprintf(error, cap, "Check expression / domain");
        return 0;
    }
    *out = v;
    if (cap)
        error[0] = 0;
    return 1;
}

int calc_evaluate(const char *s, int d, double a, double m, double *out, char *err, int cap) {
    return evaluate(s, d, a, m, 0, 0, 0, out, err, cap);
}
int calc_evaluate_xy(const char *s, int d, double a, double m, double x, double y, double *out,
                     char *err, int cap) {
    return evaluate(s, d, a, m, x, y, 1, out, err, cap);
}
