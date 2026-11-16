#define _GNU_SOURCE
#include "packages.h"
#include "services.h"
#include "library.h"
#include <mupdf/fitz.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int package_path_valid(const char *s) {
 if(!s || !*s || strlen(s)>255 || s[0]=='/' || strchr(s,'\\'))return 0;
 for(const char *p=s;*p;p++)if((unsigned char)*p<32 || *p==127)return 0;
 for(const char *p=s;;){size_t n=strcspn(p,"/");if(!n || (n==1&&*p=='.') || (n==2&&!strncmp(p,"..",2)))return 0;if(!p[n])break;p+=n+1;}
 return 1;
}
int package_mkdirs(const char *path) {
 char buf[2048];if(strlen(path)>=sizeof(buf))return 0;strcpy(buf,path);
 for(char *p=buf+1;;p++)if(*p=='/' || !*p){int end=!*p;char c=*p;*p=0;struct stat st;
   if(lstat(buf,&st)){if(errno!=ENOENT || mkdir(buf,0700))return 0;}
   else if(!S_ISDIR(st.st_mode))return 0;
   *p=c;if(end)break;}
 return 1;
}
static int parent_dir(const char *path){char b[2048];snprintf(b,sizeof(b),"%s",path);char*p=strrchr(b,'/');if(!p)return 0;*p=0;return package_mkdirs(b);}
int package_copy(const char *source,const char *target) {
 if(!parent_dir(target))return 0;
 int in=open(source,O_RDONLY|O_NOFOLLOW),out=-1,ok=0;char b[32768];ssize_t n;
 if(in<0)return 0;out=open(target,O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0700);if(out<0)goto done;
 while((n=read(in,b,sizeof(b)))>0){ssize_t sent=0;while(sent<n){ssize_t w=write(out,b+sent,n-sent);if(w<=0)goto done;sent+=w;}}
 ok=n==0 && !fsync(out);
 done:close(in);if(out>=0)close(out);return ok;
}
int package_remove(const char *path) {
 struct stat st;if(lstat(path,&st))return errno==ENOENT;
 if(!S_ISDIR(st.st_mode))return unlink(path)==0;
 DIR*d=opendir(path);if(!d)return 0;int ok=1;struct dirent*e;
 while((e=readdir(d)))if(strcmp(e->d_name,".")&&strcmp(e->d_name,"..")) {
   char b[2048];if(snprintf(b,sizeof(b),"%s/%s",path,e->d_name)>=(int)sizeof(b) || !package_remove(b))ok=0;
 }
 closedir(d);return ok && !rmdir(path);
}
static void hex(fz_sha256 *hash,char out[65]){unsigned char b[32];fz_sha256_final(hash,b);for(int i=0;i<32;i++)snprintf(out+i*2,3,"%02x",b[i]);}
int package_sha256(const char *path,char out[65]) {
 int fd=open(path,O_RDONLY|O_NOFOLLOW);if(fd<0)return 0;
 fz_sha256 h;fz_sha256_init(&h);unsigned char b[32768];ssize_t n;
 while((n=read(fd,b,sizeof(b)))>0)fz_sha256_update(&h,b,n);close(fd);hex(&h,out);return n==0;
}
int package_configuration_valid(const char *name) {
    size_t length=strlen(name);
    return length && length<64 && name[0]!='.' &&
           strspn(name,"abcdefghijklmnopqrstuvwxyz0123456789._-")==length;
}
int package_operation_valid(const char *operation) {
    size_t n=strlen(operation);
    return n>0 && n<48 && strspn(operation,"abcdefghijklmnopqrstuvwxyz0123456789_-")==n;
}
const char *const package_provider_names[PROVIDER_COUNT]={
    "startup", "text-input", "status", "notification", "sleep", "document", "books"
};

static int package_flags(const Json *json,int array,const char *const *names,
                         unsigned count,unsigned *flags) {
    if(array<0 || json->tokens[array].type!='[' || json->tokens[array].count>(int)count)return 0;
    for(int i=0;i<json->tokens[array].count;i++) {
        int item=json_at(json,array,i);
        char name[32];
        if(item<0 || json->tokens[item].type!='"' || json->tokens[item].end-json->tokens[item].start>32)return 0;
        json_string(json,item,name,sizeof(name));
        unsigned bit=0;
        while(bit<count && strcmp(name,names[bit]))bit++;
        if(bit==count || (*flags & (1u<<bit)))return 0;
        *flags|=1u<<bit;
    }
    return 1;
}

