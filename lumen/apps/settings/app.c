#include "lumen_app.h"

enum {HOME,WIFI,APPEARANCE,TIME,APPS,SYSTEM};
static int screen,tab,page,count,prefs[15],sleep_minutes=10,autostart,release_state,enabled,busy;
static int remove_row=-1,reset;
static char version[24],new_version[24],checked[48],error[160],network_name[129],total[40],sizes[5][40];
static int ratios[5];
static struct {char id[64],name[129],version[24];int saved,locked,connected,visible,remove;} rows[8];
static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static const char *name(int target) {
    const char *names[][3]={{"Einstellungen","Settings","Réglages"},{"WLAN","Wi-Fi","Wi-Fi"},
        {"Darstellung","Appearance","Apparence"},{"Datum & Uhrzeit","Date & time","Date et heure"},
        {"Apps verwalten","Manage apps","Gérer les apps"},{"System","System","Système"}};
    return word(names[target][0],names[target][1],names[target][2]);
}
static void request(const char *op,const char *data,int action){lumen_service("settings",op,data,action);}
static void load(void) {
    char number[16];snprintf(number,sizeof(number),"%d",page);
    if(screen==WIFI)request("wifi_list",number,2);
    else if(screen==APPS)request("apps",number,3);
    else if(screen==SYSTEM && tab==1)request("storage","",4);
    else request("state","",1);
}
static void show(int next) {
    screen=next;tab=page=count=0;remove_row=-1;reset=0;lumen_back=screen!=HOME;
    if(screen==WIFI)request("wifi_start","",5);
    else load();
}
static void preference(int key,int value) {
    char data[32];snprintf(data,sizeof(data),"%d %d",key,value);request("preference",data,5);
}
static const char *label(int i) {
    const char *labels[][3]={
        {"Sprache","Language","Langue"},{"Tastatursprache","Keyboard language","Langue du clavier"},
        {"Invertierte Darstellung","Invert display","Inverser l’affichage"},{"Abgerundete Ecken","Rounded corners","Coins arrondis"},
        {"Datum anzeigen","Show date","Afficher la date"},{"Uhrzeit anzeigen","Show time","Afficher l’heure"},
        {"WLAN-Status anzeigen","Show Wi-Fi status","Afficher le Wi-Fi"},{"Akkusymbol","Battery icon","Icône de batterie"},
        {"Akku-Prozentzahl","Battery percentage","Pourcentage de batterie"},{"Sperrbild","Lock screen","Écran de veille"},
        {"Uhrzeit auf Sperrbild","Time on lock screen","Heure en veille"},{"Datum auf Sperrbild","Date on lock screen","Date en veille"},
        {"Sperrbild-Hinweis","Lock screen hint","Indication en veille"},{"Automatisch über WLAN","Automatic over Wi-Fi","Automatique par Wi-Fi"},
        {"Zeitzone","Time zone","Fuseau horaire"}};
    return word(labels[i][0],labels[i][1],labels[i][2]);
}
static const char **values(int id,int *count,int *selected) {
    static const char *languages[]={"Deutsch","English","Français"};
    static const char *options[6];
    *selected=prefs[id];
    if(id<2){*count=3;return languages;}
    if(id==14) {
        static const char *zones[]={"Europe/Berlin","UTC","Europe/London","America/New_York","Asia/Tokyo"};
        *count=5;return zones;
    }
    static const int ids[]={0,5,2,1,4,3};
    options[0]=word("Buchcover","Book cover","Couverture");
    options[1]=word("Buchcover · Vollbild","Full-screen cover","Couverture plein écran");
    options[2]=word("Uhrzeit","Clock","Horloge");
    options[3]=word("Ruhiger Text","Quiet text","Texte calme");
    options[4]=word("Fotos · stündlich","Hourly photos","Photos chaque heure");
    options[5]=word("Foto aus Fotos","Selected photo","Photo sélectionnée");
    *count=prefs[id]==3 ? 6 : 5;
    *selected=0;for(int i=0;i<6;i++)if(ids[i]==prefs[id])*selected=i;
    return options;
}
static void toggle_row(const char *id,int y,const char *title,int selected) {
    lumen_text(30,y,453,48,title,PAINT_BODY,0,1);
    lumen_toggle_action(id,492,y+4,selected);ui_paint_line(28,y+55,572,y+55,1,190);
}
static void draw_wifi(void) {
    toggle_row("wifi-power",137,name(WIFI),enabled);
    if(!enabled) {
        lumen_wrapped(28,257,544,100,word("WLAN ist ausgeschaltet.","Wi-Fi is off.","Le Wi-Fi est désactivé."),PAINT_TITLE,3);
        return;
    }
    for(int i=0;i<6 && page*6+i<count;i++) {
        int y=222+i*74;
        if(rows[i].connected || (rows[i].locked && !rows[i].saved))
            ui_paint_icon_button(29,y+14,rows[i].connected ? IC_DONE : IC_LOCK);
        lumen_text(79,y+5,431,34,rows[i].name,PAINT_BODY,0,1);
        lumen_text(79,y+38,430,25,rows[i].connected ? word("Verbunden","Connected","Connecté") :
            rows[i].saved ? word("Gespeichert","Saved","Enregistré") :
            rows[i].locked ? word("Passwort geschützt","Password protected","Protégé") :
            word("Offenes Netz","Open network","Réseau ouvert"),PAINT_SECONDARY,-1,1);
        char id[24];
        snprintf(id,sizeof(id),"network-%d",i);
        lumen_custom_action(id,28,y,482,73);
        if(rows[i].saved) {
            snprintf(id,sizeof(id),"forget-%d",i);
            lumen_icon_action(id,522,y+15,IC_TRASH);
        }
        ui_paint_line(28,y+73,572,y+73,1,190);
    }
    if(!count)lumen_wrapped(28,257,544,100,busy ? word("Suche läuft …","Scanning…","Recherche…") :
        word("Keine Netzwerke gefunden.","No networks found.","Aucun réseau trouvé."),PAINT_TITLE,3);
    lumen_pager(745,page,(count+5)/6);lumen_icon_action("wifi-scan",528,745,IC_RESTART);
    if(busy)lumen_schedule(1000);
}
static void draw_appearance(void) {
    const char *tabs[]={word("Allgemein","General","Général"),word("Statusleiste","Status bar","Barre d’état"),
        word("Sperrbild","Lock screen","Veille")};
    lumen_segments("tab",28,131,544,46,tabs,3,tab);
    const int first[]={0,4,9},last[]={4,9,13};
    for(int i=first[tab];i<last[tab];i++) {
        int y=203+(i-first[tab])*68;
        char id[24];snprintf(id,sizeof(id),"pref-%d",i);
        if(i<2 || i==9) {
            int n,selected;const char **options=values(i,&n,&selected);
            lumen_select_action(id,y,label(i),options[selected<n ? selected : 0]);
        } else toggle_row(id,y,label(i),prefs[i]);
    }
    if(tab==2) {
        char time[18];
        if(sleep_minutes)snprintf(time,sizeof(time),"%d min",sleep_minutes);
        else snprintf(time,sizeof(time),"%s",word("Nie","Never","Jamais"));
        lumen_select_action("sleep",503,word("Automatisch sperren","Auto sleep","Veille automatique"),time);
    }
}
static void draw_time(void) {
    char value[40];time_t now=time(NULL);struct tm t;localtime_r(&now,&t);
    strftime(value,sizeof(value),"%d.%m.%Y  %H:%M",&t);
    ui_paint_surface(28,140,544,108,16,255,104);
    ui_paint_text(28,140,544,108,value,PAINT_TITLE);
    toggle_row("pref-13",290,label(13),prefs[13]);
    if(prefs[13])lumen_wrapped(30,385,540,104,word("Datum und Uhrzeit werden täglich über WLAN abgeglichen.",
        "Date and time synchronize over Wi-Fi daily.","La date et l’heure sont synchronisées quotidiennement en Wi-Fi."),PAINT_BODY,3);
    else {
        lumen_button_action("time",28,387,544,54,word("Datum und Uhrzeit setzen","Set date and time","Régler la date et l’heure"),0);
        int n,selected;const char **options=values(14,&n,&selected);
        lumen_select_action("pref-14",480,label(14),options[selected<n ? selected : 0]);
    }
}
static void draw_apps(void) {
    for(int i=0;i<8 && page*8+i<count;i++) {
        int y=174+i*68;
        lumen_app_icon(30,y+9,44,rows[i].id);
        lumen_text(85,y+4,240,34,rows[i].name,PAINT_BODY,0,1);
        lumen_text(85,y+36,240,24,rows[i].version,PAINT_SECONDARY,-1,1);
        char id[24];
        if(page*8+i>0) {
            snprintf(id,sizeof(id),"up-%d",i);
            lumen_icon_action(id,329,y+10,IC_UP);
        }
        if(page*8+i+1<count) {
            snprintf(id,sizeof(id),"down-%d",i);
            lumen_icon_action(id,375,y+10,IC_DOWN);
        }
        snprintf(id,sizeof(id),"visible-%d",i);
        lumen_toggle_action(id,427,y+14,rows[i].visible);
        if(rows[i].remove) {
            snprintf(id,sizeof(id),"remove-%d",i);
            lumen_icon_action(id,526,y+10,IC_TRASH);
        }
        else ui_paint_icon_button(526,y+10,IC_LOCK);
        ui_paint_line(28,y+67,572,y+67,1,190);
    }
    lumen_pager(745,page,(count+7)/8);
}
static const char *management(int i) {
    const char *labels[][3]={{"Konfiguration exportieren","Export configuration","Exporter la configuration"},
        {"Konfiguration importieren","Import configuration","Importer la configuration"},
        {"Amazon öffnen","Open Amazon","Ouvrir Amazon"},
        {"Einstellungen zurücksetzen","Reset settings","Réinitialiser les réglages"}};
    return word(labels[i][0],labels[i][1],labels[i][2]);
}
static void draw_system(void) {
    const char *tabs[]={word("Update","Update","Mise à jour"),word("Speicher","Storage","Stockage"),
        word("Verwaltung","Management","Gestion")};
    lumen_segments("tab",28,133,544,44,tabs,3,tab);
    if(!tab) {
        char value[48];snprintf(value,sizeof(value),"LUMEN %s",version);
        lumen_text(30,215,540,48,value,PAINT_TITLE,0,1);
        lumen_wrapped(30,276,540,126,word(
            "Diese Software wurde Ende 2026 von GPT-6.1 Sol entwickelt unter Aufsicht und Prompting und Lizenz von Merlin Hof.",
            "Developed in late 2026 by GPT-6.1 Sol under Merlin Hof’s supervision, prompting and license.",
            "Développé fin 2026 par GPT-6.1 Sol, sous la supervision, les instructions et la licence de Merlin Hof."),PAINT_BODY,4);
        if(*checked) {
            lumen_text(30,416,540,30,word("Zuletzt geprüft","Last checked","Dernière vérification"),PAINT_SECONDARY,-1,1);
            lumen_text(30,450,540,38,checked,PAINT_BODY,0,1);
        }
        lumen_text(30,508,540,40,release_state==1 ? word("Wird geprüft …","Checking…","Vérification…") :
            release_state==2 ? word("LUMEN ist aktuell.","LUMEN is up to date.","LUMEN est à jour.") :
            release_state>=3 ? word("Neue Version verfügbar","Update available","Mise à jour disponible") :
            "GitHub",PAINT_BODY,0,1);
        if(release_state>=3) {
            lumen_text(30,553,540,36,new_version,PAINT_SECONDARY,-1,1);
            lumen_button_action("install",28,598,544,54,word("Update installieren","Install update","Installer la mise à jour"),1);
        } else if(release_state<0)lumen_wrapped(30,553,540,100,error,PAINT_SECONDARY,3);
        lumen_button_action("check",28,672,544,54,word("Nach Updates suchen","Check for updates","Rechercher une mise à jour"),0);
        if(release_state==1)lumen_schedule(750);
    } else if(tab==1) {
        lumen_text(30,270,540,48,total,PAINT_TITLE,0,1);
        ui_paint_surface(28,370,544,28,10,255,110);
        int x=30;const int colors[]={0,72,120,175,255};
        const char *labels[]={word("Bücher","Books","Livres"),"LUMEN",
            word("Firmware","Firmware","Micrologiciel"),word("Andere","Other","Autres"),word("Frei","Free","Libre")};
        for(int i=0;i<5;i++) {
            int w=540*ratios[i]/1000,xx=30+i%2*282,y=425+i/2*67;
            if(w>2 && x+w<=571)ui_paint_surface(x,372,w-2,24,0,colors[i],-1);
            x+=w;
            ui_paint_surface(xx,y+7,15,15,3,colors[i],105);
            lumen_text(xx+27,y,230,28,labels[i],PAINT_BODY,0,1);
            lumen_text(xx+27,y+29,230,25,sizes[i],PAINT_SECONDARY,-1,1);
        }
    } else {
        toggle_row("autostart",216,word("LUMEN nach dem Booten","Start LUMEN at boot","Lancer LUMEN au démarrage"),autostart);
        const char *actions[]={"export","import","amazon","reset"};
        for(int i=0;i<4;i++)lumen_button_action(actions[i],28,304+i*84,544,60,management(i),0);
    }
}
static void draw_content(void) {
    ui_paint_heading(name(screen),0);
    if(screen==HOME) {
        const char *hints[]={word("Netzwerke und Verbindung","Networks and connection","Réseaux et connexion"),
            word("Sprache, Sperrbild und Statusleiste","Language, lock screen and status bar","Langue, veille et barre d’état"),
            word("Zeit, Zeitzone und WLAN-Abgleich","Time, time zone and synchronization","Heure, fuseau et synchronisation"),
            word("Sichtbarkeit & Reihenfolge","Visibility and order","Visibilité et ordre"),
            word("Speicher, Version und Konfiguration","Storage, updates and configuration","Stockage, mises à jour et configuration")};
        const char *sections[]={"wifi","appearance","date-time","manage","system"};
        for(int i=1;i<=5;i++)lumen_list_action(sections[i-1],134+(i-1)*72,72,name(i),hints[i-1],IC_NEXT);
    } else if(screen==WIFI)draw_wifi();
    else if(screen==APPEARANCE)draw_appearance();
    else if(screen==TIME)draw_time();
    else if(screen==APPS)draw_apps();
    else draw_system();
}
static void draw(void) {
    if(reset || remove_row>=0) {
        ui_paint_heading(name(screen),0);
        ui_paint_surface(24,275,552,220,16,255,105);
        ui_paint_text(35,294,530,88,reset ? management(3) :
            word("App endgültig entfernen?","Remove this app?","Supprimer cette application ?"),PAINT_HEADING);
        lumen_button_action("cancel",38,412,250,52,word("Abbrechen","Cancel","Annuler"),0);
        lumen_button_action("confirm",312,412,250,52,word("Bestätigen","Confirm","Confirmer"),1);
    } else draw_content();
}
static void app_change(const char *verb,int i) {
    char data[96];snprintf(data,sizeof(data),"%s %s",verb,rows[i].id);request("app_change",data,5);
}
static void read_rows(void) {
    for(int i=0;i<8;i++) {
        char key[24];
#define GET_TEXT(field) snprintf(key,sizeof(key),#field "%d",i);lumen_string(key,rows[i].field,sizeof(rows[i].field))
#define GET_INT(field) snprintf(key,sizeof(key),#field "%d",i);rows[i].field=lumen_number(key)
        GET_TEXT(id);GET_TEXT(name);GET_TEXT(version);
        GET_INT(saved);GET_INT(locked);GET_INT(connected);GET_INT(visible);GET_INT(remove);
#undef GET_TEXT
#undef GET_INT
    }
}
static int row_action(const char *id,const char *prefix,int count) {
    size_t length=strlen(prefix);
    if(strncmp(id,prefix,length) || !id[length] || id[length+1] ||
       id[length]<'0' || id[length]>='0'+count)return -1;
    return id[length]-'0';
}
static int component_input(void) {
    static const int durations[]={5,10,15,30,60,0};
    char id[32],gesture[24],value[8]="";
    if(!wire_bounded_string("id",id,sizeof(id)) ||
       !wire_bounded_string("gesture",gesture,sizeof(gesture)))return 0;
    int clicked=!strcmp(gesture,"clicked");
    lumen_string("text",value,sizeof(value));
    int changed=!strcmp(gesture,"changed") && (value[0]=='0' || value[0]=='1') && !value[1];
    if(reset || remove_row>=0) {
        if(clicked && !strcmp(id,"confirm")) {
            if(reset)request("reset","",5);
            else app_change("remove",remove_row);
        } else if(!clicked || strcmp(id,"cancel"))return 0;
        reset=0;remove_row=-1;
        return 1;
    }
    if(screen==HOME && clicked) {
        const char *sections[]={"wifi","appearance","date-time","manage","system"};
        for(int i=0;i<5;i++)if(!strcmp(id,sections[i])){show(i+1);return 1;}
    }
    if((screen==APPEARANCE || screen==SYSTEM) && !strcmp(id,"tab") &&
       !strcmp(gesture,"selected") && value[0]>='0' && value[0]<='2' && !value[1]) {
        tab=value[0]-'0';
        if(screen==SYSTEM)load();
        return 1;
    }
    if(screen==WIFI) {
        int row=row_action(id,"network-",6),forget=row_action(id,"forget-",6);
        if(changed && !strcmp(id,"wifi-power"))request("wifi_power",value,5);
        else if(clicked && enabled && !strcmp(id,"wifi-scan"))request("wifi_scan","",5);
        else if(clicked && enabled && forget>=0 && rows[forget].saved)
            request("wifi_forget",rows[forget].name,5);
        else if(clicked && enabled && row>=0 && *rows[row].name) {
            if(rows[row].locked && !rows[row].saved) {
                snprintf(network_name,sizeof(network_name),"%s",rows[row].name);
                lumen_request("secret",network_name,"",1);
            } else request("wifi_connect",rows[row].name,5);
        }
    } else if(screen==APPEARANCE || screen==TIME) {
        if(clicked && screen==APPEARANCE && tab==2 && !strcmp(id,"sleep")) {
            const char *options[]={"5 min","10 min","15 min","30 min","60 min",word("Nie","Never","Jamais")};
            int selected=1;
            for(int i=0;i<6;i++)if(durations[i]==sleep_minutes)selected=i;
            lumen_select(options,6,selected,30);
        } else if(clicked && screen==TIME && !prefs[13] && !strcmp(id,"time")) {
            time_t now=time(NULL);struct tm t;localtime_r(&now,&t);
            char text[32];strftime(text,sizeof(text),"%Y-%m-%d %H:%M",&t);
            lumen_request("input","YYYY-MM-DD HH:MM",text,2);
        } else if(!strncmp(id,"pref-",5)) {
            char *end;
            long key=strtol(id+5,&end,10);
            const int first[]={0,4,9},last[]={4,9,13};
            if(end==id+5 || *end || (screen==APPEARANCE && (key<first[tab] || key>=last[tab])) ||
               (screen==TIME && key!=13 && (key!=14 || prefs[13])))return 0;
            if(key<2 || key==9 || key==14) {
                if(clicked) {
                    int n,selected;
                    const char **options=values((int)key,&n,&selected);
                    lumen_select(options,n,selected,10+(int)key);
                }
            } else if(changed)preference((int)key,value[0]-'0');
        }
    } else if(screen==APPS) {
        int row=row_action(id,"remove-",8);
        if(clicked && row>=0 && rows[row].remove)remove_row=row;
        const char *verbs[]={"up","down","visible"},*prefixes[]={"up-","down-","visible-"};
        for(int i=0;i<3;i++) {
            row=row_action(id,prefixes[i],8);
            if(row>=0 && *rows[row].id && (i==2 ? changed : clicked)) {
                app_change(verbs[i],row);
                break;
            }
        }
    } else if(screen==SYSTEM) {
        if(!tab && clicked && !strcmp(id,"check") && release_state!=1)request("check","",5);
        else if(!tab && clicked && !strcmp(id,"install") && release_state>=3)request("install","",5);
        else if(tab==2 && changed && !strcmp(id,"autostart"))request("autostart",value,5);
        else if(tab==2 && clicked) {
            if(!strcmp(id,"reset"))reset=1;
            else if(!strcmp(id,"export") || !strcmp(id,"import") || !strcmp(id,"amazon"))request(id,"",5);
        }
    }
    return 1;
}
static int input(const Input *in) {
    static const int lock_ids[]={0,5,2,1,4,3},durations[]={5,10,15,30,60,0};
    if(in->kind==INPUT_OPEN || in->kind==INPUT_REFRESH || in->kind==INPUT_RESUME || in->kind==INPUT_TICK){load();return 1;}
    if(in->kind==INPUT_INTENT) {
        char op[32];lumen_string("operation",op,sizeof(op));
        if(!strcmp(op,"settings-section")) {
            int dest=lumen_number("section");
            if(dest>=HOME && dest<=SYSTEM)show(dest);
        }
        return 1;
    }
    if(in->kind==INPUT_BACK) {
        if(reset || remove_row>=0){reset=0;remove_row=-1;}
        else if(screen!=HOME)show(HOME);
        return 1;
    }
    if(in->kind==INPUT_PAGE){page=in->x;load();return 1;}
    if(in->kind==INPUT_COMPONENT)return component_input();
    if(in->kind==INPUT_SELECT && in->y>=0) {
        if(in->x==30 && in->y<6){char value[16];snprintf(value,sizeof(value),"%d",durations[in->y]);request("sleep",value,5);}
        else if(in->x>=10 && in->x<25) {
            int id=in->x-10,n,selected;
            if(values(id,&n,&selected) && in->y<n)preference(id,id==9 ? lock_ids[in->y] : in->y);
        }
        return 1;
    }
    if(in->kind==INPUT_TEXT) {
        char value[512];if(!lumen_string("text",value,sizeof(value)))return 0;
        if(in->x==1){
            char data[645];snprintf(data,sizeof(data),"%s\n%s",network_name,value);
            request("wifi_connect",data,5);
        } else if(in->x==2)request("time",value,5);
        return 1;
    }
    if(in->kind==INPUT_SERVICE) {
        if(in->x==1 && in->y) {
            for(int i=0;i<15;i++){char key[16];snprintf(key,sizeof(key),"p%d",i);prefs[i]=lumen_number(key);}
            lumen_language=prefs[0];sleep_minutes=lumen_number("sleep");autostart=lumen_number("autostart");
            release_state=lumen_number("release");
            lumen_string("version",version,sizeof(version));lumen_string("release_version",new_version,sizeof(new_version));
            lumen_string("checked",checked,sizeof(checked));lumen_string("error",error,sizeof(error));
        } else if((in->x==2 || in->x==3) && in->y) {
            count=lumen_number("count");page=lumen_number("page");read_rows();
            if(in->x==2){enabled=lumen_number("enabled");busy=lumen_number("busy");}
        } else if(in->x==4 && in->y) {
            lumen_string("total",total,sizeof(total));
            for(int i=0;i<5;i++) {
                char key[16];snprintf(key,sizeof(key),"size%d",i);lumen_string(key,sizes[i],sizeof(sizes[i]));
                snprintf(key,sizeof(key),"ratio%d",i);ratios[i]=lumen_number(key);
            }
        } else if(in->x==5) {
            if(!in->y)lumen_request("notify",name(screen),word("Aktion fehlgeschlagen.","Action failed.","Échec de l’action."),0);
            else load();
        }
        return 1;
    }
    return 0;
}
int main(void){lumen_run(draw,input,NULL);return 0;}
