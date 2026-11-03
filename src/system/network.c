#include "../i18n.h"
#define _GNU_SOURCE
#include "../network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifndef LUMEN_HOST
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#endif
WifiNetwork wifi_networks[WIFI_LIMIT];
int wifi_count;
static char error[256];
const char *network_error(void) { return error; }
#ifndef LUMEN_HOST
/* wpa_supplicant's documented local control protocol. Never invoke a shell or put a password in
 * argv. */
static int command(const char *cmd, char *out, int size) {
    const char *servers[] = {"/var/run/wpa_supplicant/wlan0", "/var/run/wpa_supplicant/eth0",
                             "/var/run/wpa_supplicant/mlan0", "/var/run/wpa_supplicant", NULL};
    struct sockaddr_un local = {.sun_family = AF_UNIX}, remote = {.sun_family = AF_UNIX};
    snprintf(local.sun_path, sizeof(local.sun_path), "/tmp/lumen-wifi-%ld", (long)getpid());
    int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return 0;
    unlink(local.sun_path);
    if (bind(fd, (void *)&local, sizeof(local))) {
        close(fd);
        return 0;
    }
    int ok = 0;
    for (int i = 0; servers[i]; i++) {
        snprintf(remote.sun_path, sizeof(remote.sun_path), "%s", servers[i]);
        if (!connect(fd, (void *)&remote, sizeof(remote))) {
            ok = 1;
            break;
        }
    }
    if (ok && send(fd, cmd, strlen(cmd), 0) >= 0) {
        struct pollfd p = {fd, POLLIN, 0};
        if (poll(&p, 1, 1200) > 0) {
            ssize_t n = recv(fd, out, (size_t)size - 1, 0);
            if (n >= 0) {
                out[n] = 0;
                ok = strncmp(out, "FAIL", 4) != 0 && strncmp(out, "UNKNOWN", 7) != 0;
            } else
                ok = 0;
        } else
            ok = 0;
    } else
        ok = 0;
    close(fd);
    unlink(local.sun_path);
    return ok;
}
#else
static int command(const char *cmd, char *out, int size) {
    const char *value = !strcmp(cmd, "SCAN_RESULTS")
                            ? "bssid / frequency / signal level / flags / "
                              "ssid\n00:11:22:33:44:55\t2412\t-43\t[WPA2-PSK-CCMP][ESS]"
                              "\tTestnetz\n00:11:22:33:44:66\t2412\t-70\t[ESS]\tOffenes Netz\n"
                        : !strcmp(cmd, "LIST_NETWORKS")
                            ? "network id / ssid / bssid / flags\n0\tTestnetz\tany\t[CURRENT]\n"
                        : !strcmp(cmd, "STATUS")      ? "ssid=Testnetz\nwpa_state=COMPLETED\n"
                        : !strcmp(cmd, "ADD_NETWORK") ? "1\n"
                                                      : "OK\n";
    snprintf(out, (size_t)size, "%s", value);
    return 1;
}
#endif
int network_scan(void) {
    char out[1024];
    error[0] = 0;
    if (!command("SCAN", out, sizeof(out))) {
        snprintf(
            error, sizeof(error), "%s",
            tr("WLAN-Suche nicht verfügbar. WLAN einschalten oder Kindle-Netzwerkeinstellungen "
               "öffnen."));
        return 0;
    }
    return 1;
}
static void decode_ssid(const char *in, char *out, size_t cap) {
    size_t n = 0;
    while (*in && n + 1 < cap) {
        if (in[0] == '\\' && in[1] == 'x' && in[2] && in[3]) {
            unsigned c;
            if (sscanf(in + 2, "%2x", &c) == 1) {
                out[n++] = (char)c;
                in += 4;
                continue;
            }
        }
        out[n++] = *in++;
    }
    out[n] = 0;
}
static void firmware_saved(void) {
#ifndef LUMEN_HOST
    int fds[2];
    if (pipe(fds))
        return;
    pid_t p = fork();
    if (!p) {
        close(fds[0]);
        dup2(fds[1], 1);
        close(fds[1]);
        int null = open("/dev/null", O_WRONLY);
        if (null >= 0)
            dup2(null, 2);
        execl("/usr/bin/lipc-hash-prop", "lipc-hash-prop", "-n", "com.lab126.wifid", "profileData",
              NULL);
        _exit(127);
    }
    close(fds[1]);
    char line[4096];
    FILE *f = fdopen(fds[0], "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            char *key = strstr(line, "essid");
            if (!key)
                continue;
            /* Match only a complete quoted value, never a substring of an SSID. */
            for (int i = 0; i < wifi_count; i++) {
                char quoted[260];
                snprintf(quoted, sizeof(quoted), "\"%s\"", wifi_networks[i].ssid);
                if (strstr(key, quoted))
                    wifi_networks[i].saved = 1;
            }
        }
        memset(line, 0, sizeof(line));
        fclose(f);
    } else
        close(fds[0]);
    if (p > 0)
        while (waitpid(p, NULL, 0) < 0 && errno == EINTR) {
        }
