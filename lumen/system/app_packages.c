#define _GNU_SOURCE
#include "app_packages.h"
#include "packages.h"
#include "services.h"
#include "library.h"
#include "version.h"
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
/* IDs are a package identity, never an origin or privilege class. */
static const char *keys[APP_COUNT]={
 "lumen.library","lumen.reader","lumen.settings","lumen.store","lumen.transfer","lumen.calendar","lumen.insights","lumen.minesweeper","lumen.hangman","lumen.tetris","lumen.snake","lumen.calculator","lumen.clock","lumen.notes","retired.drawing","lumen.weather","lumen.home-assistant","lumen.wikipedia","lumen.quotes","lumen.chess","lumen.tic-tac-toe","retired.numscratch","lumen.2048","lumen.solitaire","lumen.sudoku","lumen.braintrain","lumen.news","lumen.wordle","lumen.photos","lumen.languages","lumen.todo","lumen.flashcards","lumen.planetarium","lumen.nonograms","lumen.hex","lumen.status-bar","retired.tabs","lumen.lock-screen","lumen.keyboard","lumen.journal","lumen.study","lumen.bingo","lumen.connect-four","lumen.recipes","lumen.periodic-table","retired.files","lumen.knights-tour","lumen.sokoban","lumen.maze","lumen.crossword","retired.rpg","lumen.openai","lumen.sliding-puzzle","lumen.frog-solitaire","lumen.home"};