int package_manifest(const char *text,Package *p) {
 Json j;JsonToken t[4096];memset(p,0,sizeof(*p));if(!json_parse(&j,text,t,4096))return 0;
 json_string(&j,json_get(&j,0,"format"),p->format,sizeof(p->format));
 json_string(&j,json_get(&j,0,"version"),p->version,sizeof(p->version));
 json_string(&j,json_get(&j,0,"id"),p->id,sizeof(p->id));
 json_string(&j,json_get(&j,0,"name"),p->name,sizeof(p->name));
 p->launcher_order=1000;p->launcher_visible=1;
 p->api=json_number(&j,json_get(&j,0,"api"));int files=json_get(&j,0,"files");
 if(!strcmp(p->format,"lumen-app-2")) {
   static const char *const surfaces[]={"foreground","overlay","background"};
   char entry[16];
   int provider_list=json_get(&j,0,"provides");
   json_string(&j,json_get(&j,0,"entry"),entry,sizeof(entry));
   if(p->api!=2 || strcmp(entry,"app") ||
      !package_flags(&j,json_get(&j,0,"surfaces"),surfaces,3,&p->surfaces) || !p->surfaces ||
      (provider_list>=0 && !package_flags(&j,provider_list,package_provider_names,PROVIDER_COUNT,&p->providers)))return 0;
   unsigned foreground_providers=(1u<<PROVIDER_STARTUP)|(1u<<PROVIDER_DOCUMENT)|(1u<<PROVIDER_BOOKS);
   if((p->providers & foreground_providers) && !(p->surfaces & PACKAGE_FOREGROUND))return 0;
   if((p->providers & ~foreground_providers) && !(p->surfaces & PACKAGE_OVERLAY))return 0;
   int launcher=json_get(&j,0,"launcher");
   if(launcher>=0) {
     char visible[8];
     int order=json_get(&j,launcher,"order");
     double rank=json_number(&j,order);
     json_string(&j,json_get(&j,launcher,"visible"),visible,sizeof(visible));
     if(t[launcher].type!='{' || order<0 || !(rank>=0 && rank<=100000) ||
        rank!=(int)rank || (strcmp(visible,"true") && strcmp(visible,"false")))return 0;
     p->launcher_order=(int)rank;
     p->launcher_visible=!strcmp(visible,"true");
   }
 } else if(!strcmp(p->format,"lumen-app-1") && p->api==1) {
   p->surfaces=PACKAGE_FOREGROUND|PACKAGE_OVERLAY|PACKAGE_BACKGROUND;
 }
 if(files<0 || t[files].type!='[' || t[files].count<1 || t[files].count>PACKAGE_FILES || !*p->version)return 0;
 int operations=json_get(&j,0,"operations");
 if(operations>=0) {
   if(t[operations].type!='[' || t[operations].count>PACKAGE_OPERATIONS)return 0;
   p->operation_count=t[operations].count;
   for(int i=0;i<p->operation_count;i++) {
     int item=json_at(&j,operations,i);
     if(t[item].type!='"' || t[item].end-t[item].start>49)return 0;
     json_string(&j,item,p->operations[i],sizeof(p->operations[i]));
     if(!package_operation_valid(p->operations[i]))return 0;
     for(int k=0;k<i;k++)if(!strcmp(p->operations[k],p->operations[i]))return 0;
   }
 }
 if(!strcmp(p->format,"lumen-app-2") && p->operation_count && !(p->surfaces & PACKAGE_BACKGROUND))return 0;
 int configuration=json_get(&j,0,"configuration");
 if(configuration>=0) {
   if(p->api!=2 || t[configuration].type!='[' || t[configuration].count>PACKAGE_CONFIGURATION)return 0;
   p->configuration_count=t[configuration].count;
   for(int i=0;i<p->configuration_count;i++) {
     int item=json_at(&j,configuration,i);
     if(item<0 || t[item].type!='"' || t[item].end-t[item].start>65)return 0;
     json_string(&j,item,p->configuration[i],sizeof(p->configuration[i]));
     if(!package_configuration_valid(p->configuration[i]))return 0;
     for(int k=0;k<i;k++)if(!strcmp(p->configuration[k],p->configuration[i]))return 0;
   }
 }
 p->count=t[files].count;size_t total=0;
 for(int i=0;i<p->count;i++) {
   int f=json_at(&j,files,i);PackageFile*r=&p->files[i];
   json_string(&j,json_get(&j,f,"path"),r->path,sizeof(r->path));
   json_string(&j,json_get(&j,f,"sha256"),r->sha256,sizeof(r->sha256));
   double size=json_number(&j,json_get(&j,f,"size"));if(size<0 || size>64*1024*1024 || size!=(size_t)size)return 0;r->size=size;
   if(!package_path_valid(r->path) || !strcmp(r->path,"manifest.json") || strlen(r->sha256)!=64 || strspn(r->sha256,"0123456789abcdef")!=64)return 0;
   for(int k=0;k<i;k++)if(!strcmp(p->files[k].path,r->path))return 0;
   total+=r->size;if(total>160*1024*1024)return 0;
 }
 return 1;
}
int package_extract(const char *archive,const char *destination,Package *p) {
 fz_context*ctx=fz_new_context(NULL,NULL,4*1024*1024);if(!ctx)return 0;
 fz_archive*zip=NULL;fz_buffer*manifest=NULL;fz_stream*stream=NULL;int fd=-1,ok=0;
 fz_var(zip);fz_var(manifest);fz_var(stream);fz_var(fd);fz_var(ok);
 if(!package_mkdirs(destination)){fz_drop_context(ctx);return 0;}
 fz_try(ctx) {
   zip=fz_open_archive(ctx,archive);
   stream=fz_open_archive_entry(ctx,zip,"manifest.json");
   char text[65537];size_t n=fz_read(ctx,stream,(unsigned char*)text,65536);
   if(n==65536 || fz_read_byte(ctx,stream)!=EOF)fz_throw(ctx,FZ_ERROR_FORMAT,"Manifest too large");
   text[n]=0;fz_drop_stream(ctx,stream);stream=NULL;
   if(!package_manifest(text,p) || fz_count_archive_entries(ctx,zip)!=p->count+1)fz_throw(ctx,FZ_ERROR_FORMAT,"Invalid manifest");
   for(int i=0;i<p->count;i++) {
     PackageFile*r=&p->files[i];char entry[512],file[2048],actual[65];
     snprintf(entry,sizeof(entry),"payload/%s",r->path);snprintf(file,sizeof(file),"%s/%s",destination,r->path);
     if(!parent_dir(file))fz_throw(ctx,FZ_ERROR_SYSTEM,"Unsafe destination");
     fd=open(file,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0700);if(fd<0)fz_throw(ctx,FZ_ERROR_SYSTEM,"Create failed");
     stream=fz_open_archive_entry(ctx,zip,entry);fz_sha256 h;fz_sha256_init(&h);size_t total=0;unsigned char b[32768];
     while((n=fz_read(ctx,stream,b,sizeof(b)))) {
       total+=n;if(total>r->size)fz_throw(ctx,FZ_ERROR_FORMAT,"Entry too large");
       size_t sent=0;while(sent<n){ssize_t w=write(fd,b+sent,n-sent);if(w<=0)fz_throw(ctx,FZ_ERROR_SYSTEM,"Write failed");sent+=w;}
       fz_sha256_update(&h,b,n);
     }
     hex(&h,actual);if(total!=r->size || strcmp(actual,r->sha256) || fsync(fd))fz_throw(ctx,FZ_ERROR_FORMAT,"Checksum failed");
     close(fd);fd=-1;fz_drop_stream(ctx,stream);stream=NULL;
   }
   char file[2048];snprintf(file,sizeof(file),"%s/manifest.json",destination);
   if(!atomic_write(file,text,strlen(text)))fz_throw(ctx,FZ_ERROR_SYSTEM,"Manifest write failed");ok=1;
 }
 fz_always(ctx){if(fd>=0)close(fd);fz_drop_stream(ctx,stream);fz_drop_buffer(ctx,manifest);fz_drop_archive(ctx,zip);}
 fz_catch(ctx){fz_ignore_error(ctx);}
 fz_drop_context(ctx);if(!ok)package_remove(destination);return ok;
}
int package_peek(const char *archive,Package *p) {
 fz_context*ctx=fz_new_context(NULL,NULL,1024*1024);if(!ctx)return 0;
 fz_archive*zip=NULL;fz_stream*s=NULL;int ok=0;fz_var(zip);fz_var(s);fz_var(ok);
 fz_try(ctx){zip=fz_open_archive(ctx,archive);s=fz_open_archive_entry(ctx,zip,"manifest.json");char text[65537];size_t n=fz_read(ctx,s,(unsigned char*)text,65536);if(n<65536 && fz_read_byte(ctx,s)==EOF){text[n]=0;ok=package_manifest(text,p);}}
 fz_always(ctx){fz_drop_stream(ctx,s);fz_drop_archive(ctx,zip);}fz_catch(ctx){fz_ignore_error(ctx);}
 fz_drop_context(ctx);return ok;
}
