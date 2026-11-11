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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef LUMEN_HOST
#include "host_icons.h"
#endif
/* IDs are a package identity, never an origin or privilege class. */
static const char *keys[APP_COUNT]={
 "lumen.library","lumen.reader","lumen.settings","lumen.store","lumen.transfer","lumen.calendar","lumen.insights","lumen.minesweeper","lumen.hangman","lumen.tetris","lumen.snake","lumen.calculator","lumen.clock","lumen.notes","retired.drawing","lumen.weather","lumen.home-assistant","lumen.wikipedia","lumen.quotes","lumen.chess","lumen.tic-tac-toe","retired.numscratch","lumen.2048","lumen.solitaire","lumen.sudoku","lumen.braintrain","lumen.news","lumen.wordle","lumen.photos","lumen.languages","lumen.todo","lumen.flashcards","lumen.planetarium","lumen.nonograms","lumen.hex","lumen.status-bar","retired.tabs","lumen.lock-screen","lumen.keyboard","lumen.journal","lumen.study","lumen.bingo","lumen.connect-four","lumen.recipes","lumen.periodic-table","retired.files","lumen.knights-tour","lumen.sokoban","lumen.maze","lumen.crossword","retired.rpg","lumen.openai","lumen.sliding-puzzle","lumen.frog-solitaire","lumen.home"};
static struct {AppDefinition def;char key[64],name[96],version[32];int installed,removed,has_icon;} entries[APP_LIMIT];
typedef struct {
    pid_t child;
    int read_fd,write_fd,active,paused;
    size_t incoming_n,pending_n;
    uint64_t frame_serial;
    char process_hash[65],pending[65536],incoming[APP_FRAME_MAX],frame[APP_FRAME_MAX];
    char request[32768],failure[160];
} AppProcess;

static AppProcess foreground={.read_fd=-1,.write_fd=-1,.active=-1};
static AppProcess overlays[APP_OVERLAY_COUNT]={
    {.read_fd=-1,.write_fd=-1,.active=-1},
    {.read_fd=-1,.write_fd=-1,.active=-1}
};
static pid_t retired[64];
static uint64_t frame_serial;