static struct {AppDefinition def;char key[64],name[96],version[32];int installed;} entries[APP_LIMIT];
static pid_t child, retired[64];
static char process_hash[65];
static void reap_apps(void){for(int i=0;i<64;i++)if(retired[i]&&waitpid(retired[i],NULL,WNOHANG)!=0)retired[i]=0;}static int read_fd=-1,write_fd=-1,active=-1,paused;
static size_t incoming_n, pending_n;
static char pending[65536];static char incoming[APP_FRAME_MAX],frame[APP_FRAME_MAX],request[32768],failure[160];
static void root(char*out,const char*leaf){snprintf(out,2048,"%s/../apps/%s",data_root,leaf);}
static int valid_id(const char*id){return id && strlen(id)>2 && strlen(id)<64 && strspn(id,"abcdefghijklmnopqrstuvwxyz0123456789.-_")==strlen(id) && id[0]!='.' && !strstr(id,"..");}
static int identify(const char*key){for(int i=0;i<APP_LIMIT;i++)if(!strcmp(entries[i].key,key))return i;return -1;}
static int read_manifest(const char*directory,Package*p){char name[2048],text[65537];snprintf(name,sizeof(name),"%s/manifest.json",directory);FILE*f=fopen(name,"rb");if(!f)return 0;size_t n=fread(text,1,65536,f);int ok=n<65536&&!ferror(f);fclose(f);text[n]=0;return ok&&package_manifest(text,p)&&!strcmp(p->format,"lumen-app-1")&&p->api==APP_PACKAGE_API&&valid_id(p->id);}
void app_packages_scan(void) {
 for(int i=0;i<APP_LIMIT;i++){entries[i].installed=0;entries[i].def=(AppDefinition){i,entries[i].name,-1};entries[i].name[0]=0;if(i<APP_COUNT)snprintf(entries[i].key,64,"%s",keys[i]);else entries[i].key[0]=0;}
 char file[2048];root(file,"slots.txt");FILE*f=fopen(file,"rb");if(f){int slot;char id[64];while(fscanf(f,"%d %63s",&slot,id)==2)if(slot>=APP_COUNT&&slot<APP_LIMIT&&valid_id(id))snprintf(entries[slot].key,64,"%s",id);fclose(f);}
 root(file,"");DIR*recovery=opendir(file);if(recovery){struct dirent*r;while((r=readdir(recovery)))if(!strncmp(r->d_name,".previous-",10)&&valid_id(r->d_name+10)){char previous[2048],target[2048];root(previous,r->d_name);root(target,r->d_name+10);if(access(target,F_OK))rename(previous,target);else package_remove(previous);}closedir(recovery);}
 root(file,"");DIR*d=opendir(file);if(!d)return;struct dirent*e;Package*p=malloc(sizeof(*p));if(!p){closedir(d);return;}
 while((e=readdir(d)))if(valid_id(e->d_name)) {
   char dir[2048];snprintf(dir,sizeof(dir),"%s/%s",file,e->d_name);
   if(!read_manifest(dir,p)||strcmp(p->id,e->d_name))continue;
   int id=identify(p->id);if(id<0)for(int i=APP_COUNT;i<APP_LIMIT;i++)if(!*entries[i].key){id=i;snprintf(entries[i].key,64,"%s",p->id);break;}
   if(id<0)continue;entries[id].installed=1;snprintf(entries[id].name,96,"%s",p->name);snprintf(entries[id].version,32,"%s",p->version);
   entries[id].def=(AppDefinition){id,entries[id].name,id<APP_COUNT ? app_factory_definition(id)->entry : 1000+id-APP_COUNT};
 }
 closedir(d);free(p);
 char slots[4096];size_t n=0;for(int i=APP_COUNT;i<APP_LIMIT;i++)if(*entries[i].key)n+=snprintf(slots+n,sizeof(slots)-n,"%d %s\n",i,entries[i].key);
 if(active>=0 && child>0){char exe[2048],actual[65];snprintf(exe,sizeof(exe),"%s/../apps/%s/app",data_root,entries[active].key);if(!package_sha256(exe,actual)||strcmp(actual,process_hash))app_process_close();}
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
   if(ok){chmod(exe,0700);snprintf(exe,sizeof(exe),"%s/app",target);chmod(exe,0700);sync();package_remove(previous);app_packages_scan();}
 }
 if(!ok)package_remove(stage);free(p);return ok;
}
int app_package_remove(const char*id) {
 if(!valid_id(id))return 0;char dir[2048];root(dir,id);int ok=package_remove(dir);
 if(ok){int i=identify(id);if(i>=APP_COUNT)entries[i].key[0]=0;app_packages_scan();}return ok;
}
int app_package_icon(AppId id,unsigned char pixels[76*76]) {
 if(!app_package_installed(id))return 0;char file[2048],leaf[160];snprintf(leaf,sizeof(leaf),"%s/icon.pgm",entries[id].key);root(file,leaf);
 FILE*f=fopen(file,"rb");if(!f)return 0;char header[15];const char expected[]="P5\n76 76\n255\n";
 int ok=fread(header,1,sizeof(expected)-1,f)==sizeof(expected)-1&&!memcmp(header,expected,sizeof(expected)-1)&&fread(pixels,1,76*76,f)==76*76&&fgetc(f)==EOF;fclose(f);return ok;
}
static void json_text(FILE*f,const char*s){fputc('"',f);for(;*s;s++){unsigned char c=*s;if(c=='"'||c=='\\')fputc('\\',f);if(c<32)fprintf(f,"\\u%04x",c);else fputc(c,f);}fputc('"',f);}
void app_process_close(void) {
 if(child>0){kill(-child,SIGTERM);kill(child,SIGTERM);waitpid(child,NULL,WNOHANG);}
 if(child>0 && waitpid(child,NULL,WNOHANG)==0)for(int i=0;i<64;i++)if(!retired[i]){retired[i]=child;break;}
 child=0;if(read_fd>=0)close(read_fd);if(write_fd>=0)close(write_fd);read_fd=write_fd=-1;active=-1;frame[0]=request[0]=failure[0]=0;incoming_n=pending_n=0;
}
static void flush_events(void) {
 while(write_fd>=0 && pending_n){ssize_t n=write(write_fd,pending,pending_n);if(n<=0)break;memmove(pending,pending+n,pending_n-n);pending_n-=n;}
}
int app_process_send(const char*event) {
 if(write_fd<0||!event)return 0;size_t n=strlen(event);if(n>16384 || n+1>sizeof(pending)-pending_n)return 0;
 memcpy(pending+pending_n,event,n);pending_n+=n;pending[pending_n++]='\n';flush_events();return 1;
}
void app_process_pause(void){if(child>0&&!paused){app_process_send("{\"event\":\"pause\"}");paused=1;}}
int app_process_open(AppId id) {
 if(!app_package_installed(id))return 0;
 if(active==(int)id&&child>0){if(paused)app_process_send("{\"event\":\"resume\"}");paused=0;return 1;}
 reap_apps();app_process_close();signal(SIGPIPE,SIG_IGN);int in[2],out[2];if(pipe(in))return 0;if(pipe(out)){close(in[0]);close(in[1]);return 0;}
 char dir[2048],file[2048],state[2048];root(dir,entries[id].key);snprintf(file,sizeof(file),"%s/app",dir);snprintf(state,sizeof(state),"%s/app-state/%s",data_root,entries[id].key);
 char*absolute=realpath(dir,NULL);if(absolute){snprintf(dir,sizeof(dir),"%s",absolute);free(absolute);snprintf(file,sizeof(file),"%s/app",dir);}
 if(!package_mkdirs(state)){close(in[0]);close(in[1]);close(out[0]);close(out[1]);return 0;}
 absolute=realpath(state,NULL);if(absolute){snprintf(state,sizeof(state),"%s",absolute);free(absolute);}
 package_sha256(file,process_hash);
 child=fork();if(child<0){child=0;close(in[0]);close(in[1]);close(out[0]);close(out[1]);return 0;}
 if(!child){setpgid(0,0);signal(SIGTERM,SIG_DFL);signal(SIGPIPE,SIG_DFL);dup2(in[0],0);dup2(out[1],1);close(in[0]);close(in[1]);close(out[0]);close(out[1]);
   int null=open("/dev/null",O_WRONLY);dup2(null,2);close(null);for(int fd=3;fd<1024;fd++)close(fd);chdir(dir);setenv("LUMEN_APP_DATA",state,1);execl(file,file,NULL);_exit(127);}
 close(in[0]);close(out[1]);write_fd=in[1];read_fd=out[0];fcntl(write_fd,F_SETFL,O_NONBLOCK);fcntl(read_fd,F_SETFL,O_NONBLOCK);fcntl(write_fd,F_SETFD,FD_CLOEXEC);fcntl(read_fd,F_SETFD,FD_CLOEXEC);
 active=id;paused=0;char*event=NULL;size_t n=0;FILE*f=open_memstream(&event,&n);fprintf(f,"{\"event\":\"open\",\"api\":1,\"width\":600,\"height\":800,\"content_y\":56,\"language\":%d,\"data_dir\":",options[OPT_LANGUAGE]);json_text(f,state);fputc('}',f);fclose(f);int ok=app_process_send(event);free(event);return ok;
}
int app_process_poll(void) {
 reap_apps();if(child<=0)return 0;flush_events();
 int changed=0;char buf[16384];size_t received=0;ssize_t n;
 while(received<APP_FRAME_MAX*2 && (n=read(read_fd,buf,sizeof(buf)))>0) {
   received+=n;
   for(ssize_t i=0;i<n;i++) {
     if(buf[i]=='\n') {
       incoming[incoming_n]=0;
       if(strstr(incoming,"\"request\"")){snprintf(request,sizeof(request),"%s",incoming);changed=1;}
       else {snprintf(frame,sizeof(frame),"%s",incoming);changed=1;}
       incoming_n=0;
     } else if(incoming_n+1<sizeof(incoming))incoming[incoming_n++]=buf[i];
     else {snprintf(failure,sizeof(failure),"App-Ausgabe zu groß.");kill(child,SIGTERM);incoming_n=0;break;}
   }
 }
 int status;pid_t done=waitpid(child,&status,WNOHANG);if(done==child){child=0;close(read_fd);close(write_fd);read_fd=write_fd=-1;snprintf(failure,sizeof(failure),"App wurde beendet. Bitte erneut öffnen.");changed=1;}
 return changed;
}
const char *app_process_frame(void){return frame;}
const char *app_process_request(void){static char taken[32768];snprintf(taken,sizeof(taken),"%s",request);request[0]=0;return taken;}
const char *app_process_error(void){return failure;}
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
   if(!package_peek(archive,p)||strcmp(p->format,"lumen-app-1"))continue;
   int id=identify(p->id);
   if(id<0 || !app_package_installed(id) || version_newer(p->version,entries[id].version))app_package_install(archive);
 }
 free(p);closedir(d);app_packages_scan();
}
