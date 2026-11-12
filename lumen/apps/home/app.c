#include "lumen_app.h"

enum { ART_Y=48, ART_H=404, GRID_Y=80, TILE_H=126, FIRST_Y=GRID_Y+3*TILE_H,
       PAGE_Y=744, FIRST_COUNT=8, PAGE_COUNT=20, MAX_APPS=128 };
static char apps[MAX_APPS][64], selected[64];
static int count,page,settings_shortcut;

static const char *localized(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void date_draw(void) {
    static const char *days[][3]={
        {"Sonntag","Sunday","Dimanche"},{"Montag","Monday","Lundi"},{"Dienstag","Tuesday","Mardi"},
        {"Mittwoch","Wednesday","Mercredi"},{"Donnerstag","Thursday","Jeudi"},
        {"Freitag","Friday","Vendredi"},{"Samstag","Saturday","Samedi"}};
    static const char *months[][3]={
        {"Januar","January","janvier"},{"Februar","February","février"},{"März","March","mars"},
        {"April","April","avril"},{"Mai","May","mai"},{"Juni","June","juin"},
        {"Juli","July","juillet"},{"August","August","août"},{"September","September","septembre"},
        {"Oktober","October","octobre"},{"November","November","novembre"},
        {"Dezember","December","décembre"}};
    time_t now=time(NULL);
    struct tm day;
    localtime_r(&now,&day);
    char date[80], year[12];
    snprintf(year,sizeof(year),"%d",day.tm_year+1900);
    const char *month=months[day.tm_mon][lumen_language];
    if(lumen_language==1)snprintf(date,sizeof(date),"%s %d",month,day.tm_mday);
    else snprintf(date,sizeof(date),"%d%s %s",day.tm_mday,lumen_language==0 ? "." : "",month);
    lumen_asset(0,ART_Y,600,ART_H,"scene.pgm");
    ui_paint_text(108,111,384,40,days[day.tm_wday][lumen_language],PAINT_TITLE);
    ui_paint_text(96,150,408,56,date,PAINT_DISPLAY);
    ui_paint_line(235,224,266,224,1,118);
    ui_paint_line(333,224,364,224,1,118);
    ui_paint_text(267,207,66,36,year,PAINT_SECONDARY);
}
static int pages(void) {return 1+(count>FIRST_COUNT ? (count-FIRST_COUNT+PAGE_COUNT-1)/PAGE_COUNT : 0);}
static int start(void) {return page ? FIRST_COUNT+(page-1)*PAGE_COUNT : 0;}
static int top(void) {return page ? GRID_Y : FIRST_Y;}
static void set_page(int next) {
    if(next==page)return;
    page=next;
    app_state_save("page.bin",&page,sizeof(page));
}
static void draw(void) {
    if(!lumen_ready)return;
    if(!page){date_draw();lumen_schedule(60000);}
    int begin=start(),n=page ? PAGE_COUNT : FIRST_COUNT;
    for(int i=0;i<n && begin+i<count;i++)
        lumen_app_tile_action(20+i%4*140,top()+i/4*TILE_H,140,TILE_H,apps[begin+i]);
    if(settings_shortcut)lumen_icon_action("settings-shortcut",528,56,IC_SETTINGS);
    lumen_pager(PAGE_Y,page,pages());
}
static void read_catalog(void) {
    char data[MAX_APPS*65];
    lumen_string("data",data,sizeof(data));
    settings_shortcut=1;
    char *cursor=data;count=0;
    while(*cursor && count<MAX_APPS) {
        char *end=strchr(cursor,'\n');
        if(!end)end=cursor+strlen(cursor);
        size_t length=end-cursor;
        if(length && length<sizeof(apps[0])) {
            memcpy(apps[count],cursor,length);
            apps[count][length]=0;
            if(!strcmp(apps[count++],"lumen.settings"))settings_shortcut=0;
        }
        cursor=*end ? end+1 : end;
    }
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN || in->kind==INPUT_RESUME || in->kind==INPUT_REFRESH) {
        lumen_ready=count>0;
        lumen_service("catalog","list","",1);
        return 1;
    }
    if(in->kind==INPUT_SERVICE && in->x==1){if(in->y){read_catalog();if(page>=pages())set_page(pages()-1);lumen_ready=1;}return 1;}
    if(in->kind==INPUT_PAGE){if(in->x>=0 && in->x<pages())set_page(in->x);return 1;}
    if(in->kind==INPUT_SELECT) {
        if(in->x==2 && in->y==0)lumen_service("catalog","hide",selected,1);
        if(in->x==2 && in->y==1) {
            const char *choices[]={localized("Abbrechen","Cancel","Annuler"),
                                   localized("App endgültig entfernen","Remove app","Supprimer l’application")};
            lumen_select(choices,2,-1,3);
        }
        if(in->x==3 && in->y==1)lumen_service("catalog","remove",selected,1);
        return 1;
    }
    if(in->kind!=INPUT_COMPONENT)return 0;
    char id[64],gesture[24];
    if(!lumen_string("id",id,sizeof(id)) || !lumen_string("gesture",gesture,sizeof(gesture)))return 0;
    if(settings_shortcut && !strcmp(id,"settings-shortcut") && !strcmp(gesture,"clicked")) {
        lumen_open("lumen.settings");return 1;
    }
    int first=start(),last=first+(page ? PAGE_COUNT : FIRST_COUNT);
    int index=first;
    for(;index<count && index<last;index++)if(!strcmp(apps[index],id))break;
    if(index>=count || index>=last)return 0;
    if(!strcmp(gesture,"long_pressed")) {
        snprintf(selected,sizeof(selected),"%s",id);
        const char *choices[]={localized("App ausblenden","Hide app","Masquer l’application"),
                               localized("App löschen","Delete app","Supprimer l’application")};
        lumen_select(choices,2,-1,2);
    } else if(!strcmp(gesture,"clicked")) {
        if(!strcmp(id,"lumen.home"))set_page(0);
        else lumen_open(id);
    }
    return 1;
}
int main(void) {
    int saved;
    if(app_state_load("page.bin",&saved,sizeof(saved)) && saved>=0 && saved<=MAX_APPS/PAGE_COUNT+1)
        page=saved;
    lumen_run(draw,input,NULL);
    return 0;
}
