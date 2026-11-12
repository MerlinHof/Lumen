#include "i18n.h"
#define _GNU_SOURCE
#include "engine.h"
#include "library.h"
#include "services.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
static int token(Json *j, int *p, int depth) {
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
                if (s[*p] != '"' || token(j, p, depth + 1) < 0)
                    return -1;
                while (isspace((unsigned char)s[*p]))
                    ++*p;
                if (s[(*p)++] != ':')
                    return -1;
            }
            if (token(j, p, depth + 1) < 0)
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
    if (token(j, &p, 0) < 0)
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
static pid_t worker = -1, history_worker = -1;
static int history_child;
static int job;
static char error[120];
int service_busy(void) { return worker > 0; }
int service_history_busy(void) { return !history_child && history_worker > 0; }
const char *service_error(void) { return error; }
static void path(char *out, size_t n, const char *name) {
    snprintf(out, n, "%s/%s%s", data_root,
             history_child && !strncmp(name, "service-", 8) ? "history-" : "", name);
}
char *service_read(const char *name, size_t max) {
    char p[2048];
    path(p, sizeof(p), name);
    FILE *f = fopen(p, "rb");
    if (!f)
        return NULL;
    char *s = malloc(max + 1);
    if (!s) {
        fclose(f);
        return NULL;
    }
    size_t n = fread(s, 1, max, f);
    if (fgetc(f) != EOF) {
        free(s);
        s = NULL;
    } else
        s[n] = 0;
    fclose(f);
    return s;
}
int service_url_valid(const char *s) {
    if (strncmp(s, "http://", 7) && strncmp(s, "https://", 8))
        return 0;
    const char *host = strstr(s, "://") + 3;
    if (!*host || *host == '/' || strpbrk(s, "\r\n\"\\@?# ") || strlen(s) > 500)
        return 0;
    return 1;
}
static int fetch(const char *url, const char *out, const char *auth, const char *body) {
    char cfg[2048], ca[2048];
    path(cfg, sizeof(cfg), "service-private.tmp");
    FILE *f = fopen(cfg, "w");
    if (!f)
        return 0;
    chmod(cfg, 0600);
    fprintf(f, "url = \"%s\"\noutput = \"%s\"\n", url, out);
    if (auth && *auth) {
        if (strpbrk(auth, "\r\n\"\\")) {
            fclose(f);
            unlink(cfg);
            return 0;
        }
        fprintf(f, "header = \"Authorization: Bearer %s\"\n", auth);
    }
    if (body) {
        fprintf(f,
                "header = \"Content-Type: application/json\"\nrequest = \"POST\"\ndata = "
                "\"{\\\"entity_id\\\":\\\"%s\\\"}\"\n",
                body);
    }
    fclose(f);
    pid_t p = fork();
    if (p == 0) {
        int null = open("/dev/null", O_RDWR);
        dup2(null, 1);
        dup2(null, 2);
        close(null);
        snprintf(ca, sizeof(ca), "%s/../certs/ca-bundle.pem", data_root);
        if (!getenv("CURL_CA_BUNDLE") && access(ca, R_OK) == 0)
            setenv("CURL_CA_BUNDLE", ca, 1);
        execlp("curl", "curl", "--config", cfg, "--location", "--max-redirs", "5", "--proto-redir",
               "=https", "--user-agent", "LUMEN/0.12 (personal Kindle reader)", "--fail",
               "--silent", "--connect-timeout", "5", "--max-time",
               history_child       ? "30"
               : job == SERVICE_HA ? "8"
                                   : "25",
               "--max-filesize", history_child ? "8388608" : "2097152", "--proto", "=http,https",
               NULL);
        _exit(127);
    }
    if (p < 0) {
        unlink(cfg);
        return 0;
    }
    int status = 0;
    while (waitpid(p, &status, 0) < 0 && errno == EINTR) {
    }
    unlink(cfg);
    if (!(p > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
        char failure[2048];
        path(failure, sizeof(failure), "service-error-code.txt");
        char n[24];
        snprintf(n, sizeof(n), "%d", WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        atomic_write(failure, n, strlen(n));
    }
    return p > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
static int set_clock(time_t seconds) {
#ifndef LUMEN_HOST
    struct timeval now = {seconds, 0};
    if (settimeofday(&now, NULL))
        return 0;
    pid_t p = fork();
    if (!p) {
        int fd = open("/dev/null", O_RDWR);
        if (fd >= 0) {
            dup2(fd, 1);
            dup2(fd, 2);
            close(fd);
        }
        execl("/sbin/hwclock", "hwclock", "-w", "-u", NULL);
        execl("/usr/sbin/hwclock", "hwclock", "-w", "-u", NULL);
        execl("/bin/busybox", "busybox", "hwclock", "-w", "-u", NULL);
        _exit(127);
    }
    if (p > 0)
        while (waitpid(p, NULL, 0) < 0 && errno == EINTR) {
        }
    return 1;
#else
    (void)seconds;
    return 1;
#endif
}
static int ntp(void) {
    struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_DGRAM}, *a = NULL;
    if (getaddrinfo("pool.ntp.org", "123", &hints, &a))
        return 0;
    int fd = socket(AF_INET, SOCK_DGRAM, 0), ok = 0;
    if (fd >= 0) {
        struct timeval timeout = {4, 0};
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        unsigned char q[48] = {0x23}, r[48];
        struct timeval sent;
        gettimeofday(&sent, NULL);
        uint32_t tx = htonl((uint32_t)(sent.tv_sec + 2208988800ULL)),
                 fraction = htonl((uint32_t)(((uint64_t)sent.tv_usec << 32) / 1000000));
        memcpy(q + 40, &tx, 4);
        memcpy(q + 44, &fraction, 4);
        if (!connect(fd, a->ai_addr, a->ai_addrlen) && send(fd, q, 48, 0) == 48 &&
            recv(fd, r, 48, 0) == 48 && (r[0] & 7) == 4 &&
            (((r[0] >> 3) & 7) == 3 || ((r[0] >> 3) & 7) == 4) && (r[0] >> 6) != 3 && r[1] > 0 &&
            r[1] < 16 && !memcmp(r + 24, q + 40, 8)) {
            uint32_t secs;
            memcpy(&secs, r + 40, 4);
            int64_t epoch = (int64_t)ntohl(secs) - 2208988800LL;
            if (epoch < 1700000000LL)
                epoch += 4294967296LL;
            time_t t = (time_t)epoch;
            if (t > 1700000000LL && t < 4102444800LL) {
                ok = set_clock(t);
            }
        }
        close(fd);
    }
    freeaddrinfo(a);
    return ok;
}
int system_set_time(int y, int m, int d, int h, int min) {
    if (y < 2020 || y > 2099 || m < 1 || m > 12 || d < 1 || d > 31 || h < 0 || h > 23 || min < 0 ||
        min > 59)
        return 0;
    struct tm tm = {.tm_year = y - 1900,
                    .tm_mon = m - 1,
                    .tm_mday = d,
                    .tm_hour = h,
                    .tm_min = min,
                    .tm_isdst = -1};
    time_t t = mktime(&tm);
    if (tm.tm_year != y - 1900 || tm.tm_mon != m - 1 || tm.tm_mday != d)
        return 0;
    return set_clock(t);
}
void service_cancel(void) {
    if (!history_child && history_worker > 0) {
        kill(-history_worker, SIGTERM);
        kill(history_worker, SIGTERM);
        while (waitpid(history_worker, NULL, 0) < 0 && errno == EINTR) {
        }
        history_worker = -1;
    }

    if (worker > 0) {
        kill(-worker, SIGTERM);
        while (waitpid(worker, NULL, 0) < 0 && errno == EINTR) {
        }
        worker = -1;
    }
    char cfg[2048];
    path(cfg, sizeof(cfg), "service-private.tmp");
    unlink(cfg);
    if (!history_child) {
        snprintf(cfg, sizeof(cfg), "%s/history-service-private.tmp", data_root);
        unlink(cfg);
    }
}
static int history_compact(Json *j, const char *out) {
    char *result = calloc(262144, 1);
    if (!result)
        return 0;
    size_t n = 0;
    result[n++] = '[';
    for (int a = 0; a < j->tokens[0].count && a < 6; a++) {
        int array = json_at(j, 0, a), obj = json_at(j, array, 0);
        if (obj < 0)
            continue;
        char id[160];
        json_string(j, json_get(j, obj, "entity_id"), id, sizeof(id));
        if (!*id ||
            strspn(id, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.") !=
                strlen(id))
            continue;
        if (n > 1)
            result[n++] = ',';
        result[n++] = '[';
        int emitted = 0, count = j->tokens[array].count,
            step = count > 128 ? (count + 126) / 127 : 1;
        for (int k = 0; k < count && obj >= 0 && obj < j->count; k++) {
            int current = obj;
            obj = j->tokens[obj].next;
            if (k % step && k != count - 1)
                continue;
            char state[80], stamp[80];
            json_string(j, json_get(j, current, "state"), state, sizeof(state));
            json_string(j, json_get(j, current, "last_changed"), stamp, sizeof(stamp));
            char *end;
            double v = strtod(state, &end);
            if (!strcmp(state, "on"))
                v = 1;
            else if (!strcmp(state, "off"))
                v = 0;
            else if (end == state || *end || !isfinite(v))
                continue;
            if (!*stamp || strspn(stamp, "0123456789-T:+.Z") != strlen(stamp))
                continue;
            if (emitted++)
                result[n++] = ',';
            int wrote = snprintf(
                result + n, 262144 - n,
                "{\"entity_id\":\"%s\",\"state\":\"%.12g\",\"last_changed\":\"%s\"}", id, v, stamp);
            if (wrote < 0 || (size_t)wrote >= 262144 - n) {
                free(result);
                return 0;
            }
            n += wrote;
        }
        result[n++] = ']';
    }
    result[n++] = ']';
    int ok = atomic_write(out, result, n);
    free(result);
    return ok;
}
int service_start(int kind, const char *arg, const char *auth, const char *entity) {
    if (kind == SERVICE_HA_HISTORY && !history_child) {
        if (history_worker > 0 || !service_url_valid(arg) || !auth || !*auth || !entity ||
            !*entity || strlen(entity) > 1500 ||
            strspn(entity, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.,") !=
                strlen(entity))
            return 0;
        history_worker = fork();
        if (history_worker < 0)
            return 0;
        if (!history_worker) {
            setpgid(0, 0);
            signal(SIGTERM, SIG_DFL);
            history_child = 1;
            worker = -1;
            if (!service_start(kind, arg, auth, entity))
                _exit(1);
            int done = 0;
            while (!done) {
                usleep(50000);
                done = service_poll();
            }
            _exit(done == SERVICE_HA_HISTORY ? 0 : 1);
        }
        setpgid(history_worker, history_worker);
        return 1;
    }
    if (worker > 0 || kind < SERVICE_HA || kind > SERVICE_HA_HISTORY)
        return 0;
    if ((kind == SERVICE_HA || kind == SERVICE_TOGGLE || kind == SERVICE_HA_HISTORY) &&
        (!service_url_valid(arg) || !auth || !*auth))
        return 0;
    if (entity && *entity &&
        strspn(entity, kind == SERVICE_HA_HISTORY
                           ? "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.,"
                           : "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.") !=
            strlen(entity))
        return 0;
    if (kind == SERVICE_HA_HISTORY && (!entity || !*entity || strlen(entity) > 1500))
        return 0;
    if (kind == SERVICE_TOGGLE && (!entity || !strchr(entity, '.')))
        return 0;
    if (kind == SERVICE_WIKI_IMAGE && (!arg || (strncmp(arg, "https://upload.wikimedia.org/", 29) &&
                                                strncmp(arg, "https://thumb.wikimedia.org/", 28))))
        return 0;
    char failure[2048];
    path(failure, sizeof(failure), "service-error-code.txt");
    unlink(failure);
    job = kind;
    error[0] = 0;
    worker = fork();
    if (worker < 0)
        return 0;
    if (!worker) {
        if (!history_child)
            setpgid(0, 0);
        signal(SIGTERM, SIG_DFL);
        struct rlimit r = {history_child ? 12 * 1024 * 1024 : 3 * 1024 * 1024,
                           history_child ? 12 * 1024 * 1024 : 3 * 1024 * 1024};
        setrlimit(RLIMIT_FSIZE, &r);
        char out[2048], url[4096];
        path(out, sizeof(out), "service-result.tmp");
        int ok = 0;
        if (kind == SERVICE_TIME)
            ok = ntp();
        else if (kind == SERVICE_WIKI_IMAGE) {
            if (fetch(arg, out, NULL, NULL)) {
                char pgm[2048], final[2048], key[2048];
                path(pgm, sizeof(pgm), "wiki-image.pending");
                path(final, sizeof(final), "wiki-image.pgm");
                path(key, sizeof(key), "wiki-image-url.txt");
                ok = engine_image_fit(out, pgm, 500, 260) && !rename(pgm, final) &&
                     atomic_write(key, arg, strlen(arg));
                unlink(pgm);
            }
        } else {
            char base[512];
            snprintf(base, sizeof(base), "%s", arg);
            size_t n = strlen(base);
            while (n && base[n - 1] == '/')
                base[--n] = 0;
            if (kind == SERVICE_HA_HISTORY) {
                time_t start = time(NULL) - 86400;
                struct tm t;
                gmtime_r(&start, &t);
                char stamp[64];
                strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", &t);
                snprintf(url, sizeof(url),
                         "%s/api/history/period/"
                         "%s?filter_entity_id=%s&minimal_response&no_attributes&significant_"
                         "changes_only=0",
                         base, stamp, entity);
            } else if (kind == SERVICE_HA)
                snprintf(url, sizeof(url), "%s/api/states", base);
            else {
                char domain[64];
                snprintf(domain, sizeof(domain), "%.*s", (int)(strchr(entity, '.') - entity),
                         entity);
                if (strcmp(domain, "light") && strcmp(domain, "switch") && strcmp(domain, "fan") &&
                    strcmp(domain, "input_boolean"))
                    _exit(1);
                snprintf(url, sizeof(url), "%s/api/services/%s/toggle", base, domain);
            }
            ok = fetch(url, out, auth, kind == SERVICE_TOGGLE ? entity : NULL);
        }
        if (ok && kind != SERVICE_TIME && kind != SERVICE_WIKI_IMAGE) {
            char *body = service_read("service-result.tmp", history_child ? 8388608 : 2097152);
            int cap = kind == SERVICE_HA_HISTORY ? 500000 : 32000;
            JsonToken *tokens = calloc(cap, sizeof(*tokens));
            Json parsed;
            ok = body && tokens && json_parse(&parsed, body, tokens, cap);
            if (ok)
                ok = parsed.tokens[0].type == '[';
            if (ok && kind == SERVICE_HA_HISTORY)
                ok = history_compact(&parsed, out);
            free(tokens);
            free(body);
        }
        if (ok && kind != SERVICE_TIME && kind != SERVICE_WIKI_IMAGE) {
            char target[2048];
            path(target, sizeof(target),
                 kind == SERVICE_HA           ? "ha-states.json"
                 : kind == SERVICE_HA_HISTORY ? "ha-history.json"
                                              : "ha-action.json");
            ok = !rename(out, target);
        }
        unlink(out);
        _exit(ok ? 0 : 1);
    }
    if (!history_child)
        setpgid(worker, worker);
    return 1;
}
static int history_poll(void) {
    if (history_child || history_worker <= 0)
        return 0;
    int st;
    pid_t p = waitpid(history_worker, &st, WNOHANG);
    if (!p)
        return 0;
    history_worker = -1;
    return p > 0 && WIFEXITED(st) && !WEXITSTATUS(st) ? SERVICE_HA_HISTORY : -SERVICE_HA_HISTORY;
}
int service_poll(void) {
    if (worker <= 0)
        return history_poll();
    int st;
    pid_t p = waitpid(worker, &st, WNOHANG);
    if (!p)
        return history_poll();
    worker = -1;
    int ok = p > 0 && WIFEXITED(st) && WEXITSTATUS(st) == 0;
    if (!ok) {
        char *code = service_read("service-error-code.txt", 32);
        int c = code ? atoi(code) : 0;
        free(code);
        const char *message =
            job == SERVICE_TIME      ? "Zeitserver nicht erreichbar. WLAN prüfen."
            : c == 6                 ? "Servername nicht gefunden. Adresse prüfen."
            : c == 7                 ? "Server nicht erreichbar. Adresse und Port prüfen."
            : c == 22 ? "Server lehnt die Anfrage ab. Token und Zugriffsrechte prüfen."
            : c == 35 || c == 60
                ? "Sichere Verbindung fehlgeschlagen. Zertifikat und Uhrzeit prüfen."
                : "Verbindung fehlgeschlagen. WLAN / Adresse / Zugang prüfen.";
        snprintf(error, sizeof(error), "%s", tr(message));
    }
    return ok ? job : -job;
}
