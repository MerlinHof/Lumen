#include "lumen_app.h"

static char city[256]="Rosenheim",requested_city[256];
static int ready,busy;
#include "forecast.h"

static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void request(void) {
    if(busy)return;
    busy=1;
    snprintf(requested_city,sizeof(requested_city),"%s",city);
    lumen_app_service("lumen.weather","refresh_forecast",city,1);
}
static int stale(void) {
    const char *root=getenv("LUMEN_APP_DATA");
    char path[2048];struct stat info;
    int n=root ? snprintf(path,sizeof(path),"%s/forecast-v1.bin",root) : -1;
    if(n<=0 || (size_t)n>=sizeof(path) || stat(path,&info)) {
        root=getenv("LUMEN_LEGACY_DATA");
        n=root ? snprintf(path,sizeof(path),"%s/weather.json",root) : -1;
        if(*forecast.city || n<=0 || (size_t)n>=sizeof(path) || stat(path,&info))return 1;
    }
    return time(NULL)-info.st_mtime>1800;
}
static void text(int x,int y,int width,const char *value,PaintText role) {
    lumen_text(x,y,width,40,value,role,-1,1);
}
static void draw(void) {
    ui_paint_heading(word("Wetter","Weather","Météo"),2);
    lumen_icon_action("search",468,74,IC_SEARCH);lumen_icon_action("refresh",528,74,IC_RESTART);
    text(30,128,540,*forecast.place ? forecast.place : city,PAINT_HEADING);
    if(!ready) {
        lumen_wrapped(30,210,540,130,busy ?
            word("Wetter wird geladen …","Loading forecast…","Chargement des prévisions…") :
            word("Ort suchen oder Vorhersage aktualisieren.","Search for a place or refresh the forecast.",
                 "Cherchez un lieu ou actualisez les prévisions."),PAINT_TITLE,4);
        return;
    }
    char line[120];
    lumen_weather_symbol(48,185,130,!forecast.is_day && forecast.code<=1 ? -1 : forecast.code);
    snprintf(line,sizeof(line),"%d °C",forecast.temp);
    text(200,194,338,line,PAINT_DISPLAY);
    snprintf(line,sizeof(line),"%s %d° · %d km/h",word("Gefühlt","Feels like","Ressenti"),forecast.feel,forecast.wind);
    text(200,265,338,line,PAINT_BODY);
    ui_paint_line(48,318,552,318,1,190);
    snprintf(line,sizeof(line),"%s %.5s · %s %.5s",word("Aufgang","Sunrise","Lever"),
             strlen(forecast.rise)>11 ? forecast.rise+11 : "--:--",word("Untergang","Sunset","Coucher"),
             strlen(forecast.set)>11 ? forecast.set+11 : "--:--");
    text(48,333,504,line,PAINT_BODY);
    snprintf(line,sizeof(line),"%s %d h %02d min",word("Tageslicht","Daylight","Lumière du jour"),forecast.light/3600,forecast.light/60%60);
    text(48,367,504,line,PAINT_SECONDARY);
    text(28,429,544,word("Vorhersage","Forecast","Prévisions"),PAINT_HEADING);
    for(int i=0;i<5;i++) {
        int x=28+i*112;const char *date=forecast.days[i].date;
        snprintf(line,sizeof(line),"%.2s.%.2s.",strlen(date)>8 ? date+8 : "--",strlen(date)>5 ? date+5 : "--");
        ui_paint_text(x,473,100,34,line,PAINT_SECONDARY);
        lumen_weather_symbol(x+12,505,80,forecast.days[i].code);
        snprintf(line,sizeof(line),"%d / %d°",forecast.days[i].high,forecast.days[i].low);
        ui_paint_text(x,591,100,36,line,PAINT_BODY);
        snprintf(line,sizeof(line),"%d%%",forecast.days[i].rain);
        ui_paint_text(x,623,100,32,line,PAINT_SECONDARY);
    }
    snprintf(line,sizeof(line),"Open-Meteo · %.16s%s",forecast.stamp,busy ? " …" : "");
    text(30,697,540,line,PAINT_SECONDARY);
}
static int input(const Input *in) {
    if(in->kind==INPUT_PAUSE){busy=0;return 1;}
    if(in->kind==INPUT_OPEN || in->kind==INPUT_RESUME || in->kind==INPUT_REFRESH) {
        ready=load_forecast();
        if(!ready || stale())request();
        return 1;
    }
    if(in->kind==INPUT_SERVICE && in->x==1) {
        busy=0;
        if(strcmp(requested_city,city)){request();return 1;}
        if(in->y)ready=load_forecast();
        else lumen_request("notify","",word("Wetter nicht geladen. Ort und WLAN prüfen.",
            "Forecast unavailable. Check the location and Wi-Fi.",
            "Prévisions indisponibles. Vérifiez le lieu et le Wi-Fi."),0);
        return 1;
    }
    if(in->kind==INPUT_TEXT && in->x==2) {
        char value[256];if(lumen_string("text",value,sizeof(value)) && *value) {
            snprintf(city,sizeof(city),"%s",value);
            lumen_storage_write("city.txt",city,strlen(city));
            ready=0;forecast=(Forecast){0};request();
        }
        return 1;
    }
    if(in->kind==INPUT_COMPONENT) {
        char id[24],gesture[24];
        if(lumen_string("id",id,sizeof(id)) && lumen_string("gesture",gesture,sizeof(gesture)) && !strcmp(gesture,"clicked")) {
            if(!strcmp(id,"refresh"))request();
            else if(!strcmp(id,"search"))
                lumen_request("input",word("Wetter: Ort","Weather location","Lieu"),city,2);
        }
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
    lumen_service_handler=forecast_operation;
    lumen_run(draw,input,NULL);
    return 0;
}
