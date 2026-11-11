#include "lumen_app.h"

enum { DAY, WEEK, MONTH, YEAR, NOTE_ACTION=1, JUMP_ACTION, DELETE_ACTION };
static struct tm date;
static int view=MONTH,today;
static char note[512];
static unsigned char marks[12][32];
static int marked_year=-1,activity_month;
static int activity[32];
static const char *months[3][12]={
    {"Januar","Februar","März","April","Mai","Juni","Juli","August","September","Oktober","November","Dezember"},
    {"January","February","March","April","May","June","July","August","September","October","November","December"},
    {"Janvier","Février","Mars","Avril","Mai","Juin","Juillet","Août","Septembre","Octobre","Novembre","Décembre"}
};
static const char *weekdays[3][7]={{"Mo","Di","Mi","Do","Fr","Sa","So"},{"Mon","Tue","Wed","Thu","Fri","Sat","Sun"},
                                     {"Lun","Mar","Mer","Jeu","Ven","Sam","Dim"}};
static int language(void){return lumen_language==1 ? 1 : lumen_language==2 ? 2 : 0;}
static const char *word(const char *de,const char *en,const char *fr) {
    return language()==1 ? en : language()==2 ? fr : de;
}
static int key(const struct tm *t){return (t->tm_year+1900)*10000+(t->tm_mon+1)*100+t->tm_mday;}
static int days_in(int year,int month) {
    static const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    return days[month]+(month==1 && year%4==0 && (year%100!=0 || year%400==0));
}
static void filename(char out[20],int day){snprintf(out,20,"%08d.txt",day);}
static void day_note(int day,char out[512]) {
    char name[20];filename(name,day);size_t n=0;
    out[0]=0;
    if(lumen_storage_read(name,out,511,&n)){out[n]=0;return;}
    const char *root=getenv("LUMEN_LEGACY_DATA");
    char path[2048];
    if(!root || snprintf(path,sizeof(path),"%s/calendar/%s",root,name)>=(int)sizeof(path))return;
    FILE *file=fopen(path,"rb");
    if(file){n=fread(out,1,511,file);out[n]=0;fclose(file);}
}
static void load_marks(void) {
    if(marked_year==date.tm_year)return;
    marked_year=date.tm_year;
    char preview[512];
    for(int m=0;m<12;m++)for(int d=1;d<=days_in(date.tm_year+1900,m);d++) {
        day_note((date.tm_year+1900)*10000+(m+1)*100+d,preview);
        marks[m][d]=*preview!=0;
    }
}
static void fetch_activity(void) {
    int month=key(&date)/100;
    if(month==activity_month)return;
    char value[16];snprintf(value,sizeof(value),"%d",month);
    lumen_service("reading","month",value,4);
}
static void load(void){day_note(key(&date),note);}
static void update_today(void) {
    time_t now=time(NULL);struct tm t;localtime_r(&now,&t);
    today=key(&t);
}
static void reset(void) {
    time_t now=time(NULL);localtime_r(&now,&date);
    date.tm_hour=12;date.tm_isdst=-1;mktime(&date);
    today=key(&date);load();
}
static void move_date(struct tm target) {
    target.tm_hour=12;target.tm_isdst=-1;mktime(&target);
    if(target.tm_year>=0 && target.tm_year<=200){date=target;load();}
}
static void shift(int delta) {
    struct tm next=date;
    if(view<=WEEK)next.tm_mday+=delta*(view==WEEK ? 7 : 1);
    else {
        int day=next.tm_mday;next.tm_mday=1;
        if(view==MONTH)next.tm_mon+=delta;else next.tm_year+=delta;
        mktime(&next);
        int max=days_in(next.tm_year+1900,next.tm_mon);
        next.tm_mday=day<max ? day : max;
    }
    move_date(next);
}
static void write_note(const char *value) {
    char name[20];filename(name,key(&date));
    if(lumen_storage_write(name,value,strlen(value))) {
        snprintf(note,sizeof(note),"%s",value);
        if(marked_year==date.tm_year)marks[date.tm_mon][date.tm_mday]=*note!=0;
    }
    else lumen_request("notify","",word("Speichern fehlgeschlagen.","Could not save.","Enregistrement impossible."),0);
}
static void start_note(void) {
    char title[40];strftime(title,sizeof(title),"%d.%m.%Y",&date);
    lumen_multiline(title,note,NOTE_ACTION);
}
static void line(int x,int y,int w){ui_paint_line(x,y,x+w,y,1,190);}
static void draw_month(void) {
    load_marks();
    struct tm first=date;first.tm_mday=1;mktime(&first);
    int offset=(first.tm_wday+6)%7;
    for(int i=0;i<7;i++)ui_paint_text(27+i*78,218,78,34,weekdays[language()][i],PAINT_SECONDARY);
    line(28,251,544);
    int month=days_in(date.tm_year+1900,date.tm_mon);
    for(int day=1;day<=month;day++) {
        int cell=offset+day-1,x=27+cell%7*78,y=258+cell/7*58;
        int day_key=key(&first)+day-1;
        char label[8];snprintf(label,sizeof(label),"%d",day);
        ui_paint_hitbox(x,y,78,56);
        if(day==date.tm_mday)ui_paint_ellipse(x+39,y+24,22,22,0);
        else if(day_key==today)ui_paint_ring(x+39,y+24,22,2);
        lumen_text(x,y,78,48,label,PAINT_BODY,day==date.tm_mday ? 255 : 0,0);
        if(marks[date.tm_mon][day])ui_paint_ellipse(x+61,y+9,3,3,0);
        if(activity_month==day_key/100 && activity[day]>0)ui_paint_line(x+29,y+54,x+49,y+54,2,160);
    }
    line(28,616,544);
    char label[40];strftime(label,sizeof(label),"%d.%m.%Y",&date);
    if(activity_month==key(&date)/100 && activity[date.tm_mday]>0) {
        size_t n=strlen(label);snprintf(label+n,sizeof(label)-n," · %d min",activity[date.tm_mday]/60);
    }
    lumen_text(30,622,480,39,label,PAINT_HEADING,0,1);
    lumen_wrapped(30,671,490,80,*note ? note : word("Keine Tagesnotiz","No note for this day","Aucune note pour ce jour"),PAINT_SECONDARY,2);
    if(*note)ui_paint_icon_button(528,700,IC_TRASH);
}
static void draw_week(void) {
    struct tm start=date;start.tm_mday-=(start.tm_wday+6)%7;
    for(int col=0;col<7;col++) {
        struct tm day=start;day.tm_mday+=col;mktime(&day);
        int x=28+col*78;char label[40],preview[512];
        ui_paint_text(x,200,78,35,weekdays[language()][col],PAINT_SECONDARY);
        strftime(label,sizeof(label),"%d.%m.",&day);
        ui_paint_text(x,235,78,38,label,PAINT_SECONDARY);
        if(key(&day)==today)ui_paint_line(x+14,274,x+64,274,3,0);
        ui_paint_line(x,289,x,659,1,205);
        ui_paint_hitbox(x,200,78,460);
        day_note(key(&day),preview);
        lumen_wrapped(x+7,306,64,350,*preview ? preview : "—",PAINT_SECONDARY,12);
    }
    ui_paint_line(574,289,574,659,1,205);
    line(28,289,546);line(28,659,546);
}
static void draw_year(void) {
    load_marks();
    for(int m=0;m<12;m++) {
        struct tm first=date;first.tm_mon=m;first.tm_mday=1;mktime(&first);
        int x=28+m%3*185,y=205+m/3*123;
        ui_paint_hitbox(x,y,174,112);
        ui_paint_text(x,y,174,30,months[language()][m],PAINT_BODY);
        int offset=(first.tm_wday+6)%7;
        for(int d=0;d<days_in(date.tm_year+1900,m);d++) {
            int cell=offset+d;
            int marked=marks[m][d+1] || key(&first)+d==today;
            ui_paint_ellipse(x+12+cell%7*22,y+39+cell/7*12,marked ? 4 : 2,marked ? 4 : 2,marked ? 0 : 190);
        }
    }
}
static void draw(void) {
    const char *tabs[]={word("Tag","Day","Jour"),word("Woche","Week","Semaine"),
                        word("Monat","Month","Mois"),word("Jahr","Year","Année")};
    lumen_segments("view",28,74,410,44,tabs,4,view);
    ui_paint_icon_button(468,74,IC_TODAY);ui_paint_icon_button(528,74,IC_NOTE_PLUS);
    int top=view==MONTH ? 154 : 136;
    ui_paint_icon_button(28,top,IC_PREV);ui_paint_icon_button(528,top,IC_NEXT);
    char label[100];
    if(view==WEEK)strftime(label,sizeof(label),word("Woche %V · %G","Week %V · %G","Semaine %V · %G"),&date);
    else if(view==YEAR)strftime(label,sizeof(label),"%Y",&date);
    else snprintf(label,sizeof(label),"%s %d",months[language()][date.tm_mon],date.tm_year+1900);
    if(view==MONTH)ui_paint_hitbox(92,top,416,44);
    ui_paint_text(92,top,416,44,label,PAINT_TITLE);
    if(view==MONTH)draw_month();
    else if(view==WEEK)draw_week();
    else if(view==YEAR)draw_year();
    else {
        strftime(label,sizeof(label),"%d.%m.%Y",&date);
        lumen_text(30,208,540,68,label,PAINT_DISPLAY,0,1);
        line(28,294,544);
        lumen_text(30,315,490,42,word("Tagesnotiz","Day note","Note du jour"),PAINT_TITLE,0,1);
        lumen_wrapped(30,378,540,340,*note ? note : word("Noch keine Notiz für diesen Tag.",
            "There is no note for this day yet.","Il n’y a pas encore de note pour ce jour."),PAINT_BODY,12);
        ui_paint_hitbox(28,360,544,365);
        if(*note)ui_paint_icon_button(528,315,IC_TRASH);
    }
}
static void delete_note(void) {
    const char *labels[]={word("Abbrechen","Cancel","Annuler"),word("Notiz löschen","Delete note","Supprimer la note")};
    lumen_select(labels,2,-1,DELETE_ACTION);
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN){reset();fetch_activity();return 1;}
    if(in->kind==INPUT_REFRESH || in->kind==INPUT_RESUME){activity_month=0;update_today();load();fetch_activity();return 1;}
    if(in->kind==INPUT_SERVICE && in->x==4 && in->y) {
        activity_month=lumen_number("month");
        for(int i=1;i<=31;i++){char name[16];snprintf(name,sizeof(name),"day%d",i);activity[i]=lumen_number(name);}
        return 1;
    }
    if(in->kind==INPUT_COMPONENT) {
        char id[16],gesture[16],value[8];lumen_string("id",id,sizeof(id));lumen_string("gesture",gesture,sizeof(gesture));
        if(!strcmp(id,"view") && !strcmp(gesture,"selected") && lumen_string("text",value,sizeof(value)) &&
           strlen(value)==1 && value[0]>='0' && value[0]<='3'){view=value[0]-'0';if(view==MONTH)fetch_activity();}
        return 1;
    }
    if(in->kind==INPUT_TEXT) {
        char value[512];if(!lumen_string("text",value,sizeof(value)))return 0;
        if(in->x==NOTE_ACTION)write_note(value);
        else if(in->x==JUMP_ACTION) {
            int year,month;char tail;
            if(sscanf(value,"%d-%d%c",&year,&month,&tail)==2 && year>=1900 && year<=2100 && month>=1 && month<=12) {
                struct tm next=date;next.tm_year=year-1900;next.tm_mon=month-1;
                int max=days_in(year,next.tm_mon);if(next.tm_mday>max)next.tm_mday=max;
                move_date(next);fetch_activity();
            }
        }
        return 1;
    }
    if(in->kind==INPUT_SELECT && in->x==DELETE_ACTION && in->y==1){write_note("");return 1;}
    if(in->kind==INPUT_SWIPE && abs(in->dx)>abs(in->dy)){shift(in->dx<0 ? 1 : -1);if(view==MONTH)fetch_activity();return 1;}
    if(in->kind!=INPUT_TAP)return 0;
    int x=in->x,y=in->y,top=view==MONTH ? 154 : 136;
    if(y>=74 && y<118){if(x>=528)start_note();else if(x>=468)reset();}
    else if(y>=top && y<top+44) {
        if(x>=28 && x<72)shift(-1);
        else if(x>=528 && x<572)shift(1);
        else if(view==MONTH && x>=92 && x<508) {
            char label[24];strftime(label,sizeof(label),"%Y-%m",&date);
            lumen_request("input","YYYY-MM",label,JUMP_ACTION);
        }
    } else if(view==MONTH && x>=27 && x<573 && y>=258 && y<606) {
        struct tm first=date;first.tm_mday=1;mktime(&first);
        int day=(y-258)/58*7+(x-27)/78-(first.tm_wday+6)%7+1;
        if(day>=1 && day<=days_in(date.tm_year+1900,date.tm_mon)){date.tm_mday=day;move_date(date);}
    } else if(view==WEEK && x>=28 && x<574 && y>=200 && y<660) {
        struct tm next=date;next.tm_mday+=(x-28)/78-(next.tm_wday+6)%7;
        move_date(next);view=DAY;
    } else if(view==YEAR && x>=28 && x<573 && y>=205 && y<697) {
        int month=(y-205)/123*3+(x-28)/185;
        if(month<12){struct tm next=date;next.tm_mon=month;next.tm_mday=1;move_date(next);view=MONTH;fetch_activity();}
    } else if(view==DAY && *note && x>=528 && y>=315 && y<359)delete_note();
    else if(view==MONTH && *note && x>=528 && y>=700 && y<744)delete_note();
    else if(view==DAY && x>=28 && x<572 && y>=360 && y<725)start_note();
    if(view==MONTH && ((y<118 && x>=468 && x<528) || (y>=top && y<top+44 && (x<72 || x>=528))))fetch_activity();
    return 1;
}
int main(void){reset();lumen_run(draw,input,NULL);return 0;}
