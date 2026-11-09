#include "lumen_app.h"
#include <errno.h>
#include <poll.h>

static char message[2048];
static int64_t deadline;
static int generation;
static const char *localized(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void banner(void) {
    lumen_first=1;
    if(*message) {
        printf("{\"generation\":%d,\"capture\":[19,57,558,91],\"ui\":[",generation);
        lumen_cmd("raised",19,57,558,91);putchar('}');
        lumen_cmd("wrap",37,76,470,65);
        fputs(",\"text\":",stdout);lumen_json(message);fputs(",\"lines\":2}",stdout);
        ui_paint_icon_button(525,75,IC_CLOSE);
    } else lumen_begin();
    lumen_end();
}
static void foreground(void) {
    lumen_begin();
    ui_paint_heading(localized("Mitteilungen","Notifications","Notifications"),0);
    lumen_text(28,153,544,48,localized(
        "Mitteilungen erscheinen kurz über der offenen App.",
        "Notifications appear briefly above the open app.",
        "Les notifications apparaissent brièvement sur l’application."),
        PAINT_BODY,0,1);
    lumen_button(28,238,544,52,localized("Vorschau anzeigen","Show a preview","Afficher un aperçu"),0);
    lumen_end();
}
int main(void) {
    setvbuf(stdin,NULL,_IONBF,0);
    int overlay=0;
    for(;;) {
        int timeout=-1;
        if(overlay && *message) {
            int64_t remaining=deadline-ui_clock_millis();
            if(remaining<=0){*message=0;banner();}
            else timeout=(int)remaining;
        }
        struct pollfd input={.fd=STDIN_FILENO,.events=POLLIN};
        int result=poll(&input,1,timeout);
        if(result<0 && errno==EINTR)continue;
        if(!result)continue;
        if(result<0 || !lumen_next())break;
        if(lumen_is("open")) {
            char role[24];lumen_string("role",role,sizeof(role));
            overlay=!strcmp(role,"overlay");
            lumen_language=lumen_number("language");
            if(!overlay)foreground();
        } else if(overlay && lumen_is("notify")) {
            generation=lumen_number("generation");
            lumen_string("text",message,sizeof(message));
            int ms=lumen_number("duration");
            if(ms<1 || ms>60000)ms=6000;
            deadline=ui_clock_millis()+ms;
            banner();
        } else if(overlay && (lumen_is("dismiss") ||
                  (lumen_is("input") && lumen_number("kind")==INPUT_TAP &&
                   lumen_number("x")>=515 && lumen_number("x")<577 &&
                   lumen_number("y")>=57 && lumen_number("y")<148))) {
            *message=0;banner();
        } else if(!overlay && lumen_is("input") && lumen_number("kind")==INPUT_TAP &&
                  lumen_number("y")>=238 && lumen_number("y")<290) {
            lumen_request("notify","",localized("Eine Mitteilung von LUMEN.","A notification from LUMEN.","Une notification de LUMEN."),0);
        }
    }
    return 0;
}
