#include "i18n.h"
#define _GNU_SOURCE
#include "engine.h"
#include "library.h"
#define LUMEN_JSON_IMPLEMENTATION
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
static pid_t worker = -1;
static int job;
static char error[120];
int service_busy(void) { return worker > 0; }
const char *service_error(void) { return error; }
static void path(char *out, size_t n, const char *name) {
    snprintf(out,n,"%s/%s",data_root,name);
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
    if (worker > 0) {
        kill(-worker, SIGTERM);
        while (waitpid(worker, NULL, 0) < 0 && errno == EINTR) {
        }
        worker = -1;
    }
    char cfg[2048];
    path(cfg, sizeof(cfg), "service-private.tmp");
    unlink(cfg);
}
int service_start(int kind,const char *arg) {
    (void)arg;
    if(worker>0 || kind!=SERVICE_TIME)return 0;
    char failure[2048];
    path(failure, sizeof(failure), "service-error-code.txt");
    unlink(failure);
    job = kind;
    error[0] = 0;
    worker = fork();
    if (worker < 0)
        return 0;
    if (!worker) {
        setpgid(0,0);
        signal(SIGTERM, SIG_DFL);
        struct rlimit r={3*1024*1024,3*1024*1024};
        setrlimit(RLIMIT_FSIZE, &r);
        int ok=ntp();
        _exit(ok ? 0 : 1);
    }
    setpgid(worker,worker);
    return 1;
}
int service_poll(void) {
    if (worker <= 0)
        return 0;
    int st;
    pid_t p = waitpid(worker, &st, WNOHANG);
    if (!p)
        return 0;
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