static void process_close(AppProcess *process);
static void reap_apps(void) {
    for(int i=0;i<64;i++)
        if(retired[i] && waitpid(retired[i],NULL,WNOHANG)!=0)retired[i]=0;
}
static void refresh_process(AppProcess *process) {
    if(process->active<0 || process->child<=0)return;
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
 if(!app_package_installed(id) || !package_path_valid(name) || strchr(name,'/'))return 0;
 int n=snprintf(path,size,"%s/../apps/%s/%s",data_root,entries[id].key,name);
 return n>0 && (size_t)n<size;
}
static int read_manifest(const char*directory,Package*p){char name[2048],text[65537];snprintf(name,sizeof(name),"%s/manifest.json",directory);FILE*f=fopen(name,"rb");if(!f)return 0;size_t n=fread(text,1,65536,f);int ok=n<65536&&!ferror(f);fclose(f);text[n]=0;return ok&&package_manifest(text,p)&&!strcmp(p->format,"lumen-app-1")&&p->api==APP_PACKAGE_API&&valid_id(p->id);}
void app_packages_scan(void) {
 for(int i=0;i<APP_LIMIT;i++){entries[i].installed=0;entries[i].has_icon=0;entries[i].def=(AppDefinition){i,entries[i].name,-1};entries[i].name[0]=0;if(i<APP_COUNT)snprintf(entries[i].key,64,"%s",keys[i]);else entries[i].key[0]=0;}
 char file[2048];root(file,"slots.txt");FILE*f=fopen(file,"rb");if(f){int slot;char id[64];while(fscanf(f,"%d %63s",&slot,id)==2)if(slot>=APP_COUNT&&slot<APP_LIMIT&&valid_id(id))snprintf(entries[slot].key,64,"%s",id);fclose(f);}
 for(int i=0;i<APP_LIMIT;i++)entries[i].removed=*entries[i].key && deleted(entries[i].key);
 root(file,"");DIR*recovery=opendir(file);if(recovery){struct dirent*r;while((r=readdir(recovery)))if(!strncmp(r->d_name,".previous-",10)&&valid_id(r->d_name+10)){char previous[2048],target[2048];root(previous,r->d_name);root(target,r->d_name+10);if(access(target,F_OK))rename(previous,target);else package_remove(previous);}closedir(recovery);}
 root(file,"");DIR*d=opendir(file);if(!d)return;struct dirent*e;Package*p=malloc(sizeof(*p));if(!p){closedir(d);return;}
 while((e=readdir(d)))if(valid_id(e->d_name)) {
   char dir[2048];snprintf(dir,sizeof(dir),"%s/%s",file,e->d_name);
   if(!read_manifest(dir,p)||strcmp(p->id,e->d_name)||deleted(p->id))continue;
   int id=identify(p->id);if(id<0)for(int i=APP_COUNT;i<APP_LIMIT;i++)if(!*entries[i].key){id=i;snprintf(entries[i].key,64,"%s",p->id);break;}
   if(id<0)continue;entries[id].installed=1;snprintf(entries[id].name,96,"%s",p->name);snprintf(entries[id].version,32,"%s",p->version);
   entries[id].def=(AppDefinition){id,entries[id].name,id<APP_COUNT ? app_factory_definition(id)->entry : 1000+id-APP_COUNT};
 }
 closedir(d);free(p);
 char slots[4096];size_t n=0;for(int i=APP_COUNT;i<APP_LIMIT;i++)if(*entries[i].key)n+=snprintf(slots+n,sizeof(slots)-n,"%d %s\n",i,entries[i].key);
 refresh_process(&foreground);
 for(int i=0;i<APP_OVERLAY_COUNT;i++)refresh_process(&overlays[i]);
 root(file,"slots.txt");char old[4096];size_t old_n=0;FILE*before=fopen(file,"rb");if(before){old_n=fread(old,1,sizeof(old),before);fclose(before);}
 if(old_n!=n || memcmp(old,slots,n))atomic_write(file,slots,n);
}
const AppDefinition *app_package_definition(AppId id){return id>=0&&id<APP_LIMIT && (entries[id].installed || id>=APP_COUNT) ? &entries[id].def : NULL;}
int app_package_installed(AppId id){return id>=0&&id<APP_LIMIT&&entries[id].installed;}
const char *app_package_key(AppId id){return id>=0&&id<APP_LIMIT?entries[id].key:"";}
const char *app_package_version(AppId id){return app_package_installed(id)?entries[id].version:LUMEN_VERSION;}
static int executable_valid(const char*file){FILE*f=fopen(file,"rb");if(!f)return 0;unsigned char b[20];int ok=fread(b,1,20,f)==20&&!memcmp(b,"\177ELF\1\1\1",7)&&b[16]==2&&!b[17]&&b[18]==40&&!b[19];fclose(f);
#ifdef LUMEN_HOST
 struct stat st;ok=!stat(file,&st)&&S_ISREG(st.st_mode)&&st.st_size>100;
#endif
 return ok;}
