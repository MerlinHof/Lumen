#include "lumen_app.h"

static void item(const char *id,int x,int width,const char *label,int value,int flag) {
    lumen_cmd("status_item",x,0,width,56);
    fputs(",\"id\":",stdout);lumen_json(id);
    fputs(",\"text\":",stdout);lumen_json(label);
    printf(",\"value\":%d,\"flag\":%d}",value,flag);
}
static void legacy_status(void) {
    char date[24],clock[16],timer[72],percent[16];
    lumen_string("date",date,sizeof(date));
    lumen_string("clock",clock,sizeof(clock));
    lumen_string("timer",timer,sizeof(timer));
    int battery=lumen_number("battery"),back=lumen_number("back");
    int right=581,left=back ? 68 : 24;
    lumen_begin();
    if(back)item("back",0,62,"",0,0);
    if(lumen_number("show_battery")) {
        right-=32;
        item("battery",right,32,"",battery,lumen_number("charging"));
    }
    if(lumen_number("show_percent")) {
        if(right<581)right-=8;
        snprintf(percent,sizeof(percent),"%d%%",battery);
        right-=(int)strlen(percent)*13;
        item("percent",right,(int)strlen(percent)*13,percent,0,0);
    }
    if(lumen_number("show_wifi")) {
        if(right<581)right-=16;
        right-=30;
        item("wifi",right,30,"",0,lumen_number("offline"));
    }
    if(lumen_number("show_clock")) {
        if(right<581)right-=13;
        right-=65;
        item("clock",right,65,clock,0,0);
    }
    if(lumen_number("show_date")) {
        if(right<581)right-=12;
        right-=72;
        item("date",right,72,date,0,0);
    }
    int name_width=right-left-12;
    if(name_width>195)name_width=195;
    if(name_width>0)item("name",left,name_width,"",1,0);
    left+=name_width>0 ? name_width+12 : 0;
    if(*timer && right-left>98)item("timer",left,right-left,timer,0,0);
    lumen_end();
}

static void label(int x,int width,const char *text,int centered,int live,int small) {
    lumen_cmd("hud_text",x,0,width,56);
    fputs(",\"text\":",stdout);lumen_json(text);
    printf(",\"center\":%d,\"live\":%d,\"small\":%d}",centered,live,small);
}
static void symbol(int x,int y,int width,int icon) {
    lumen_cmd("symbol",x,y,width,width);
    printf(",\"icon\":%d}",icon);
}
static void action(int x,int width,const char *command,const char *package,const char *intent) {
    if(x<0){width+=x;x=0;}
    if(width>600-x)width=600-x;
    lumen_cmd("overlay_action",x,0,width,56);
    fputs(",\"command\":",stdout);lumen_json(command);
    fputs(",\"app\":",stdout);lumen_json(package);
    fputs(",\"intent\":",stdout);lumen_json(intent);
    putchar('}');
}
static void meter(int x,int battery,int charging) {
    ui_paint_surface(x,19,30,18,9,0,0);
    ui_paint_surface(x+2,21,26,14,7,255,0);
    ui_paint_surface(x+30,26,2,4,1,0,0);
    int fill=22*battery/100;
    if(fill>22)fill=22;
    if(fill>0)ui_paint_surface(x+4,23,fill,10,5,0,0);
    if(charging) {
        ui_paint_line(x+18,20,x+11,29,5,255);
        ui_paint_line(x+11,29,x+18,28,5,255);
        ui_paint_line(x+18,28,x+13,35,5,255);
        ui_paint_line(x+18,20,x+11,29,2,0);
        ui_paint_line(x+11,29,x+18,28,2,0);
        ui_paint_line(x+18,28,x+13,35,2,0);
    }
}
static void status(void) {
    char date[24],clock[16],timer[72],percent[16];
    lumen_string("date",date,sizeof(date));
    lumen_string("clock",clock,sizeof(clock));
    lumen_string("timer",timer,sizeof(timer));
    int battery=lumen_number("battery"),back=lumen_number("back");
    int right=581,left=back ? 68 : 24,battery_left=-1;
    lumen_begin();
    if(back) {
        symbol(9,6,44,IC_BACK);
        action(0,62,"back","","");
    }
    if(lumen_number("show_battery")) {
        right-=32;battery_left=right;
        meter(right,battery,lumen_number("charging"));
    }
    if(lumen_number("show_percent")) {
        if(right<581)right-=8;
        snprintf(percent,sizeof(percent),"%d%%",battery);
        int width=(int)strlen(percent)*13;
        right-=width;battery_left=right;
        label(right,width,percent,1,0,0);
    }
    if(battery_left>=0)action(battery_left-6,587-battery_left,"battery","","");
    if(lumen_number("show_wifi")) {
        if(right<581)right-=16;
        right-=30;
        lumen_cmd("wifi_symbol",right,6,30,44);
        printf(",\"offline\":%d}",lumen_number("offline"));
        action(right-5,44,"open","lumen.settings","wifi");
    }
    if(lumen_number("show_clock")) {
        if(right<581)right-=13;
        right-=65;
        label(right,65,clock,1,0,0);
        action(right,65,"open","lumen.clock","clock-face");
    }
    if(lumen_number("show_date")) {
        if(right<581)right-=12;
        right-=72;
        label(right,72,date,1,0,0);
        action(right,72,"open","lumen.calendar","");
    }
    int name_width=right-left-12;
    if(name_width>195)name_width=195;
    if(name_width>0)label(left,name_width,"",0,1,0);
    left+=name_width>0 ? name_width+12 : 0;
    if(*timer && right-left>98) {
        symbol(left,15,25,IC_CLOCK);
        label(left+27,right-left-27,timer,1,0,1);
        action(left,right-left,"open","lumen.clock","clock-timer");
    }
    lumen_end();
}
int main(void) {
    int overlay=0,surface_api=0;
    while(lumen_next()) {
        if(lumen_is("open")) {
            char role[24];lumen_string("role",role,sizeof(role));
            surface_api=lumen_number("surface_api");
            overlay=!strcmp(role,"overlay");lumen_language=lumen_number("language");
            if(!overlay) {
                lumen_begin();
                lumen_page(lumen_language==0 ? "Statusleiste" : lumen_language==2 ? "Barre d’état" : "Status Bar");
                lumen_component_button("settings",lumen_language==0 ? "Einstellungen" :
                    lumen_language==2 ? "Réglages" : "Settings",0);
                lumen_end();
            }
         } else if(overlay && lumen_is("system_state")) {
            if(surface_api>=2)status();
            else legacy_status();
        }
        else if(!overlay && lumen_is("component"))lumen_open("lumen.settings");
    }
    return 0;
}