#endif
}
int network_results(void) {
    char result[16384], saved[8192];
    if (!command("SCAN_RESULTS", result, sizeof(result))) {
        snprintf(error, sizeof(error), "%s",
                 tr("Keine Suchergebnisse. Kindle-Netzwerkeinstellungen sind als Alternative "
                    "verfügbar."));
        return 0;
    }
    wifi_count = 0;
    char *state, *line = strtok_r(result, "\n", &state);
    while ((line = strtok_r(NULL, "\n", &state)) && wifi_count < WIFI_LIMIT) {
        char *fields[5], *q = line;
        int n = 0;
        fields[n++] = q;
        while (*q && n < 5) {
            if (*q == '\t') {
                *q = 0;
                fields[n++] = q + 1;
            }
            q++;
        }
        if (n != 5 || !*fields[4])
            continue;
        WifiNetwork b = {.signal = atoi(fields[2]), .network_id = -1};
        decode_ssid(fields[4], b.ssid, sizeof(b.ssid));
        snprintf(b.flags, sizeof(b.flags), "%s", fields[3]);
        int duplicate = 0;
        for (int i = 0; i < wifi_count; i++)
            if (!strcmp(wifi_networks[i].ssid, b.ssid)) {
                duplicate = 1;
                if (b.signal > wifi_networks[i].signal)
                    wifi_networks[i] = b;
                break;
            }
        if (!duplicate)
            wifi_networks[wifi_count++] = b;
    }
    if (command("LIST_NETWORKS", saved, sizeof(saved))) {
        char *save, *l = strtok_r(saved, "\n", &save);
        while ((l = strtok_r(NULL, "\n", &save))) {
            char *tab = strchr(l, '\t');
            if (!tab)
                continue;
            *tab = 0;
            int id = atoi(l);
            char *end = strchr(tab + 1, '\t');
            if (end)
                *end = 0;
            char name[128];
            decode_ssid(tab + 1, name, sizeof(name));
            for (int i = 0; i < wifi_count; i++)
                if (!strcmp(name, wifi_networks[i].ssid)) {
                    wifi_networks[i].saved = 1;
                    wifi_networks[i].network_id = id;
                }
        }
    }
    firmware_saved();
    char current[128];
    network_status(current, sizeof(current));
    for (int i = 0; i < wifi_count; i++)
        for (int j = i + 1; j < wifi_count; j++)
            if ((!strcmp(wifi_networks[j].ssid, current) &&
                 strcmp(wifi_networks[i].ssid, current)) ||
                (strcmp(wifi_networks[i].ssid, current) &&
                 wifi_networks[j].signal > wifi_networks[i].signal)) {
                WifiNetwork b = wifi_networks[i];
                wifi_networks[i] = wifi_networks[j];
                wifi_networks[j] = b;
            }
    return 1;
}
#ifndef LUMEN_HOST
static int input_command(const char *program, char *const argv[], const char *input, char *output,
                         size_t cap) {
    int in[2], out[2];
    if (pipe(in))
        return 0;
    if (pipe(out)) {
        close(in[0]);
        close(in[1]);
        return 0;
    }
    pid_t p = fork();
    if (!p) {
        dup2(in[0], 0);
        dup2(out[1], 1);
        int null = open("/dev/null", O_WRONLY);
        if (null >= 0)
            dup2(null, 2);
        close(in[0]);
        close(in[1]);
        close(out[0]);
        close(out[1]);
        alarm(4);
        execv(program, argv);
        _exit(127);
    }
    close(in[0]);
    close(out[1]);
    if (p < 0) {
        close(in[1]);
        close(out[0]);
        return 0;
    }
    size_t n = strlen(input), off = 0;
    while (off < n) {
        ssize_t w = write(in[1], input + off, n - off);
        if (w <= 0)
            break;
        off += (size_t)w;
    }
    close(in[1]);
    size_t used = 0;
    char discard[256];
    ssize_t got;
    while ((got = read(out[0], used + 1 < cap ? output + used : discard,
                       used + 1 < cap ? cap - used - 1 : sizeof(discard))) > 0)
        if (used + 1 < cap)
            used += (size_t)got;
    output[used] = 0;
    close(out[0]);
    int status = 0;
    while (waitpid(p, &status, 0) < 0 && errno == EINTR) {
    }
    return off == n && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
#endif
static int firmware_save(const WifiNetwork *network, const char *password) {
#ifndef LUMEN_HOST
    char response[4096] = "", psk[65] = "", input[1024], ssid[260];
    int secured = strstr(network->flags, "WPA") != NULL;
    if (secured) {
        char secret[80];
        snprintf(secret, sizeof(secret), "%s\n", password);
        char *args[] = {"wpa_passphrase", (char *)network->ssid, NULL};
        int ok = input_command("/usr/bin/wpa_passphrase", args, secret, response, sizeof(response));
        if (!ok)
            ok =
                input_command("/usr/sbin/wpa_passphrase", args, secret, response, sizeof(response));
        memset(secret, 0, sizeof(secret));
        if (!ok) {
            memset(response, 0, sizeof(response));
            return 0;
        }
        char *state, *line = strtok_r(response, "\n", &state);
        while (line) {
            while (*line == ' ' || *line == '\t')
                line++;
            if (!strncmp(line, "psk=", 4) && strlen(line + 4) == 64 &&
                strspn(line + 4, "0123456789abcdefABCDEF") == 64)
                snprintf(psk, sizeof(psk), "%s", line + 4);
            line = strtok_r(NULL, "\n", &state);
        }
        memset(response, 0, sizeof(response));
        if (!*psk)
            return 0;
    }
    size_t j = 0;
    for (const unsigned char *p = (const unsigned char *)network->ssid; *p && j + 3 < sizeof(ssid);
         p++) {
        if (*p < 32) {
            memset(psk, 0, sizeof(psk));
            return 0;
        }
        if (*p == '"' || *p == '\\')
            ssid[j++] = '\\';
        ssid[j++] = *p;
    }
    ssid[j] = 0;
    snprintf(input, sizeof(input),
             "{essid=\"%s\",secured=\"%s\",psk=\"%s\",store_nw_user_pref=(0)}\n", ssid,
             secured ? "yes" : "no", psk);
    char *args[] = {"lipc-hash-prop", "com.lab126.wifid", "createProfile", NULL};
    int ok = input_command("/usr/bin/lipc-hash-prop", args, input, response, sizeof(response));
    memset(input, 0, sizeof(input));
    memset(response, 0, sizeof(response));
    memset(psk, 0, sizeof(psk));
    return ok;
#else
    (void)network;
    (void)password;
    return 1;
#endif
}
static int firmware_profile(const char *property, const char *ssid);
int network_connect(int index, const char *password) {
    if (index < 0 || index >= wifi_count)
        return 0;
    WifiNetwork *b = &wifi_networks[index];
    if (b->saved && !*password && firmware_profile("cmConnect", b->ssid))
        return 1;
    char out[1024], cmd[1024];
    int id = b->network_id, created = 0;
    if (!b->saved || *password) {
        if (strstr(b->flags, "EAP") || strstr(b->flags, "SAE")) {
            snprintf(error, sizeof(error), "%s",
                     tr("Dieses Netz benötigt die Kindle-Netzwerkeinstellungen."));
            return 0;
        }
        int secured = strstr(b->flags, "WPA") != NULL;
        if (secured && (strlen(password) < 8 || strlen(password) > 63)) {
            snprintf(error, sizeof(error), "%s", tr("WPA-Passwort: 8 bis 63 Zeichen."));
            return 0;
        }
        for (const unsigned char *p = (const unsigned char *)password; *p; p++)
            if (*p < 32) {
                snprintf(error, sizeof(error), "%s", tr("Ungültiges WLAN-Passwort."));
                return 0;
            }
        if (strstr(b->flags, "WEP")) {
            snprintf(error, sizeof(error), "%s",
                     tr("WEP bitte über die Kindle-Netzwerkeinstellungen verbinden."));
            return 0;
        }
        if (firmware_save(b, password) && firmware_profile("cmConnect", b->ssid)) {
            b->saved = 1;
            error[0] = 0;
            return 1;
        }
        if (!command("ADD_NETWORK", out, sizeof(out)))
            goto fail;
        id = atoi(out);
        created = 1;
        char hex[257];
        size_t n = strlen(b->ssid);
        for (size_t i = 0; i < n; i++)
            snprintf(hex + i * 2, 3, "%02x", (unsigned char)b->ssid[i]);
        snprintf(cmd, sizeof(cmd), "SET_NETWORK %d ssid %s", id, hex);
        if (!command(cmd, out, sizeof(out)))
            goto fail;
        snprintf(cmd, sizeof(cmd), "SET_NETWORK %d key_mgmt %s", id, secured ? "WPA-PSK" : "NONE");
        if (!command(cmd, out, sizeof(out)))
            goto fail;
        if (secured) {
            char escaped[256];
            size_t j = 0;
            for (const char *p = password; *p; p++) {
                if ((unsigned char)*p < 32)
                    goto fail;
                if (*p == '"' || *p == '\\')
                    escaped[j++] = '\\';
                escaped[j++] = *p;
            }
            escaped[j] = 0;
            snprintf(cmd, sizeof(cmd), "SET_NETWORK %d psk \"%s\"", id, escaped);
            int ok = command(cmd, out, sizeof(out));
            memset(cmd, 0, sizeof(cmd));
            memset(escaped, 0, sizeof(escaped));
            if (!ok)
                goto fail;
        }
    }
    snprintf(cmd, sizeof(cmd), "SELECT_NETWORK %d", id);
    if (!command(cmd, out, sizeof(out)))
        goto fail;
    /* Existing profiles stay untouched. Firmware may disable SAVE_CONFIG; connection still
     * succeeds. */
    command("SAVE_CONFIG", out, sizeof(out));
    error[0] = 0;
    return 1;
fail:
    if (created) {
        snprintf(cmd, sizeof(cmd), "REMOVE_NETWORK %d", id);
        command(cmd, out, sizeof(out));
    }
    snprintf(error, sizeof(error), "%s",
             tr("Verbindung nicht eingerichtet. Kindle-Netzwerkeinstellungen verwenden."));
    return 0;
}
int network_disconnect(void) {
    char out[1024];
    return command("DISCONNECT", out, sizeof(out));
}
void network_status(char *ssid, int size) {
    char out[4096];
    ssid[0] = 0;
    if (!command("STATUS", out, sizeof(out)))
        return;
    char *state, *line = strtok_r(out, "\n", &state);
    while (line) {
        if (!strncmp(line, "ssid=", 5))
            decode_ssid(line + 5, ssid, (size_t)size);
        line = strtok_r(NULL, "\n", &state);
    }
}

static int firmware_profile(const char *property, const char *ssid) {
#ifndef LUMEN_HOST
    pid_t p = fork();
    if (!p) {
        execl("/usr/bin/lipc-set-prop", "lipc-set-prop", "com.lab126.wifid", property, ssid, NULL);
        _exit(127);
    }
    if (p < 0)
        return 0;
    int status = 0;
    while (waitpid(p, &status, 0) < 0 && errno == EINTR) {
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#else
    (void)property;
    (void)ssid;
    return 1;
#endif
}
int network_forget(int index) {
    if (index < 0 || index >= wifi_count || !wifi_networks[index].saved)
        return 0;
    WifiNetwork *n = &wifi_networks[index];
    /* wifid owns persistent credentials and can recreate supplicant profiles.
       Removing only the supplicant ID is temporary and SAVE_CONFIG may be disabled. */
    int deleted = firmware_profile("deleteProfile", n->ssid);
    fprintf(stderr, "wifi_forget_firmware=%d\n", deleted);
    if (!deleted) {
        snprintf(error, sizeof(error), "%s", tr("Netzwerk konnte nicht entfernt werden."));
        return 0;
    }
    if (n->network_id >= 0) {
        char cmd[80], out[1024];
        snprintf(cmd, sizeof(cmd), "REMOVE_NETWORK %d", n->network_id);
        command(cmd, out, sizeof(out));
    }
    n->saved = 0;
    n->network_id = -1;
    error[0] = 0;
    return 1;
}
