#define _GNU_SOURCE
#include "app_packages.h"
#include "packages.h"
#include "services.h"
#include "library.h"
#include "version.h"
#include "system_update.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include "../../tools/sdk/lumen_wire.h"
#include "../../tools/sdk/lumen_storage.h"
#include "lumen_package_defaults.h"
/* IDs are a package identity, never an origin or privilege class. */
static struct {AppDefinition def;char key[64],name[96],version[32];int installed,removed,has_icon,api,launcher_order,launcher_visible;unsigned surfaces,providers;int periodic_interval;char periodic_operation[48];} entries[APP_LIMIT];
static int provider_ids[PROVIDER_COUNT]={-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
enum { APP_REQUEST_MAX=49152, APP_REQUEST_LIMIT=8 };
typedef struct {
    pid_t child;
    int read_fd,write_fd,active,paused,frame_back,frame_ready,frame_tick,frame_loading;
    size_t incoming_n,pending_n;
    uint64_t frame_serial, generation;
    char process_hash[65],pending[65536],incoming[APP_FRAME_MAX],frame[APP_FRAME_MAX];
    char *requests[APP_REQUEST_LIMIT];
    unsigned request_first,request_count;
    char failure[160],data_dir[2048],render_dir[64];
} AppProcess;

enum { KEPT_APP_COUNT=3 };
static AppProcess initial_foreground={.read_fd=-1,.write_fd=-1,.active=-1};
static AppProcess *active_foreground=&initial_foreground;
static AppProcess *kept_apps[KEPT_APP_COUNT];
#define foreground (*active_foreground)
static AppProcess *overlays[APP_OVERLAY_COUNT];
static int overlay_provider_slots[PROVIDER_COUNT]={-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static AppProcess *service,*periodic_process,*system_query;
static int64_t system_query_deadline;
static void (*system_query_result)(const char *data);
static int64_t periodic_due[APP_LIMIT],periodic_deadline;
static int64_t periodic_probe;
static char periodic_key[APP_LIMIT][64];
static uint64_t process_generation,service_caller;
static int64_t service_deadline;
static int service_action;
static int service_background_owner=-1;
static pid_t retired[64];
static uint64_t frame_serial;

static void process_close(AppProcess *process);
static int process_poll(AppProcess *process);
static void reap_apps(void) {
    for(int i=0;i<64;i++)
        if(retired[i] && waitpid(retired[i],NULL,WNOHANG)!=0)retired[i]=0;
}
static void refresh_process(AppProcess *process) {
    if(process->active<0 || process->child<=0)return;
    if(!app_package_installed(process->active)){process_close(process);return;}
    char exe[2048],hash[65];
    snprintf(exe,sizeof(exe),"%s/../apps/%s/app",data_root,entries[process->active].key);
    if(!package_sha256(exe,hash) || strcmp(hash,process->process_hash))process_close(process);
}
static void root(char*out,const char*leaf){snprintf(out,2048,"%s/../apps/%s",data_root,leaf);}
static int valid_id(const char*id){return id && strlen(id)>2 && strlen(id)<64 && strspn(id,"abcdefghijklmnopqrstuvwxyz0123456789.-_")==strlen(id) && id[0]!='.' && !strstr(id,"..");}
static void deleted_path(char *path,const char *id) {
 char leaf[96];snprintf(leaf,sizeof(leaf),".deleted-%s",id);root(path,leaf);
}
static int deleted(const char *id) {
 char path[2048];deleted_path(path,id);return !access(path,F_OK);
}
int app_package_deleted(AppId id) {
 return id>=0 && id<APP_LIMIT && entries[id].removed;
}
int app_package_find(const char *key) {
 if(!key || !*key)return -1;
 for(int i=0;i<APP_LIMIT;i++)if(!strcmp(entries[i].key,key))return i;
 return -1;
}
#define identify app_package_find
int app_package_asset(AppId id,const char *name,char *path,size_t size) {
 if(!app_package_installed(id) || !package_path_valid(name))return 0;
 int n=snprintf(path,size,"%s/../apps/%s/%s",data_root,entries[id].key,name);
 return n>0 && (size_t)n<size;
}
static int app_manifest_valid(const Package *p) {
    return valid_id(p->id) && *p->name &&
           ((p->api==1 && !strcmp(p->format,"lumen-app-1")) ||
            (p->api==APP_PACKAGE_API && !strcmp(p->format,"lumen-app-2")));
}
int app_package_provider_id(PackageProvider role) {
    return role>=0 && role<PROVIDER_COUNT ? provider_ids[role] : -1;
}
const char *app_package_provider(PackageProvider role) {
    if(role<0 || role>=PROVIDER_COUNT || provider_ids[role]<0)return "";
    return entries[provider_ids[role]].key;
}
const char *app_legacy_key(int slot) {
    return slot>=0 && slot<55 ? legacy_keys[slot] : NULL;
}
const char *app_factory_provider(PackageProvider role) {
    return role>=0 && role<PROVIDER_COUNT ? builtin_providers[role] : "";
}
static void load_providers(void) {
    for(int role=0;role<PROVIDER_COUNT;role++) {
        int primary=identify(app_factory_provider(role));
        if(app_package_installed(primary) && (entries[primary].providers & (1u<<role))) {
            provider_ids[role]=primary;
            continue;
        }
        provider_ids[role]=-1;
        if(role==PROVIDER_DEVICE_CONTROL)continue;
        for(int id=0;id<APP_LIMIT;id++) {
            if(!entries[id].installed || !(entries[id].providers & (1u<<role)))continue;
            int fallback=provider_ids[role];
            if(fallback<0 || strcmp(entries[id].key,entries[fallback].key)<0)provider_ids[role]=id;
        }
    }
    char path[2048], line[160];
    snprintf(path,sizeof(path),"%s/providers.conf",data_root);
    FILE *file=fopen(path,"r");
    if(!file)return;
    while(fgets(line,sizeof(line),file)) {
        if(!strchr(line,'\n') && !feof(file)) {
            int ch;
            do ch=fgetc(file); while(ch!='\n' && ch!=EOF);
            continue;
        }
        char *value=strchr(line,'=');
        if(!value)continue;
        *value++=0;
        value[strcspn(value,"\r\n")]=0;
        int id=identify(value);
        if(!app_package_installed(id))continue;
        for(int role=0;role<PROVIDER_COUNT;role++)
            if(!strcmp(line,package_provider_names[role]) && (entries[id].providers & (1u<<role)))
                provider_ids[role]=id;
    }
    fclose(file);
}

static int read_manifest(const char*directory,Package*p){char name[2048],text[65537];snprintf(name,sizeof(name),"%s/manifest.json",directory);FILE*f=fopen(name,"rb");if(!f)return 0;size_t n=fread(text,1,65536,f);int ok=n<65536&&!ferror(f);fclose(f);text[n]=0;return ok&&package_manifest(text,p)&&app_manifest_valid(p);}
static int unused_app_handle(void) {
    for(int i=0;i<APP_LIMIT;i++)if(!*entries[i].key)return i;
    return -1;
}
void app_packages_scan(void) {
    for(int i=0;i<APP_LIMIT;i++) {
        entries[i].installed=0;
        entries[i].has_icon=0;
        entries[i].removed=*entries[i].key && deleted(entries[i].key);
    }
    char directory[2048];
    root(directory,"");
    DIR *recovery=opendir(directory);
    if(recovery) {
        struct dirent *item;
        while((item=readdir(recovery))) {
            if(strncmp(item->d_name,".previous-",10) || !valid_id(item->d_name+10))continue;
            char previous[2048],target[2048];
            root(previous,item->d_name);
            root(target,item->d_name+10);
            if(access(target,F_OK))rename(previous,target);
            else package_remove(previous);
        }
        closedir(recovery);
    }
    DIR *installed=opendir(directory);
    Package *manifest=installed ? malloc(sizeof(*manifest)) : NULL;
    if(manifest) {
        struct dirent *item;
        while((item=readdir(installed))) {
            if(!valid_id(item->d_name))continue;
            char path[2048];
            root(path,item->d_name);
            if(!read_manifest(path,manifest) || strcmp(manifest->id,item->d_name) || deleted(manifest->id))continue;
            int id=identify(manifest->id);
            if(id<0)id=unused_app_handle();
            if(id<0)continue;
            entries[id].installed=1;
            entries[id].removed=0;
            entries[id].api=manifest->api;
            entries[id].surfaces=manifest->surfaces;
            entries[id].providers=manifest->providers;
            entries[id].periodic_interval=manifest->periodic_interval;
            snprintf(entries[id].periodic_operation,sizeof(entries[id].periodic_operation),"%s",manifest->periodic_operation);
            if(strcmp(periodic_key[id],manifest->id)) {
                periodic_due[id]=0;
                snprintf(periodic_key[id],sizeof(periodic_key[id]),"%s",manifest->id);
            }
            entries[id].launcher_order=manifest->launcher_order;
            entries[id].launcher_visible=manifest->launcher_visible;
            snprintf(entries[id].key,sizeof(entries[id].key),"%s",manifest->id);
            snprintf(entries[id].name,sizeof(entries[id].name),"%s",manifest->name);
            snprintf(entries[id].version,sizeof(entries[id].version),"%s",manifest->version);
            entries[id].def=(AppDefinition){id,entries[id].name};
        }
        free(manifest);
    }
    if(installed)closedir(installed);
    load_providers();
    refresh_process(&foreground);
    for(int i=0;i<KEPT_APP_COUNT;i++)if(kept_apps[i])refresh_process(kept_apps[i]);
    if(service)refresh_process(service);
    if(periodic_process)refresh_process(periodic_process);
    for(int i=0;i<APP_OVERLAY_COUNT;i++)if(overlays[i])refresh_process(overlays[i]);
}
const AppDefinition *app_definition(AppId id){return app_package_installed(id) ? &entries[id].def : NULL;}
int app_package_installed(AppId id){return id>=0&&id<APP_LIMIT&&entries[id].installed;}
int app_package_foreground(AppId id){return app_package_installed(id) && (entries[id].surfaces & PACKAGE_FOREGROUND);}
int app_package_launcher_order(AppId id){return app_package_installed(id) ? entries[id].launcher_order : 100000;}
int app_package_launcher_visible(AppId id){return !app_package_installed(id) || entries[id].launcher_visible;}
const char *app_package_key(AppId id){return id>=0&&id<APP_LIMIT?entries[id].key:"";}
const char *app_package_version(AppId id){return app_package_installed(id)?entries[id].version:LUMEN_VERSION;}
static int executable_valid(const char*file){FILE*f=fopen(file,"rb");if(!f)return 0;unsigned char b[20];int ok=fread(b,1,20,f)==20&&!memcmp(b,"\177ELF\1\1\1",7)&&b[16]==2&&!b[17]&&b[18]==40&&!b[19];fclose(f);
#ifdef LUMEN_HOST
 struct stat st;ok=!stat(file,&st)&&S_ISREG(st.st_mode)&&st.st_size>100;
#endif
 return ok;}
int app_package_configure(const char *id,const char *key,const char *bytes,size_t length) {
    if(!valid_id(id) || !package_configuration_valid(key) || deleted(id) ||
       !length || length>2048 || memchr(bytes,0,length))return 0;
    char directory[2048];
    root(directory,id);
    Package *manifest=malloc(sizeof(*manifest));
    if(!manifest)return 0;
    int allowed=0;
    if(read_manifest(directory,manifest) && !strcmp(manifest->id,id))
        for(int i=0;i<manifest->configuration_count;i++)
            if(!strcmp(key,manifest->configuration[i]))allowed=1;
    free(manifest);
    if(!allowed)return 0;
    char state[2048],legacy[2048];
    int n=snprintf(state,sizeof(state),"%s/../appdata/%s",data_root,id);
    if(n<1 || (size_t)n>=sizeof(state))return 0;
    n=snprintf(legacy,sizeof(legacy),"%s/app-state/%s",data_root,id);
    if(n<1 || (size_t)n>=sizeof(legacy))return 0;
    if(access(state,F_OK) && !access(legacy,F_OK))
        return lumen_storage_write_at(legacy,key,bytes,length);
    return package_mkdirs(state) && lumen_storage_write_at(state,key,bytes,length);
}

int app_package_install(const char*archive) {
 char stage[2048],target[2048],previous[2048],exe[2048];root(stage,".installing");package_remove(stage);
 Package*p=malloc(sizeof(*p));if(!p)return 0;
 int ok=package_extract(archive,stage,p)&&app_manifest_valid(p);
 snprintf(exe,sizeof(exe),"%s/app",stage);ok=ok&&executable_valid(exe);
 if(ok){app_packages_scan();ok=identify(p->id)>=0 || unused_app_handle()>=0;}
 if(ok) {
   root(target,p->id);snprintf(previous,sizeof(previous),"%s/../apps/.previous-%s",data_root,p->id);package_remove(previous);
   struct stat st;int existed=!lstat(target,&st);if(existed && (!S_ISDIR(st.st_mode)||rename(target,previous)))ok=0;
   if(ok&&existed)sync();
   if(ok&&rename(stage,target)){if(existed)rename(previous,target);ok=0;}
   if(ok){chmod(exe,0700);snprintf(exe,sizeof(exe),"%s/app",target);chmod(exe,0700);char marker[2048];deleted_path(marker,p->id);unlink(marker);sync();package_remove(previous);app_packages_scan();}
 }
 if(!ok)package_remove(stage);free(p);return ok;
}
int app_package_can_remove(AppId id) {
    if(id<0 || id>=APP_LIMIT || !*entries[id].key || app_package_deleted(id))return 0;
    for(size_t i=0;i<sizeof(builtin_required)/sizeof(*builtin_required);i++)
        if(!strcmp(entries[id].key,builtin_required[i]))return 0;
    for(int role=0;role<PROVIDER_COUNT;role++)
        if(provider_ids[role]==id || !strcmp(entries[id].key,app_factory_provider(role)))return 0;
    return 1;
}
int app_package_remove(const char*id) {
 if(!valid_id(id) || !app_package_can_remove(app_package_find(id)))return 0;
 char key[64], dir[2048], marker[2048];snprintf(key,sizeof(key),"%s",id);
 root(dir,key);deleted_path(marker,key);
 if(!atomic_write(marker,"1",1))return 0;
 int ok=package_remove(dir);
 if(!ok)unlink(marker);
 app_packages_scan();
 return ok;
}
static int read_icon(const char *path,unsigned char *pixels) {
 FILE *file=fopen(path,"rb");
 if(!file)return 0;
 static const char header[]="P5\n76 76\n255\n";
 char actual[sizeof(header)-1];
 int ok=fread(actual,1,sizeof(actual),file)==sizeof(actual) &&
        !memcmp(actual,header,sizeof(actual)) &&
        fread(pixels,1,76*76,file)==76*76 && fgetc(file)==EOF;
 fclose(file);
 return ok;
}
int app_package_icon(AppId id,unsigned char pixels[76*76]) {
 if(id<0 || id>=APP_LIMIT || !*entries[id].key || entries[id].has_icon<0)return 0;
 char file[2048],leaf[160];
 int ok=0;
 if(app_package_installed(id)) {
     snprintf(leaf,sizeof(leaf),"%s/assets/icon.pgm",entries[id].key);root(file,leaf);
     ok=read_icon(file,pixels);
     if(!ok) {
         snprintf(leaf,sizeof(leaf),"%s/icon.pgm",entries[id].key);root(file,leaf);
         ok=read_icon(file,pixels);
     }
 }
 entries[id].has_icon=ok ? 1 : -1;
 return ok;
}
static void json_text(FILE *out, const char *text) {
    fputc('"', out);
    for (; *text; text++) {
        unsigned char c = *text;
        switch (c) {
        case '"': case '\\': fputc('\\', out); fputc(c, out); break;
        case '\n': fputs("\\n", out); break;
        case '\r': fputs("\\r", out); break;
        case '\t': fputs("\\t", out); break;
        default:
            if (c < 32) fprintf(out, "\\u%04x", c);
            else fputc(c, out);
        }
    }
    fputc('"', out);
}
static void clear_requests(AppProcess *p) {
    for(unsigned i=0;i<p->request_count;i++)
        free(p->requests[(p->request_first+i)%APP_REQUEST_LIMIT]);
    p->request_first=p->request_count=0;
}
static char *take_request(AppProcess *p) {
    if(!p->request_count)return NULL;
    char *request=p->requests[p->request_first];
    p->request_first=(p->request_first+1)%APP_REQUEST_LIMIT;
    p->request_count--;
    return request;
}
static int queue_request(AppProcess *p) {
    if(p->request_count==APP_REQUEST_LIMIT)return 0;
    char *request=strdup(p->incoming);
    if(!request)return 0;
    p->requests[(p->request_first+p->request_count)%APP_REQUEST_LIMIT]=request;
    p->request_count++;
    return 1;
}
static void process_close(AppProcess *p) {
 clear_requests(p);
 if(*p->render_dir) {package_remove(p->render_dir);p->render_dir[0]=0;}
 if(p->child>0){kill(-p->child,SIGTERM);kill(p->child,SIGTERM);waitpid(p->child,NULL,WNOHANG);}
 if(p->child>0 && waitpid(p->child,NULL,WNOHANG)==0)for(int i=0;i<64;i++)if(!retired[i]){retired[i]=p->child;break;}
 p->child=0;if(p->read_fd>=0)close(p->read_fd);if(p->write_fd>=0)close(p->write_fd);p->read_fd=p->write_fd=-1;p->active=-1;p->frame[0]=p->failure[0]=p->data_dir[0]=0;p->incoming_n=p->pending_n=0;p->frame_back=p->frame_ready=p->frame_tick=p->frame_loading=0;
}
static void flush_events(AppProcess *p) {
 while(p->write_fd>=0 && p->pending_n){ssize_t n=write(p->write_fd,p->pending,p->pending_n);if(n<=0)break;memmove(p->pending,p->pending+n,p->pending_n-n);p->pending_n-=n;}
}
static int process_send(AppProcess *p,const char*event) {
 if(p->write_fd<0||!event)return 0;size_t n=strlen(event);if(n>49152 || n+1>sizeof(p->pending)-p->pending_n)return 0;
 memcpy(p->pending+p->pending_n,event,n);p->pending_n+=n;p->pending[p->pending_n++]='\n';flush_events(p);return 1;
}
static void process_pause(AppProcess *p){
    if(p->child>0 && !p->paused) {
        process_send(p,"{\"event\":\"pause\"}");
        p->paused=1;
        clear_requests(p);
    }
}
static void close_inherited_fds(void) {
#ifdef __linux__
    DIR *directory=opendir("/proc/self/fd");
#else
    DIR *directory=opendir("/dev/fd");
#endif
    if(!directory) {
        for(int fd=3;fd<1024;fd++)close(fd);
        return;
    }
    int own=dirfd(directory);
    struct dirent *entry;
    while((entry=readdir(directory))) {
        char *end;
        long fd=strtol(entry->d_name,&end,10);
        if(!*end && fd>=3 && fd<=INT_MAX && fd!=own)close((int)fd);
    }
    closedir(directory);
}
static int process_open(AppProcess *p,AppId id,const char *role) {
    unsigned surface=!strcmp(role,"overlay") ? PACKAGE_OVERLAY :
                     !strcmp(role,"service") ? PACKAGE_BACKGROUND : PACKAGE_FOREGROUND;
    if(app_package_installed(id) && !(entries[id].surfaces & surface))return 0;
 if(!app_package_installed(id))return 0;
 if(p->active==(int)id && p->child>0 && p->paused)process_poll(p);
 if(p->active==(int)id && p->child>0) {
    if(p->paused) {
        char event[128];
        int background_action=p==&foreground && service_background_owner==(int)id ? service_action : 0;
        snprintf(event,sizeof(event),"{\"event\":\"resume\",\"background_action\":%d,\"language\":%d}",background_action,options[OPT_LANGUAGE]);
        process_send(p,event);
    }
    p->paused=0;
    return 1;
 }
 reap_apps();process_close(p);signal(SIGPIPE,SIG_IGN);int in[2],out[2];if(pipe(in))return 0;if(pipe(out)){close(in[0]);close(in[1]);return 0;}
 char dir[2048],file[2048],state[2048];root(dir,entries[id].key);snprintf(file,sizeof(file),"%s/app",dir);snprintf(state,sizeof(state),"%s/../appdata/%s",data_root,entries[id].key);
 char*absolute=realpath(dir,NULL);if(absolute){snprintf(dir,sizeof(dir),"%s",absolute);free(absolute);snprintf(file,sizeof(file),"%s/app",dir);}
 char old_state[2048],parent[2048];
 snprintf(old_state,sizeof(old_state),"%s/app-state/%s",data_root,entries[id].key);
 snprintf(parent,sizeof(parent),"%s/../appdata",data_root);
 int state_ready=package_mkdirs(parent);
 struct stat old_info,new_info;
 if(state_ready && !lstat(state,&new_info))
     state_ready=S_ISDIR(new_info.st_mode);
 else if(state_ready && errno==ENOENT && !lstat(old_state,&old_info))
     {
         state_ready=S_ISDIR(old_info.st_mode);
         if(state_ready && system_update_trial())snprintf(state,sizeof(state),"%s",old_state);
         else if(state_ready)state_ready=!rename(old_state,state);
     }
 else if(state_ready && errno==ENOENT)
     state_ready=package_mkdirs(state);
 else state_ready=0;
 if(!state_ready){close(in[0]);close(in[1]);close(out[0]);close(out[1]);return 0;}
 absolute=realpath(state,NULL);if(absolute){snprintf(state,sizeof(state),"%s",absolute);free(absolute);}
 char *legacy=realpath(data_root,NULL);
 if(legacy){snprintf(old_state,sizeof(old_state),"%s",legacy);free(legacy);}
 else old_state[0]=0;
 package_sha256(file,p->process_hash);
 if(!strcmp(role,"foreground")) {
     snprintf(p->render_dir,sizeof(p->render_dir),"/tmp/lumen-surface-XXXXXX");
     if(!mkdtemp(p->render_dir))p->render_dir[0]=0;
 }
 p->child=fork();if(p->child<0){p->child=0;process_close(p);close(in[0]);close(in[1]);close(out[0]);close(out[1]);return 0;}
 if(!p->child){setpgid(0,0);signal(SIGTERM,SIG_DFL);signal(SIGPIPE,SIG_DFL);dup2(in[0],0);dup2(out[1],1);close(in[0]);close(in[1]);close(out[0]);close(out[1]);
   int null=open("/dev/null",O_WRONLY);dup2(null,2);close(null);close_inherited_fds();chdir(dir);setenv("LUMEN_APP_DATA",state,1);setenv("LUMEN_APP_ASSETS",dir,1);setenv("LUMEN_LEGACY_DATA",old_state,1);
   if(*p->render_dir)setenv("LUMEN_APP_SURFACES",p->render_dir,1);
   else unsetenv("LUMEN_APP_SURFACES");
   char ca[2048];
   int ca_length=snprintf(ca,sizeof(ca),"%s/../certs/ca-bundle.pem",old_state);
   if(*old_state && ca_length>0 && (size_t)ca_length<sizeof(ca) && !access(ca,R_OK))
       setenv("CURL_CA_BUNDLE",ca,0);
   execl(file,file,NULL);_exit(127);}
 close(in[0]);close(out[1]);p->write_fd=in[1];p->read_fd=out[0];fcntl(p->write_fd,F_SETFL,O_NONBLOCK);fcntl(p->read_fd,F_SETFL,O_NONBLOCK);fcntl(p->write_fd,F_SETFD,FD_CLOEXEC);fcntl(p->read_fd,F_SETFD,FD_CLOEXEC);
 snprintf(p->data_dir,sizeof(p->data_dir),"%s",state);
 p->active=id;p->paused=0;p->generation=++process_generation;
 if(p==service || p==periodic_process || p==system_query)return 1;
 char*event=NULL;size_t n=0;FILE*f=open_memstream(&event,&n);
 int background_action=p==&foreground && service_background_owner==(int)id ? service_action : 0;
 fprintf(f,"{\"event\":\"open\",\"api\":%d,\"surface_api\":2,\"catalog_query\":1,\"reader_preferences\":1,\"width\":600,\"height\":800,\"content_y\":56,\"language\":%d,\"background_action\":%d,\"data_dir\":",entries[id].api,options[OPT_LANGUAGE],background_action);json_text(f,state);fputs(",\"role\":",f);json_text(f,role);fputc('}',f);fclose(f);int ok=process_send(p,event);free(event);return ok;
}
static int wire_true(const char *key) {
    const WireField *field=wire_get(key);
    return field && field->end-field->value==4 && !memcmp(field->value,"true",4);
}
static int process_poll(AppProcess *p) {
 reap_apps();if(p->child<=0)return 0;flush_events(p);
 int changed=0;char buf[16384];size_t received=0;ssize_t n=-1;
 while(received<APP_FRAME_MAX*2 && (n=read(p->read_fd,buf,sizeof(buf)))>0) {
   received+=n;
   for(ssize_t i=0;i<n;i++) {
     if(buf[i]=='\n') {
       p->incoming[p->incoming_n]=0;
       if(strlen(p->incoming)==p->incoming_n && wire_read(p->incoming)) {
           const WireField *request=wire_get("request");
           if(request) {
               if(*request->value=='"' && p->incoming_n<APP_REQUEST_MAX && !p->paused &&
                  (p==&foreground || p==service || p==periodic_process || p==system_query)) {
                   if(!queue_request(p)) {
                       process_close(p);
                       snprintf(p->failure,sizeof(p->failure),"Zu viele App-Anfragen.");
                       return 1;
                   }
                   changed=1;
                   if(p==service || p==periodic_process || p==system_query){p->incoming_n=0;return changed;}
               }
           } else if(p!=service && p!=periodic_process && p!=system_query) {
               p->frame_back=wire_true("back");
               p->frame_ready=wire_true("ready");
               p->frame_tick=lumen_number("tick");
               const WireField *ready=wire_get("ready");
               p->frame_loading=ready && ready->end-ready->value==5 &&
                   !memcmp(ready->value,"false",5);
               p->frame_serial=++frame_serial;
               if(strcmp(p->frame,p->incoming)) {
                   memcpy(p->frame,p->incoming,p->incoming_n+1);
                   changed=1;
               }
           }
       }
       p->incoming_n=0;
     } else if(p->incoming_n+1<sizeof(p->incoming))p->incoming[p->incoming_n++]=buf[i];
     else {
       process_close(p);
       snprintf(p->failure,sizeof(p->failure),"App-Ausgabe zu groß.");
       return 1;
     }
   }
 }
 int status;pid_t done=waitpid(p->child,&status,WNOHANG);
 if(done==p->child || n==0) {
   if(done==p->child)p->child=0;
   process_close(p);
   snprintf(p->failure,sizeof(p->failure),"App wurde beendet. Bitte erneut öffnen.");
   changed=1;
 }
 return changed;
}

enum { SERVICE_TIMEOUT_MS=30000, BACKGROUND_SERVICE_TIMEOUT_MS=130000 };
static int64_t service_now(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC,&now);
    return (int64_t)now.tv_sec*1000+now.tv_nsec/1000000;
}
static void system_query_close(void) {
    if(!system_query)return;
    process_close(system_query);
    free(system_query);
    system_query=NULL;
    system_query_result=NULL;
}
void app_system_query_poll(void) {
    if(!system_query)return;
    process_poll(system_query);
    char *response=take_request(system_query);
    if(response) {
        char data[2048],kind[48];
        int ok=wire_read(response) && wire_true("ok") &&
            wire_bounded_string("request",kind,sizeof(kind)) && !strcmp(kind,"service_result") &&
            wire_bounded_string("data",data,sizeof(data));
        free(response);
        void (*complete)(const char *)=system_query_result;
        system_query_close();
        if(ok)complete(data);
    } else if(system_query->child<=0 || service_now()>=system_query_deadline)system_query_close();
}
int app_system_request(PackageProvider provider,const char *operation,void (*complete)(const char *)) {
    if(system_query || !complete || !package_operation_valid(operation))return 0;
    int id=app_package_provider_id(provider);
    if(id<0 || !(entries[id].surfaces & PACKAGE_BACKGROUND))return 0;
    char directory[2048];
    root(directory,entries[id].key);
    Package *manifest=malloc(sizeof(*manifest));
    if(!manifest)return 0;
    int declared=0;
    if(read_manifest(directory,manifest))
        for(int i=0;i<manifest->operation_count;i++)if(!strcmp(operation,manifest->operations[i]))declared=1;
    free(manifest);
    if(!declared)return 0;
    system_query=calloc(1,sizeof(*system_query));
    if(!system_query)return 0;
    system_query->read_fd=system_query->write_fd=system_query->active=-1;
    if(!process_open(system_query,id,"service")){system_query_close();return 0;}
    char event[240];
    snprintf(event,sizeof(event),
        "{\"event\":\"service_call\",\"language\":%d,\"caller\":\"system\",\"operation\":\"%s\",\"data\":\"\"}",
        options[OPT_LANGUAGE],operation);
    if(!process_send(system_query,event)){system_query_close();return 0;}
    system_query_result=complete;
    system_query_deadline=service_now()+5000;
    return 1;
}
static void periodic_close(void) {
    if(!periodic_process)return;
    process_close(periodic_process);
    free(periodic_process);
    periodic_process=NULL;
}
void app_periodic_poll(int awake) {
    int64_t now=service_now();
    if(periodic_process) {
        process_poll(periodic_process);
        if(periodic_process->child<=0 || periodic_process->request_count || now>=periodic_deadline)
            periodic_close();
        return;
    }
    if(!awake || now<periodic_probe)return;
    periodic_probe=now+30000;
    int id=-1;
    for(int i=0;i<APP_LIMIT;i++)
        if(entries[i].installed && entries[i].periodic_interval && periodic_due[i]<=now &&
           foreground.active!=i && (!service || service->active!=i) &&
           (id<0 || periodic_due[i]<periodic_due[id]))id=i;
    if(id<0 || !device_wifi_enabled())return;
    char address[80];
    device_ip(address,sizeof(address));
    if(!*address)return;
    periodic_due[id]=now+(int64_t)entries[id].periodic_interval*1000;
    periodic_process=calloc(1,sizeof(*periodic_process));
    if(!periodic_process)return;
    periodic_process->read_fd=periodic_process->write_fd=periodic_process->active=-1;
    if(!process_open(periodic_process,id,"service")){periodic_close();return;}
    char event[240];
    snprintf(event,sizeof(event),
        "{\"event\":\"service_call\",\"language\":%d,\"caller\":\"\",\"operation\":\"%s\",\"data\":\"\"}",
        options[OPT_LANGUAGE],entries[id].periodic_operation);
    if(!process_send(periodic_process,event)){periodic_close();return;}
    periodic_deadline=now+210000;
}
/* Transfer runs in its own process. Its short-lived export subprocess
 * never touches the foreground package or its IPC descriptors. */
int app_package_export(const char *id,const char *operation,const char *data,
                       const char *output,char *result,size_t capacity) {
    if(!valid_id(id) || deleted(id) || strncmp(operation,"export_",7) ||
       !package_operation_valid(operation) || strlen(data)>LUMEN_SERVICE_DATA_MAX)return 0;
    char directory[2048];
    root(directory,id);
    Package *manifest=malloc(sizeof(*manifest));
    if(!manifest)return 0;
    int declared=0;
    if(read_manifest(directory,manifest) && !strcmp(id,manifest->id) &&
       (manifest->surfaces & PACKAGE_BACKGROUND))
        for(int i=0;i<manifest->operation_count;i++)
            if(!strcmp(operation,manifest->operations[i]))declared=1;
    free(manifest);
    if(!declared)return 0;

    char *assets=realpath(directory,NULL),*legacy=realpath(data_root,NULL);
    if(!assets || !legacy){free(assets);free(legacy);return 0;}
    FILE *input=tmpfile();
    if(!input){free(assets);free(legacy);return 0;}
    fputs("{\"event\":\"service_call\",\"caller\":\"transfer\",\"operation\":",input);
    json_text(input,operation);
    fputs(",\"data\":",input);
    json_text(input,data);
    fputs("}\n",input);
    int out[2];
    if(fflush(input) || fseek(input,0,SEEK_SET) || pipe(out)){
        fclose(input);free(assets);free(legacy);return 0;
    }
    pid_t child=fork();
    if(child==0) {
        setpgid(0,0);
        signal(SIGTERM,SIG_DFL);signal(SIGINT,SIG_DFL);signal(SIGPIPE,SIG_DFL);
        close(out[0]);
        if(dup2(fileno(input),STDIN_FILENO)<0 || dup2(out[1],STDOUT_FILENO)<0)_exit(127);
        close_inherited_fds();
        if(chdir(assets) || setenv("LUMEN_APP_ASSETS",assets,1) ||
           setenv("LUMEN_LEGACY_DATA",legacy,1) || setenv("LUMEN_EXPORT_DATA",output,1))_exit(127);
        unsetenv("LUMEN_APP_DATA");
        execl("./app","./app",(char *)NULL);
        _exit(127);
    }
    fclose(input);close(out[1]);free(assets);free(legacy);
    if(child<0){close(out[0]);return 0;}
    setpgid(child,child);
    char line[4096];size_t used=0;
    int64_t deadline=service_now()+5000;
    while(used+1<sizeof(line)) {
        int64_t left=deadline-service_now();
        if(left<=0)break;
        struct pollfd fd={out[0],POLLIN,0};
        int ready=poll(&fd,1,(int)left);
        if(ready<0 && errno==EINTR)continue;
        if(ready<=0)break;
        ssize_t size=read(out[0],line+used,sizeof(line)-used-1);
        if(size<0 && errno==EINTR)continue;
        if(size<=0)break;
        used+=(size_t)size;
        if(memchr(line,'\n',used))break;
    }
    close(out[0]);
    kill(-child,SIGKILL);kill(child,SIGKILL);
    while(waitpid(child,NULL,0)<0 && errno==EINTR){}
    line[used]=0;
    char kind[32],*newline=strchr(line,'\n');
    if(!newline)return 0;
    *newline=0;
    return wire_read(line) && wire_bounded_string("request",kind,sizeof(kind)) &&
        !strcmp(kind,"service_result") && wire_true("ok") &&
        wire_bounded_string("data",result,capacity);
}
static void service_result(int ok,const char *status,const char *data) {
    char *event=NULL;
    size_t size=0;
    FILE *stream=open_memstream(&event,&size);
    if(!stream)return;
    fprintf(stream,"{\"event\":\"service\",\"action\":%d,\"ok\":%d,\"status\":",service_action,ok);
    json_text(stream,status);
    fputs(",\"data\":",stream);
    json_text(stream,data);
    fputc('}',stream);
    if(!fclose(stream))process_send(&foreground,event);
    free(event);
}
static void service_close(void) {
    if(!service)return;
    process_close(service);
    free(service);
    service=NULL;
}
static void service_finish(int ok,const char *status,const char *data) {
    service_close();
    int same_caller=foreground.generation==service_caller ||
        (service_background_owner>=0 && foreground.active==service_background_owner);
    if(foreground.child>0 && same_caller && !foreground.paused)service_result(ok,status,data);
    service_caller=0;
    service_background_owner=-1;
}
static int start_worker(const char *id,const char *operation,const char *data,int action,int background) {
    if(service_caller && service_background_owner<0 &&
       (foreground.generation!=service_caller || foreground.paused))
        service_finish(0,"cancelled","");
    if(service_caller || foreground.child<=0 || foreground.paused ||
       !package_operation_valid(operation) || strlen(data)>LUMEN_SERVICE_DATA_MAX)return 0;
    int app=identify(id);
    if(!app_package_installed(app) || (background && (foreground.active!=app || action<=0)))return 0;
    char directory[2048];
    root(directory,id);
    Package *manifest=malloc(sizeof(*manifest));
    if(!manifest)return 0;
    int declared=0;
    if(read_manifest(directory,manifest))
        for(int i=0;i<manifest->operation_count;i++)
            if(!strcmp(operation,manifest->operations[i]))declared=1;
    free(manifest);
    if(!declared)return 0;
    service=calloc(1,sizeof(*service));
    if(!service)return 0;
    service->read_fd=service->write_fd=service->active=-1;
    if(!process_open(service,app,"service")){service_close();return 0;}
    char *event=NULL;
    size_t size=0;
    FILE *stream=open_memstream(&event,&size);
    if(!stream){service_close();return 0;}
    fprintf(stream,"{\"event\":\"service_call\",\"language\":%d,\"caller\":",options[OPT_LANGUAGE]);
    json_text(stream,entries[foreground.active].key);
    fputs(",\"operation\":",stream);
    json_text(stream,operation);
    fputs(",\"data\":",stream);
    json_text(stream,data);
    fputc('}',stream);
    int ok=!fclose(stream) && process_send(service,event);
    free(event);
    if(!ok){service_close();return 0;}
    service_action=action;
    service_caller=foreground.generation;
    service_background_owner=background ? app : -1;
    service_deadline=service_now()+(background ? BACKGROUND_SERVICE_TIMEOUT_MS : SERVICE_TIMEOUT_MS);
    return 1;
}
int app_service_start(const char *id,const char *operation,const char *data,int action) {
    return start_worker(id,operation,data,action,0);
}
int app_service_start_background(const char *id,const char *operation,const char *data,int action) {
    return start_worker(id,operation,data,action,1);
}
int app_service_fd(void){return service ? service->read_fd : -1;}
int app_service_wait_ms(void) {
    if(!service_caller)return -1;
    int64_t left=service_deadline-service_now();
    return left>0 ? (int)left : 0;
}
void app_service_poll(void) {
    if(!service_caller)return;
    if(service_background_owner<0 &&
       (foreground.generation!=service_caller || foreground.paused || foreground.child<=0)) {
        service_finish(0,"cancelled","");
        return;
    }
    process_poll(service);
    char *request=take_request(service);
    if(request) {
        int parsed=wire_read(request);
        char kind[32],data[LUMEN_SERVICE_DATA_MAX+1];
        const WireField *status=wire_get("ok");
        int ok=wire_true("ok");
        int valid=parsed && wire_bounded_string("request",kind,sizeof(kind)) && !strcmp(kind,"service_result") &&
           (ok || (status && status->end-status->value==5 && !memcmp(status->value,"false",5))) &&
           wire_bounded_string("data",data,sizeof(data));
        free(request);
        if(valid) {
            service_finish(ok,ok ? "success" : "error",data);
            return;
        }
        service_finish(0,"invalid_result","");
        return;
    }
    if(service->child<=0)service_finish(0,"crashed","");
    else if(!app_service_wait_ms())service_finish(0,"timeout","");
}

static void dispose_foreground(AppProcess *process) {
    if(!process)return;
    process_close(process);
    if(process!=&initial_foreground)free(process);
}
static void keep_foreground(AppProcess *process) {
    if(process->child<=0){dispose_foreground(process);return;}
    process_pause(process);
    dispose_foreground(kept_apps[KEPT_APP_COUNT-1]);
    for(int i=KEPT_APP_COUNT-1;i>0;i--)kept_apps[i]=kept_apps[i-1];
    kept_apps[0]=process;
}
void app_process_close(void) {
    periodic_close();
    system_query_close();
    service_close();service_caller=0;service_background_owner=-1;
    dispose_foreground(active_foreground);
    active_foreground=&initial_foreground;
    for(int i=0;i<KEPT_APP_COUNT;i++){
        dispose_foreground(kept_apps[i]);kept_apps[i]=NULL;
    }
    for(int i=0;i<APP_OVERLAY_COUNT;i++)app_overlay_close(i);
}
/* A forked system worker must close its inherited pipe ends without
 * signalling the parent's children or removing their transient surfaces. */
void app_process_detach_inherited(void) {
    AppProcess *processes[]={active_foreground,service,
        kept_apps[0],kept_apps[1],kept_apps[2]};
    for(size_t i=0;i<sizeof(processes)/sizeof(processes[0]);i++) {
        if(!processes[i])continue;
        processes[i]->child=0;
        processes[i]->render_dir[0]=0;
    }
    for(int i=0;i<APP_OVERLAY_COUNT;i++)if(overlays[i])overlays[i]->child=0;
    memset(retired,0,sizeof(retired));
    app_process_close();
}
void app_process_pause(void) {
    process_pause(&foreground);
    if(service_caller && service_background_owner<0)service_finish(0,"cancelled","");
}
int app_process_open(AppId id) {
    if(!app_package_foreground(id))return 0;
    if(foreground.active==(int)id)return process_open(&foreground,id,"foreground");
    AppProcess *next=NULL;
    for(int i=0;i<KEPT_APP_COUNT;i++)if(kept_apps[i] && kept_apps[i]->active==(int)id) {
        next=kept_apps[i];
        for(int j=i;j<KEPT_APP_COUNT-1;j++)kept_apps[j]=kept_apps[j+1];
        kept_apps[KEPT_APP_COUNT-1]=NULL;
        break;
    }
    if(!next && foreground.child<=0)return process_open(&foreground,id,"foreground");
    if(!next) {
        next=calloc(1,sizeof(*next));
        if(!next)return 0;
        next->active=next->read_fd=next->write_fd=-1;
    }
    app_process_pause();
    AppProcess *previous=active_foreground;
    active_foreground=next;
    keep_foreground(previous);
    return process_open(&foreground,id,"foreground");
}
int app_process_owner(void){return foreground.active;}
const char *app_process_data_dir(void){return foreground.data_dir;}
const char *app_process_render_dir(void){return foreground.render_dir;}
int app_process_fd(void){return foreground.read_fd;}
int app_overlay_fd(int slot){return slot>=0 && slot<APP_OVERLAY_COUNT && overlays[slot] ? overlays[slot]->read_fd : -1;}
int app_process_pending(void){
 if(foreground.pending_n || (service && service->pending_n))return 1;
 for(int i=0;i<APP_OVERLAY_COUNT;i++)if(overlays[i] && overlays[i]->pending_n)return 1;
 return 0;
}
void app_process_flush(void){flush_events(&foreground);if(service)flush_events(service);for(int i=0;i<APP_OVERLAY_COUNT;i++)if(overlays[i])flush_events(overlays[i]);}
int app_process_send(const char *event){return process_send(&foreground,event);}
int app_process_poll(void){return process_poll(&foreground);}
int app_process_frame_back(void){return foreground.frame_back;}
int app_process_frame_ready(void){return foreground.frame_ready;}
int app_process_frame_tick(void){return foreground.frame_tick;}
int app_process_frame_loading(void){return foreground.frame_loading || !*foreground.frame;}
const char *app_process_frame(void){return foreground.frame;}
uint64_t app_process_frame_serial(void){return foreground.frame_serial;}
const char *app_process_request(void){
    static char taken[APP_REQUEST_MAX];
    char *request=take_request(&foreground);
    if(!request)return "";
    memcpy(taken,request,strlen(request)+1);
    free(request);
    return taken;
}
const char *app_process_error(void){return foreground.failure;}
static AppProcess *overlay_process(int slot) {
    return slot>=0 && slot<APP_OVERLAY_COUNT ? overlays[slot] : NULL;
}
int app_overlay_open(const char *id,const char *event) {
    int app=identify(id);
    if(!app_package_installed(app) || !(entries[app].surfaces & PACKAGE_OVERLAY))return -1;
    int slot=0;
    while(slot<APP_OVERLAY_COUNT && overlays[slot])slot++;
    if(slot==APP_OVERLAY_COUNT)return -1;
    AppProcess *process=calloc(1,sizeof(*process));
    if(!process)return -1;
    overlays[slot]=process;
    process->active=process->read_fd=process->write_fd=-1;
    if(process_open(process,app,"overlay") && process_send(process,event))return slot;
    app_overlay_close(slot);
    return -1;
}
int app_provider_overlay_slot(PackageProvider role) {
    if(role<PROVIDER_TEXT_INPUT || role>PROVIDER_SLEEP)return -1;
    return overlay_provider_slots[role];
}
PackageProvider app_overlay_role(int slot) {
    if(slot>=0)
        for(int role=0;role<PROVIDER_COUNT;role++)
            if(overlay_provider_slots[role]==slot)return (PackageProvider)role;
    return PROVIDER_COUNT;
}
int app_provider_overlay_send(PackageProvider role,const char *event) {
    if(role<0 || role>=PROVIDER_COUNT)return 0;
    int id=app_package_provider_id(role);
    if(id<0 || !(entries[id].surfaces & PACKAGE_OVERLAY))return 0;
    int slot=overlay_provider_slots[role];
    AppProcess *process=overlay_process(slot);
    const char *key=app_package_provider(role);
    if(process && process->child>0 && process->active==identify(key))
        return process_send(process,event);
    if(process)app_overlay_close(slot);
    slot=app_overlay_open(key,event);
    overlay_provider_slots[role]=slot;
    return slot>=0;
}
void app_overlay_close(int slot) {
    AppProcess *process=overlay_process(slot);
    if(!process)return;
    for(int role=0;role<PROVIDER_COUNT;role++)
        if(overlay_provider_slots[role]==slot)overlay_provider_slots[role]=-1;
    process_close(process);
    free(process);
    overlays[slot]=NULL;
}
int app_overlay_event(int slot,const char *event) {
    AppProcess *process=overlay_process(slot);
    return process && process_send(process,event);
}
int app_overlay_poll(int slot) {
    AppProcess *process=overlay_process(slot);
    return process && process_poll(process);
}
const char *app_overlay_frame(int slot) {
    AppProcess *process=overlay_process(slot);
    return process ? process->frame : "";
}
uint64_t app_overlay_frame_serial(int slot) {
    AppProcess *process=overlay_process(slot);
    return process ? process->frame_serial : 0;
}
static int version_newer(const char*a,const char*b){for(int i=0;i<3;i++){long x=strtol(a,(char**)&a,10),y=strtol(b,(char**)&b,10);if(x!=y)return x>y;if(*a=='.')a++;if(*b=='.')b++;}return 0;}
void app_packages_seed(void) {
 char directory[2048];snprintf(directory,sizeof(directory),"%s/../app-packages",data_root);
#ifdef LUMEN_HOST
 if(access(directory,F_OK))snprintf(directory,sizeof(directory),"build/apps/host");
#endif
 DIR*d=opendir(directory);if(!d)return;Package*p=malloc(sizeof(*p));if(!p){closedir(d);return;}
 struct dirent*e;
 while((e=readdir(d))) {
   if(e->d_name[0]=='.')continue;char archive[2048];snprintf(archive,sizeof(archive),"%s/%s",directory,e->d_name);
#ifdef LUMEN_HOST
   struct stat st;if(!stat(archive,&st)&&S_ISDIR(st.st_mode))snprintf(archive,sizeof(archive),"%s/%s/%s.lumen-app",directory,e->d_name,e->d_name);
#endif
   if(!package_peek(archive,p)||!app_manifest_valid(p)||deleted(p->id))continue;
   int id=identify(p->id);
   if(id<0 || !app_package_installed(id) || version_newer(p->version,entries[id].version))app_package_install(archive);
 }
 free(p);closedir(d);app_packages_scan();
}
