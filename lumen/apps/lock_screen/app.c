#include "lumen_app.h"

static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void sleep_frame(void) {
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
int main(void) {
    int overlay=0;
    while(lumen_next()) {
        if(lumen_is("open")) {
            char role[16];lumen_string("role",role,sizeof(role));
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
            sleep_frame();
        } else if(!overlay && lumen_is("component"))lumen_open("lumen.settings");
    }
    return 0;
}
