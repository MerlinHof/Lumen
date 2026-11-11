#include "lumen_app.h"

static int active,paired,port,timeout,settings;
static char address[128],code[24];
static const char *t(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void draw(void) {
    if(settings) {
        lumen_page(t("Dateiübertragung","File Transfer","Transfert de fichiers"));
        char number[16];
        snprintf(number,sizeof(number),"%d",port);
        lumen_textfield("port",t("Server-Port","Server port","Port du serveur"),number);
        snprintf(number,sizeof(number),"%d",timeout);
        lumen_textfield("timeout",t("Inaktivität (Minuten)","Idle timeout (minutes)","Inactivité (minutes)"),number);
        return;
    }
    ui_paint_heading(t("Dateiübertragung","File Transfer","Transfert de fichiers"),1);
    ui_paint_icon_button(528,74,IC_SETTINGS);
    lumen_text(30,145,430,48,t("Transfer-Server","Transfer server","Serveur de transfert"),PAINT_HEADING,0,1);
    lumen_toggle(492,150,active);
    ui_paint_line(28,207,572,207,1,189);
    if(!active) {
        lumen_wrapped(30,244,542,204,t(
            "Bücher und Fotos übertragen, Notizen exportieren und eigene Apps installieren. Aktiviere den Server und öffne den QR-Code im selben WLAN.",
            "Transfer books and photos, export notes and install apps. Start the server and scan the QR code on the same Wi-Fi network.",
            "Transférez des livres et des photos, exportez vos notes et installez des apps. Démarrez le serveur et scannez le code QR sur le même réseau Wi-Fi."),PAINT_TITLE,5);
        lumen_wrapped(30,484,542,124,t(
            "Der Server pausiert im Ruhemodus. Beim nächsten Start wird die aktuelle WLAN-Adresse angezeigt.",
            "The server stops when the Kindle sleeps. Starting it again shows the current Wi-Fi address.",
            "Le serveur s’arrête pendant la veille. Redémarrez-le pour afficher l’adresse Wi-Fi actuelle."),PAINT_BODY,4);
        return;
    }
    lumen_schedule(2200);
    lumen_text(30,216,542,40,paired ?
        t("Verbunden · Zugriff bestätigt","Connected · access granted","Connecté · accès autorisé") :
        t("Bereit · im selben WLAN öffnen","Ready · open on the same Wi-Fi","Prêt · ouvrir sur le même Wi-Fi"),PAINT_BODY,0,1);
    lumen_text(30,255,542,31,t("ADRESSE","ADDRESS","ADRESSE"),PAINT_SECONDARY,100,1);
    lumen_wrapped(30,290,542,64,*address ? address : t("Keine WLAN-Adresse","No Wi-Fi address","Aucune adresse Wi-Fi"),PAINT_HEADING,2);
    if(*address) {
        char url[160];
        snprintf(url,sizeof(url),"%s/#%s",address,code);
        lumen_qr(180,355,240,url);
    }
    ui_paint_text(28,596,544,35,t("Verbindungscode","Pairing code","Code de connexion"),PAINT_SECONDARY);
    ui_paint_text(28,630,544,62,code,PAINT_DISPLAY);
    char label[120];
    snprintf(label,sizeof(label),t("Automatisches Ende nach %d min ohne Zugriff",
        "Stops after %d minutes of inactivity","Arrêt après %d minutes d’inactivité"),timeout);
    ui_paint_text(28,696,544,30,label,PAINT_SECONDARY);
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN || in->kind==INPUT_RESUME || in->kind==INPUT_TICK) {
        lumen_service("transfer","status","",1);return 1;
    }
    if(in->kind==INPUT_SERVICE && in->x==1) {
        active=lumen_number("running");paired=lumen_number("paired");
        port=lumen_number("port");timeout=lumen_number("timeout");
        lumen_string("address",address,sizeof(address));lumen_string("code",code,sizeof(code));
        if(!in->y) {
            char error[256];
            if(lumen_string("error",error,sizeof(error)) && *error)lumen_request("notify","",error,0);
        }
        return 1;
    }
    if(in->kind==INPUT_BACK){settings=lumen_back=0;lumen_service("transfer","status","",1);return 1;}
    if(in->kind==INPUT_COMPONENT && in->x && settings) {
        char id[32],text[32];
        if(lumen_string("id",id,sizeof(id)) && lumen_string("text",text,sizeof(text)) &&
           (!strcmp(id,"port") || !strcmp(id,"timeout")))
            lumen_service("transfer",id,text,1);
        return 1;
    }
    if(in->kind!=INPUT_TAP || settings)return 1;
    if(in->x>=516 && in->y>=72 && in->y<124)settings=lumen_back=1;
    else if(in->x>=478 && in->y>=142 && in->y<198)
        lumen_service("transfer",active ? "stop" : "start","",1);
    return 1;
}
int main(void){lumen_run(draw,input,NULL);return 0;}
