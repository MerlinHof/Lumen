#include "lumen_app.h"
#include <math.h>

static int clock_style,fullscreen,timer_panel;
static int timer_minutes=20,timer_total_seconds=1200;
static int alarm_hour=7,alarm_minute,alarm_enabled;
static int64_t timer_due,timer_paused;
static int64_t milliseconds(void) {
    struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);
    return (int64_t)t.tv_sec*1000+t.tv_nsec/1000000;
}
static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static const char *tr(const char *value) {
    static const char *strings[][3]={
        {"Uhr","Clock","Horloge"},{"Wecker","Alarm","Réveil"},
        {"Fortsetzen","Resume","Reprendre"},{"Timer starten","Start timer","Démarrer"},
        {"Zurücksetzen","Reset","Réinitialiser"},{"Weckzeit","Alarm time","Heure du réveil"},
        {"Wecker aktiv","Alarm enabled","Réveil activé"},
        {"Einmaliger Wecker zur eingestellten Uhrzeit.","One-time alarm at the set time.","Réveil unique à l’heure choisie."},
        {"Visuelles Signal beim Lesen. Im Tiefschlaf kein Wecksignal.",
         "Visual alert while reading. Deep sleep silences the alarm.",
         "Alerte visuelle pendant la lecture. Pas d’alerte en veille profonde."},
        {"Verbleibende Zeit","Time remaining","Temps restant"}
    };
    for(size_t i=0;i<sizeof(strings)/sizeof(*strings);i++)
        if(!strcmp(value,strings[i][0]))return word(strings[i][0],strings[i][1],strings[i][2]);
    return value;
}
static const char *face_label(int row);
enum {UI_SECONDARY=PAINT_SECONDARY,UI_PRIMARY=PAINT_BODY,UI_BODY=PAINT_BODY,
      UI_DISPLAY=PAINT_DISPLAY,UI_TITLE=PAINT_TITLE,RADIUS_LARGE=20};
#define ui_font(role) (role)
#define rect(x,y,w,h,tone) ui_paint_surface(x,y,w,h,0,tone,-1)
#define control_surface(x,y,w,h,r,tone) ui_paint_surface(x,y,w,h,r,tone,-1)
#define stroke_color ui_paint_line
#define aa_stroke(x,y,u,v,w) ui_paint_line(x,y,u,v,w,0)
#define aa_disc(x,y,r,tone) ui_paint_ellipse(x,y,r,r,tone)
#define ring ui_paint_ring
#define aa_arc_color lumen_arc
#define aa_arc(x,y,r,a,b,w) lumen_arc(x,y,r,a,b,w,0)
#define type_display lumen_display_text
#define divider(x,y,w) ui_paint_line(x,y,x+w,y,1,185)
#define centered(x,y,w,h,text,role) ui_paint_text(x,y,w,h,tr(text),role)
#define label_at(x,y,w,h,text,role) lumen_text(x,y,w,h,tr(text),role,-1,1)
#define icon_sized(x,y,id,size) ui_paint_icon_button(x,y,id)
#define key_outline(x,y,w,h) ui_paint_surface(x,y,w,h,16,255,105)
#define toggle lumen_toggle
#define wrapped(x,y,text,role,w,lines) lumen_wrapped(x,y,w,lines*34,tr(text),role,lines)
#include "ring.h"
#include "view.h"

