#define _GNU_SOURCE
#include "system_update.h"
#include "packages.h"
#include "library.h"
#include "runtime_files.h"
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
static void path(char*out,const char*leaf){snprintf(out,2048,"%s/../.update/%s",data_root,leaf);}
static int runtime_path(const char*p) {
 if(!package_path_valid(p) || strpbrk(p," \t\r\n"))return 0;
 if(!strcmp(p,"documents/LUMEN.sh"))return 1;
 if(strncmp(p,"lumen/",6))return 0;p+=6;
 if(*p=='.')return 0;
 const char*dirs[]={"app-packages/","bin/","system/","fonts/","lib/","dictionary/","home/","catalogs/","certs/","photos/"};
 for(unsigned i=0;i<sizeof(dirs)/sizeof(*dirs);i++)if(!strncmp(p,dirs[i],strlen(dirs[i])))return 1;
 return !strchr(p,'/') && (strstr(p,".sh") || strstr(p,".md") || strstr(p,".txt") || !strcmp(p,"LICENSE"));
}
static int read_manifest(const char*file,Package*p) {
 FILE*f=fopen(file,"rb");if(!f)return 0;char text[65537];size_t n=fread(text,1,65536,f);int ok=n<65536 && !ferror(f);fclose(f);text[n]=0;
 return ok && package_manifest(text,p);
}
int system_update_trial(void){char p[2048];path(p,"system-trial");return !access(p,F_OK);}
int system_update_stage(const char*archive,const char*version) {
 char stage[2048],helper[2048],binary[2048],ready[2048],journal[2048];
 path(stage,"system-stage");path(helper,"system-installer");path(ready,"system-ready");path(journal,"system-journal");
 if(!access(journal,F_OK))return 0;
 package_remove(stage);Package*p=calloc(1,sizeof(*p));if(!p)return 0;
 int ok=package_extract(archive,stage,p) && !strcmp(p->format,"lumen-system-1") && !strcmp(p->version,version);
 int executable=0;
 for(int i=0;ok && i<p->count;i++) {
   ok=runtime_path(p->files[i].path);
   if(!strcmp(p->files[i].path,"lumen/bin/lumen")) {
     snprintf(binary,sizeof(binary),"%s/%s",stage,p->files[i].path);FILE*f=fopen(binary,"rb");unsigned char head[20];
     executable=f && fread(head,1,20,f)==20 && !memcmp(head,"\177ELF\1\1\1",7) && head[16]==2 && !head[17] && head[18]==40 && !head[19];if(f)fclose(f);
#ifdef LUMEN_HOST
     executable=1;
#endif
   }
 }
 free(p);snprintf(binary,sizeof(binary),"%s/../bin/lumen",data_root);
 if(ok && executable)ok=package_copy(binary,helper) && !chmod(helper,0700) && atomic_write(ready,version,strlen(version));else ok=0;
 if(!ok)package_remove(stage);return ok;
}
/* Every original is durably copied before the write-ahead journal is committed.
 * Recovery is repeatable: backups are copied, never consumed during rollback. */
