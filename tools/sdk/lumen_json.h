#ifndef LUMEN_JSON_H
#define LUMEN_JSON_H
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
/* Bounded JSON tree. Object lookups never traverse into sibling objects. */
typedef struct {
    int start, end, next, count, type;
} JsonToken;
typedef struct {
    const char *text;
    JsonToken *tokens;
    int count, capacity;
} Json;
int json_parse(Json *j, const char *text, JsonToken *tokens, int cap);
int json_get(const Json *j, int object, const char *key);
int json_at(const Json *j, int array, int index);
void json_string(const Json *j, int token, char *out, int cap);
double json_number(const Json *j, int token);

#ifdef LUMEN_JSON_IMPLEMENTATION
static int json_token(Json *j, int *p, int depth) {
    if (depth > 64 || j->count >= j->capacity)
        return -1;
    const char *s = j->text;
    while (isspace((unsigned char)s[*p]))
        ++*p;
    int i = j->count++;
    JsonToken *t = &j->tokens[i];
    *t = (JsonToken){.start = *p, .type = s[*p]};
    if (s[*p] == '{' || s[*p] == '[') {
        char close = s[(*p)++] == '{' ? '}' : ']';
        while (1) {
            while (isspace((unsigned char)s[*p]))
                ++*p;
            if (s[*p] == close) {
                ++*p;
                break;
            }
            if (t->count) {
                if (s[*p] != ',')
                    return -1;
                ++*p;
            }
            if (t->type == '{') {
                while (isspace((unsigned char)s[*p]))
                    ++*p;
                if (s[*p] != '"' || json_token(j, p, depth + 1) < 0)
                    return -1;
                while (isspace((unsigned char)s[*p]))
                    ++*p;
                if (s[(*p)++] != ':')
                    return -1;
            }
            if (json_token(j, p, depth + 1) < 0)
                return -1;
            t->count++;
        }
    } else if (s[*p] == '"') {
        ++*p;
        while (s[*p] && s[*p] != '"') {
            if ((unsigned char)s[*p] < 32)
                return -1;
            if (s[*p] == '\\') {
                ++*p;
                if (!s[*p])
                    return -1;
            }
            ++*p;
        }
        if (s[(*p)++] != '"')
            return -1;
    } else {
        int start = *p;
        while (s[*p] && !strchr(",]} \t\n\r", s[*p]))
            ++*p;
        if (start == *p)
            return -1;
    }
    t->end = *p;
    t->next = j->count;
    return i;
}
int json_parse(Json *j, const char *s, JsonToken *t, int cap) {
    *j = (Json){s, t, 0, cap};
    int p = 0;
    if (json_token(j, &p, 0) < 0)
        return 0;
    while (isspace((unsigned char)s[p]))
        p++;
    return !s[p];
}
void json_string(const Json *j, int i, char *out, int cap) {
    if (cap < 1)
        return;
    out[0] = 0;
    if (i < 0 || i >= j->count)
        return;
    JsonToken t = j->tokens[i];
    int p = t.start + (t.type == '"'), end = t.end - (t.type == '"'), n = 0;
    while (p < end && n < cap - 1) {
        unsigned char c = j->text[p++];
        if (c == '\\' && p < end) {
            c = j->text[p++];
            if (c == 'n')
                c = '\n';
            else if (c == 't')
                c = ' ';
            else if (c == 'r')
                continue;
            else if (c == 'u' && p + 4 <= end) {
                char hex[5];
                memcpy(hex, j->text + p, 4);
                hex[4] = 0;
                unsigned v = (unsigned)strtoul(hex, NULL, 16);
                p += 4;
                if (v < 128)
                    c = v;
                else {
                    if (v < 2048 && n + 2 < cap) {
                        out[n++] = 0xc0 | (v >> 6);
                        c = 0x80 | (v & 63);
                    } else if (v >= 2048 && n + 3 < cap && !(v >= 0xd800 && v <= 0xdfff)) {
                        out[n++] = 0xe0 | (v >> 12);
                        out[n++] = 0x80 | ((v >> 6) & 63);
                        c = 0x80 | (v & 63);
                    } else
                        c = '?';
                }
            }
        }
        if (c)
            out[n++] = c;
    }
    out[n] = 0;
}
int json_get(const Json *j, int i, const char *key) {
    if (i < 0 || i >= j->count || j->tokens[i].type != '{')
        return -1;
    for (int k = i + 1; k < j->tokens[i].next;) {
        char name[128];
        json_string(j, k, name, sizeof(name));
        int v = k + 1;
        if (!strcmp(name, key))
            return v;
        k = j->tokens[v].next;
    }
    return -1;
}
int json_at(const Json *j, int i, int index) {
    if (i < 0 || i >= j->count || j->tokens[i].type != '[' || index < 0)
        return -1;
    int k = i + 1;
    while (index-- && k < j->tokens[i].next)
        k = j->tokens[k].next;
    return k < j->tokens[i].next ? k : -1;
}
double json_number(const Json *j, int i) {
    if (i < 0 || i >= j->count)
        return NAN;
    char b[64];
    json_string(j, i, b, sizeof(b));
    char *end;
    double v = strtod(b, &end);
    return *end || end == b ? NAN : v;
}
#endif
#endif
