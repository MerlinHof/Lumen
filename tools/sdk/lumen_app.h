#ifndef LUMEN_APP_SDK_H
#define LUMEN_APP_SDK_H
/* LUMEN App Protocol 1. Header-only client, independent of the system binary.
 * stdin: newline-delimited events. stdout: frames/requests. stderr: diagnostics.
 * Apps own state in LUMEN_APP_DATA, never in their replaceable package folder. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include "../../lumen/system/device.h"
#include "../../lumen/system/ui/icon_ids.h"
typedef enum{PAINT_BODY,PAINT_HEADING,PAINT_DISPLAY,PAINT_SECONDARY}PaintText;
static char lumen_event[20000];static int lumen_first,lumen_language,lumen_back;
static void lumen_json(const char*s){putchar('"');for(;*s;s++){unsigned char c=*s;if(c=='"'||c=='\\')putchar('\\');if(c<32)printf("\\u%04x",c);else putchar(c);}putchar('"');}
static int lumen_number(const char*key){char needle[80];snprintf(needle,sizeof(needle),"\"%s\":",key);const char*p=strstr(lumen_event,needle);return p?atoi(p+strlen(needle)):0;}
static int lumen_string(const char*key,char*out,size_t cap){char needle[80];snprintf(needle,sizeof(needle),"\"%s\":",key);const char*p=strstr(lumen_event,needle);size_t n=0;if(!cap)return 0;out[0]=0;if(!p)return 0;p+=strlen(needle);while(*p==' ')p++;if(*p++!='"')return 0;while(*p&&*p!='"'&&n+1<cap){unsigned char c=*p++;if(c=='\\'){c=*p++;if(c=='n')c='\n';else if(c=='r')c='\r';else if(c=='t')c='\t';else if(c=='u'){unsigned value=0;if(sscanf(p,"%4x",&value)!=1)return 0;p+=4;if(value<128)c=value;else if(value<2048){if(n+2>=cap)break;out[n++]=0xc0|(value>>6);c=0x80|(value&63);}else{if(n+3>=cap)break;out[n++]=0xe0|(value>>12);out[n++]=0x80|((value>>6)&63);c=0x80|(value&63);}}}out[n++]=c;}out[n]=0;return 1;}
static int lumen_next(void){if(!fgets(lumen_event,sizeof(lumen_event),stdin))return 0;return 1;}
static int lumen_is(const char*event){char b[80];snprintf(b,sizeof(b),"\"event\":\"%s\"",event);return strstr(lumen_event,b)!=NULL;}
static void lumen_begin(void){lumen_first=1;printf("{\"back\":%s,\"ui\":[",lumen_back?"true":"false");}
static void lumen_end(void){puts("]}");fflush(stdout);}
static void lumen_cmd(const char*type,int x,int y,int w,int h){if(!lumen_first)putchar(',');lumen_first=0;fputs("{\"type\":",stdout);lumen_json(type);printf(",\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d",x,y,w,h);}
static void ui_paint_heading(const char*s,int actions){lumen_cmd("heading",28,74,544,44);fputs(",\"text\":",stdout);lumen_json(s);printf(",\"actions\":%d}",actions);}
static void ui_paint_text(int x,int y,int w,int h,const char*s,PaintText role){lumen_cmd("text",x,y,w,h);fputs(",\"text\":",stdout);lumen_json(s);printf(",\"role\":%d}",role);}
static void lumen_text(int x,int y,int w,int h,const char*s,PaintText role,int gray,int left) {
 lumen_cmd("text",x,y,w,h);fputs(",\"text\":",stdout);lumen_json(s);
 printf(",\"role\":%d,\"ink\":%d,\"left\":%d}",role,gray,left);
}
static void lumen_mono(int x,int y,int w,int h,const char*s,int gray) {
 lumen_cmd("mono",x,y,w,h);fputs(",\"text\":",stdout);lumen_json(s);
 printf(",\"ink\":%d}",gray);
}
static void ui_paint_icon_button(int x,int y,int icon){lumen_cmd("icon",x,y,44,44);printf(",\"icon\":%d}",icon);}
static void ui_paint_hitbox(int x,int y,int w,int h){lumen_cmd("hit",x,y,w,h);putchar('}');}
static void ui_paint_surface(int x,int y,int w,int h,int radius,int gray,int outline){lumen_cmd("surface",x,y,w,h);printf(",\"radius\":%d,\"gray\":%d,\"outline\":%d}",radius,gray,outline);}
static void ui_paint_ellipse(int x,int y,int rx,int ry,int gray){lumen_cmd("ellipse",x,y,0,0);printf(",\"rx\":%d,\"ry\":%d,\"gray\":%d}",rx,ry,gray);}
static void ui_paint_line(int x,int y,int x2,int y2,int weight,int gray){lumen_cmd("line",x,y,0,0);printf(",\"x2\":%d,\"y2\":%d,\"weight\":%d,\"gray\":%d}",x2,y2,weight,gray);}
static void lumen_hex_cell(int x,int y,int gray){lumen_cmd("hex_cell",x,y,0,0);printf(",\"gray\":%d}",gray);}
static void ui_paint_ring(int x,int y,int radius,int weight){lumen_cmd("ring",x,y,0,0);printf(",\"radius\":%d,\"weight\":%d}",radius,weight);}
static void lumen_button(int x,int y,int w,int h,const char*s,int primary){lumen_cmd("button",x,y,w,h);fputs(",\"text\":",stdout);lumen_json(s);printf(",\"primary\":%d}",primary);}
static void lumen_toggle(int x,int y,int value){lumen_cmd("toggle",x,y,64,40);printf(",\"value\":%d}",value);}
static void lumen_checkbox(int x,int y,int value){lumen_cmd("checkbox",x,y,30,30);printf(",\"value\":%d}",value);}
static void lumen_select_row(int y,const char*label,const char*value){lumen_cmd("select",30,y,540,48);fputs(",\"text\":",stdout);lumen_json(label);fputs(",\"value\":",stdout);lumen_json(value);putchar('}');}
static void lumen_select(const char**options,int count,int selected,int action){fputs("{\"request\":\"select\",\"options\":[",stdout);for(int i=0;i<count;i++){if(i)putchar(',');lumen_json(options[i]);}printf("],\"selected\":%d,\"action\":%d}\n",selected,action);fflush(stdout);}
static int64_t ui_clock_millis(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000LL+t.tv_nsec/1000000;}
static int app_state_save(const char*name,const void*bytes,size_t size){const char*root=getenv("LUMEN_APP_DATA");if(!root||strchr(name,'/')||strstr(name,".."))return 0;char p[2048],temp[2048];snprintf(p,sizeof(p),"%s/%s",root,name);snprintf(temp,sizeof(temp),"%s.new",p);FILE*f=fopen(temp,"wb");if(!f)return 0;int ok=fwrite(bytes,1,size,f)==size;if(fclose(f))ok=0;return ok&&!rename(temp,p);}
static int app_state_load(const char*name,void*bytes,size_t size){const char*root=getenv("LUMEN_APP_DATA");if(!root||strchr(name,'/')||strstr(name,".."))return 0;char p[2048];snprintf(p,sizeof(p),"%s/%s",root,name);FILE*f=fopen(p,"rb");if(!f)return 0;int ok=fread(bytes,1,size,f)==size&&fgetc(f)==EOF;fclose(f);return ok;}
/* One-time reads for apps that predate private state. Save into app-state after importing. */
static int lumen_legacy_state(const char *name,void *bytes,size_t size) {
 const char *root=getenv("LUMEN_APP_DATA");
 if(!root || !*name || strchr(name,'/') || strstr(name,".."))return 0;
 char path[2048];
 if(snprintf(path,sizeof(path),"%s/../../%s",root,name)>=(int)sizeof(path))return 0;
 FILE *f=fopen(path,"rb");if(!f)return 0;
 int ok=fread(bytes,1,size,f)==size && fgetc(f)==EOF;fclose(f);return ok;
}
static void lumen_request(const char*kind,const char*title,const char*text,int action){fputs("{\"request\":",stdout);lumen_json(kind);fputs(",\"title\":",stdout);lumen_json(title);fputs(",\"text\":",stdout);lumen_json(text);printf(",\"action\":%d}\n",action);fflush(stdout);}
/* Existing small app modules can reuse their Input-based controller directly. */
static void lumen_open(const char*id){fputs("{\"request\":\"open\",\"app\":",stdout);lumen_json(id);puts("}");fflush(stdout);}
static void lumen_run(void(*draw)(void),int(*input)(const Input*),void(*save)(void)) {
 static char output[65536];setvbuf(stdout,output,_IOFBF,sizeof(output));
 while(lumen_next()) {
   if(lumen_is("pause")){if(save)save();continue;}
   if(lumen_is("open"))lumen_language=lumen_number("language");
   if(lumen_is("back")){Input in={.kind=INPUT_BACK};input(&in);if(save)save();}
   if(lumen_is("text")||lumen_is("select")){Input in={.kind=lumen_is("text")?INPUT_TEXT:INPUT_SELECT,.x=lumen_number("action"),.y=lumen_number("value")};input(&in);if(save)save();}
   if(lumen_is("input")){Input in={lumen_number("kind"),lumen_number("x"),lumen_number("y"),lumen_number("dx"),lumen_number("dy")};input(&in);if(save)save();}
   if(lumen_is("open")||lumen_is("resume")||lumen_is("input")||lumen_is("back")||lumen_is("text")||lumen_is("select")){lumen_begin();draw();lumen_end();}
 }
 if(save)save();
}
#endif
