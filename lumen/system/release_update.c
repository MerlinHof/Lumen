#define _GNU_SOURCE
#include "release_update.h"
#include "library.h"
#include "services.h"
#include "version.h"
#include "wireless_update.h"
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
#define RELEASE_ROOT "https://github.com/MerlinHof/Lumen/releases/download/"
static pid_t worker;
static int state, loaded;
static time_t checked;
static char version[32], error[192];
static void path(char *out,const char *name) { snprintf(out,2048,"%s/%s",data_root,name); }
static int download(const char *url,const char *file,size_t limit) {
    if (strpbrk(url,"\r\n\"\\")) return 0;
    pid_t p=fork();
    if (p==0) {
        char size[32],ca[2048];
        snprintf(size,sizeof(size),"%zu",limit);
        snprintf(ca,sizeof(ca),"%s/../certs/ca-bundle.pem",data_root);
        if (!getenv("CURL_CA_BUNDLE") && !access(ca,R_OK)) setenv("CURL_CA_BUNDLE",ca,1);
        int fd=open("/dev/null",O_RDWR);dup2(fd,1);dup2(fd,2);close(fd);
        execlp("curl","curl","--fail","--silent","--location","--max-redirs","5",
               "--proto","=https","--proto-redir","=https","--connect-timeout","8",
               "--max-time","180","--max-filesize",size,"--user-agent","LUMEN/" LUMEN_VERSION,
               "--output",file,url,NULL);
        _exit(127);
    }
    if (p<0) return 0;
    int status=0;while(waitpid(p,&status,0)<0) { if (errno!=EINTR) return 0; }
    return WIFEXITED(status) && !WEXITSTATUS(status);
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
static void check_worker(int install,pid_t parent) {
    char api[2048],meta[2048],binary[2048];
    path(api,"release-api.tmp");path(meta,"release-meta.tmp");
    snprintf(binary,sizeof(binary),"%s/../.update/github-download",data_root);
    Json json;JsonToken tokens[4096];char *text=NULL;
    char tag[32]="",url[2048]="",hash[65]="",relative[1024],expected[65],actual[65];
    const char *failure="GitHub nicht erreichbar. WLAN prüfen.";
    if(!download("https://api.github.com/repos/MerlinHof/Lumen/releases/latest",api,262144))goto failed;
    text=read_json(api,&json,tokens,4096);if(!text)goto failed;
    json_string(&json,json_get(&json,0,"tag_name"),tag,sizeof(tag));
    int major,minor,cm,cn;sscanf(LUMEN_VERSION,"%d.%d",&cm,&cn);
    if(sscanf(tag,"v%d.%d",&major,&minor)!=2)goto failed;
    if(major<cm || (major==cm && minor<=cn)){result(2,tag,"");goto done;}
    int assets=json_get(&json,0,"assets");
    if(assets<0 || json.tokens[assets].type!='[')goto failed;
    for(int i=0;i<json.tokens[assets].count;i++) {
        int asset=json_at(&json,assets,i);char name[128];
        json_string(&json,json_get(&json,asset,"name"),name,sizeof(name));
        if(strstr(name,".wireless.json"))json_string(&json,json_get(&json,asset,"browser_download_url"),url,sizeof(url));
    }
    if(!install){result(3,tag,"");goto done;}
    failure="Dieses Update benötigt USB: Laufzeitdateien unterscheiden sich.";
    if(strncmp(url,RELEASE_ROOT,strlen(RELEASE_ROOT)))goto failed;
    free(text);text=NULL;
    if(!download(url,meta,262144))goto failed;
    text=read_json(meta,&json,tokens,4096);if(!text)goto failed;
    char v[32];json_string(&json,json_get(&json,0,"version"),v,sizeof(v));
    if(strcmp(v,tag+1))goto failed;
    int resources=json_get(&json,0,"resources");
    if(resources<0 || json.tokens[resources].type!='[' || json.tokens[resources].count<10)goto failed;
    for(int i=0;i<json.tokens[resources].count;i++) {
        int item=json_at(&json,resources,i);char local[2048];
        json_string(&json,json_get(&json,item,"path"),relative,sizeof(relative));
        json_string(&json,json_get(&json,item,"sha256"),expected,sizeof(expected));
        if(!*relative || relative[0]=='/' || strstr(relative,"..") || strchr(relative,'\\') ||
           !strncmp(relative,"data/",5) || strlen(expected)!=64)goto failed;
        snprintf(local,sizeof(local),"%s/../%s",data_root,relative);
        if(!digest_file(local,actual) || strcmp(expected,actual))goto failed;
    }
    json_string(&json,json_get(&json,0,"url"),url,sizeof(url));
    json_string(&json,json_get(&json,0,"sha256"),hash,sizeof(hash));
    long length=(long)json_number(&json,json_get(&json,0,"size"));
    if(strncmp(url,RELEASE_ROOT,strlen(RELEASE_ROOT)) || strlen(hash)!=64 || length<4096 || length>64*1024*1024)goto failed;
    failure="Update konnte nicht geprüft werden. Bitte erneut versuchen.";
    if(update_trial()){failure="Zuerst das installierte Test-Update bestätigen.";goto failed;}
    struct statvfs disk;if(statvfs(data_root,&disk) || (uint64_t)disk.f_bavail*disk.f_frsize<(uint64_t)length+8*1024*1024)goto failed;
    char dir[2048];snprintf(dir,sizeof(dir),"%s/../.update",data_root);mkdir(dir,0700);
    if(!download(url,binary,64*1024*1024) || !digest_file(binary,actual) || strcmp(actual,hash))goto failed;
    struct stat info;if(stat(binary,&info) || info.st_size!=length)goto failed;
    FILE *executable=fopen(binary,"rb");unsigned char head[20];
    int valid=executable && fread(head,1,sizeof(head),executable)==sizeof(head) &&
              !memcmp(head,"\177ELF\1\1\1",7) && head[16]==2 && !head[17] && head[18]==40 && !head[19];
    if(executable)fclose(executable);if(!valid)goto failed;
    if(getppid()!=parent)goto failed;
    char incoming[2048],receipt[2048],restart[2048];
    snprintf(incoming,sizeof(incoming),"%s/incoming",dir);
    snprintf(receipt,sizeof(receipt),"%s/received",dir);
    snprintf(restart,sizeof(restart),"%s/relaunch",dir);
    if(rename(binary,incoming) || !atomic_write(receipt,hash,64) || !update_approve(hash) || !atomic_write(restart,"1",1))goto failed;
    result(3,tag,"");kill(parent,SIGUSR1);goto done;
failed: result(-1,tag,failure);
done: free(text);unlink(api);unlink(meta);unlink(binary);_exit(0);
}
int release_start(int install) {
    if(worker || (install && state!=3))return 0;
    pid_t parent=getpid();worker=fork();if(worker<0){worker=0;return 0;}
    if(!worker){setpgid(0,0);signal(SIGTERM,SIG_DFL);check_worker(install,parent);}
    state=1;error[0]=0;return 1;
}
int release_poll(void) {
    if(worker){int status;pid_t done=waitpid(worker,&status,WNOHANG);if(done==0)return 0;worker=0;
        if(done<0 || !WIFEXITED(status) || WEXITSTATUS(status)){state=-1;snprintf(error,sizeof(error),"Update konnte nicht geprüft werden. Bitte erneut versuchen.");return 1;}}
    else if(state || loaded)return 0;
    loaded=1;
    char p[2048];path(p,"release-check.txt");FILE *f=fopen(p,"rb");if(!f){if(state==1){state=-1;snprintf(error,sizeof(error),"Update konnte nicht geprüft werden. Bitte erneut versuchen.");return 1;}return 0;}
    long long t=0;int code=0;char v[32];
    if(fscanf(f,"%d %lld %31s\n",&code,&t,v)==3){state=code;checked=t;snprintf(version,sizeof(version),"%s",v);fgets(error,sizeof(error),f);}
    fclose(f);
    int ma,mi,ca,ci;sscanf(LUMEN_VERSION,"%d.%d",&ca,&ci);
    if(state==3 && sscanf(version,"v%d.%d",&ma,&mi)==2 && (ma<ca || (ma==ca && mi<=ci)))state=2;
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
