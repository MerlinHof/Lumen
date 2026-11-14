#include "i18n.h"
#define _GNU_SOURCE
#include "connect.h"
#include "../apps/terminal/session.h"
#include "device.h"
#include "version.h"
#include "wireless_update.h"
#include "app_packages.h"
#include "packages.h"
#include "engine.h"
#include "library.h"
#include "services.h"
#include <arpa/inet.h>
#include <dirent.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <limits.h>
#include <net/if.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#ifndef LUMEN_HOST
#include <sys/prctl.h>
#endif
static pid_t service = -1;
static int listenfd = -1, activeport;
static char address[100], code[33], error[160], host[80];
static volatile sig_atomic_t live = 1;
static int authenticated;
static time_t auth_window;
static int auth_failures;
static char network_interface[32] = "wlan0";
static volatile sig_atomic_t client_fd = -1;
static int refresh_address(void);
const char *connect_address(void) {
    if (service > 0)
        refresh_address();
    return address;
}
const char *connect_code(void) { return code; }
const char *connect_error(void) { return error; }
static void terminate(int n) {
    (void)n;
    live = 0;
    if (client_fd >= 0)
        shutdown(client_fd, SHUT_RDWR);
}
static int private_ip(uint32_t ip) {
    ip = ntohl(ip);
    return (ip >> 24) == 10 || (ip >> 20) == 0xac1 || (ip >> 16) == 0xc0a8 || (ip >> 24) == 127;
}
static int writeall(int fd, const void *buf, size_t n) {
    while (n) {
        ssize_t w = send(fd, buf, n, MSG_NOSIGNAL);
        if (w < 0 && errno == EINTR)
            continue;
        if (w <= 0)
            return 0;
        buf = (const char *)buf + w;
        n -= (size_t)w;
    }
    return 1;
}
static void response(int fd, int status, const char *type, const char *body) {
    char hdr[640];
    int n = snprintf(
        hdr, sizeof(hdr),
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: "
        "close\r\nCache-Control: no-store\r\nX-Content-Type-Options: "
        "nosniff\r\nContent-Security-Policy: default-src 'self'; style-src 'self' 'unsafe-inline'; "
        "script-src 'self' 'unsafe-inline'; frame-ancestors 'none'\r\n\r\n",
        status,
        status == 200   ? "OK"
        : status == 201 ? "Created"
                        : "Error",
        type, strlen(body));
    fprintf(stderr, "http_response=%d bytes=%zu\n", status, strlen(body));
    writeall(fd, hdr, (size_t)n);
    writeall(fd, body, strlen(body));
}
static TerminalSession web_shell;
static void books_changed(void) {
    char marker[2048];
    snprintf(marker,sizeof(marker),"%s/books-changed",data_root);
    int file=open(marker,O_WRONLY|O_CREAT|O_CLOEXEC,0600);
    if(file>=0)close(file);
#ifndef LUMEN_HOST
    kill(getppid(),SIGUSR1);
#endif
}
static void json(FILE *f, const char *s) {
    fputc('"', f);
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') {
            fputc('\\', f);
            fputc(c, f);
        } else if (c < 32)
            fprintf(f, "\\u%04x", c);
        else
            fputc(c, f);
    }
    fputc('"', f);
}
static int safe_equal(const char *a, const char *b) {
    size_t n = strlen(a), m = strlen(b);
    unsigned diff = (unsigned)(n ^ m);
    for (size_t i = 0; i < 32; i++)
        diff |= (i < n ? (unsigned char)a[i] : 0) ^ (i < m ? (unsigned char)b[i] : 0);
    return diff == 0;
}
static int hex(int c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}
static int decode(const char *s, char *out, size_t cap) {
    size_t n = 0;
    while (*s) {
        int c = (unsigned char)*s++;
        if (c == '%') {
            if (!s[0] || !s[1] || hex(s[0]) < 0 || hex(s[1]) < 0)
                return 0;
            c = hex(s[0]) * 16 + hex(s[1]);
            s += 2;
        } else if (c == '+')
            c = ' ';
        if (!c || c < 32 || n + 1 >= cap)
            return 0;
        out[n++] = (char)c;
    }
    out[n] = 0;
    return 1;
}
static int arg(const char *query, const char *key, char *out, size_t cap) {
    size_t k = strlen(key);
    while (query && *query) {
        const char *end = strchr(query, '&');
        size_t n = end ? (size_t)(end - query) : strlen(query);
        if (n > k && query[k] == '=' && !memcmp(query, key, k)) {
            char raw[1024];
            if (n - k - 1 >= sizeof(raw))
                return 0;
            memcpy(raw, query + k + 1, n - k - 1);
            raw[n - k - 1] = 0;
            return decode(raw, out, cap);
        }
        query = end ? end + 1 : NULL;
    }
    return 0;
}
static int safename(const char *s) {
    size_t n = strlen(s);
    return n > 0 && n < 241 && s[0] != '.' && !strchr(s, '/') && !strchr(s, '\\') &&
           !strchr(s, '\r') && !strchr(s, '\n');
}
static int extension(const char *name) {
    const char *e = strrchr(name, '.');
    if (!e)
        return 0;
    const char *ok[] = {".epub", ".pdf", ".txt",  ".html", ".cbz",
                        ".mobi", ".azw", ".azw3", ".kfx",  NULL};
    for (int i = 0; ok[i]; i++)
        if (!strcasecmp(e, ok[i]))
            return 1;
    return 0;
}
static Book *bookid(const char *query) {
    char id[32];
    if (!arg(query, "id", id, sizeof(id)) || strlen(id) != 16)
        return NULL;
    char *end;
    uint64_t number = strtoull(id, &end, 16);
    if (*end)
        return NULL;
    for (int i = 0; i < library_count; i++)
        if (library[i].id == number)
            return &library[i];
    return NULL;
}
static int realbook(const Book *b, char *path) {
    char root[4096];
    if (!b || !realpath(library_root, root) || !realpath(b->path, path))
        return 0;
    size_t n = strlen(root);
    struct stat st;
    struct stat original;
    if (lstat(b->path, &original) || !S_ISREG(original.st_mode))
        return 0;
    return !strncmp(root, path, n) && path[n] == '/' && !lstat(path, &st) && S_ISREG(st.st_mode);
}
static void listing(int fd) {
    char *body = NULL;
    size_t n = 0;
    FILE *f = open_memstream(&body, &n);
    if (!f) {
        response(fd, 500, "text/plain", "Out of memory");
        return;
    }
    fputs("{\"books\":[", f);
    for (int i = 0; i < library_count; i++) {
        Book *b = &library[i];
        if (i)
            fputc(',', f);
        fprintf(f, "{\"id\":\"%016llx\",\"title\":", (unsigned long long)b->id);
        json(f, b->title);
        fputs(",\"author\":", f);
        json(f, b->author);
        fputs(",\"format\":", f);
        json(f, b->format);
        fprintf(f, ",\"size\":%llu}", (unsigned long long)b->size);
    }
    struct statvfs st;
    memset(&st, 0, sizeof(st));
    statvfs(library_root, &st);
    fprintf(f, "],\"free\":%llu}", (unsigned long long)st.f_bavail * st.f_frsize);
    fclose(f);
    response(fd, 200, "application/json; charset=utf-8", body);
    free(body);
}
static void download(int fd, Book *b) {
    char path[4096];
    if (!realbook(b, path)) {
        response(fd, 404, "text/plain", "Not found");
        return;
    }
    int in = open(path, O_RDONLY | O_NOFOLLOW);
    if (in < 0) {
        response(fd, 404, "text/plain", "Not found");
        return;
    }
    struct stat st;
    fstat(in, &st);
    char hdr[400];
    int n =
        snprintf(hdr, sizeof(hdr),
                 "HTTP/1.1 200 OK\r\nContent-Type: "
                 "application/octet-stream\r\nContent-Disposition: attachment\r\nContent-Length: "
                 "%llu\r\nConnection: close\r\nX-Content-Type-Options: nosniff\r\n\r\n",
                 (unsigned long long)st.st_size);
    writeall(fd, hdr, (size_t)n);
    char buf[32768];
    ssize_t got;
    while ((got = read(in, buf, sizeof(buf))) > 0)
        if (!writeall(fd, buf, (size_t)got))
            break;
    close(in);
}
static void photo_list(int fd) {
    char path[2048];
    snprintf(path, sizeof(path), "%s/photos", data_root);
    DIR *d = opendir(path);
    char *body = NULL;
    size_t len = 0;
    FILE *f = open_memstream(&body, &len);
    if (!f) {
        if (d)
            closedir(d);
        response(fd, 500, "text/plain", "Memory");
        return;
    }
    fputs("{\"photos\":[", f);
    int first = 1;
    struct dirent *e;
    if (d) {
        while ((e = readdir(d))) {
            size_t n = strlen(e->d_name);
            if (n < 5 || !safename(e->d_name) || strcmp(e->d_name + n - 4, ".pgm"))
                continue;
            if (!first)
                fputc(',', f);
            first = 0;
            json(f, e->d_name);
        }
        closedir(d);
    }
    fputs("]}", f);
    fclose(f);
    response(fd, 200, "application/json", body);
    free(body);
}
static void upload(int fd, const char *query, uint64_t length, const char *initial, size_t initialn,
                   int image) {
    char name[256], dest[2048], temp[2048];
    if (!arg(query, "name", name, sizeof(name)) || !safename(name) ||
        (!image && !extension(name))) {
        response(fd, 400, "text/plain", "Invalid book filename");
        return;
    }
    if (length > (image ? 12ULL : 512ULL) * 1024 * 1024) {
        response(fd, 413, "text/plain", "Limit: 512 MB per book");
        return;
    }
    char studyarg[8], uploadroot[2048];
    int studying =
        !image && arg(query, "study", studyarg, sizeof(studyarg)) && !strcmp(studyarg, "1");
    if (studying) {
        snprintf(uploadroot, sizeof(uploadroot), "%s/study", data_root);
        mkdir(uploadroot, 0700);
    } else
        snprintf(uploadroot, sizeof(uploadroot), "%s", library_root);
    if (snprintf(dest, sizeof(dest), "%s/%s", uploadroot, name) >= (int)sizeof(dest)) {
        response(fd, 400, "text/plain", "Filename too long");
        return;
    }
    snprintf(temp, sizeof(temp), "%s/.lumen-upload-%ld", uploadroot, (long)getpid());
    if (!image && access(dest, F_OK) == 0) {
        response(fd, 409, "text/plain", "A file with that name already exists");
        return;
    }
    struct statvfs space;
    if (!statvfs(library_root, &space) && length > (uint64_t)space.f_bavail * space.f_frsize) {
        response(fd, 507, "text/plain", "Not enough storage");
        return;
    }
    int out = open(temp, O_CREAT | O_EXCL | O_WRONLY | O_NOFOLLOW, 0600);
    if (out < 0) {
        response(fd, 500, "text/plain", "Cannot create upload");
        return;
    }
    uint64_t left = length;
    int ok = 1;
    char buf[32768];
    while (left) {
        size_t take;
        if (initialn) {
            take = initialn > left ? (size_t)left : initialn;
            if (write(out, initial, take) != (ssize_t)take) {
                ok = 0;
                break;
            }
            initial += take;
            initialn -= take;
        } else {
            take = left > sizeof(buf) ? sizeof(buf) : (size_t)left;
            ssize_t r = recv(fd, buf, take, 0);
            if (r <= 0) {
                ok = 0;
                break;
            }
            take = (size_t)r;
            size_t off = 0;
            while (off < take) {
                ssize_t w = write(out, buf + off, take - off);
                if (w <= 0) {
                    ok = 0;
                    break;
                }
                off += (size_t)w;
            }
            if (!ok)
                break;
        }
        left -= take;
    }
    if (fsync(out))
        ok = 0;
    if (close(out))
        ok = 0;
    if (image) {
        char cache[2048], final[2048];
        int photo_reserved = 0;
        snprintf(cache, sizeof(cache), "%s/lockscreen-upload.pgm", data_root);
        snprintf(final, sizeof(final), "%s/lockscreen.pgm", data_root);
        if (image == 2) {
            char dir[2048];
            snprintf(dir, sizeof(dir), "%s/photos", data_root);
            mkdir(dir, 0700);
            int reserved = 0;
            for (int i = 0; i < 512; i++) {
                snprintf(final, sizeof(final), "%s/photos/photo-%lld-%03d.pgm", data_root,
                         (long long)time(NULL), i);
                int r = open(final, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
                if (r >= 0) {
                    close(r);
                    reserved = 1;
                    photo_reserved = 1;
                    break;
                }
            }
            if (!reserved)
                ok = 0;
        }
        if (ok)
            ok = engine_image_fit(temp, cache, 600, 800);
        if (ok)
            ok = !rename(cache, final);
        if (ok && image == 2) {
            char dir[2048], thumb[2048];
            snprintf(dir, sizeof(dir), "%s/photos/.thumbs", data_root);
            mkdir(dir, 0700);
            const char *name = strrchr(final, '/');
            snprintf(thumb, sizeof(thumb), "%s/photos/.thumbs/%s", data_root,
                     name ? name + 1 : "image.pgm");
            engine_photo_thumbnail(final, thumb);
        }
        if (!ok && image == 2 && photo_reserved)
            unlink(final);
        unlink(cache);
        unlink(temp);
        response(fd, ok ? 201 : 400, "application/json",
                 ok ? "{\"ok\":true}" : "Invalid or oversized image; previous image retained");
        return;
    }
    if (ok) { /* link refuses to replace an existing book. */
        int reserve = open(dest, O_CREAT | O_EXCL | O_WRONLY | O_NOFOLLOW, 0600);
        if (reserve < 0)
            ok = 0;
        else {
            close(reserve);
            if (rename(temp, dest)) {
                unlink(dest);
                ok = 0;
            }
        }
    }
    unlink(temp);
    if (ok) {
        if (!studying) {
            books_changed();
            library_scan();
        }
        response(fd, 201, "application/json", "{\"ok\":true}");
    } else
        response(fd, 500, "text/plain", "Upload did not complete; original files retained");
}
#include "connect_web.h"
#include "markdown.h"
#include "connect_exports.h"
#include "connect_notes.h"
static void handle(int fd,const char *initial,size_t initial_size) {
    authenticated = 0;
    struct timeval tv = {20, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    char request[16385];
    size_t used = initial_size;
    memcpy(request,initial,used);
    request[used]=0;
    char *end = strstr(request,"\r\n\r\n");
    while (!end && used < 16384) {
        ssize_t n = recv(fd, request + used, 16384 - used, 0);
        if (n <= 0)
            return;
        used += (size_t)n;
        request[used] = 0;
        end = strstr(request, "\r\n\r\n");
        if (end)
            break;
    }
    if (!end) {
        response(fd, 431, "text/plain", "Headers too large");
        return;
    }
    char *body = end + 4;
    size_t bodyn = used - (size_t)(body - request);
    *end = 0;
    char method[12], url[2048], version[12];
    if (sscanf(request, "%11s %2047s %11s", method, url, version) != 3 ||
        strcmp(version, "HTTP/1.1")) {
        response(fd, 400, "text/plain", "HTTP/1.1 required");
        return;
    }
    char auth[200] = "", origin[160] = "", hostname[160] = "";
    uint64_t length = 0;
    int haslength = 0, bad = 0;
    char *line = strstr(request, "\r\n");
    while (line) {
        line += 2;
        char *next = strstr(line, "\r\n");
        if (next)
            *next = 0;
        char *colon = strchr(line, ':');
        if (colon) {
            *colon = 0;
            char *value = colon + 1;
            while (*value == ' ')
                value++;
            if (!strcasecmp(line, "Authorization"))
                snprintf(auth, sizeof(auth), "%s", value);
            else if (!strcasecmp(line, "Host"))
                snprintf(hostname, sizeof(hostname), "%s", value);
            else if (!strcasecmp(line, "Origin"))
                snprintf(origin, sizeof(origin), "%s", value);
            else if (!strcasecmp(line, "Content-Length")) {
                char *tail;
                errno = 0;
                unsigned long long number = strtoull(value, &tail, 10);
                if (haslength || errno || tail == value || *tail || value[0] == '-' ||
                    value[0] == '+')
                    bad = 1;
                haslength = 1;
                length = number;
            } else if (!strcasecmp(line, "Transfer-Encoding"))
                bad = 1;
        }
        if (!next)
            break;
        line = next;
    }
    struct sockaddr_in local;
    socklen_t local_size = sizeof(local);
    char local_ip[INET_ADDRSTRLEN] = "", expected_host[80] = "", expected_origin[100] = "";
    if (getsockname(fd, (void *)&local, &local_size) == 0 && local.sin_family == AF_INET) {
        inet_ntop(AF_INET, &local.sin_addr, local_ip, sizeof(local_ip));
        snprintf(expected_host, sizeof(expected_host), "%s:%u", local_ip, ntohs(local.sin_port));
        snprintf(expected_origin, sizeof(expected_origin), "http://%s", expected_host);
    }
    if (bad || !*expected_host || strcmp(hostname, expected_host) ||
        (*origin && strcmp(origin, expected_origin))) {
        response(fd, 403, "text/plain", "Request origin or framing rejected");
        return;
    }
    if (!strcmp(method, "GET") && !strcmp(url, "/")) {
        response(fd, 200, "text/html; charset=utf-8", connect_html);
        return;
    }
    if (time(NULL) - auth_window >= 60) {
        auth_window = time(NULL);
        auth_failures = 0;
    }
    if (auth_failures >= 5) {
        response(fd, 429, "text/plain", "Too many attempts. Wait one minute.");
        return;
    }
    if (strncmp(auth, "Bearer ", 7) || !safe_equal(auth + 7, code)) {
        auth_failures++;
        response(fd, 401, "text/plain", "Pair using the code shown on your Kindle");
        return;
    }
    authenticated = 1;
    char seen[2048], stamp[40];
    snprintf(seen, sizeof(seen), "%s/connect-peer16.txt", data_root);
    snprintf(stamp, sizeof(stamp), "%lld", (long long)time(NULL));
    struct stat seenst;
    if (stat(seen, &seenst) || time(NULL) - seenst.st_mtime > 10)
        atomic_write(seen, stamp, strlen(stamp));
    char *query = strchr(url, '?');
    if (query)
        *query++ = 0;
    if (!strcmp(url,"/api/terminal")) {
        int allowed=0;
#ifndef LUMEN_HOST
        allowed=1;
#endif
        if (!strcmp(method,"GET")) {
            if(!allowed){response(fd,200,"application/json","{\"enabled\":false}");return;}
            char name[96]="kindle",*reply=NULL;size_t size=0;
            gethostname(name,sizeof(name)-1);
            FILE *stream=open_memstream(&reply,&size);
            if(!stream){response(fd,500,"text/plain","Cannot return hostname");return;}
            fputs("{\"enabled\":true,\"hostname\":",stream);json(stream,name);fputc('}',stream);
            fclose(stream);response(fd,200,"application/json",reply);free(reply);
            return;
        }
        if (!allowed) {response(fd,403,"text/plain","Terminal is available on a Kindle");return;}
        char command[2048], output[32768];
        if (strcmp(method,"POST") || !haslength || !length || length>=sizeof(command) || bodyn>length) {
            response(fd,400,"text/plain","Invalid command");
            return;
        }
        memcpy(command,body,bodyn);
        while(bodyn<length) {
            ssize_t n=recv(fd,command+bodyn,length-bodyn,0);
            if(n<=0)return;
            bodyn+=n;
        }
        if(memchr(command,0,bodyn)) {response(fd,400,"text/plain","Invalid command");return;}
        command[bodyn]=0;
        int result=terminal_session_command(&web_shell,"/mnt/us",command,output,sizeof(output));
        char *reply=NULL;size_t size=0;FILE *stream=open_memstream(&reply,&size);
        if(!stream) {response(fd,500,"text/plain","Cannot return command output");return;}
        fprintf(stream,"{\"exit\":%d,\"output\":",result);
        json(stream,output);fputc('}',stream);fclose(stream);
        response(fd,200,"application/json",reply);free(reply);
        return;
    }
    if(!strcmp(method,"GET") && !strcmp(url,"/api/apps")) {
        app_packages_scan();char*body=NULL;size_t size=0;FILE*f=open_memstream(&body,&size);fputs("{\"apps\":[",f);int first=1;
        for(int i=0;i<APP_LIMIT;i++) {
            const AppDefinition*a=app_definition(i);if(!a||a->entry<0)continue;
            if(!first)fputc(',',f);first=0;fputs("{\"id\":",f);json(f,app_package_key(i));fputs(",\"name\":",f);json(f,a->name);fputs(",\"names\":[",f);for(int l=0;l<3;l++){if(l)fputc(',',f);json(f,tr_language(a->name,l));}fputs("],\"version\":",f);json(f,app_package_version(i));fputc('}',f);
        }
        fputs("]}",f);fclose(f);response(fd,200,"application/json",body);free(body);return;
    }
    if(!strcmp(method,"PUT") && !strcmp(url,"/api/apps/install")) {
        if(!haslength || length<100 || length>64*1024*1024 || bodyn>length){response(fd,400,"application/json","{}");return;}
        struct statvfs space;if(statvfs(data_root,&space) || (uint64_t)space.f_bavail*space.f_frsize<3*length+8*1024*1024){response(fd,507,"application/json","{}");return;}
        char dir[2048],archive[2048];snprintf(dir,sizeof(dir),"%s/../apps",data_root);package_mkdirs(dir);snprintf(archive,sizeof(archive),"%s/.upload-%ld",dir,(long)getpid());
        unlink(archive);int out=open(archive,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600),ok=out>=0;uint64_t left=length;char bytes[32768];
        while(ok && left){const char*p=body;size_t n=bodyn;if(!n){ssize_t got=recv(fd,bytes,left<sizeof(bytes)?left:sizeof(bytes),0);if(got<=0){ok=0;break;}p=bytes;n=got;}size_t sent=0;while(sent<n){ssize_t w=write(out,p+sent,n-sent);if(w<=0){ok=0;break;}sent+=w;}left-=n;bodyn=0;}
        if(out>=0){if(fsync(out))ok=0;if(close(out))ok=0;}
        if(ok)ok=app_package_install(archive);unlink(archive);response(fd,ok?201:400,"application/json",ok?"{\"installed\":true}":"{\"error\":\"Invalid app package\"}");
#ifndef LUMEN_HOST
        if(ok)kill(getppid(),SIGUSR1);
#endif
        return;
    }
    if (!strcmp(method, "PUT") && (!strcmp(url, "/api/update") || !strcmp(url, "/api/custom-app"))) {
        char digest[UPDATE_DIGEST_SIZE], result[128];
        int status = haslength ? update_receive(fd,length,body,bodyn,digest) : 411;
        if (status == 201)
            snprintf(result,sizeof(result),"{\"sha256\":\"%s\"}",digest);
        else
            snprintf(result,sizeof(result),"{\"error\":\"Update upload rejected\"}");
        response(fd,status,"application/json",result);
        return;
    }
    if (!strcmp(method,"GET") && (!strcmp(url,"/api/custom-app/status") || !strcmp(url,"/api/system/status"))) {
        char result[100];
        snprintf(result,sizeof(result),"{\"version\":\"%s\",\"trial\":%s}",LUMEN_VERSION,update_trial() ? "true" : "false");
        response(fd,200,"application/json",result);
        return;
    }
    if (!strcmp(method,"GET") && (!strcmp(url,"/api/custom-app/guide") || !strcmp(url,"/api/apps/guide"))) {
        char path[2048], guide[24000];
        snprintf(path,sizeof(path),"%s/../system/custom-app-guide.txt",data_root);
#ifdef LUMEN_HOST
        FILE *f=fopen("lumen/assets/web/custom-app-guide.txt","rb");
#else
        FILE *f=fopen(path,"rb");
#endif
        if (!f) { response(fd,404,"text/plain","Guide unavailable");return; }
        size_t n=fread(guide,1,sizeof(guide)-1,f);
        fclose(f);
        guide[n]=0;
        response(fd,200,"text/plain; charset=utf-8",guide);
        return;
    }
    if (!strcmp(method,"POST") && (!strcmp(url,"/api/custom-app/keep") || !strcmp(url,"/api/system/keep"))) {
        response(fd,update_keep() ? 200 : 409,"application/json","{}");
        return;
    }
    if (!strcmp(method,"POST") && (!strcmp(url,"/api/custom-app/install") ||
                                  !strcmp(url,"/api/custom-app/rollback"))) {
        char digest[65], path[2048];
        int install=!strcmp(url,"/api/custom-app/install");
        int ok=install ? arg(query,"sha256",digest,sizeof(digest)) && strlen(digest)==64 &&
                         update_approve(digest) : update_trial();
        if (ok && !install) {
            snprintf(path,sizeof(path),"%s/../.update/system-starting",data_root);
            ok=atomic_write(path,"1",1);
        }
        snprintf(path,sizeof(path),"%s/../.update/relaunch",data_root);
        if (ok) ok=atomic_write(path,"1",1);
        response(fd,ok ? 200 : 409,"application/json","{}");
#ifndef LUMEN_HOST
        if (ok) kill(getppid(),SIGUSR1);
#endif
        return;
    }
    /* Keep the File Transfer API used by existing browsers and bookmarks. */
    if (!strcmp(method,"GET") && !strcmp(url,"/api/notes")) {
        web_export(fd,"lumen.notes","export_list","");
        return;
    }
    if (!strcmp(method,"GET") && !strcmp(url,"/api/note")) {
        char id[40],format[12]="txt",*end;
        if(!arg(query,"id",id,sizeof(id))) {response(fd,400,"text/plain","Invalid note");return;}
        long number=strtol(id,&end,10);
        if(end==id || *end || number<=0 || number>INT_MAX) {
            response(fd,400,"text/plain","Invalid note");return;
        }
        arg(query,"format",format,sizeof(format));
        web_export(fd,"lumen.notes",!strcmp(format,"pdf") ? "export_pdf" : "export_text",id);
        return;
    }
    if (!strcmp(method, "PUT") && !strcmp(url, "/api/setup") && haslength) {
        web_setup16(fd, query, length, body, bodyn);
        return;
    }
    if (!strcmp(url, "/api/home-assistant") && !strcmp(method, "PUT")) {
        if (!haslength || length == 0 || length > 2048) {
            response(fd, 400, "text/plain", "Invalid setup length");
            return;
        }
        char payload[2049];
        size_t n = bodyn > length ? (size_t)length : bodyn;
        memcpy(payload, body, n);
        while (n < length) {
            ssize_t got = recv(fd, payload + n, (size_t)length - n, 0);
            if (got <= 0)
                return;
            n += got;
        }
        payload[n] = 0;
        Json j;
        JsonToken tok[32];
        char address[512], secret[512];
        if (!json_parse(&j, payload, tok, 32)) {
            response(fd, 400, "text/plain", "Invalid setup");
            return;
        }
        int u = json_get(&j, 0, "url"), t = json_get(&j, 0, "token");
        if (u < 0 || t < 0 || j.tokens[u].end - j.tokens[u].start >= 510 ||
            j.tokens[t].end - j.tokens[t].start >= 510) {
            response(fd, 400, "text/plain", "Setup too long");
            return;
        }
        json_string(&j, u, address, sizeof(address));
        json_string(&j, t, secret, sizeof(secret));
        if (!service_url_valid(address) || !*secret || strpbrk(secret, "\r\n\"\\")) {
            response(fd, 400, "text/plain", "Check address and token");
            return;
        }
        char a[2048], b[2048];
        snprintf(a, sizeof(a), "%s/ha-url.txt", data_root);
        snprintf(b, sizeof(b), "%s/ha-token.txt", data_root);
        int ok =
            atomic_write(b, secret, strlen(secret)) && atomic_write(a, address, strlen(address));
        memset(payload, 0, sizeof(payload));
        memset(secret, 0, sizeof(secret));
        response(fd, ok ? 200 : 500, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
        return;
    }
    if (!strcmp(url, "/api/photos")) {
        if (!strcmp(method, "GET")) {
            photo_list(fd);
            return;
        }
        if (!strcmp(method, "PUT")) {
            if (!haslength) {
                response(fd, 411, "text/plain", "Length required");
                return;
            }
            upload(fd, "name=photo", length, body, bodyn, 2);
            return;
        }
    }
    if (!strcmp(url, "/api/photo")) {
        char name[256], path[2048];
        size_t n;
        if (!arg(query, "name", name, sizeof(name)) || !safename(name) || (n = strlen(name)) < 5 ||
            strcmp(name + n - 4, ".pgm")) {
            response(fd, 400, "text/plain", "Invalid photo");
            return;
        }
        snprintf(path, sizeof(path), "%s/photos/%s", data_root, name);
        struct stat st;
        if (lstat(path, &st) || !S_ISREG(st.st_mode)) {
            response(fd, 404, "text/plain", "Not found");
            return;
        }
        if (!strcmp(method, "DELETE")) {
            if (unlink(path))
                response(fd, 500, "text/plain", "Delete failed");
            else {
                char cache[2048];
                snprintf(cache, sizeof(cache), "%s/photos/.thumbs/%s", data_root, name);
                unlink(cache);
                response(fd, 200, "application/json", "{}");
            }
            return;
        }
        if (!strcmp(method, "GET")) {
            if (st.st_size > 480064) {
                response(fd, 400, "text/plain", "Invalid image");
                return;
            }
            FILE *f = fopen(path, "rb");
            if (!f) {
                response(fd, 404, "text/plain", "Not found");
                return;
            }
            char thumb[8];
            if (arg(query, "thumb", thumb, sizeof(thumb))) {
                fclose(f);
                char dir[2048], cache[2048];
                snprintf(dir, sizeof(dir), "%s/photos/.thumbs", data_root);
                mkdir(dir, 0700);
                snprintf(cache, sizeof(cache), "%s/photos/.thumbs/%s", data_root, name);
                if (access(cache, R_OK) && !engine_photo_thumbnail(path, cache)) {
                    response(fd, 500, "text/plain", "Thumbnail failed");
                    return;
                }
                FILE *t = fopen(cache, "rb");
                unsigned char tiny[18511];
                if (!t) {
                    response(fd, 404, "text/plain", "Not found");
                    return;
                }
                size_t bytes = fread(tiny, 1, sizeof(tiny), t);
                fclose(t);
                char h[512];
                snprintf(
                    h, sizeof(h),
                    "HTTP/1.1 200 OK\r\nContent-Type: image/x-portable-graymap\r\nContent-Length: "
                    "%zu\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n",
                    bytes);
                writeall(fd, h, strlen(h));
                writeall(fd, tiny, bytes);
                return;
            }
            char h[512];
            snprintf(h, sizeof(h),
                     "HTTP/1.1 200 OK\r\nContent-Type: image/x-portable-graymap\r\nContent-Length: "
                     "%lld\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n",
                     (long long)st.st_size);
            writeall(fd, h, strlen(h));
            char buf[8192];
            size_t r;
            while ((r = fread(buf, 1, sizeof(buf), f)))
                writeall(fd, buf, r);
            fclose(f);
            return;
        }
    }
    if (!strcmp(method, "GET") && !strcmp(url, "/api/books")) {
        listing(fd);
        return;
    }
    if (!strcmp(url, "/api/book")) {
        Book *b = bookid(query);
        if (!b) {
            response(fd, 404, "text/plain", "Not found");
            return;
        }
        if (!strcmp(method, "GET")) {
            download(fd, b);
            return;
        }
        if (!strcmp(method, "DELETE")) {
            char path[4096];
            if (!realbook(b, path) || unlink(path)) {
                response(fd, 409, "text/plain", "Cannot delete book");
                return;
            }
            books_changed();
            library_scan();
            response(fd, 200, "application/json", "{\"ok\":true}");
            return;
        }
    }
    if (!strcmp(method, "PUT") && !strcmp(url, "/api/upload")) {
        if (!haslength) {
            response(fd, 411, "text/plain", "Content-Length required");
            return;
        }
        upload(fd, query, length, body, bodyn, 0);
        return;
    }
    if (!strcmp(method, "POST") && !strcmp(url, "/api/rename")) {
        Book *b = bookid(query);
        char name[256], old[4096], dest[4096];
        if (!realbook(b, old) || !arg(query, "name", name, sizeof(name)) || !safename(name) ||
            !extension(name)) {
            response(fd, 400, "text/plain", "Invalid name");
            return;
        }
        char *slash = strrchr(old, '/');
        size_t dir = (size_t)(slash - old);
        if (dir + strlen(name) + 2 >= sizeof(dest)) {
            response(fd, 400, "text/plain", "Name too long");
            return;
        }
        memcpy(dest, old, dir + 1);
        strcpy(dest + dir + 1, name);
        if (!library_rename((int)(b - library), name)) {
            response(fd, 409, "text/plain", "Name already exists or rename unavailable");
            return;
        }
        books_changed();
        library_scan();
        response(fd, 200, "application/json", "{\"ok\":true}");
        return;
    }
    response(fd, 404, "text/plain", "Not found");
}
static int firewall_rule(int enable, int output) {
#ifndef LUMEN_HOST
    char port[12];
    snprintf(port, sizeof(port), "%d", activeport);
    pid_t p = fork();
    if (!p) {
        execlp("iptables", "iptables", enable ? "-I" : "-D", output ? "OUTPUT" : "INPUT",
               output ? "-o" : "-i", network_interface, "-p", "tcp", output ? "--sport" : "--dport",
               port, "-j", "ACCEPT", NULL);
        _exit(127);
    }
    int st = 0;
    if (p < 0)
        return 0;
    while (waitpid(p, &st, 0) < 0 && errno == EINTR) {
    }
    return WIFEXITED(st) && WEXITSTATUS(st) == 0;
#else
    (void)enable;
    (void)output;
    return 1;
#endif
}
static int firewall(int enable) {
    if (enable) {
        if (!firewall_rule(1, 0))
            return 0;
        if (!firewall_rule(1, 1)) {
            firewall_rule(0, 0);
            return 0;
        }
        return 1;
    }
    int a = firewall_rule(0, 0), b = firewall_rule(0, 1);
    return a && b;
}

int connect_running(void) {
    if (service <= 0)
        return 0;
    int status;
    pid_t result = waitpid(service, &status, WNOHANG);
    if (result == service || (result < 0 && errno == ECHILD)) {
        service = -1;
        return 0;
    }
    return 1;
}
void connect_stop(void) {
    if (service > 0) {
        kill(service, SIGTERM);
        while (waitpid(service, NULL, 0) < 0 && errno == EINTR) {
        }
        service = -1;
    }
    code[0] = 0;
}
static int refresh_address(void) {
    struct ifaddrs *ifs = NULL;
    char ip[INET_ADDRSTRLEN] = "";
    if (getifaddrs(&ifs))
        return 0;
    for (struct ifaddrs *a = ifs; a; a = a->ifa_next) {
        if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET || !(a->ifa_flags & IFF_UP))
            continue;
        if (strcmp(a->ifa_name, network_interface))
            continue;
        struct sockaddr_in *v = (struct sockaddr_in *)a->ifa_addr;
        if (private_ip(v->sin_addr.s_addr))
            inet_ntop(AF_INET, &v->sin_addr, ip, sizeof(ip));
    }
    freeifaddrs(ifs);
    if (!*ip) {
        address[0] = 0;
        return 0;
    }
    snprintf(host, sizeof(host), "%s:%d", ip, activeport);
    snprintf(address, sizeof(address), "http://%s", host);
    return 1;
}
int connect_start(int port, int timeout) {
    if (connect_running())
        return 1;
    if (port < 1024 || port > 65535 || timeout < 1 || timeout > 60)
        return 0;
    char ip[INET_ADDRSTRLEN] = "";
    struct ifaddrs *ifs = NULL;
    if (!getifaddrs(&ifs)) {
        for (struct ifaddrs *a = ifs; a; a = a->ifa_next) {
            if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET || !(a->ifa_flags & IFF_UP))
                continue;
#ifdef LUMEN_HOST
            if (strcmp(a->ifa_name, "lo0") && strcmp(a->ifa_name, "lo"))
                continue;
#else
            if (strncmp(a->ifa_name, "wlan", 4) && strncmp(a->ifa_name, "mlan", 4) &&
                strncmp(a->ifa_name, "eth", 3))
                continue;
#endif
            struct sockaddr_in *v = (struct sockaddr_in *)a->ifa_addr;
            if (private_ip(v->sin_addr.s_addr)) {
                inet_ntop(AF_INET, &v->sin_addr, ip, sizeof(ip));
                snprintf(network_interface, sizeof(network_interface), "%s", a->ifa_name);
            }
        }
        freeifaddrs(ifs);
    }
    if (!*ip) {
        snprintf(error, sizeof(error), "%s", tr("WLAN verbinden, bevor Connect gestartet wird."));
        return 0;
    }
    int random = open("/dev/urandom", O_RDONLY);
    unsigned char entropy[16];
    if (random < 0 || read(random, entropy, sizeof(entropy)) != sizeof(entropy)) {
        if (random >= 0)
            close(random);
        return 0;
    }
    close(random);
    const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    for (int i = 0; i < 4; i++)
        code[i] = alphabet[entropy[i] & 31];
    code[4] = 0;
    auth_window = 0;
    auth_failures = 0;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0)
        return 0;
    int yes = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
#ifdef LUMEN_HOST
    inet_pton(AF_INET, ip, &addr.sin_addr);
#else
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (setsockopt(listenfd, SOL_SOCKET, SO_BINDTODEVICE, network_interface,
                   strlen(network_interface) + 1)) {
        close(listenfd);
        listenfd = -1;
        snprintf(error, sizeof(error), "%s", tr("WLAN-Schnittstelle nicht verfügbar."));
        return 0;
    }
#endif
    if (bind(listenfd, (void *)&addr, sizeof(addr)) || listen(listenfd, 32)) {
        snprintf(error, sizeof(error), tr("Port %d ist nicht verfügbar."), port);
        close(listenfd);
        return 0;
    }
    activeport = port;
    live = 1;
    fprintf(stderr, "connect_listen=%s:%d interface=%s\n", ip, port, network_interface);
    snprintf(host, sizeof(host), "%s:%d", ip, port);
    snprintf(address, sizeof(address), "http://%s", host);
    char seen[2048];
    snprintf(seen, sizeof(seen), "%s/connect-peer16.txt", data_root);
    unlink(seen);
    int ready[2];
    if (pipe(ready)) {
        close(listenfd);
        return 0;
    }
    sigset_t blocked, oldmask;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGTERM);
    sigaddset(&blocked, SIGINT);
    sigprocmask(SIG_BLOCK, &blocked, &oldmask);
    service = fork();
    if (service < 0) {
        sigprocmask(SIG_SETMASK, &oldmask, NULL);
        close(ready[0]);
        close(ready[1]);
        close(listenfd);
        return 0;
    }
    if (!service) {
        close(ready[0]);
#ifndef LUMEN_HOST
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        if (getppid() == 1)
            _exit(1);
#endif
        signal(SIGTERM, terminate);
        signal(SIGINT, terminate);
        signal(SIGPIPE, SIG_IGN);
        sigprocmask(SIG_SETMASK, &oldmask, NULL);
        engine_close();
        char success = firewall(1) ? 1 : 0;
        write(ready[1], &success, 1);
        close(ready[1]);
        if (!success)
            _exit(1);
        struct Pending {
            int fd;
            int64_t accepted;
            size_t used;
            char header[16385];
        } pending[12];
        memset(pending,0,sizeof(pending));
        for (int i=0;i<12;i++) pending[i].fd=-1;
        struct timespec clock;
        clock_gettime(CLOCK_MONOTONIC,&clock);
        int64_t last=(int64_t)clock.tv_sec*1000+clock.tv_nsec/1000000;
        device_hold_awake(1);
        while (live) {
            clock_gettime(CLOCK_MONOTONIC,&clock);
            int64_t now=(int64_t)clock.tv_sec*1000+clock.tv_nsec/1000000;
            if (now-last >= (int64_t)timeout*60000) break;
            struct pollfd sockets[13]={{listenfd,POLLIN,0}};
            for (int i=0;i<12;i++) {
                if (pending[i].fd>=0 && now-pending[i].accepted>=10000) {
                    close(pending[i].fd);
                    pending[i].fd=-1;
                }
                sockets[i+1]=(struct pollfd){pending[i].fd,POLLIN,0};
            }
            int result=poll(sockets,13,500);
            if (result<0) { if (errno==EINTR) continue;break; }
            if (sockets[0].revents&POLLIN) {
                struct sockaddr_in peer;
                socklen_t size=sizeof(peer);
                int fd=accept(listenfd,(void *)&peer,&size), slot=-1;
                for (int i=0;i<12;i++) if (pending[i].fd<0) {slot=i;break;}
                if (fd>=0) {
                    if (slot<0 || !private_ip(peer.sin_addr.s_addr)) close(fd);
                    else {
                        pending[slot].fd=fd;
                        pending[slot].accepted=now;
                        pending[slot].used=0;
                    }
                }
            }
            for (int i=0;i<12 && live;i++) {
                struct Pending *client=&pending[i];
                if (client->fd<0 || !sockets[i+1].revents) continue;
                ssize_t n=recv(client->fd,client->header+client->used,
                               16384-client->used,MSG_DONTWAIT);
                if (n<0 && (errno==EAGAIN || errno==EINTR)) continue;
                if (n>0) {
                    client->used+=(size_t)n;
                    client->header[client->used]=0;
                    if (!strstr(client->header,"\r\n\r\n") && client->used<16384) continue;
                    client_fd=client->fd;
                    handle(client->fd,client->header,client->used);
                    shutdown(client->fd,SHUT_WR);
                    if (authenticated) {
                        clock_gettime(CLOCK_MONOTONIC,&clock);
                        last=(int64_t)clock.tv_sec*1000+clock.tv_nsec/1000000;
                    }
                    client_fd=-1;
                }
                close(client->fd);
                client->fd=-1;
            }
        }
        for (int i=0;i<12;i++) if (pending[i].fd>=0) close(pending[i].fd);
        terminal_session_close(&web_shell);
        device_hold_awake(0);
        close(listenfd);
        firewall(0);
        _exit(0);
    }
    sigprocmask(SIG_SETMASK, &oldmask, NULL);
    close(ready[1]);
    char success = 0;
    ssize_t n;
    struct pollfd startup = {ready[0], POLLIN, 0};
    int ready_result;
    do {
        ready_result = poll(&startup, 1, 4000);
    } while (ready_result < 0 && errno == EINTR);
    n = ready_result > 0 ? read(ready[0], &success, 1) : -1;
    close(ready[0]);
    close(listenfd);
    listenfd = -1;
    if (n != 1 || !success) {
        connect_stop();
        snprintf(error, sizeof(error), "%s",
                 tr("Netzwerkfreigabe fehlgeschlagen. Diagnose prüfen."));
        return 0;
    }
    return 1;
}
