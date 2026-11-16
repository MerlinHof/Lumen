#include "lumen_app.h"

static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void legacy_sleep_frame(void) {
    int mode=lumen_number("mode"),book=lumen_number("book");
    const char *background=mode==4 ? "album" : mode==3 ? "photo" :
        mode==5 && book ? "cover_fill" : mode==0 && book ? "cover" : "paper";
    const char *headline=mode==2 ? "" : word("Ein Moment der Ruhe.","A moment of quiet.","Un moment de calme.");
    const char *hint=lumen_number("hint") ?
        word("Mit der Power-Taste weiterlesen","Press the power button to continue","Appuyez sur le bouton d’alimentation pour continuer") : "";
    time_t now=time(NULL);
    struct tm local;localtime_r(&now,&local);
    char clock[16]="",date[24]="";
    int show_clock=mode==2 || lumen_number("time"),show_date=lumen_number("date");
    if(show_clock)strftime(clock,sizeof(clock),"%H:%M",&local);
    if(show_date)strftime(date,sizeof(date),"%d.%m.%Y",&local);
    fputs("{\"ui\":[],\"background\":",stdout);lumen_json(background);
    fputs(",\"headline\":",stdout);lumen_json(headline);
    fputs(",\"hint\":",stdout);lumen_json(hint);
    fputs(",\"clock\":",stdout);lumen_json(clock);
    fputs(",\"date\":",stdout);lumen_json(date);
    printf(",\"clock_y\":%d,\"date_y\":%d}\n",mode==2 ? 285 : 18,
           mode==2 ? 418 : show_clock ? 136 : 22);
    fflush(stdout);
}

static void image(const char *source,int x,int y,int w,int h) {
    lumen_cmd("device_image",x,y,w,h);
    fputs(",\"text\":",stdout);lumen_json(source);putchar('}');
}
static void sleep_frame(void) {
    int mode=lumen_number("mode"),book=lumen_number("book");
    int full_image=mode==4 || mode==3 || (mode==5 && book);
    const char *headline=word("Ein Moment der Ruhe.","A moment of quiet.","Un moment de calme.");
    const char *hint=word("Mit der Power-Taste weiterlesen","Press the power button to continue","Appuyez sur le bouton d’alimentation pour continuer");
    time_t now=time(NULL);
    struct tm local;
    localtime_r(&now,&local);
    char clock[16]="",date[24]="";
    int show_clock=mode==2 || lumen_number("time"),show_date=lumen_number("date");
    if(show_clock)strftime(clock,sizeof(clock),"%H:%M",&local);
    if(show_date)strftime(date,sizeof(date),"%d.%m.%Y",&local);
    lumen_begin();
    if(mode!=2 && !(mode==0 && book))
        lumen_display_text(28,300,544,80,headline,1,34,0);
    if(lumen_number("hint"))
        ui_paint_text(28,704,544,48,hint,PAINT_SECONDARY);
    if(full_image)
        image(mode==4 ? "wallpaper_album" : mode==3 ? "wallpaper" : "document_cover_fill",0,0,600,800);
    else if(mode==0 && book) {
        char title[384],author[320];
        lumen_string("title",title,sizeof(title));lumen_string("author",author,sizeof(author));
        image("document_cover",162,113,276,377);
        ui_paint_text(44,505,512,55,title,PAINT_TITLE);
        ui_paint_text(44,564,512,44,author,PAINT_SECONDARY);
    }
    if(*clock) {
        int y=mode==2 ? 285 : 18;
        for(int dy=-2;dy<=2;dy++)for(int dx=-2;dx<=2;dx++)
            if(dx*dx+dy*dy<=5 && (dx || dy))lumen_display_text(20+dx,y+dy,560,130,clock,1,108,255);
        lumen_display_text(20,y,560,130,clock,1,108,0);
    }
    if(*date) {
        int y=mode==2 ? 418 : show_clock ? 136 : 22;
        lumen_display_text(20,y-1,560,38,date,0,25,255);
        lumen_display_text(20,y+1,560,38,date,0,25,255);
        lumen_display_text(20,y,560,38,date,0,25,0);
    }
    lumen_end();
}
int main(void) {
    int overlay=0,surface_api=0;
    while(lumen_next()) {
        if(lumen_is("open")) {
            char role[16];lumen_string("role",role,sizeof(role));
            surface_api=lumen_number("surface_api");
            overlay=!strcmp(role,"overlay");
            lumen_language=lumen_number("language");
            if(!overlay) {
                lumen_begin();
                lumen_page(word("Sperrbildschirm","Lock Screen","Écran de veille"));
                lumen_component_button("settings",word("Einstellungen","Settings","Réglages"),0);
                lumen_end();
            }
        } else if(overlay && lumen_is("sleep_state")) {
            lumen_language=lumen_number("language");
            if(surface_api>=2)sleep_frame();
            else legacy_sleep_frame();
        } else if(!overlay && lumen_is("component"))lumen_open("lumen.settings");
    }
    return 0;
}
