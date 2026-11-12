#include "lumen_app.h"

static int tab,page,count,goal=20,goal_saved,remaining=-1,estimate;
static unsigned seconds[31],total,sessions;
static struct {
    char title[320];
    int seconds,pages,sessions,position,length;
} books[5];

static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void fetch(void) {
    if(tab!=1){lumen_service("reading","summary","",1);return;}
    char number[16];snprintf(number,sizeof(number),"%d",page);
    lumen_service("reading","books",number,2);
}
static void text(int x,int y,int w,const char *label,PaintText role) {
    lumen_text(x,y,w,38,label,role,-1,1);
}
static void progress_bar(int x,int y,int w,int n,int max) {
    ui_paint_surface(x,y,w,5,2,215,215);
    if(max>0 && n>0) {
        int fill=n>=max ? w : (int)((int64_t)n*w/max);
        ui_paint_surface(x,y,fill,5,2,0,0);
    }
}
static void metric(int x,int y,const char *label,const char *value) {
    ui_paint_surface(x,y,260,108,10,255,115);
    text(x+15,y+10,230,label,PAINT_SECONDARY);
    text(x+15,y+48,230,value,PAINT_TITLE);
}
static void overview(void) {
    unsigned mins=seconds[0]/60,week=0,goals=0;
    int streak=0;
    for(int i=0;i<31;i++) {
        if(!seconds[i] && i)break;
        if(seconds[i])streak++;
    }
    for(int i=0;i<7;i++){week+=seconds[i]/60;goals+=seconds[i]>=(unsigned)goal*60;}
    char line[180];
    snprintf(line,sizeof(line),"%u / %d min",mins,goal);
    metric(28,196,word("Heute","Today","Aujourd’hui"),line);
    snprintf(line,sizeof(line),"%d %s",streak,word("Tage","days","jours"));
    metric(312,196,word("Leseserie","Reading streak","Série de lecture"),line);
    snprintf(line,sizeof(line),"%u h %02u m",total/3600,total/60%60);
    metric(28,326,word("Lesezeit","Time spent reading","Temps de lecture"),line);
    snprintf(line,sizeof(line),"%u",sessions);
    metric(312,326,word("Lesesitzungen","Reading sessions","Séances de lecture"),line);
    progress_bar(28,456,544,(int)mins,goal);
    text(30,488,330,word("Mein Tagesziel","My daily goal","Mon objectif quotidien"),PAINT_BODY);
    lumen_button_action("goal",390,484,182,44,word("Ändern","Change","Modifier"),0);
    if(remaining>=0) {
        snprintf(line,sizeof(line),"%s: %d %s",word("Aktuelles Buch","Current book","Livre actuel"),
                 remaining,word("Seiten übrig","pages left","pages restantes"));
        text(30,555,540,line,PAINT_BODY);
        if(estimate>0) {
            snprintf(line,sizeof(line),"~ %d min %s",estimate,word("bis zum Ende","to finish","pour terminer"));
            text(30,599,540,line,PAINT_BODY);
        }
    }
    snprintf(line,sizeof(line),"%u min %s · %u / 7 %s",week,word("diese Woche","this week","cette semaine"),
             goals,word("Tagesziele","daily goals","objectifs quotidiens"));
    text(30,645,540,line,PAINT_BODY);
    if(mins>=(unsigned)goal)text(30,687,540,word("Tagesziel erreicht. Schön gelesen!","Daily goal reached.",
                                               "Objectif quotidien atteint."),PAINT_SECONDARY);
    else {
        snprintf(line,sizeof(line),"%s %u min",word("Bis zum Tagesziel:","To reach your goal:","Pour atteindre votre objectif :"),goal-mins);
        text(30,687,540,line,PAINT_SECONDARY);
    }
}
static void draw(void) {
    ui_paint_heading(word("Lesestatistik","Reading Statistics","Statistiques de lecture"),0);
    const char *tabs[]={word("Überblick","Overview","Aperçu"),word("Bücher","Books","Livres"),word("30 Tage","30 days","30 jours")};
    lumen_segments("stats",28,128,544,42,tabs,3,tab);
    if(!tab){overview();return;}
    char line[180];
    if(tab==1) {
        for(int i=0;i<5 && page*5+i<count;i++) {
            int y=190+i*91;
            text(30,y,540,books[i].title,PAINT_BODY);
            snprintf(line,sizeof(line),"%d min · %d %s · %d %s",books[i].seconds/60,books[i].pages,
                     word("Seiten","pages","pages"),books[i].sessions,word("Sitzungen","sessions","séances"));
            text(30,y+35,540,line,PAINT_SECONDARY);
            progress_bar(30,y+66,540,books[i].position,books[i].length);
        }
        if(!count)lumen_wrapped(30,220,540,110,word("Deine Leseaktivität wird pro Buch erfasst.",
                     "Reading activity is recorded per book.","L’activité de lecture est enregistrée pour chaque livre."),PAINT_TITLE,4);
        lumen_pager(745,page,(count+4)/5);
        return;
    }
    unsigned max=60,active=0;
    uint64_t sum=0;
    for(int i=0;i<30;i++){sum+=seconds[i];active+=seconds[i]>0;if(seconds[i]>max)max=seconds[i];}
    snprintf(line,sizeof(line),"%u %s · %.1f h",active,word("aktive Tage","active days","jours actifs"),sum/3600.0);
    text(30,214,540,line,PAINT_HEADING);
    for(int i=0;i<30;i++) {
        unsigned value=seconds[29-i];
        int height=(int)(240ULL*value/max),ink=value ? 0 : 220;
        ui_paint_surface(30+i*18,565-height,12,height ? height : 2,6,ink,ink);
    }
    text(30,590,260,word("Vor 30 Tagen","30 days ago","Il y a 30 jours"),PAINT_SECONDARY);
    text(470,590,110,word("Heute","Today","Aujourd’hui"),PAINT_SECONDARY);
    snprintf(line,sizeof(line),"Ø %u min / %s",active ? (unsigned)(sum/60/active) : 0,
             word("aktivem Tag","active day","jour actif"));
    text(30,653,540,line,PAINT_BODY);
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN || in->kind==INPUT_RESUME || in->kind==INPUT_REFRESH){fetch();return 1;}
    if(in->kind==INPUT_PAGE && tab==1){page=in->x;fetch();return 1;}
    if(in->kind==INPUT_COMPONENT) {
        char id[24],gesture[24],selection[8];
        lumen_string("id",id,sizeof(id));lumen_string("gesture",gesture,sizeof(gesture));
        if(!strcmp(id,"stats") && !strcmp(gesture,"selected") &&
           lumen_string("text",selection,sizeof(selection)) && strlen(selection)==1 &&
           selection[0]>='0' && selection[0]<='2'){tab=selection[0]-'0';page=0;fetch();}
        if(!tab && !strcmp(id,"goal") && !strcmp(gesture,"clicked")) {
            char value[16];snprintf(value,sizeof(value),"%d",goal);
            lumen_request("input",word("Tagesziel: Minuten","Daily goal in minutes","Objectif quotidien en minutes"),value,3);
        }
        return 1;
    }
    if(in->kind==INPUT_SERVICE && in->y) {
        if(in->x==1) {
            total=lumen_number("total");sessions=lumen_number("sessions");
            remaining=lumen_number("remaining");estimate=lumen_number("estimate");
            int old_goal=lumen_number("goal");
            if(!goal_saved && old_goal>=1 && old_goal<=240)goal=old_goal;
            for(int i=0;i<31;i++){char key[16];snprintf(key,sizeof(key),"day%d",i);seconds[i]=lumen_number(key);}
        } else if(in->x==2) {
            count=lumen_number("count");page=lumen_number("page");
            for(int i=0;i<5;i++) {
                char key[24];
                snprintf(key,sizeof(key),"title%d",i);lumen_string(key,books[i].title,sizeof(books[i].title));
                snprintf(key,sizeof(key),"seconds%d",i);books[i].seconds=lumen_number(key);
                snprintf(key,sizeof(key),"pages%d",i);books[i].pages=lumen_number(key);
                snprintf(key,sizeof(key),"sessions%d",i);books[i].sessions=lumen_number(key);
                snprintf(key,sizeof(key),"position%d",i);books[i].position=lumen_number(key);
                snprintf(key,sizeof(key),"length%d",i);books[i].length=lumen_number(key);
            }
        }
        return 1;
    }
    if(in->kind==INPUT_TEXT && in->x==3) {
        char raw[24],*end;
        if(lumen_string("text",raw,sizeof(raw))) {
            long value=strtol(raw,&end,10);
            if(!*end && value>=1 && value<=240){goal=(int)value;goal_saved=app_state_save("goal.bin",&goal,sizeof(goal));}
        }
        return 1;
    }
    return 1;
}
int main(void) {
    int value;
    if(app_state_load("goal.bin",&value,sizeof(value)) && value>=1 && value<=240){goal=value;goal_saved=1;}
    lumen_run(draw,input,NULL);
    return 0;
}