int system_update_rollback(void) {
 char journal[2048],backup[2048],trial[2048],ready[2048];path(journal,"system-journal");path(backup,"system-previous");path(trial,"system-trial");path(ready,"system-ready");
 FILE*f=fopen(journal,"rb");if(!f)return errno==ENOENT;
 int old,ok=1;char rel[256];
 while(fscanf(f,"%d %255s",&old,rel)==2) {
   if(!runtime_path(rel)){ok=0;break;}
   char saved[2048],target[2048],tmp[2048];snprintf(saved,sizeof(saved),"%s/%s",backup,rel);
   snprintf(target,sizeof(target),"%s/../../%s",data_root,rel);snprintf(tmp,sizeof(tmp),"%s.new",target);
   if(old){if(!package_copy(saved,tmp) || rename(tmp,target)){ok=0;break;}}
   else if(unlink(target) && errno!=ENOENT){ok=0;break;}
 }
 if(ferror(f))ok=0;fclose(f);sync();
 if(ok){unlink(journal);sync();unlink(trial);unlink(ready);package_remove(backup);char helper[2048];path(helper,"system-installer");unlink(helper);}
 return ok;
}
int system_update_keep(void) {
 if(!system_update_trial())return 0;
 char p[2048];/* Removing the journal is the commit point; leftover backups are harmless. */
 path(p,"system-journal");if(unlink(p) && errno!=ENOENT)return 0;
 sync();path(p,"system-trial");unlink(p);path(p,"system-previous");package_remove(p);path(p,"system-installer");unlink(p);sync();return 1;
}
int system_update_maintenance(void) {
 char journal[2048],ready[2048],stage[2048],backup[2048],trial[2048],manifest[2048];
 path(journal,"system-journal");path(ready,"system-ready");path(stage,"system-stage");path(backup,"system-previous");path(trial,"system-trial");
 if(!access(journal,F_OK)) {
     if(system_update_trial())return 1;
     if(!system_update_rollback())return 0;
 }
 if(access(ready,F_OK))return 1;
 Package*p=calloc(1,sizeof(*p));if(!p)return 0;
 snprintf(manifest,sizeof(manifest),"%s/manifest.json",stage);
 if(!read_manifest(manifest,p) || strcmp(p->format,"lumen-system-1")){free(p);return 0;}
 struct Change{char rel[256];int old;} changes[PACKAGE_FILES*2];int count=0,ok=1;
 for(int i=0;i<p->count;i++) {
   char target[2048],actual[65],source[2048];PackageFile*r=&p->files[i];
   if(!runtime_path(r->path)){ok=0;break;}
   snprintf(source,sizeof(source),"%s/%s",stage,r->path);
   if(!package_sha256(source,actual) || strcmp(actual,r->sha256)){ok=0;break;}
   snprintf(target,sizeof(target),"%s/../../%s",data_root,r->path);
   char parent[2048];snprintf(parent,sizeof(parent),"%s",target);char*slash=strrchr(parent,'/');*slash=0;
   if(!package_mkdirs(parent)){ok=0;break;}
   if(package_sha256(target,actual) && !strcmp(actual,r->sha256))continue;
   struct stat st;int old=!lstat(target,&st);if(old && !S_ISREG(st.st_mode)){ok=0;break;}
   snprintf(changes[count].rel,256,"%s",r->path);changes[count++].old=old;
 }
 for(unsigned i=0;ok && i<sizeof(runtime_files)/sizeof(*runtime_files);i++) {
   int found=0;for(int j=0;j<p->count;j++)if(!strcmp(runtime_files[i],p->files[j].path))found=1;
   if(found)continue;char file[2048];snprintf(file,sizeof(file),"%s/../../%s",data_root,runtime_files[i]);
   if(access(file,F_OK))continue;if(count>=PACKAGE_FILES*2){ok=0;break;}
   snprintf(changes[count].rel,256,"%s",runtime_files[i]);changes[count++].old=1;
 }
 package_remove(backup);
 char log[PACKAGE_FILES*2*272];size_t used=0;
 for(int i=0;ok && i<count;i++) {
   char source[2048],saved[2048];snprintf(source,sizeof(source),"%s/../../%s",data_root,changes[i].rel);snprintf(saved,sizeof(saved),"%s/%s",backup,changes[i].rel);
   if(changes[i].old && !package_copy(source,saved)){ok=0;break;}
   used+=snprintf(log+used,sizeof(log)-used,"%d %s\n",changes[i].old,changes[i].rel);
 }
 if(!ok || !atomic_write(journal,log,used)){free(p);return 0;}
 unlink(ready);sync();
 for(int i=0;ok && i<count;i++) {
   char target[2048],source[2048],parent[2048];snprintf(target,sizeof(target),"%s/../../%s",data_root,changes[i].rel);snprintf(source,sizeof(source),"%s/%s",stage,changes[i].rel);
   snprintf(parent,sizeof(parent),"%s",target);char*slash=strrchr(parent,'/');*slash=0;
   if(!package_mkdirs(parent)){ok=0;break;}
   if(!access(source,F_OK)){if(rename(source,target))ok=0;}
   else if(unlink(target) && errno!=ENOENT)ok=0;
#ifdef LUMEN_HOST
   const char*fail=getenv("LUMEN_TEST_UPDATE_CRASH");if(fail&&atoi(fail)==i+1)_exit(73);
#endif
 }
 sync();if(ok)ok=atomic_write(trial,p->version,strlen(p->version));free(p);
 if(!ok)return system_update_rollback() && 0;
 package_remove(stage);sync();return 1;
}
