#define _GNU_SOURCE
#include "release_update.h"
#include "../../tools/sdk/lumen_http.h"
#include "library.h"
#include "services.h"
#include "version.h"
#include "wireless_update.h"
#include "system_update.h"
#include "mupdf/fitz/crypt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <unistd.h>
#define API_ROOT "https://api.github.com/repos/MerlinHof/Lumen/releases/"
#define RELEASE_ROOT "https://github.com/MerlinHof/Lumen/releases/download/"
static pid_t worker;
static int state, loaded, installing, last_step, last_percent;
static time_t checked;
static char version[32], error[192];
static void path(char *out,const char *name) { snprintf(out,2048,"%s/%s",data_root,name); }
static void progress_file(int step, unsigned long long expected) {
    char file[2048], value[48];
    path(file,"release-progress.txt");
    int n=snprintf(value,sizeof(value),"%d %llu\n",step,expected);
    atomic_write(file,value,n);
}
static int update_progress(int *percent) {
    char file[2048];
    path(file,"release-progress.txt");
    FILE *f=fopen(file,"rb");
    int step=0;unsigned long long expected=0;
    if(f){if(fscanf(f,"%d %llu",&step,&expected)!=2)step=0;fclose(f);}
    *percent=0;
    if(step==1 && expected) {
        snprintf(file,sizeof(file),"%s/../.update/github-download",data_root);
        struct stat st;
        if(!stat(file,&st) && st.st_size>0) {
            unsigned long long count=st.st_size;
            *percent=count>=expected ? 100 : count*100/expected;
        }
    }
    return step;
}
int release_progress(int *percent) {
    *percent=last_percent;
    return last_step;
}
int release_activate(void) {
    char ready[2048], restart[2048];
    snprintf(ready,sizeof(ready),"%s/../.update/system-ready",data_root);
    snprintf(restart,sizeof(restart),"%s/../.update/relaunch",data_root);
    return state == 4 && !access(ready,F_OK) && atomic_write(restart,"1",1);
}
static LumenHttpResult download(const char *url,const char *file,size_t limit) {
    char ca[2048];
    snprintf(ca,sizeof(ca),"%s/../certs/ca-bundle.pem",data_root);
    if(!getenv("CURL_CA_BUNDLE") && !access(ca,R_OK))setenv("CURL_CA_BUNDLE",ca,1);
    int fd=open(file,O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW|O_CLOEXEC,0600);
    if(fd<0)return LUMEN_HTTP_IO;
    LumenHttpResult status=lumen_http_download_result((LumenHttpRequest){.url=url,.redirects=5,
         .connect_timeout=12,.timeout=limit>262144 ? 600 : 45},fd,limit);
    if(close(fd))status=LUMEN_HTTP_IO;
    if(status!=LUMEN_HTTP_OK)unlink(file);
    return status;
}
static int digest_file(const char *name,char out[65]) {
    FILE *f=fopen(name,"rb");if(!f)return 0;
    fz_sha256 hash;fz_sha256_init(&hash);
    unsigned char b[32768],sum[32];size_t n;
    while((n=fread(b,1,sizeof(b),f)))fz_sha256_update(&hash,b,n);
    int ok=!ferror(f);fclose(f);fz_sha256_final(&hash,sum);
    for(int i=0;i<32;i++)snprintf(out+2*i,3,"%02x",sum[i]);
    return ok;
}
static char *read_json(const char *file,Json *json,JsonToken *tokens,int cap) {
    FILE *f=fopen(file,"rb");if(!f)return NULL;
    char *text=malloc(262145);if(!text){fclose(f);return NULL;}
    size_t n=fread(text,1,262144,f);int ok=!ferror(f)&&fgetc(f)==EOF;fclose(f);text[n]=0;
    if(!ok || !json_parse(json,text,tokens,cap)){free(text);return NULL;}return text;
}
static void result(int code,const char *v,const char *message) {
    char p[2048],b[320];path(p,"release-check.txt");
    snprintf(b,sizeof(b),"%d %lld %s\n%s",code,(long long)time(NULL),*v?v:"-",message);
    atomic_write(p,b,strlen(b));
}
int release_beta_enabled(void) {
    char file[2048];path(file,"release-beta");
    return access(file,F_OK)==0;
}
int release_set_beta(int enabled) {
    if(worker || state==4)return 0;
    char file[2048];path(file,"release-beta");
    if(enabled) {
        if(!atomic_write(file,"1",1))return 0;
    } else if(unlink(file) && errno!=ENOENT)return 0;
    path(file,"release-check.txt");unlink(file);
    state=0;loaded=1;checked=0;version[0]=error[0]=0;
    return 1;
}
static int release_number(const char *value) {
    int major,minor;char tail;
    if(sscanf(value,"%d.%d%c",&major,&minor,&tail)!=2 ||
       major<1 || major>9999 || minor<0 || minor>9)return -1;
    return major*10+minor;
}
static int release_candidate(const char *file,const char *endpoint,const char *prefix,
                             char tag[32],char url[2048]) {
    if(download(endpoint,file,262144)!=LUMEN_HTTP_OK)return -1;
    Json json;JsonToken tokens[16384];
    char *text=read_json(file,&json,tokens,16384);
    if(!text)return -1;
    char candidate[32]="";
    json_string(&json,json_get(&json,0,"tag_name"),candidate,sizeof(candidate));
    int beta=!strcmp(prefix,"beta");
    size_t skip=beta ? 0 : strlen(prefix);
    int value=beta || strncmp(candidate,prefix,skip) ? -1 : release_number(candidate+skip);
    char beta_version[32]="";
    if(beta && !strcmp(candidate,"lumen-beta")) {
        int list=json_get(&json,0,"assets");
        if(list>=0 && json.tokens[list].type=='[')for(int i=0;i<json.tokens[list].count;i++) {
            char name[128],tail;
            int asset=json_at(&json,list,i),major,minor;
            json_string(&json,json_get(&json,asset,"name"),name,sizeof(name));
            if(sscanf(name,"LUMEN-%d.%d.wireless.json%c",&major,&minor,&tail)!=2)continue;
            char number[32],expected[128];
            snprintf(number,sizeof(number),"%d.%d",major,minor);
            snprintf(expected,sizeof(expected),"LUMEN-%s.wireless.json",number);
            int parsed=release_number(number);
            if(!strcmp(name,expected) && parsed>value) {
                value=parsed;
                snprintf(beta_version,sizeof(beta_version),"%s",number);
            }
        }
    }
    const char *number=beta ? beta_version : candidate+skip;
    if(value>=0) {
        snprintf(tag,32,"v%s",number);
        int assets=json_get(&json,0,"assets");
        char expected[64],address[2048];
        snprintf(expected,sizeof(expected),"LUMEN-%s.wireless.json",number);
        int count=assets>=0 && json.tokens[assets].type=='[' ? json.tokens[assets].count : 0;
        for(int i=0;i<count;i++) {
            int asset=json_at(&json,assets,i);char name[128];
            json_string(&json,json_get(&json,asset,"name"),name,sizeof(name));
            if(strcmp(name,expected))continue;
            json_string(&json,json_get(&json,asset,"browser_download_url"),address,sizeof(address));
            char exact[256];
            snprintf(exact,sizeof(exact),RELEASE_ROOT "%s/%s",candidate,expected);
            if(!strcmp(address,exact))snprintf(url,2048,"%s",address);
            break;
        }
    }
    free(text);return value;
}
static void check_worker(int install,pid_t parent) {
    nice(8);
    char api[2048],meta[2048],binary[2048];
    path(api,"release-api.tmp");path(meta,"release-meta.tmp");
    snprintf(binary,sizeof(binary),"%s/../.update/github-download",data_root);
    Json json;JsonToken tokens[4096];char *text=NULL;
    char tag[32]="",url[2048]="",hash[65]="",actual[65];
    const char *failure="GitHub nicht erreichbar. WLAN prüfen.";
    int newest=release_candidate(api,API_ROOT "latest","v",tag,url);
    if(release_beta_enabled()) {
        char beta_tag[32]="",beta_url[2048]="";
        int beta=release_candidate(api,API_ROOT "tags/lumen-beta","beta",beta_tag,beta_url);
        if(beta>newest && *beta_url) {
            newest=beta;
            snprintf(tag,sizeof(tag),"%s",beta_tag);
            snprintf(url,sizeof(url),"%s",beta_url);
        }
    }
    if(newest<0)goto failed;
    if(newest<=release_number(LUMEN_VERSION)){result(2,tag,"");goto done;}
    if(!install){result(3,tag,"");goto done;}
    failure="Dieses Update benötigt USB: Laufzeitdateien unterscheiden sich.";
    if(!*url)goto failed;
    char root[256];
    snprintf(root,sizeof(root),"%.*s",(int)(strrchr(url,'/')-url+1),url);
    if(download(url,meta,262144)!=LUMEN_HTTP_OK)goto failed;
    text=read_json(meta,&json,tokens,4096);if(!text)goto failed;
    char v[32];json_string(&json,json_get(&json,0,"version"),v,sizeof(v));
    if(strcmp(v,tag+1))goto failed;
    char format[40];json_string(&json,json_get(&json,0,"format"),format,sizeof(format));
    if(strcmp(format,"lumen-system-1"))goto failed;
    json_string(&json,json_get(&json,0,"url"),url,sizeof(url));
    json_string(&json,json_get(&json,0,"sha256"),hash,sizeof(hash));
    double bytes=json_number(&json,json_get(&json,0,"size"));
    double installed=json_number(&json,json_get(&json,0,"installed_size"));
    char expected_binary[320];
    snprintf(expected_binary,sizeof(expected_binary),"%sLUMEN-%s.lumen-system",root,v);
    if(strcmp(url,expected_binary) || strlen(hash)!=64 ||
       strspn(hash,"0123456789abcdef")!=64 ||
       !(bytes>=4096 && bytes<=128*1024*1024) || !(installed>=4096 && installed<=160*1024*1024))goto failed;
    long length=(long)bytes;
    failure="Nicht genügend freier Speicher für das Update.";
    struct statvfs disk;if(statvfs(data_root,&disk) || (uint64_t)disk.f_bavail*disk.f_frsize<(uint64_t)length+2*(uint64_t)installed+16*1024*1024)goto failed;
    char dir[2048];snprintf(dir,sizeof(dir),"%s/../.update",data_root);mkdir(dir,0700);
    progress_file(1,length);
    failure="Update-Download fehlgeschlagen. WLAN prüfen und erneut versuchen.";
    if(download(url,binary,(size_t)length)!=LUMEN_HTTP_OK)goto failed;
    progress_file(2,length);
    failure="Die Update-Datei stimmt nicht mit der Prüfsumme überein.";
    if(!digest_file(binary,actual) || strcmp(actual,hash))goto failed;
    struct stat info;if(stat(binary,&info) || info.st_size!=length)goto failed;
    progress_file(3,length);
    failure="Das Update konnte nicht auf dem Kindle vorbereitet werden.";
    if(getppid()!=parent || !system_update_stage(binary,tag+1))goto failed;
    result(4,tag,"");
    goto done;
failed: result(-1,tag,failure);
done: free(text);unlink(api);unlink(meta);unlink(binary);_exit(0);
}
int release_start(int install) {
    if(worker || (install && state!=3))return 0;
    installing=install;last_step=last_percent=0;
    if(install)progress_file(0,0);
    pid_t parent=getpid();worker=fork();if(worker<0){worker=0;return 0;}
    if(!worker){setpgid(0,0);signal(SIGTERM,SIG_DFL);check_worker(install,parent);}
    state=1;error[0]=0;return 1;
}
int release_poll(void) {
    if(worker){int status;pid_t done=waitpid(worker,&status,WNOHANG);if(done==0) {
            if (!installing) return 0;
            int percent, step=update_progress(&percent);
            int changed=step!=last_step || percent!=last_percent;
            last_step=step;last_percent=percent;
            return changed;
        } worker=0;installing=0;
        if(done<0 || !WIFEXITED(status) || WEXITSTATUS(status)){state=-1;snprintf(error,sizeof(error),"Update konnte nicht geprüft werden. Bitte erneut versuchen.");return 1;}}
    else if(state || loaded)return 0;
    loaded=1;
    char p[2048];path(p,"release-check.txt");FILE *f=fopen(p,"rb");if(!f){if(state==1){state=-1;snprintf(error,sizeof(error),"Update konnte nicht geprüft werden. Bitte erneut versuchen.");return 1;}return 0;}
    long long t=0;int code=0;char v[32];
    if(fscanf(f,"%d %lld %31s\n",&code,&t,v)==3){state=code;checked=t;snprintf(version,sizeof(version),"%s",v);fgets(error,sizeof(error),f);}
    fclose(f);
    int ma,mi,ca,ci;sscanf(LUMEN_VERSION,"%d.%d",&ca,&ci);
    if((state==3 || state==4) && sscanf(version,"v%d.%d",&ma,&mi)==2 && (ma<ca || (ma==ca && mi<=ci)))state=2;
    if(state==4) {
        snprintf(p,sizeof(p),"%s/../.update/system-ready",data_root);
        if(access(p,F_OK))state=3;
    }
    return 1;
}
int release_state(void){return state;}
const char *release_version(void){return version;}
const char *release_error(void){return error;}
time_t release_checked(void){return checked;}

void release_cancel(void) {
    if(worker){kill(-worker,SIGTERM);kill(worker,SIGTERM);while(waitpid(worker,NULL,0)<0 && errno==EINTR){}worker=0;}
    char file[2048];path(file,"release-api.tmp");unlink(file);path(file,"release-meta.tmp");unlink(file);
    snprintf(file,sizeof(file),"%s/../.update/github-download",data_root);unlink(file);
}