static const char *face_label(int row) {
    static const char *en[]={"Classic · numbers","Classic","Roman","Station","Minimal","Orbit",
        "Bauhaus","Sundial · 24 h","Compass","Petals","Typography","Editorial","Hour by hour","Night"};
    static const char *fr[]={"Classique · chiffres","Classique","Romain","Gare","Minimal","Orbite",
        "Bauhaus","Cadran · 24 h","Boussole","Pétales","Typographie","Éditorial","Heure par heure","Nuit"};
    return word(clock_styles[row],en[row],fr[row]);
}
static void request_state(void){lumen_service("clock","state","",1);}
static void change(const char *operation,int value) {
    char data[32];snprintf(data,sizeof(data),"%d",value);lumen_service("clock",operation,data,1);
}
static void draw(void) {
    lumen_fullscreen=fullscreen;
    if(fullscreen==1)clock_face(1);
    else if(fullscreen==2)v1_timer_ring(1);
    else app_timer();
    if(timer_panel==1 && timer_due)lumen_schedule(1000);
    else if(!timer_panel)lumen_schedule((60-time(NULL)%60)*1000);
}
static void load_face(void) {
    if(app_state_load("face.bin",&clock_style,sizeof(clock_style))) {
        if(clock_style<0 || clock_style>13)clock_style=0;
        return;
    }
    char data[256];size_t size;
    int h,m,a,d,g;long long due;
    if(lumen_legacy_read("tools-settings.txt",data,sizeof(data)-1,&size)) {
        data[size]=0;
        if(sscanf(data,"%d %d %d %d %lld %d %d",&h,&m,&a,&d,&due,&g,&clock_style)!=7 ||
           clock_style<0 || clock_style>13)clock_style=0;
    }
}
static void activate(const char *id) {
    if(!strcmp(id,"fullscreen")){fullscreen=timer_panel==1 ? 2 : 1;lumen_back=1;}
    else if(!strcmp(id,"faces")) {
        const char *labels[CLOCK_FACE_COUNT];
        for(int i=0;i<CLOCK_FACE_COUNT;i++)labels[i]=face_label(i);
        lumen_select(labels,CLOCK_FACE_COUNT,clock_style_row(),2);
    } else if(!strcmp(id,"start-pause"))change("timer",0);
    else if(!strcmp(id,"reset"))change("timer",1);
    else if(!strcmp(id,"shorter") && timer_minutes>1)change("minutes",timer_minutes-1);
    else if(!strcmp(id,"longer") && timer_minutes<1440)change("minutes",timer_minutes+1);
    else if(!strcmp(id,"duration")) {
        char value[16];snprintf(value,sizeof(value),"%d",timer_minutes);
        lumen_request("input",word("Timer: Minuten","Timer minutes","Minuterie : minutes"),value,3);
    } else if(!strcmp(id,"hour-up"))change("hour",(alarm_hour+1)%24);
    else if(!strcmp(id,"hour-down"))change("hour",(alarm_hour+23)%24);
    else if(!strcmp(id,"minute-up"))change("minute",(alarm_minute+1)%60);
    else if(!strcmp(id,"minute-down"))change("minute",(alarm_minute+59)%60);
    else if(!strcmp(id,"arm"))change("armed",!alarm_enabled);
    else if(strlen(id)==8 && !strncmp(id,"preset-",7) && id[7]>='0' && id[7]<='3')
        change("minutes",(int[]){5,10,20,30}[id[7]-'0']);
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN){load_face();request_state();return 1;}
    if(in->kind==INPUT_RESUME || in->kind==INPUT_REFRESH){request_state();return 1;}
    if(in->kind==INPUT_TICK && timer_panel==1 && timer_due && timer_due<=milliseconds()){
        timer_due=0;request_state();return 1;
    }
    if(in->kind==INPUT_INTENT){
        char operation[32];lumen_string("operation",operation,sizeof(operation));
        if(!strcmp(operation,"clock-face"))timer_panel=0;
        if(!strcmp(operation,"clock-timer"))timer_panel=1;
        fullscreen=lumen_back=0;request_state();return 1;
    }
    if(in->kind==INPUT_SERVICE && in->x==1 && in->y) {
        timer_minutes=lumen_number("minutes");timer_total_seconds=lumen_number("total");
        int remaining=lumen_number("remaining");
        timer_due=lumen_number("running") ? milliseconds()+remaining : 0;
        timer_paused=lumen_number("paused") ? remaining : 0;
        alarm_hour=lumen_number("hour");alarm_minute=lumen_number("minute");
        alarm_enabled=lumen_number("armed");return 1;
    }
    if(in->kind==INPUT_BACK && fullscreen){fullscreen=lumen_back=0;return 1;}
    if(in->kind==INPUT_COMPONENT){
        char id[24],value[8],gesture[24];
        if(!lumen_string("id",id,sizeof(id)) || !lumen_string("gesture",gesture,sizeof(gesture)))return 0;
        if(!strcmp(gesture,"selected") && !strcmp(id,"mode") &&
           lumen_string("text",value,sizeof(value)) && strlen(value)==1 && value[0]>='0' && value[0]<='2'){
            timer_panel=value[0]-'0';request_state();
        } else if(!strcmp(gesture,"clicked"))activate(id);
        return 1;
    }
    if(in->kind==INPUT_SELECT && in->x==2 && in->y>=0 && in->y<CLOCK_FACE_COUNT) {
        clock_style=clock_style_ids[in->y];app_state_save("face.bin",&clock_style,sizeof(clock_style));return 1;
    }
    if(in->kind==INPUT_TEXT && in->x==3) {
        char value[24],*end;
        if(lumen_string("text",value,sizeof(value))){
            long minutes=strtol(value,&end,10);
            if(!*end && minutes>=1 && minutes<=1440)change("minutes",minutes);
        }
        return 1;
    }
    if(in->kind==INPUT_TAP && fullscreen) {
        fullscreen=lumen_back=0;
        return 1;
    }
    return 0;
}
int main(void){lumen_run(draw,input,NULL);return 0;}
