#include "../src/calc.h"
#include "../src/services.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static double evaluate(const char *s, int deg) {
    double v;
    char e[128];
    assert(calc_evaluate(s, deg, 3, 5, &v, e, sizeof(e)));
    return v;
}
int main(void) {
    assert(evaluate("2+3*4", 0) == 14);
    assert(evaluate("(2+3)*4", 0) == 20);
    assert(evaluate("2^3^2", 0) == 512);
    assert(evaluate("-2^2", 0) == -4);
    assert(evaluate("2^-2", 0) == .25);
    assert(fabs(evaluate("sin(pi/3)^2+sqrt(5^2+3^2)", 0) - 6.5809518948453) < 1e-10);
    assert(fabs(evaluate("sin(30)+cos(60)", 1) - 1) < 1e-12);
    assert(evaluate("5!", 0) == 120);
    assert(evaluate("200*10%", 0) == 20);
    assert(evaluate("2(3+4)", 0) == 14);
    assert(evaluate("ans+mem", 0) == 8);
    assert(fabs(evaluate("ln(e)+log(100)+cbrt(-8)", 0) - 1) < 1e-10);
    double v;
    char e[128];
    const char *bad[] = {"1/0",  "sqrt(-1)", "log(0)",  "1+",    "((2)", "171!",
                         "2.5!", "tan(90)",  "exit(0)", "1e999", NULL};
    for (int i = 0; bad[i]; i++)
        assert(!calc_evaluate(bad[i], 1, 0, 0, &v, e, sizeof(e)));
    char deep[1024];
    memset(deep, '(', 500);
    deep[500] = '1';
    memset(deep + 501, ')', 500);
    deep[1001] = 0;
    assert(!calc_evaluate(deep, 0, 0, 0, &v, e, sizeof(e)));
    Json j;
    JsonToken t[128];
    const char *json = "{\"a\":{\"name\":\"inner\"},\"name\":\"Sch\\u00e4tzing\",\"values\":[-2.5,"
                       "3,{\"x\":true}]}";
    assert(json_parse(&j, json, t, 128));
    char s[100];
    json_string(&j, json_get(&j, 0, "name"), s, sizeof(s));
    assert(!strcmp(s, "Schätzing"));
    int a = json_get(&j, 0, "values");
    assert(json_number(&j, json_at(&j, a, 0)) == -2.5);
    assert(json_at(&j, a, 3) == -1);
    assert(json_get(&j, json_get(&j, 0, "a"), "values") == -1);
    assert(!json_parse(&j, "{\"x\":[1,2}", t, 128));
    assert(!json_parse(&j, "{\"x\":\"broken}", t, 128));
    assert(!json_parse(&j, json, t, 2));
    assert(service_url_valid("http://homeassistant.local:8123"));
    assert(service_url_valid("https://ha.example.org/base"));
    assert(!service_url_valid("file:///etc/passwd"));
    assert(!service_url_valid("http://user:pass@host"));
    assert(!service_url_valid("http://host\nHeader: inject"));
    assert(system_set_time(2028, 2, 29, 21, 4));
    assert(!system_set_time(2027, 2, 29, 21, 4));
    assert(!system_set_time(2026, 13, 1, 21, 4));
    puts("PASS: scientific precedence, radians/degrees, functions, implicit multiplication, "
         "memory, domains, recursion bounds, JSON nesting/UTF-8/bounds, HA URL validation and "
         "Gregorian date validation.");
}
