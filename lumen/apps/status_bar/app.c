#include "lumen_app.h"

static void item(const char *id,int x,int width,const char *label,int value,int flag) {
    lumen_cmd("status_item",x,0,width,56);
    fputs(",\"id\":",stdout);lumen_json(id);
    fputs(",\"text\":",stdout);lumen_json(label);
    printf(",\"value\":%d,\"flag\":%d}",value,flag);
}
static void status(void) {
    char name[96],date[24],clock[16],timer[72],percent[16];
    lumen_string("name",name,sizeof(name));
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
    int measured=(int)strlen(name)*13;
    if(name_width>measured)name_width=measured;
    if(name_width>0 && *name)item("name",left,name_width,name,0,0);
    left+=name_width>0 ? name_width+12 : 0;
    if(*timer && right-left>98)item("timer",left,right-left,timer,0,0);
    lumen_end();
}
int main(void) {
    int overlay=0;
    while(lumen_next()) {
        if(lumen_is("open")) {
            char role[24];lumen_string("role",role,sizeof(role));
            overlay=!strcmp(role,"overlay");lumen_language=lumen_number("language");
            if(!overlay) {
                lumen_begin();
                lumen_page(lumen_language==0 ? "Statusleiste" : lumen_language==2 ? "Barre d’état" : "Status Bar");
                lumen_component_button("settings",lumen_language==0 ? "Einstellungen" :
                    lumen_language==2 ? "Réglages" : "Settings",0);
                lumen_end();
            }
        } else if(overlay && lumen_is("system_state"))status();
        else if(!overlay && lumen_is("component"))lumen_open("lumen.settings");
    }
    return 0;
}
