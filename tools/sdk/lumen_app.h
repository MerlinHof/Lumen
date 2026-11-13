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
#include <fcntl.h>
#include "lumen_input.h"
#include "lumen_icons.h"
typedef enum{PAINT_BODY,PAINT_HEADING,PAINT_DISPLAY,PAINT_SECONDARY,PAINT_TITLE}PaintText;
static char lumen_event[65536];static int lumen_first,lumen_language,lumen_back,lumen_frame_tick,lumen_fullscreen,lumen_ready=1;
static void lumen_json(const char *s) {
 putchar('"');
 for(;*s;s++) {
     unsigned char c=*s;
     if(c==34 || c=='\\'){putchar('\\');putchar(c);}
     else if(c=='\n')fputs("\\n",stdout);
     else if(c=='\r')fputs("\\r",stdout);
     else if(c=='\t')fputs("\\t",stdout);
     else if(c<32)printf("\\u%04x",c);
     else putchar(c);
 }
 putchar('"');
}
#include "lumen_wire.h"
static int lumen_next(void) {
 while(fgets(lumen_event,sizeof(lumen_event),stdin)) {
     if(!strchr(lumen_event,'\n') && !feof(stdin)) {
         int c;while((c=getchar())!=EOF && c!='\n'){}
         continue;
     }
     if(wire_read(lumen_event))return 1;
 }
 wire_event.count=0;return 0;
}
static int lumen_is(const char *event) {
 char name[48];return lumen_string("event",name,sizeof(name)) && !strcmp(name,event);
}
static void lumen_begin(void){lumen_first=1;lumen_frame_tick=0;printf("{\"back\":%s,\"ui\":[",lumen_back?"true":"false");}
static void lumen_end(void){printf("],\"tick\":%d,\"fullscreen\":%d,\"ready\":%s}\n",lumen_frame_tick,lumen_fullscreen,lumen_ready ? "true" : "false");fflush(stdout);}
static void lumen_schedule(int milliseconds){lumen_frame_tick=milliseconds;}
static void lumen_cmd(const char*type,int x,int y,int w,int h){if(!lumen_first)putchar(',');lumen_first=0;fputs("{\"type\":",stdout);lumen_json(type);printf(",\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d",x,y,w,h);}
static void ui_paint_heading(const char*s,int actions){lumen_cmd("heading",28,74,544,44);fputs(",\"text\":",stdout);lumen_json(s);printf(",\"actions\":%d}",actions);}
static void lumen_formula(int x,int y,int w,int h,const char *expression) {
 lumen_cmd("formula",x,y,w,h);fputs(",\"text\":",stdout);lumen_json(expression);putchar('}');
}
static void lumen_editor_line(int x,int y,int w,int h,const char *value,int cursor) {
 lumen_cmd("editor_line",x,y,w,h);fputs(",\"text\":",stdout);lumen_json(value);
 printf(",\"cursor\":%d}",cursor);
}
static inline void lumen_editor_action(const char *id,int x,int y,int w,int h,const char *value,int cursor) {
 lumen_cmd("editor_line",x,y,w,h);
 fputs(",\"id\":",stdout);lumen_json(id);
 fputs(",\"text\":",stdout);lumen_json(value);
 printf(",\"cursor\":%d}",cursor);
}
static void ui_paint_text(int x,int y,int w,int h,const char*s,PaintText role){lumen_cmd("text",x,y,w,h);fputs(",\"text\":",stdout);lumen_json(s);printf(",\"role\":%d}",role);}
static void lumen_text(int x,int y,int w,int h,const char*s,PaintText role,int gray,int left) {
 lumen_cmd("text",x,y,w,h);fputs(",\"text\":",stdout);lumen_json(s);
 printf(",\"role\":%d,\"ink\":%d,\"left\":%d}",role,gray,left);
}
static void lumen_mono(int x,int y,int w,int h,const char*s,int gray) {
 lumen_cmd("mono",x,y,w,h);fputs(",\"text\":",stdout);lumen_json(s);
 printf(",\"ink\":%d}",gray);
}
static void lumen_mono_columns(int x,int y,int w,int h,const char*s,int gray,int advance) {
 lumen_cmd("mono",x,y,w,h);fputs(",\"text\":",stdout);lumen_json(s);
 printf(",\"ink\":%d,\"advance\":%d}",gray,advance);
}
static void lumen_list_action(const char *id,int y,int height,const char *title,const char *detail,int icon) {
 lumen_cmd("list_action",28,y,544,height);
 fputs(",\"id\":",stdout);lumen_json(id);
 fputs(",\"text\":",stdout);lumen_json(title);
 fputs(",\"detail\":",stdout);lumen_json(detail);
 printf(",\"icon\":%d}",icon);
}
static void lumen_icon_action(const char *id,int x,int y,int icon) {
 lumen_cmd("icon_action",x,y,44,44);fputs(",\"id\":",stdout);lumen_json(id);
 printf(",\"icon\":%d}",icon);
}
static void ui_paint_icon_button(int x,int y,int icon){lumen_cmd("icon",x,y,44,44);printf(",\"icon\":%d}",icon);}
static void ui_paint_hitbox(int x,int y,int w,int h){lumen_cmd("hit",x,y,w,h);putchar('}');}
static void ui_paint_surface(int x,int y,int w,int h,int radius,int gray,int outline){lumen_cmd("surface",x,y,w,h);printf(",\"radius\":%d,\"gray\":%d,\"outline\":%d}",radius,gray,outline);}
static void ui_paint_ellipse(int x,int y,int rx,int ry,int gray){lumen_cmd("ellipse",x,y,0,0);printf(",\"rx\":%d,\"ry\":%d,\"gray\":%d}",rx,ry,gray);}
/* A batch contains at most 3000 points with radii from 1 to 4. */
static void lumen_points_begin(int gray) {
 lumen_cmd("points",0,56,0,0);printf(",\"gray\":%d,\"points\":[",gray);
}
static void lumen_point(int index,int x,int y,int radius) {
 printf("%s%d,%d,%d",index?",":"",x,y,radius);
}
static void lumen_points_end(void){fputs("]}",stdout);}
static void lumen_arc(int x,int y,int radius,int start,int end,int weight,int gray) {
 lumen_cmd("arc",x,y,0,0);printf(",\"radius\":%d,\"start\":%d,\"end\":%d,\"weight\":%d,\"gray\":%d}",radius,start,end,weight,gray);
}
static void lumen_display_text(int x,int y,int w,int h,const char *value,int family,int size,int gray) {
 lumen_cmd("display_text",x,y,w,h);fputs(",\"text\":",stdout);lumen_json(value);
 printf(",\"family\":%d,\"size\":%d,\"gray\":%d}",family,size,gray);
}
static void ui_paint_line(int x,int y,int x2,int y2,int weight,int gray){lumen_cmd("line",x,y,0,0);printf(",\"x2\":%d,\"y2\":%d,\"weight\":%d,\"gray\":%d}",x2,y2,weight,gray);}
static void lumen_weather_symbol(int x,int y,int size,int code) {
 lumen_cmd("weather_symbol",x,y,size,size);
 printf(",\"code\":%d}",code);
}
static void lumen_chess_piece(int x,int y,int kind,int rotated) {
 lumen_cmd("chess_piece",x,y,66,66);
 printf(",\"piece\":%d,\"rotated\":%d}",kind,!!rotated);
}
static void lumen_hex_cell(int x,int y,int gray){lumen_cmd("hex_cell",x,y,0,0);printf(",\"gray\":%d}",gray);}
static void ui_paint_ring(int x,int y,int radius,int weight){lumen_cmd("ring",x,y,0,0);printf(",\"radius\":%d,\"weight\":%d}",radius,weight);}
static void lumen_button(int x,int y,int w,int h,const char*s,int primary){lumen_cmd("button",x,y,w,h);fputs(",\"text\":",stdout);lumen_json(s);printf(",\"primary\":%d}",primary);}
static void lumen_toggle(int x,int y,int value){lumen_cmd("toggle",x,y,64,40);printf(",\"value\":%d}",value);}
static void lumen_checkbox(int x,int y,int value){lumen_cmd("checkbox",x,y,30,30);printf(",\"value\":%d}",value);}
static void lumen_select_row(int y,const char*label,const char*value){lumen_cmd("select",30,y,540,48);fputs(",\"text\":",stdout);lumen_json(label);fputs(",\"value\":",stdout);lumen_json(value);putchar('}');}
static void lumen_select(const char**options,int count,int selected,int action){fputs("{\"request\":\"select\",\"options\":[",stdout);for(int i=0;i<count;i++){if(i)putchar(',');lumen_json(options[i]);}printf("],\"selected\":%d,\"action\":%d}\n",selected,action);fflush(stdout);}
static int64_t ui_clock_millis(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000LL+t.tv_nsec/1000000;}
static int lumen_storage_path(const char *name,char *path,size_t capacity) {
 const char *root=getenv("LUMEN_APP_DATA");
 if(!root || !*root || !name || !*name || name[0]=='.' || strchr(name,'/') || strchr(name,'\\'))return 0;
 int written=snprintf(path,capacity,"%s/%s",root,name);
 return written>0 && (size_t)written<capacity;
}
#include "lumen_storage.h"
static int app_state_save(const char *name,const void *bytes,size_t size) {
 return lumen_storage_write(name,bytes,size);
}
static int app_state_load(const char *name,void *bytes,size_t size) {
 size_t actual;
 return lumen_storage_read(name,bytes,size,&actual) && actual==size;
}
/* One-time import from linked apps. The runtime supplies the original data root. */
static int lumen_legacy_read(const char *name,void *bytes,size_t size,size_t *actual) {
 if(actual)*actual=0;
 const char *root=getenv("LUMEN_LEGACY_DATA");
 if(!root || !*root || !name || !*name || name[0]=='.' || strchr(name,'/'))return 0;
 return lumen_storage_read_at(root,name,bytes,size,actual);
}
static int lumen_legacy_state(const char *name,void *bytes,size_t size) {
 size_t actual;
 return lumen_legacy_read(name,bytes,size,&actual) && actual==size;
}
static void lumen_service(const char *name,const char *operation,const char *data,int action) {
 fputs("{\"request\":\"service\",\"service\":",stdout);lumen_json(name);
 fputs(",\"operation\":",stdout);lumen_json(operation);
 fputs(",\"text\":",stdout);lumen_json(data);printf(",\"action\":%d}\n",action);fflush(stdout);
}
static void lumen_app_icon(int x,int y,int size,const char *key) {
 lumen_cmd("app_icon",x,y,size,size);fputs(",\"text\":",stdout);lumen_json(key);putchar('}');
}
static void lumen_app_tile(int x,int y,int width,const char *id) {
 lumen_cmd("app_tile",x,y,width,110);fputs(",\"text\":",stdout);lumen_json(id);putchar('}');
}
static inline void lumen_app_tile_action(int x,int y,int width,int height,const char *id) {
 lumen_cmd("app_tile",x,y,width,height);
 fputs(",\"text\":",stdout);lumen_json(id);
 fputs(",\"id\":",stdout);lumen_json(id);putchar('}');
}
static void lumen_note_surface(void) {
 lumen_cmd("note_surface",0,56,600,688);putchar('}');
}
static void lumen_document_page(int chrome) {
 lumen_cmd("document_page",0,56,600,680);printf(",\"chrome\":%d}",chrome);
}
static void lumen_photo(int x,int y,int w,int h,const char *key,int fill,int rotation,int full) {
 lumen_cmd("photo",x,y,w,h);fputs(",\"text\":",stdout);lumen_json(key);
 printf(",\"fill\":%d,\"rotation\":%d,\"full\":%d}",fill,rotation,full);
}
static void lumen_book_cover(int x,int y,int w,int h,const char *key) {
 lumen_cmd("book_cover",x,y,w,h);fputs(",\"text\":",stdout);lumen_json(key);putchar('}');
}
static void lumen_book_label(int x,int y,int w,const char *title,const char *author) {
 lumen_cmd("book_label",x,y,w,135);fputs(",\"text\":",stdout);lumen_json(title);
 fputs(",\"author\":",stdout);lumen_json(author);putchar('}');
}
static void lumen_catalog_cover(int x,int y,int w,int h,const char *key) {
 lumen_cmd("catalog_cover",x,y,w,h);fputs(",\"text\":",stdout);lumen_json(key);putchar('}');
}
static void lumen_asset(int x,int y,int w,int h,const char *name) {
 lumen_cmd("asset",x,y,w,h);fputs(",\"text\":",stdout);lumen_json(name);putchar('}');
}
static void lumen_sprite(const char *asset,int x,int y,int size,int index,int transparent) {
 lumen_cmd("sprite",x,y,size,size);
 fputs(",\"text\":",stdout);lumen_json(asset);
 printf(",\"index\":%d,\"transparent\":%d}",index,!!transparent);
}
static void lumen_flow_list(int columns,int page) {
 lumen_cmd("flow_list",28,137,544,583);
 printf(",\"columns\":%d,\"page\":%d,\"rows\":[",columns,page);
}
static void lumen_flow_row(int row,const char *id,const char *title,int column,int checked,int pinned) {
 if(row)putchar(',');
 fputs("{\"id\":",stdout);lumen_json(id);
 fputs(",\"title\":",stdout);lumen_json(title);
 printf(",\"column\":%d,\"checked\":%d,\"pinned\":%d}",column,!!checked,!!pinned);
}
static void lumen_flow_end(void){fputs("]}",stdout);}
static void lumen_segments(const char *id,int x,int y,int w,int h,const char **labels,int count,int selected) {
 lumen_cmd("component_segments",x,y,w,h);
 fputs(",\"id\":",stdout);lumen_json(id);
 fputs(",\"labels\":[",stdout);
 for(int i=0;i<count;i++){if(i)putchar(',');lumen_json(labels[i]);}
 printf("],\"selected\":%d}",selected);
}
static void lumen_card(const char *id,int x,int y,int w,int h,const char *title,const char *detail) {
 lumen_cmd("component_card",x,y,w,h);
 fputs(",\"id\":",stdout);lumen_json(id);
 fputs(",\"text\":",stdout);lumen_json(title);
 fputs(",\"detail\":",stdout);lumen_json(detail);putchar('}');
}
static void lumen_page(const char *title) {
 lumen_cmd("page_layout",28,74,544,44);
 fputs(",\"text\":",stdout);lumen_json(title);putchar('}');
}
static void lumen_component_button(const char *id,const char *label,int primary) {
 lumen_cmd("component_button",28,0,544,50);
 fputs(",\"id\":",stdout);lumen_json(id);
 fputs(",\"text\":",stdout);lumen_json(label);
 printf(",\"primary\":%d}",primary);
}
static inline void lumen_button_action(const char *id,int x,int y,int w,int h,const char *label,int primary) {
 lumen_cmd("button_action",x,y,w,h);
 fputs(",\"id\":",stdout);lumen_json(id);
 fputs(",\"text\":",stdout);lumen_json(label);
 printf(",\"primary\":%d}",primary);
}
static inline void lumen_custom_action(const char *id,int x,int y,int w,int h) {
 lumen_cmd("custom_action",x,y,w,h);
 fputs(",\"id\":",stdout);lumen_json(id);putchar('}');
}
static inline void lumen_select_action(const char *id,int y,const char *label,const char *value) {
 lumen_cmd("select_action",28,y,544,48);
 fputs(",\"id\":",stdout);lumen_json(id);
 fputs(",\"text\":",stdout);lumen_json(label);
 fputs(",\"value\":",stdout);lumen_json(value);putchar('}');
}
static void lumen_component_choice(const char *id,const char *label,int checked,int checkbox) {
 lumen_cmd(checkbox ? "component_checkbox" : "component_toggle",28,0,544,50);
 fputs(",\"id\":",stdout);lumen_json(id);
 fputs(",\"text\":",stdout);lumen_json(label);
 printf(",\"checked\":%d}",!!checked);
}
static void lumen_component_toggle(const char *id,const char *label,int checked) {
 lumen_component_choice(id,label,checked,0);
}
static void lumen_component_checkbox(const char *id,const char *label,int checked) {
 lumen_component_choice(id,label,checked,1);
}
static void lumen_textfield(const char *id,const char *label,const char *value) {
 lumen_cmd("component_field",28,0,544,52);
 fputs(",\"id\":",stdout);lumen_json(id);
 fputs(",\"text\":",stdout);lumen_json(label);
 fputs(",\"value\":",stdout);lumen_json(value);putchar('}');
}
static void lumen_qr(int x,int y,int side,const char *value) {
 lumen_cmd("qr",x,y,side,side);fputs(",\"text\":",stdout);lumen_json(value);putchar('}');
}
static void lumen_pager(int y,int page,int count) {
 lumen_cmd("pager",205,y,189,44);printf(",\"page\":%d,\"count\":%d}",page,count);
}
static void lumen_wrapped(int x,int y,int w,int h,const char *text,PaintText role,int lines) {
 lumen_cmd("wrap",x,y,w,h);fputs(",\"text\":",stdout);lumen_json(text);
 printf(",\"role\":%d,\"lines\":%d}",role,lines);
}
static void lumen_request(const char*kind,const char*title,const char*text,int action){fputs("{\"request\":",stdout);lumen_json(kind);fputs(",\"title\":",stdout);lumen_json(title);fputs(",\"text\":",stdout);lumen_json(text);printf(",\"action\":%d}\n",action);fflush(stdout);}
/* Existing small app modules can reuse their Input-based controller directly. */
static void lumen_multiline(const char *title,const char *text,int action) {
 lumen_request("multiline",title,text,action);
}
static void lumen_app_open_with(const char *app,const char *action,const char *object,
                                const char *title,const char *text) {
 fputs("{\"request\":\"intent\",\"app\":",stdout);lumen_json(app);
 fputs(",\"operation\":",stdout);lumen_json(action);
 fputs(",\"object\":",stdout);lumen_json(object);
 fputs(",\"title\":",stdout);lumen_json(title);
 fputs(",\"text\":",stdout);lumen_json(text);puts("}");fflush(stdout);
}
/* A positive request ID asks the destination to return a result. */
static void lumen_call(const char *app,const char *operation,const char *data,int request_id) {
 fputs("{\"request\":\"intent\",\"app\":",stdout);lumen_json(app);
 fputs(",\"operation\":",stdout);lumen_json(operation);
 fputs(",\"text\":",stdout);lumen_json(data);
 printf(",\"request_id\":%d}\n",request_id);fflush(stdout);
}
static void lumen_return(int request_id,int success,const char *data) {
 printf("{\"request\":\"intent_return\",\"request_id\":%d,\"ok\":%d,\"text\":",request_id,!!success);
 lumen_json(data);puts("}");fflush(stdout);
}
/* Pass a Markdown document by reference, scoped to this app's private data. */
static void lumen_reader_file(const char *title,const char *path) {
 fputs("{\"request\":\"intent\",\"app\":\"lumen.reader\",\"operation\":\"open_file\",\"title\":",stdout);
 lumen_json(title);
 fputs(",\"document\":{\"type\":\"markdown\",\"path\":",stdout);lumen_json(path);
 puts("}}");fflush(stdout);
}
static void lumen_open(const char*id){fputs("{\"request\":\"open\",\"app\":",stdout);lumen_json(id);puts("}");fflush(stdout);}
static void (*lumen_service_handler)(const char *operation,const char *data);
static inline void lumen_app_service(const char *app,const char *operation,const char *data,int action) {
 fputs("{\"request\":\"app_service\",\"app\":",stdout);lumen_json(app);
 fputs(",\"operation\":",stdout);lumen_json(operation);
 fputs(",\"text\":",stdout);lumen_json(data);
 printf(",\"action\":%d}\n",action);fflush(stdout);
}
static inline void lumen_app_service_result(int ok,const char *data) {
 fputs(ok ? "{\"request\":\"service_result\",\"ok\":true,\"data\":" :
            "{\"request\":\"service_result\",\"ok\":false,\"data\":",stdout);
 lumen_json(data);puts("}");fflush(stdout);
}
static void lumen_run(void(*draw)(void),int(*input)(const Input*),void(*save)(void)) {
 static char output[65536];setvbuf(stdout,output,_IOFBF,sizeof(output));
 while(lumen_next()) {
   if(lumen_is("service_call")) {
      char operation[48],data[4097];
      lumen_language=lumen_number("language");
      if(lumen_service_handler && wire_bounded_string("operation",operation,sizeof(operation)) &&
         wire_bounded_string("data",data,sizeof(data)))lumen_service_handler(operation,data);
      else lumen_app_service_result(0,"");
      continue;
   }
   if(lumen_is("pause")){Input in={.kind=INPUT_PAUSE};input(&in);if(save)save();continue;}
   if(lumen_is("open")) {
      lumen_language=lumen_number("language");
      Input in={.kind=INPUT_OPEN};input(&in);
   }
   if(lumen_is("resume")){Input in={.kind=INPUT_RESUME};input(&in);}
   if(lumen_is("service")){Input in={.kind=INPUT_SERVICE,.x=lumen_number("action"),.y=lumen_number("ok")};input(&in);if(save)save();}
   if(lumen_is("component")){Input in={.kind=INPUT_COMPONENT,.x=lumen_number("changed")};input(&in);if(save)save();}
   char operation[48];
   int named_intent=lumen_is("intent") && lumen_string("operation",operation,sizeof(operation));
   if(named_intent||lumen_is("intent_result")) {
      Input in={.kind=lumen_is("intent")?INPUT_INTENT:INPUT_INTENT_RESULT,
                .x=lumen_number("request_id"),.y=lumen_number("ok")};
      input(&in);if(save)save();
   }
   if(lumen_is("refresh")){Input in={.kind=INPUT_REFRESH};input(&in);}
   if(lumen_is("tick")){Input in={.kind=INPUT_TICK};input(&in);}
   if(lumen_is("page")){Input in={.kind=INPUT_PAGE,.x=lumen_number("page")};input(&in);if(save)save();}
   if(lumen_is("viewport")){Input in={.kind=INPUT_VIEWPORT,.y=lumen_number("bottom")};input(&in);}
   if(lumen_is("back")){Input in={.kind=INPUT_BACK};input(&in);if(save)save();}
   if(lumen_is("text")||lumen_is("select")){Input in={.kind=lumen_is("text")?INPUT_TEXT:INPUT_SELECT,.x=lumen_number("action"),.y=lumen_number("value")};input(&in);if(save)save();}
   if(lumen_is("input")){Input in={lumen_number("kind"),lumen_number("x"),lumen_number("y"),lumen_number("dx"),lumen_number("dy")};input(&in);if(save)save();}
   if(lumen_is("open")||lumen_is("resume")||lumen_is("input")||lumen_is("back")||lumen_is("text")||lumen_is("select")||lumen_is("viewport")||lumen_is("service")||lumen_is("page")||lumen_is("tick")||lumen_is("refresh")||lumen_is("component")||named_intent||lumen_is("intent_result")){lumen_begin();draw();lumen_end();}
 }
 if(save)save();
}
#endif
