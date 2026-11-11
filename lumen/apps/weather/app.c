#include "lumen_app.h"

static char city[256]="Rosenheim",place[256],stamp[64],rise[64],set[64];
static int ready,busy,temp,feel,wind,code,is_day,light;
static struct {char date[64];int code,high,low,rain;} days[5];

static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void request(const char *operation){lumen_service("weather",operation,city,1);}
static void text(int x,int y,int width,const char *value,PaintText role) {
    lumen_text(x,y,width,40,value,role,-1,1);
}
static void draw(void) {
    ui_paint_heading(word("Wetter","Weather","Météo"),2);
    ui_paint_icon_button(468,74,IC_SEARCH);ui_paint_icon_button(528,74,IC_RESTART);
    text(30,128,540,*place ? place : city,PAINT_HEADING);
    if(!ready) {
        lumen_wrapped(30,210,540,130,busy ?
            word("Wetter wird geladen …","Loading forecast…","Chargement des prévisions…") :
            word("Ort suchen oder Vorhersage aktualisieren.","Search for a place or refresh the forecast.",
                 "Cherchez un lieu ou actualisez les prévisions."),PAINT_TITLE,4);
        return;
    }
    char line[120];
    lumen_weather_symbol(48,185,130,!is_day && code<=1 ? -1 : code);
    snprintf(line,sizeof(line),"%d °C",temp);
    text(200,194,338,line,PAINT_DISPLAY);
    snprintf(line,sizeof(line),"%s %d° · %d km/h",word("Gefühlt","Feels like","Ressenti"),feel,wind);
    text(200,265,338,line,PAINT_BODY);
    ui_paint_line(48,318,552,318,1,190);
    snprintf(line,sizeof(line),"%s %.5s · %s %.5s",word("Aufgang","Sunrise","Lever"),
             strlen(rise)>11 ? rise+11 : "--:--",word("Untergang","Sunset","Coucher"),
             strlen(set)>11 ? set+11 : "--:--");
    text(48,333,504,line,PAINT_BODY);
    snprintf(line,sizeof(line),"%s %d h %02d min",word("Tageslicht","Daylight","Lumière du jour"),light/3600,light/60%60);
    text(48,367,504,line,PAINT_SECONDARY);
    text(28,429,544,word("Vorhersage","Forecast","Prévisions"),PAINT_HEADING);
    for(int i=0;i<5;i++) {
        int x=28+i*112;const char *date=days[i].date;
        snprintf(line,sizeof(line),"%.2s.%.2s.",strlen(date)>8 ? date+8 : "--",strlen(date)>5 ? date+5 : "--");
        ui_paint_text(x,473,100,34,line,PAINT_SECONDARY);
        lumen_weather_symbol(x+12,505,80,days[i].code);
        snprintf(line,sizeof(line),"%d / %d°",days[i].high,days[i].low);
        ui_paint_text(x,591,100,36,line,PAINT_BODY);
        snprintf(line,sizeof(line),"%d%%",days[i].rain);
        ui_paint_text(x,623,100,32,line,PAINT_SECONDARY);
    }
    snprintf(line,sizeof(line),"Open-Meteo · %.16s%s",stamp,busy ? " …" : "");
    text(30,697,540,line,PAINT_SECONDARY);
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN){request("open");return 1;}
    if(in->kind==INPUT_RESUME || in->kind==INPUT_REFRESH){request("status");return 1;}
    if(in->kind==INPUT_SERVICE && in->x==1 && in->y) {
        ready=lumen_number("forecast");busy=lumen_number("busy");
        temp=lumen_number("temp");feel=lumen_number("feel");wind=lumen_number("wind");code=lumen_number("code");
        light=lumen_number("light");is_day=lumen_number("daylight_now");
        lumen_string("place",place,sizeof(place));lumen_string("stamp",stamp,sizeof(stamp));
        lumen_string("rise0",rise,sizeof(rise));lumen_string("set0",set,sizeof(set));
        for(int i=0;i<5;i++) {
            char field[16];
            snprintf(field,sizeof(field),"date%d",i);lumen_string(field,days[i].date,sizeof(days[i].date));
            snprintf(field,sizeof(field),"code%d",i);days[i].code=lumen_number(field);
            snprintf(field,sizeof(field),"high%d",i);days[i].high=lumen_number(field);
            snprintf(field,sizeof(field),"low%d",i);days[i].low=lumen_number(field);
            snprintf(field,sizeof(field),"rain%d",i);days[i].rain=lumen_number(field);
        }
        return 1;
    }
    if(in->kind==INPUT_TEXT && in->x==2) {
        char value[256];if(lumen_string("text",value,sizeof(value)) && *value) {
            snprintf(city,sizeof(city),"%s",value);lumen_storage_write("city.txt",city,strlen(city));request("refresh");
        }
        return 1;
    }
    if(in->kind==INPUT_TAP && in->y>=74 && in->y<118) {
        if(in->x>=528 && in->x<572)request("refresh");
        else if(in->x>=468 && in->x<512)lumen_request("input",word("Wetter: Ort","Weather location","Lieu"),city,2);
    }
    return 1;
}
int main(void) {
    char saved[256];size_t n=0;
    if((lumen_storage_read("city.txt",saved,sizeof(saved)-1,&n) ||
        lumen_legacy_read("weather-city.txt",saved,sizeof(saved)-1,&n)) && n) {
        while(n && (saved[n-1]=='\r' || saved[n-1]=='\n'))n--;
        if(n){saved[n]=0;snprintf(city,sizeof(city),"%s",saved);}
    }
    lumen_run(draw,input,NULL);
    return 0;
}
