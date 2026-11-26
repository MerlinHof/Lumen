/* Overlay bounds in the logical viewport govern compositing and touch capture. */
typedef struct {
    int x,y,w,h;
} OverlayBounds;

typedef enum { OVERLAY_PERSISTENT, OVERLAY_FLOATING, OVERLAY_MODAL, OVERLAY_FULLSCREEN } OverlayPolicy;

typedef struct {
    OverlayBounds bounds;
    OverlayPolicy policy;
    unsigned sequence;
    int owner,handle,process;
} PackageOverlay;

enum { MAX_PACKAGE_OVERLAYS=3 };
static PackageOverlay package_overlays[MAX_PACKAGE_OVERLAYS];
static unsigned overlay_sequence;
static int overlay_capture=-1;

static void overlay_paint(const char *frame,OverlayBounds bounds,const char *title) {
    static unsigned char previous[WIDTH*HEIGHT];
    int widgets=widget_count;
    if(!*frame || bounds.x<0 || bounds.y<0 || bounds.w<=0 || bounds.h<=0 ||
       bounds.w>WIDTH-bounds.x || bounds.h>HEIGHT-bounds.y)return;
    int full_surface=!bounds.x && !bounds.y && bounds.w==WIDTH && bounds.h==HEIGHT;
    if(!full_surface)memcpy(previous,ui_pixels,sizeof(ui_pixels));
    overlay_live_title=title;
    package_paint_frame_mode(frame,PACKAGE_PAINT_OVERLAY);
    overlay_live_title=NULL;
    for(int y=0;!full_surface && y<HEIGHT;y++) {
        size_t offset=(size_t)y*WIDTH;
        if(y<bounds.y || y>=bounds.y+bounds.h) {
            memcpy(ui_pixels+offset,previous+offset,WIDTH);
            continue;
        }
        memcpy(ui_pixels+offset,previous+offset,bounds.x);
        size_t right=offset+bounds.x+bounds.w;
        memcpy(ui_pixels+right,previous+right,WIDTH-bounds.x-bounds.w);
    }
    widget_count=widgets;
}
typedef struct {
    OverlayBounds bounds;
    char command[24],app[64],intent[48];
} OverlayAction;
static int overlay_actions(const char *frame,OverlayAction *actions,int capacity) {
    Json json;
    JsonToken tokens[2048];
    if(!json_parse(&json,frame,tokens,2048))return 0;
    int list=json_get(&json,0,"ui"),count=0;
    for(int i=0;count<capacity;i++) {
        int item=json_at(&json,list,i);
        if(item<0)break;
        char type[24];
        json_string(&json,json_get(&json,item,"type"),type,sizeof(type));
        if(strcmp(type,"overlay_action"))continue;
        int relative=package_value(&json,0,"geometry")==1;
        OverlayBounds bounds={package_position(&json,item,"x",WIDTH,relative),
                              package_position(&json,item,"y",HEIGHT,relative),
                              package_position(&json,item,"w",WIDTH,relative),
                              package_position(&json,item,"h",HEIGHT,relative)};
        if(bounds.x<0 || bounds.y<0 || bounds.w<1 || bounds.h<1 ||
           bounds.w>WIDTH-bounds.x || bounds.h>HEIGHT-bounds.y)continue;
        OverlayAction *action=&actions[count++];
        action->bounds=bounds;
        json_string(&json,json_get(&json,item,"command"),action->command,sizeof(action->command));
        json_string(&json,json_get(&json,item,"app"),action->app,sizeof(action->app));
        json_string(&json,json_get(&json,item,"intent"),action->intent,sizeof(action->intent));
    }
    return count;
}
static const OverlayAction *overlay_slot_actions(int slot,int *count) {
    static struct {uint64_t serial;int count;OverlayAction actions[16];} cache[APP_OVERLAY_COUNT];
    *count=0;
    if(slot<0 || slot>=APP_OVERLAY_COUNT || app_overlay_fd(slot)<0)return NULL;
    uint64_t serial=app_overlay_frame_serial(slot);
    if(cache[slot].serial!=serial) {
        cache[slot].serial=serial;
        cache[slot].count=overlay_actions(app_overlay_frame(slot),cache[slot].actions,16);
    }
    *count=cache[slot].count;
    return cache[slot].actions;
}
static int overlay_contains(OverlayBounds box,int x,int y) {
    return x>=box.x && x<box.x+box.w && y>=box.y && y<box.y+box.h;
}
static const OverlayAction *overlay_hit_action(int slot,OverlayBounds layer,int x,int y,const char *command) {
    int count;
    const OverlayAction *actions=overlay_slot_actions(slot,&count);
    if(!overlay_contains(layer,x,y))return NULL;
    while(count>0) {
        const OverlayAction *action=&actions[--count];
        if((!command || !strcmp(action->command,command)) && overlay_contains(action->bounds,x,y))
            return action;
    }
    return NULL;
}
static void package_overlay_close(int index) {
    app_overlay_close(package_overlays[index].process);
    package_overlays[index]=(PackageOverlay){0};
    if(overlay_capture==index)overlay_capture=-1;
}
static void package_overlays_leave(int owner) {
    for(int i=0;i<MAX_PACKAGE_OVERLAYS;i++)
        if(package_overlays[i].sequence && package_overlays[i].owner==owner &&
           package_overlays[i].policy!=OVERLAY_PERSISTENT)
            package_overlay_close(i);
}
static void package_overlay_request(Json *json,const char *kind,int action) {
    int owner=app_process_owner();
    int handle=package_value(json,0,"handle");
    int free_slot=-1,existing=-1;
    for(int i=0;i<MAX_PACKAGE_OVERLAYS;i++) {
        if(!package_overlays[i].sequence && free_slot<0)free_slot=i;
        if(package_overlays[i].sequence && package_overlays[i].owner==owner &&
           package_overlays[i].handle==handle)existing=i;
    }
    if(!strcmp(kind,"overlay_close")) {
        if(existing>=0)package_overlay_close(existing);
        package_acknowledge(action,existing>=0);
        return;
    }
    char key[64], presentation[16],anchor[16];
    json_string(json,json_get(json,0,"app"),key,sizeof(key));
    json_string(json,json_get(json,0,"presentation"),presentation,sizeof(presentation));
    json_string(json,json_get(json,0,"anchor"),anchor,sizeof(anchor));
    int policy=!strcmp(presentation,"persistent") ? OVERLAY_PERSISTENT :
               !strcmp(presentation,"floating") ? OVERLAY_FLOATING :
               !strcmp(presentation,"modal") ? OVERLAY_MODAL :
               !strcmp(presentation,"fullscreen") ? OVERLAY_FULLSCREEN : -1;
    int index=existing>=0 ? existing : free_slot;
    int relative=package_value(json,0,"geometry")==1;
    int w=package_position(json,0,"width",WIDTH,relative);
    int h=package_position(json,0,"height",HEIGHT,relative);
    if(policy==OVERLAY_FULLSCREEN){w=WIDTH;h=HEIGHT-STATUS_HEIGHT;}
    if(owner<0 || index<0 || handle<=0 || policy<0 || w<32 || w>WIDTH ||
       h<32 || h>HEIGHT-STATUS_HEIGHT ||
       (strcmp(anchor,"top") && strcmp(anchor,"center") && strcmp(anchor,"bottom"))) {
        package_acknowledge(action,0);
        return;
    }
    int y=policy==OVERLAY_FULLSCREEN || !strcmp(anchor,"top") ? STATUS_HEIGHT :
          !strcmp(anchor,"bottom") ? HEIGHT-h : STATUS_HEIGHT+(HEIGHT-STATUS_HEIGHT-h)/2;
    OverlayBounds bounds={(WIDTH-w)/2,y,w,h};
    char event[224];
    snprintf(event,sizeof(event),"{\"event\":\"overlay_open\",\"presentation\":\"%s\",\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d}",
             presentation,bounds.x,bounds.y,w,h);
    if(existing>=0)package_overlay_close(existing);
    int process=app_overlay_open(key,event);
    if(process>=0) {
        if(++overlay_sequence==0)++overlay_sequence;
        package_overlays[index]=(PackageOverlay){bounds,policy,overlay_sequence,owner,handle,process};
    }
    package_acknowledge(action,process>=0);
}
static uint64_t overlay_priority(const PackageOverlay *overlay) {
    return ((uint64_t)overlay->policy<<32)|overlay->sequence;
}
static int package_overlay_front(uint64_t before) {
    int front=-1;
    uint64_t highest=0;
    for(int i=0;i<MAX_PACKAGE_OVERLAYS;i++) {
        if(!package_overlays[i].sequence)continue;
        uint64_t priority=overlay_priority(&package_overlays[i]);
        if((!before || priority<before) && priority>highest){front=i;highest=priority;}
    }
    return front;
}
static int package_overlays_allowed(void) {
    return !sleeping && !updating && !editor_ui.keyboard && !dropdown && !power_menu && !extra_ringing();
}
static void package_overlays_draw(void) {
    int stack[MAX_PACKAGE_OVERLAYS],count=0;
    uint64_t before=0;
    for(int i;(i=package_overlay_front(before))>=0;) {
        stack[count++]=i;
        before=overlay_priority(&package_overlays[i]);
    }
    while(count) {
        int i=stack[--count];
        const char *frame=app_overlay_frame(package_overlays[i].process);
        if(*frame)overlay_paint(frame,package_overlays[i].bounds,NULL);
    }
}
static int package_overlays_input(const Input *input) {
    if(input->kind==INPUT_CANCEL){overlay_capture=-1;return 0;}
    int pointer=input->kind==INPUT_DOWN || input->kind==INPUT_MOVE ||
                input->kind==INPUT_HOLD || input->kind==INPUT_TAP || input->kind==INPUT_SWIPE;
    if(!pointer)return 0;
    for(uint64_t before=0;;) {
        int index=overlay_capture>=0 ? overlay_capture : package_overlay_front(before);
        if(index<0)return 0;
        PackageOverlay *layer=&package_overlays[index];
        OverlayBounds box=layer->bounds;
        int inside=overlay_contains(box,input->x,input->y);
        if(overlay_capture>=0 || inside || layer->policy>=OVERLAY_MODAL) {
            if(input->kind==INPUT_DOWN)overlay_capture=index;
            if(input->kind==INPUT_TAP || input->kind==INPUT_SWIPE)overlay_capture=-1;
            if(inside || layer->policy!=OVERLAY_MODAL) {
                char event[256];
                package_format_input(event,sizeof(event),input,0);
                app_overlay_event(layer->process,event);
            }
            return 1;
        }
        before=overlay_priority(layer);
    }
}
static int package_overlay_poll(int slot) {
    int changed=app_overlay_poll(slot);
    if(app_overlay_fd(slot)<0)
        for(int i=0;i<MAX_PACKAGE_OVERLAYS;i++)
            if(package_overlays[i].sequence && package_overlays[i].process==slot)
                package_overlay_close(i);
    return changed;
}
