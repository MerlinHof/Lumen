#include "lumen_app.h"
#include <math.h>

enum {DASHBOARD, DEVICES, CONFIG, GRAPH};
typedef struct {
    char id[160],name[192],value[64],unit[32];
    int sensor,saved,count;
    double t[128],v[128];
} Device;
static Device devices[8];
static int view,page,count,busy,configured,token_set,trace=-1,hours=24;
static char search[128],url[512],graph_id[160];
static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void request(const char *op,const char *data,int action){lumen_service("home_assistant",op,data,action);}
static void list(void) {
    if(view==CONFIG){request("config","",3);return;}
    if(view==GRAPH){request("graph",graph_id,4);return;}
    char data[145];snprintf(data,sizeof(data),"%d\n%s",page,view==DEVICES ? search : "");
    request(view==DEVICES ? "devices" : "dashboard",data,1);
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
static void plot(const Device *d,int x,int y,int w,int h,int detail) {
    double now=time(NULL),start=detail ? now-hours*3600 : d->count ? d->t[0] : now;
    double end=detail ? now : d->count ? d->t[d->count-1] : now;
    if(end<=start)end=start+1;
    double lo=INFINITY,hi=-INFINITY;
    for(int i=0;i<d->count;i++)if(d->t[i]>=start && d->t[i]<=end) {
        lo=fmin(lo,d->v[i]);hi=fmax(hi,d->v[i]);
    }
    if(!isfinite(lo) || !isfinite(hi)) {
        if(detail)ui_paint_text(x,y,w,h,busy ? word("Verlauf lädt …","Loading history…","Chargement…") :
            word("Kein Verlauf","No history","Aucun historique"),PAINT_BODY);
        return;
    }
    double pad=fmax(.01,(hi-lo)*.08);lo-=pad;hi+=pad;
    if(detail) {
        ui_paint_surface(x,y,w,h,10,255,140);
        for(int i=0;i<5;i++) {
            int py=y+8+i*(h-16)/4;
            char label[32];snprintf(label,sizeof(label),"%.4g",hi-(hi-lo)*i/4);
            lumen_text(8,py-12,x-12,24,label,PAINT_SECONDARY,-1,1);
            ui_paint_line(x+2,py,x+w-2,py,1,205);
            time_t t=(time_t)(start+(end-start)*i/4);struct tm local;localtime_r(&t,&local);
            strftime(label,sizeof(label),"%H:%M",&local);
            ui_paint_text(x+i*(w-58)/4,y+h+6,58,30,label,PAINT_SECONDARY);
        }
    }
    int previous=-1,last_x=0,last_y=0,nearest=-1,dist=w+1;
    for(int i=0;i<d->count;i++) {
        if(d->t[i]<start || d->t[i]>end)continue;
        int px=x+3+(int)((d->t[i]-start)/(end-start)*(w-6));
        int py=y+h-7-(int)((d->v[i]-lo)/(hi-lo)*(h-14));
        if(previous>=0)ui_paint_line(last_x,last_y,px,py,detail ? 2 : 1,detail ? 35 : 144);
        last_x=px;last_y=py;previous=i;
        if(trace>=0 && abs(px-trace)<dist){nearest=i;dist=abs(px-trace);}
    }
    if(detail && nearest>=0) {
        int px=x+3+(int)((d->t[nearest]-start)/(end-start)*(w-6));
        ui_paint_line(px,y+1,px,y+h-2,2,0);
        time_t t=(time_t)d->t[nearest];struct tm local;localtime_r(&t,&local);
        char value[80],stamp[32];strftime(stamp,sizeof(stamp),"%d.%m. %H:%M",&local);
        snprintf(value,sizeof(value),"%s  ·  %.6g %s",stamp,d->v[nearest],d->unit);
        ui_paint_surface(x+12,y+12,w-24,40,9,255,150);
        ui_paint_text(x+12,y+12,w-24,40,value,PAINT_BODY);
    }
}
static void draw(void) {
    ui_paint_heading("Home Assistant",view==DASHBOARD || view==DEVICES ? 2 : 0);
    if(view==CONFIG) {
        lumen_text(30,143,540,35,word("Serveradresse","Server address","Adresse du serveur"),PAINT_BODY,0,1);
        lumen_button(28,188,544,52,*url ? url : "http://homeassistant.local:8123",0);
        lumen_text(30,281,540,35,word("Zugriffstoken","Access token","Jeton d’accès"),PAINT_BODY,0,1);
        lumen_button(28,327,544,52,token_set ? "••••••••" : word("Token eingeben","Enter token","Saisir le jeton"),0);
        lumen_wrapped(30,414,540,132,word("Adresse und Token lassen sich auch über Dateiübertragung im Browser einrichten.",
            "Set the address and token here or in the File Transfer web interface.",
            "Configurez l’adresse et le jeton ici ou via l’interface web de transfert."),PAINT_BODY,4);
        lumen_button(28,607,544,52,word("Verbinden","Connect","Connecter"),1);
        return;
    }
    if(view==GRAPH) {
        Device *d=&devices[0];char label[112];
        snprintf(label,sizeof(label),"%s %s",state_label(d),d->unit);
        lumen_text(30,131,540,40,d->name,PAINT_BODY,0,1);
        lumen_text(30,171,540,42,label,PAINT_TITLE,0,1);
        plot(d,66,229,492,413,1);
        const char *spans[]={"1 h","6 h","24 h"};
        lumen_segments("span",28,716,544,48,spans,3,hours==1 ? 0 : hours==6 ? 1 : 2);
        return;
    }
    ui_paint_icon_button(468,74,IC_SETTINGS);
    ui_paint_icon_button(528,74,view==DEVICES ? IC_DONE : IC_DEVICES);
    if(!configured) {
        lumen_wrapped(30,175,540,120,word("Serveradresse und Zugriffstoken einrichten.",
            "Set up the server address and access token.","Configurez l’adresse du serveur et le jeton d’accès."),PAINT_HEADING,4);
        lumen_button(28,420,544,48,word("Einrichten","Set up","Configurer"),1);
        return;
    }
    if(view==DEVICES) {
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
            ui_paint_line(28,y+67,572,y+67,1,190);
        } else {
            int x=28+i%2*282,y=148+i/2*140;
            ui_paint_surface(x,y,262,126,20,255,100);
            if(d->sensor)plot(d,x+5,y+7,252,110,0);
            lumen_text(x+13,y+8,236,31,d->name,PAINT_BODY,0,1);
            lumen_text(x+13,y+41,d->sensor ? 236 : 146,51,value,d->sensor ? PAINT_TITLE : PAINT_BODY,0,1);
            if(!d->sensor)lumen_toggle(x+175,y+51,!strcmp(d->value,"on"));
            ui_paint_hitbox(x,y,262,126);
        }
    }
    if(!count)lumen_wrapped(30,view==DEVICES ? 265 : 220,540,105,
        busy ? word("Geräte werden geladen …","Loading devices…","Chargement des appareils…") :
        word("Keine Geräte gefunden.","No devices found.","Aucun appareil trouvé."),PAINT_HEADING,3);
    lumen_pager(745,page,(count+per-1)/per);
}
static void row(int i) {
    Device *d=&devices[i];char key[24],points[4200];
    snprintf(key,sizeof(key),"id%d",i);lumen_string(key,d->id,sizeof(d->id));
    snprintf(key,sizeof(key),"name%d",i);lumen_string(key,d->name,sizeof(d->name));
    snprintf(key,sizeof(key),"value%d",i);lumen_string(key,d->value,sizeof(d->value));
    snprintf(key,sizeof(key),"unit%d",i);lumen_string(key,d->unit,sizeof(d->unit));
    snprintf(key,sizeof(key),"sensor%d",i);d->sensor=lumen_number(key);
    snprintf(key,sizeof(key),"saved%d",i);d->saved=lumen_number(key);
    snprintf(key,sizeof(key),"points%d",i);lumen_string(key,points,sizeof(points));
    d->count=0;
    const char *at=points;
    while(*at && d->count<128) {
        int len=0;double time,value;
        if(sscanf(at,"%lf %lf\n%n",&time,&value,&len)!=2 || len<1 || !isfinite(time) || !isfinite(value))break;
        d->t[d->count]=time;d->v[d->count++]=value;at+=len;
    }
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN){request("refresh","",5);return 1;}
    if(in->kind==INPUT_RESUME || in->kind==INPUT_REFRESH || in->kind==INPUT_TICK){list();return 1;}
    if(in->kind==INPUT_BACK && view!=DASHBOARD){show(DASHBOARD);return 1;}
    if(in->kind==INPUT_PAGE){page=in->x;list();return 1;}
    if(in->kind==INPUT_COMPONENT && view==GRAPH) {
        char value[8];lumen_string("text",value,sizeof(value));
        if(value[0]>='0' && value[0]<='2'){hours=(int[]){1,6,24}[value[0]-'0'];trace=-1;}
        return 1;
    }
    if(in->kind==INPUT_TEXT) {
        char text[512];if(!lumen_string("text",text,sizeof(text)))return 0;
        if(in->x==1){snprintf(search,sizeof(search),"%.*s",(int)sizeof(search)-1,text);page=0;list();}
        else if(in->x==2 || in->x==3)request(in->x==2 ? "url" : "token",text,2);
        return 1;
    }
    if(in->kind==INPUT_SERVICE) {
        if(in->x==1 && in->y && (view==DASHBOARD || view==DEVICES)) {
            page=lumen_number("page");count=lumen_number("count");
            busy=lumen_number("busy");configured=lumen_number("configured");
            for(int i=0;i<8;i++)row(i);
        } else if(in->x==3 && in->y && view==CONFIG) {
            lumen_string("url",url,sizeof(url));token_set=lumen_number("token_set");
        } else if(in->x==4 && in->y && view==GRAPH){row(0);busy=lumen_number("busy");}
        else if(in->x==2 || in->x==5)list();
        return 1;
    }
    if(in->kind!=INPUT_TAP)return 0;
    int x=in->x,y=in->y;
    if(view==CONFIG) {
        if(y>=188 && y<240)lumen_request("input","Home Assistant URL",url,2);
        else if(y>=327 && y<379)lumen_request("secret",word("Zugriffstoken","Access token","Jeton d’accès"),"",3);
        else if(y>=607 && y<659){view=DEVICES;lumen_back=1;page=count=0;request("refresh","",5);}
        return 1;
    }
    if(view==GRAPH) {
        if(y>=229 && y<642 && x>=66 && x<558)trace=x;
        return 1;
    }
    if(y>=74 && y<118 && x>=468)show(x<528 ? CONFIG : view==DEVICES ? DASHBOARD : DEVICES);
    else if(!configured && y>=420 && y<468)show(CONFIG);
    else if(view==DEVICES && y>=186 && y<234)
        lumen_request("input",word("Geräte suchen","Search devices","Rechercher des appareils"),search,1);
    else if(view==DEVICES && y>=250 && y<658) {
        int i=(y-250)/68;if(*devices[i].id)request("favorite",devices[i].id,2);
    } else if(view==DASHBOARD && x>=28 && x<572 && y>=148 && y<708 && (y-148)%140<126) {
        int i=(y-148)/140*2+(x>=310);
        if(*devices[i].id) {
            if(devices[i].sensor){snprintf(graph_id,sizeof(graph_id),"%s",devices[i].id);show(GRAPH);}
            else request("toggle",devices[i].id,2);
        }
    }
    return 1;
}
static void draw_scheduled(void) {
    draw();
    if(view==DASHBOARD || view==GRAPH)lumen_schedule(5000);
}
int main(void){lumen_run(draw_scheduled,input,NULL);return 0;}
