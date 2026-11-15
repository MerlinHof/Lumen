#include "lumen_app.h"

enum { LIMIT=256, DONE=2, PINNED=4, ADD=1, EDIT, DELETE, DEADLINE };
typedef struct {char title[200];int state;int64_t created;} LegacyTask;
typedef struct { char title[200]; int state; int64_t created,deadline; int timed; } Task;
static Task tasks[LIMIT];
static unsigned char just_done[LIMIT];
static int count,board,tab,page,editing=-1;

static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void load(void) {
    if(!app_state_load("tasks-v2.bin",tasks,sizeof(tasks))) {
        static LegacyTask old[LIMIT];
        if(!app_state_load("tasks.bin",old,sizeof(old))) {
            static unsigned char legacy[8+sizeof(old)];
            if(lumen_legacy_state("tasks-1.bin",legacy,sizeof(legacy))) {
                uint32_t header[2];memcpy(header,legacy,8);
                if(header[0]==0x4c554d16 && header[1]==sizeof(old))memcpy(old,legacy+8,sizeof(old));
            }
        }
        for(int i=0;i<LIMIT;i++) {
            memcpy(tasks[i].title,old[i].title,sizeof(tasks[i].title));
            tasks[i].state=old[i].state;tasks[i].created=old[i].created;
        }
    }
    for(int i=0;i<LIMIT;i++) {
        tasks[i].title[199]=0;
        if(*tasks[i].title){
            tasks[i].state&=DONE|PINNED;
            tasks[count++]=tasks[i];
        }
    }
    memset(tasks+count,0,(LIMIT-count)*sizeof(*tasks));
}
static void save(void) {
    if(!app_state_save("tasks-v2.bin",tasks,sizeof(tasks)))
        lumen_request("notify","",word("Speichern fehlgeschlagen.","Could not save.","Enregistrement impossible."),0);
}
static void deadline_label(const Task *task,char *out,size_t capacity) {
    *out=0;
    if(!task->deadline)return;
    time_t date=(time_t)task->deadline;
    struct tm time;
    if(localtime_r(&date,&time))strftime(out,capacity,task->timed ? "%Y-%m-%d %H:%M" : "%Y-%m-%d",&time);
}
static int deadline_parse(const char *text,int64_t *deadline,int *timed) {
    if(!*text){*deadline=*timed=0;return 1;}
    size_t size=strlen(text);
    if(size!=10 && size!=16)return 0;
    int year,month,day,hour=23,minute=59,read=0;
    int fields=size==16 ? sscanf(text,"%4d-%2d-%2d %2d:%2d%n",&year,&month,&day,&hour,&minute,&read) :
                          sscanf(text,"%4d-%2d-%2d%n",&year,&month,&day,&read);
    if(fields!=(size==16 ? 5 : 3) || read!=(int)size || year<1970 || year>9999)return 0;
    struct tm value={.tm_year=year-1900,.tm_mon=month-1,.tm_mday=day,
                     .tm_hour=hour,.tm_min=minute,.tm_sec=size==10 ? 59 : 0,.tm_isdst=-1};
    time_t seconds=mktime(&value);
    if(seconds<0)return 0;
    char normalized[32];
    if(!strftime(normalized,sizeof(normalized),size==16 ? "%Y-%m-%d %H:%M" : "%Y-%m-%d",&value) ||
       strcmp(text,normalized))return 0;
    *deadline=(int64_t)seconds;*timed=size==16;
    return 1;
}
static int task_order(const void *a,const void *b) {
    int i=*(const int *)a,j=*(const int *)b;
    int x=!!(tasks[i].state&PINNED),y=!!(tasks[j].state&PINNED);
    if(x!=y)return y-x;
    int64_t first=tasks[i].deadline,second=tasks[j].deadline;
    if((first!=0)!=(second!=0))return first ? -1 : 1;
    if(first!=second)return first<second ? -1 : 1;
    return i-j;
}
static void draw(void) {
    lumen_icon_action("add",528,74,IC_PLUS);
    lumen_icon_action("board",468,74,IC_GRID);
    const char *tabs[]={"ToDo",word("Erledigt","Done","Terminé")};
    if(!board)lumen_segments("view",28,74,410,44,tabs,2,tab);
    else for(int i=0;i<2;i++)ui_paint_text(28+i*282,139,262,44,tabs[i],PAINT_HEADING);
    int rows=0;
    lumen_flow_list(board ? 2 : 1,page);
    int order[LIMIT];for(int i=0;i<count;i++)order[i]=i;
    qsort(order,count,sizeof(*order),task_order);
    time_t now=time(NULL);
    int review_deadlines=0;
    for(int rank=0;rank<count;rank++) {
        int i=order[rank],checked=!!(tasks[i].state&DONE);
        if(!board && (checked^!!just_done[i])!=tab)continue;
        char id[16],due[32];snprintf(id,sizeof(id),"%d",i);
        deadline_label(&tasks[i],due,sizeof(due));
        int warning=!checked && tasks[i].deadline && tasks[i].deadline<=(int64_t)now+86400;
        if(!checked && tasks[i].deadline>(int64_t)now+86400)review_deadlines=1;
        lumen_flow_row_detail(rows++,id,tasks[i].title,board ? checked : 0,checked,
            tasks[i].state&PINNED,due,warning);
    }
    lumen_flow_end();
    if(review_deadlines)lumen_schedule(60000);
    if(!rows) {
        ui_paint_icon_button(278,290,IC_DONE);
        ui_paint_text(28,349,544,40,word("Hier ist alles erledigt.","All clear here.","Tout est fait ici."),PAINT_HEADING);
        ui_paint_text(28,395,544,35,word("Mit + eine Aufgabe hinzufügen","Tap + to add a task","Touchez + pour ajouter une tâche"),PAINT_SECONDARY);
    }
}
static int input(const Input *in) {
    if(in->kind==INPUT_PAUSE){memset(just_done,0,sizeof(just_done));return 1;}
    if(in->kind==INPUT_PAGE){page=in->x;return 1;}
    if(in->kind==INPUT_COMPONENT) {
        char id[24],gesture[24],value[8];
        lumen_string("id",id,sizeof(id));lumen_string("gesture",gesture,sizeof(gesture));
        if(!strcmp(id,"view") && !board && !strcmp(gesture,"selected") &&
           lumen_string("text",value,sizeof(value))) {
            tab=!strcmp(value,"1");page=0;return 1;
        }
        if(!strcmp(gesture,"clicked") && !strcmp(id,"add")) {
            if(count<LIMIT)lumen_request("input",word("Neue Aufgabe","New task","Nouvelle tâche"),"",ADD);
            return 1;
        }
        if(!strcmp(gesture,"clicked") && !strcmp(id,"board")){board^=1;page=tab=0;return 1;}
        char *end;long index=strtol(id,&end,10);
        if(!*id || *end || index<0 || index>=count)return 0;
        int i=(int)index;
        if(!strcmp(gesture,"toggled")) {
            tasks[i].state^=DONE;
            if(!board && !tab)just_done[i]=!!(tasks[i].state&DONE);
            save();
        } else if(!strcmp(gesture,"pinned")) {
            tasks[i].state^=PINNED;page=0;save();
        } else if(!strcmp(gesture,"deadline")) {
            char due[32];deadline_label(&tasks[i],due,sizeof(due));editing=i;
            lumen_request("input",word("Frist: JJJJ-MM-TT [HH:MM]","Due: YYYY-MM-DD [HH:MM]",
                "Échéance : AAAA-MM-JJ [HH:MM]"),due,DEADLINE);
        } else if(!strcmp(gesture,"deleted")) {
            editing=i;
            const char *choices[]={word("Abbrechen","Cancel","Annuler"),word("Aufgabe löschen","Delete task","Supprimer la tâche")};
            lumen_select(choices,2,-1,DELETE);
        } else if(!strcmp(gesture,"clicked")) {
            editing=i;
            lumen_request("input",word("Aufgabe bearbeiten","Edit task","Modifier la tâche"),tasks[i].title,EDIT);
        }
        return 1;
    }
    if(in->kind==INPUT_SELECT && in->x==DELETE) {
        if(in->y==1 && editing>=0 && editing<count) {
            memmove(tasks+editing,tasks+editing+1,(count-editing-1)*sizeof(*tasks));
            memmove(just_done+editing,just_done+editing+1,(size_t)(count-editing-1));
            memset(tasks+--count,0,sizeof(*tasks));just_done[count]=0;save();
        }
        editing=-1;return 1;
    }
    if(in->kind==INPUT_TEXT) {
        char title[200];
        if(!wire_bounded_string("text",title,sizeof(title)))return 0;
        if(in->x==DEADLINE && editing>=0 && editing<count) {
            Task *task=&tasks[editing];editing=-1;
            if(deadline_parse(title,&task->deadline,&task->timed)){page=0;save();}
            else lumen_request("notify","",word("Ungültige Frist. JJJJ-MM-TT [HH:MM] verwenden.",
                "Invalid deadline. Use YYYY-MM-DD [HH:MM].","Échéance invalide. Utilisez AAAA-MM-JJ [HH:MM]."),0);
            return 1;
        }
        if(!*title)return 0;
        if(in->x==ADD && count<LIMIT) {
            Task *task=&tasks[count++];
            *task=(Task){.created=time(NULL)};snprintf(task->title,sizeof(task->title),"%s",title);
            page=tab=0;save();
        } else if(in->x==EDIT && editing>=0 && editing<count) {
            snprintf(tasks[editing].title,sizeof(tasks[editing].title),"%s",title);editing=-1;save();
        }
        return 1;
    }
    return 1;
}
int main(void){load();lumen_run(draw,input,NULL);return 0;}