int app_package_install(const char*archive) {
 char stage[2048],target[2048],previous[2048],exe[2048];root(stage,".installing");package_remove(stage);
 Package*p=malloc(sizeof(*p));if(!p)return 0;
 int ok=package_extract(archive,stage,p)&&!strcmp(p->format,"lumen-app-1")&&p->api==APP_PACKAGE_API&&valid_id(p->id)&&*p->name;
 snprintf(exe,sizeof(exe),"%s/app",stage);ok=ok&&executable_valid(exe);
 if(ok){app_packages_scan();int id=identify(p->id);if(id<0){for(int i=APP_COUNT;i<APP_LIMIT;i++)if(!*entries[i].key){id=i;break;}}ok=id>=0;}
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
    static const char *essential[]={"lumen.home","lumen.settings","lumen.reader",
        "lumen.keyboard","lumen.notifications","lumen.status-bar","lumen.lock-screen","lumen.transfer"};
    for(size_t i=0;i<sizeof(essential)/sizeof(*essential);i++)
        if(!strcmp(entries[id].key,essential[i]))return 0;
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
 if(!ok && id<APP_COUNT) {
     snprintf(file,sizeof(file),"%s/../home/icons/%s.pgm",data_root,entries[id].key);
     ok=read_icon(file,pixels);
 }
#ifdef LUMEN_HOST
 if(!ok)for(size_t i=0;i<sizeof(host_icons)/sizeof(*host_icons);i++)
     if(!strcmp(entries[id].key,host_icons[i].id)){ok=read_icon(host_icons[i].path,pixels);break;}
#endif
 entries[id].has_icon=ok ? 1 : -1;
 return ok;
}
static void json_text(FILE*f,const char*s){fputc('"',f);for(;*s;s++){unsigned char c=*s;if(c=='"'||c=='\\')fputc('\\',f);if(c<32)fprintf(f,"\\u%04x",c);else fputc(c,f);}fputc('"',f);}
static void process_close(AppProcess *p) {
 if(p->child>0){kill(-p->child,SIGTERM);kill(p->child,SIGTERM);waitpid(p->child,NULL,WNOHANG);}
 if(p->child>0 && waitpid(p->child,NULL,WNOHANG)==0)for(int i=0;i<64;i++)if(!retired[i]){retired[i]=p->child;break;}
 p->child=0;if(p->read_fd>=0)close(p->read_fd);if(p->write_fd>=0)close(p->write_fd);p->read_fd=p->write_fd=-1;p->active=-1;p->frame[0]=p->request[0]=p->failure[0]=0;p->incoming_n=p->pending_n=0;
}
static void flush_events(AppProcess *p) {
 while(p->write_fd>=0 && p->pending_n){ssize_t n=write(p->write_fd,p->pending,p->pending_n);if(n<=0)break;memmove(p->pending,p->pending+n,p->pending_n-n);p->pending_n-=n;}
}
static int process_send(AppProcess *p,const char*event) {
 if(p->write_fd<0||!event)return 0;size_t n=strlen(event);if(n>16384 || n+1>sizeof(p->pending)-p->pending_n)return 0;
 memcpy(p->pending+p->pending_n,event,n);p->pending_n+=n;p->pending[p->pending_n++]='\n';flush_events(p);return 1;
}
static void process_pause(AppProcess *p){if(p->child>0&&!p->paused){process_send(p,"{\"event\":\"pause\"}");p->paused=1;}}
static int process_open(AppProcess *p,AppId id,const char *role) {
 if(!app_package_installed(id))return 0;
 if(p->active==(int)id&&p->child>0){if(p->paused)process_send(p,"{\"event\":\"resume\"}");p->paused=0;return 1;}
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
 p->child=fork();if(p->child<0){p->child=0;close(in[0]);close(in[1]);close(out[0]);close(out[1]);return 0;}
 if(!p->child){setpgid(0,0);signal(SIGTERM,SIG_DFL);signal(SIGPIPE,SIG_DFL);dup2(in[0],0);dup2(out[1],1);close(in[0]);close(in[1]);close(out[0]);close(out[1]);
   int null=open("/dev/null",O_WRONLY);dup2(null,2);close(null);for(int fd=3;fd<1024;fd++)close(fd);chdir(dir);setenv("LUMEN_APP_DATA",state,1);setenv("LUMEN_APP_ASSETS",dir,1);setenv("LUMEN_LEGACY_DATA",old_state,1);execl(file,file,NULL);_exit(127);}
 close(in[0]);close(out[1]);p->write_fd=in[1];p->read_fd=out[0];fcntl(p->write_fd,F_SETFL,O_NONBLOCK);fcntl(p->read_fd,F_SETFL,O_NONBLOCK);fcntl(p->write_fd,F_SETFD,FD_CLOEXEC);fcntl(p->read_fd,F_SETFD,FD_CLOEXEC);
 p->active=id;p->paused=0;char*event=NULL;size_t n=0;FILE*f=open_memstream(&event,&n);fprintf(f,"{\"event\":\"open\",\"api\":1,\"width\":600,\"height\":800,\"content_y\":56,\"language\":%d,\"data_dir\":",options[OPT_LANGUAGE]);json_text(f,state);fputs(",\"role\":",f);json_text(f,role);fputc('}',f);fclose(f);int ok=process_send(p,event);free(event);return ok;
}
static int process_poll(AppProcess *p) {
 reap_apps();if(p->child<=0)return 0;flush_events(p);
 int changed=0;char buf[16384];size_t received=0;ssize_t n=-1;
 while(received<APP_FRAME_MAX*2 && (n=read(p->read_fd,buf,sizeof(buf)))>0) {
   received+=n;
   for(ssize_t i=0;i<n;i++) {
     if(buf[i]=='\n') {
       p->incoming[p->incoming_n]=0;
       if(strstr(p->incoming,"\"request\"")){snprintf(p->request,sizeof(p->request),"%s",p->incoming);changed=1;}
       else {
           p->frame_serial=++frame_serial;
           if(strcmp(p->frame,p->incoming)){snprintf(p->frame,sizeof(p->frame),"%s",p->incoming);changed=1;}
       }
       p->incoming_n=0;
     } else if(p->incoming_n+1<sizeof(p->incoming))p->incoming[p->incoming_n++]=buf[i];
     else {snprintf(p->failure,sizeof(p->failure),"App-Ausgabe zu groß.");kill(p->child,SIGTERM);p->incoming_n=0;break;}
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

void app_process_close(void){process_close(&foreground);for(int i=0;i<APP_OVERLAY_COUNT;i++)process_close(&overlays[i]);}
void app_process_pause(void){process_pause(&foreground);}
int app_process_open(AppId id){return process_open(&foreground,id,"foreground");}
int app_process_fd(void){return foreground.read_fd;}
int app_overlay_fd(int slot){return slot>=0 && slot<APP_OVERLAY_COUNT ? overlays[slot].read_fd : -1;}
int app_process_pending(void){
 if(foreground.pending_n)return 1;
 for(int i=0;i<APP_OVERLAY_COUNT;i++)if(overlays[i].pending_n)return 1;
 return 0;
}
void app_process_flush(void){flush_events(&foreground);for(int i=0;i<APP_OVERLAY_COUNT;i++)flush_events(&overlays[i]);}
int app_process_send(const char *event){return process_send(&foreground,event);}
int app_process_poll(void){return process_poll(&foreground);}
const char *app_process_frame(void){return foreground.frame;}
uint64_t app_process_frame_serial(void){return foreground.frame_serial;}
const char *app_process_request(void){
    static char taken[32768];
    snprintf(taken,sizeof(taken),"%s",foreground.request);
    foreground.request[0]=0;
    return taken;
}
const char *app_process_error(void){return foreground.failure;}
int app_overlay_send(int slot,const char *id,const char *event){
    if(slot<0 || slot>=APP_OVERLAY_COUNT)return 0;
    int app=identify(id);
    AppProcess *process=&overlays[slot];
    return app>=0 && process_open(process,app,"overlay") && process_send(process,event);
}
int app_overlay_poll(int slot){return slot>=0 && slot<APP_OVERLAY_COUNT && process_poll(&overlays[slot]);}
const char *app_overlay_frame(int slot){return slot>=0 && slot<APP_OVERLAY_COUNT ? overlays[slot].frame : "";}
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
   if(!package_peek(archive,p)||strcmp(p->format,"lumen-app-1")||deleted(p->id))continue;
   int id=identify(p->id);
   if(id<0 || !app_package_installed(id) || version_newer(p->version,entries[id].version))app_package_install(archive);
 }
 free(p);closedir(d);app_packages_scan();
}
