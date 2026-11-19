#include "lumen_app.h"
#include "lumen_worker.h"
#include <math.h>
#include <ctype.h>
#include "lumen_http.h"
#include "lumen_text.h"
#define LUMEN_JSON_IMPLEMENTATION
#include "lumen_json.h"

enum {DASHBOARD, DEVICES, CONFIG, GRAPH};
typedef struct {
    char id[160],name[192],value[64],unit[32];
    int sensor,saved,count,categories;
    char category[8][48];
    double t[128],v[128];
} Device;
static Device devices[8];
static int view,page,count,busy,configured,token_set,trace=-1,hours=24;
static char search[128],graph_id[160];
static Device graph_device;
#include "controller.h"
static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void list(void) {
    ha_update();
    configured=*ha_url && *ha_token;
    token_set=!!*ha_token;
    busy=states_worker!=0;
    if(view==CONFIG)return;
    if(view==GRAPH) {
        for(int i=0;i<ha_count;i++)if(!strcmp(ha_entities[i].id,graph_id)) {
            ha_device(&graph_device,&ha_entities[i],1);
            break;
        }
        ha_fetch_history(graph_id,hours);
        busy=history_worker!=0;
        return;
    }
    int visible[512],per=view==DEVICES ? 6 : 8;
    count=ha_visible(visible,view==DEVICES,search);
    if(page<0)page=0;
    if(page*per>=count)page=count ? (count-1)/per : 0;
    memset(devices,0,sizeof(devices));
    char ids[1500]="";
    for(int i=0;i<per && page*per+i<count;i++) {
        ha_device(&devices[i],&ha_entities[visible[page*per+i]],0);
        if(view==DASHBOARD && devices[i].sensor) {
            size_t used=strlen(ids);
            if(used+strlen(devices[i].id)+2<sizeof(ids))
                snprintf(ids+used,sizeof(ids)-used,"%s%s",used ? "," : "",devices[i].id);
        }
    }
    if(view==DASHBOARD)ha_fetch_history(ids,0);
}
static void show(int next) {
    view=next;page=count=0;trace=-1;lumen_back=view!=DASHBOARD;list();
}
static const char *state_label(const Device *d) {
    if(!strcmp(d->value,"on"))return word("Ein","On","Activé");
    if(!strcmp(d->value,"off"))return word("Aus","Off","Désactivé");
    if(!strcmp(d->value,"unavailable") || !strcmp(d->value,"unknown"))
        return word("Nicht verfügbar","Unavailable","Indisponible");
    return d->value;
}
static void open_history(const Device *device) {
    graph_device=*device;
    snprintf(graph_id,sizeof(graph_id),"%s",device->id);
    show(GRAPH);
}
static void plot(const Device *d,int x,int y,int w,int h,int detail) {
    double now=time(NULL),start=detail ? now-hours*3600 : d->count ? d->t[0] : now;
    double end=detail ? now : d->count ? d->t[d->count-1] : now;
    if(end<=start)end=start+1;
    int carry=-1;
    for(int i=0;i<d->count && d->t[i]<start;i++)carry=i;
    double lo=carry>=0 ? d->v[carry] : INFINITY;
    double hi=carry>=0 ? d->v[carry] : -INFINITY;
    for(int i=0;i<d->count;i++)if(d->t[i]>=start && d->t[i]<=end) {
        lo=fmin(lo,d->v[i]);hi=fmax(hi,d->v[i]);
    }
    if(!isfinite(lo) || !isfinite(hi)) {
        if(detail)ui_paint_text(x,y,w,h,busy ? word("Verlauf lädt …","Loading history…","Chargement…") :
            ha_history_retry>time(NULL) ? word("Verlauf nicht geladen · erneut tippen",
                "Could not load history · tap to retry","Échec du chargement · touchez pour réessayer") :
            word("Kein Verlauf","No history","Aucun historique"),PAINT_BODY);
        return;
    }
    double pad=fmax(.01,(hi-lo)*.08);lo-=pad;hi+=pad;
    if(detail) {
        ui_paint_surface(x,y,w,h,10,255,140);
        int rows=d->categories ? d->categories : 5;
        for(int i=0;i<rows;i++) {
            int py=y+8+i*(h-16)/(rows>1 ? rows-1 : 2);
            char label[48];
            if(d->categories) {
                int category=d->categories-1-i;
                py=y+h-7-(int)((category-lo)/(hi-lo)*(h-14));
                if(py<y || py>y+h)continue;
                snprintf(label,sizeof(label),"%s",d->category[category]);
            } else snprintf(label,sizeof(label),"%.4g",hi-(hi-lo)*i/4);
            lumen_text(8,py-12,x-12,24,label,PAINT_SECONDARY,-1,1);
            ui_paint_line(x+2,py,x+w-2,py,1,205);
        }
        for(int i=0;i<5;i++) {
            char label[32];
            time_t t=(time_t)(start+(end-start)*i/4);struct tm local;localtime_r(&t,&local);
            strftime(label,sizeof(label),"%H:%M",&local);
            ui_paint_text(x+i*(w-58)/4,y+h+6,58,30,label,PAINT_SECONDARY);
        }
    }
    int previous=-1,last_x=0,last_y=0,nearest=-1,dist=w+1;
    for(int i=carry>=0 ? carry : 0;i<d->count;i++) {
        if(d->t[i]<start && i!=carry)continue;
        if(d->t[i]>end)break;
        int px=x+3+(int)((fmax(d->t[i],start)-start)/(end-start)*(w-6));
        int py=y+h-7-(int)((d->v[i]-lo)/(hi-lo)*(h-14));
        if(previous>=0) {
            if(d->categories) {
                ui_paint_line(last_x,last_y,px,last_y,detail ? 2 : 1,detail ? 35 : 144);
                ui_paint_line(px,last_y,px,py,detail ? 2 : 1,detail ? 35 : 144);
            } else ui_paint_line(last_x,last_y,px,py,detail ? 2 : 1,detail ? 35 : 144);
        }
        last_x=px;last_y=py;previous=i;
        if(trace>=0 && abs(px-trace)<dist){nearest=i;dist=abs(px-trace);}
    }
    if(previous>=0) {
        if(detail)ui_paint_line(last_x,last_y,x+w-3,last_y,2,35);
        else ui_paint_ellipse(last_x,last_y,2,2,144);
    }
    if(detail && nearest>=0) {
        int px=x+3+(int)((fmax(d->t[nearest],start)-start)/(end-start)*(w-6));
        ui_paint_line(px,y+1,px,y+h-2,2,0);
        time_t t=(time_t)d->t[nearest];struct tm local;localtime_r(&t,&local);
        char value[80],stamp[32];strftime(stamp,sizeof(stamp),"%d.%m. %H:%M",&local);
        if(d->categories)snprintf(value,sizeof(value),"%s  ·  %s",stamp,d->category[(int)d->v[nearest]]);
        else snprintf(value,sizeof(value),"%s  ·  %.6g %s",stamp,d->v[nearest],d->unit);
        ui_paint_surface(x+12,y+12,w-24,40,9,255,150);
        ui_paint_text(x+12,y+12,w-24,40,value,PAINT_BODY);
    }
}
static void draw(void) {
    if(view==CONFIG) {
        lumen_form("Home Assistant");
        lumen_form_choice("address",word("Serveradresse","Server address","Adresse du serveur"),*ha_url ? ha_url : "homeassistant.local");
        lumen_form_choice("token",word("Zugriffstoken","Access token","Jeton d’accès"),
                          token_set ? "••••••••" : word("Eingeben","Enter","Saisir"));
        lumen_form_text(word("Adresse und Token können auch über die Dateiübertragung eingerichtet werden.",
            "You can also configure the address and token through File Transfer.",
            "La configuration est aussi possible via le transfert de fichiers."),PAINT_BODY);
        lumen_component_button("connect",word("Verbinden","Connect","Connecter"),1);
        return;
    }
    ui_paint_heading("Home Assistant",view==GRAPH ? 1 : 2);
    if(view==GRAPH) {
        lumen_icon_action("history-refresh",528,74,IC_RESTART);
        Device *d=&graph_device;char label[112];
        snprintf(label,sizeof(label),"%s %s",state_label(d),d->unit);
        lumen_text(30,131,540,40,d->name,PAINT_BODY,0,1);
        lumen_text(30,171,540,42,label,PAINT_TITLE,0,1);
        int left=d->categories ? 108 : 66;
        plot(d,left,229,558-left,413,1);
        const char *spans[]={"1 h","6 h","24 h"};
        lumen_segments("span",28,716,544,48,spans,3,hours==1 ? 0 : hours==6 ? 1 : 2);
        return;
    }
    lumen_icon_action("settings",468,74,IC_SETTINGS);
    lumen_icon_action("devices",528,74,view==DEVICES ? IC_DONE : IC_DEVICES);
    if(!configured) {
        lumen_wrapped(30,175,540,120,word("Serveradresse und Zugriffstoken einrichten.",
            "Set up the server address and access token.","Configurez l’adresse du serveur et le jeton d’accès."),PAINT_HEADING,4);
        lumen_button_action("settings",28,420,544,48,word("Einrichten","Set up","Configurer"),1);
        return;
    }
    if(view==DEVICES) {
        lumen_custom_action("search",28,186,544,48);
        ui_paint_surface(28,186,544,48,10,255,105);
        ui_paint_icon_button(29,188,IC_SEARCH);
        lumen_text(79,188,477,44,*search ? search : word("Geräte suchen","Search devices","Rechercher"),PAINT_BODY,*search ? 0 : 100,1);
    }
    int per=view==DEVICES ? 6 : 8;
    for(int i=0;i<per && page*per+i<count;i++) {
        Device *d=&devices[i];char value[112];
        snprintf(value,sizeof(value),"%s%s%s",state_label(d),*d->unit ? " " : "",d->unit);
        if(view==DEVICES) {
            int y=250+i*68;
            lumen_text(30,y+4,445,33,d->name,PAINT_BODY,0,1);
            lumen_text(30,y+35,445,26,value,PAINT_SECONDARY,-1,1);
            lumen_toggle(492,y+14,d->saved);
            lumen_index_action("favorite",i,28,y,544,67);
            lumen_list_divider(28,y+67,572);
        } else {
            int x=28+i%2*282,y=148+i/2*140;
            ui_paint_surface(x,y,262,126,20,255,100);
            if(d->sensor)plot(d,x+5,y+7,252,110,0);
            lumen_text(x+13,y+8,236,31,d->name,PAINT_BODY,0,1);
            lumen_text(x+13,y+41,d->sensor ? 236 : 146,51,value,d->sensor ? PAINT_TITLE : PAINT_BODY,0,1);
            lumen_index_action("device",i,x,y,262,126);
            if(!d->sensor) {
                char id[24];snprintf(id,sizeof(id),"power:%d",i);
                lumen_nested_toggle(id,x+175,y+51,!strcmp(d->value,"on"));
            }
        }
    }
    if(!count)lumen_wrapped(30,view==DEVICES ? 265 : 220,540,105,
        busy ? word("Geräte werden geladen …","Loading devices…","Chargement des appareils…") :
        word("Keine Geräte gefunden.","No devices found.","Aucun appareil trouvé."),PAINT_HEADING,3);
    lumen_pager(745,page,(count+per-1)/per);
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN){ha_init();list();return 1;}
    if(in->kind==INPUT_RESUME || in->kind==INPUT_REFRESH)ha_configure();
    if(in->kind==INPUT_RESUME || in->kind==INPUT_REFRESH || in->kind==INPUT_TICK){list();return 1;}
    if(in->kind==INPUT_BACK && view!=DASHBOARD){show(DASHBOARD);return 1;}
    if(in->kind==INPUT_PAGE){page=in->x;list();return 1;}
    if(in->kind==INPUT_COMPONENT && view==GRAPH) {
        char value[8],id[32];
        lumen_string("text",value,sizeof(value));
        lumen_string("id",id,sizeof(id));
        if(!strcmp(id,"span") && value[0]>='0' && value[0]<='2'){hours=(int[]){1,6,24}[value[0]-'0'];trace=-1;list();}
        else if(!strcmp(id,"history-refresh") && !history_worker) {
            ha_history_at=ha_history_retry=0;
            list();
        }
        return 1;
    }
    if(in->kind==INPUT_TEXT) {
        char text[512];if(!lumen_string("text",text,sizeof(text)))return 0;
        if(in->x==1){snprintf(search,sizeof(search),"%.*s",(int)sizeof(search)-1,text);page=0;list();}
        else if(in->x==2 || in->x==3) {
            int address=in->x==2;
            if((address ? ha_url_valid(text) : ha_token_valid(text)) &&
               ha_set_connection(address ? text : ha_url,address ? ha_token : text)) {
                snprintf(address ? ha_url : ha_token,sizeof(ha_url),"%s",text);
                ha_refresh_at=0;
                lumen_worker_cancel(&history_worker);
                ha_history_at=ha_history_retry=0;
                ha_history_key[0]=ha_history_pending[0]=0;
            }
            list();
        }
        return 1;
    }
    if(view==GRAPH && in->kind==INPUT_TAP) {
        if(in->y>=229 && in->y<642 && in->x>=66 && in->x<558) {
            if(ha_history_retry && !history_worker){ha_history_retry=0;list();}
            else trace=in->x;
        }
        return 1;
    }
    char id[64];
    if(view==DASHBOARD && lumen_action(in,id,"long_pressed")) {
        int i=lumen_action_index(id,"device",8);
        if(i>=0 && *devices[i].id)open_history(&devices[i]);
        return 1;
    }
    if(!lumen_action(in,id,"clicked") && !lumen_action(in,id,"changed"))return 0;
    if(view==CONFIG) {
        if(!strcmp(id,"address"))lumen_request("input","Home Assistant URL",ha_url,2);
        else if(!strcmp(id,"token"))lumen_request("secret",word("Zugriffstoken","Access token","Jeton d’accès"),"",3);
        else if(!strcmp(id,"connect")){ha_configure();ha_refresh_at=0;show(DEVICES);}
        return 1;
    }
    if(!strcmp(id,"settings"))show(CONFIG);
    else if(!strcmp(id,"devices"))show(view==DEVICES ? DASHBOARD : DEVICES);
    else if(view==DEVICES && !strcmp(id,"search"))
        lumen_request("input",word("Geräte suchen","Search devices","Rechercher des appareils"),search,1);
    else if(view==DEVICES) {
        int i=lumen_action_index(id,"favorite",6);
        if(i>=0 && *devices[i].id){ha_toggle_favorite(devices[i].id);list();}
    } else if(view==DASHBOARD) {
        int i=lumen_action_index(id,"device",8);
        if(i>=0 && *devices[i].id)open_history(&devices[i]);
        i=lumen_action_index(id,"power",8);
        if(i>=0 && *devices[i].id)ha_toggle_device(&devices[i]);
    }
    return 1;
}
static void draw_scheduled(void) {
    draw();
    if(states_worker || history_worker)lumen_schedule(250);
    else if(view==DASHBOARD || view==GRAPH)lumen_schedule(5000);
}
int main(void){lumen_run(draw_scheduled,input,NULL);lumen_worker_cancel(&history_worker);lumen_worker_cancel(&states_worker);return 0;}
