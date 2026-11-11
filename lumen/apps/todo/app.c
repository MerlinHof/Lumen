#include "lumen_app.h"

enum { LIMIT=256, DONE=2, PINNED=4, ADD=1, EDIT, DELETE };
typedef struct {char title[200];int state;int64_t created;} Task;
static Task tasks[LIMIT];
static unsigned char just_done[LIMIT];
static int count,board,tab,page,editing=-1;

static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void load(void) {
    if(!app_state_load("tasks.bin",tasks,sizeof(tasks))) {
        static unsigned char legacy[8+sizeof(tasks)];
        if(lumen_legacy_state("tasks-1.bin",legacy,sizeof(legacy))) {
            uint32_t header[2];memcpy(header,legacy,8);
            if(header[0]==0x4c554d16 && header[1]==sizeof(tasks))memcpy(tasks,legacy+8,sizeof(tasks));
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
    if(!app_state_save("tasks.bin",tasks,sizeof(tasks)))
        lumen_request("notify","",word("Speichern fehlgeschlagen.","Could not save.","Enregistrement impossible."),0);
}
static void draw(void) {
    ui_paint_icon_button(528,74,IC_PLUS);
    ui_paint_icon_button(468,74,IC_GRID);
    const char *tabs[]={"ToDo",word("Erledigt","Done","Terminé")};
    if(!board)lumen_segments("view",28,74,410,44,tabs,2,tab);
    else for(int i=0;i<2;i++)ui_paint_text(28+i*282,139,262,44,tabs[i],PAINT_HEADING);
    int rows=0;
    lumen_flow_list(board ? 2 : 1,page);
    for(int pinned=1;pinned>=0;pinned--)for(int i=count-1;i>=0;i--) {
        int checked=!!(tasks[i].state&DONE);
        if(!!(tasks[i].state&PINNED)!=pinned ||
           (!board && (checked^!!just_done[i])!=tab))continue;
        char id[16];snprintf(id,sizeof(id),"%d",i);
        lumen_flow_row(rows++,id,tasks[i].title,board ? checked : 0,checked,pinned);
    }
    lumen_flow_end();
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
        char *end;long index=strtol(id,&end,10);
        if(!*id || *end || index<0 || index>=count)return 0;
        int i=(int)index;
        if(!strcmp(gesture,"toggled")) {
            tasks[i].state^=DONE;
            if(!board && !tab)just_done[i]=!!(tasks[i].state&DONE);
            save();
        } else if(!strcmp(gesture,"pinned")) {
            tasks[i].state^=PINNED;page=0;save();
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
        if(!lumen_string("text",title,sizeof(title)) || !*title)return 0;
        if(in->x==ADD && count<LIMIT) {
            Task *task=&tasks[count++];
            *task=(Task){.created=time(NULL)};snprintf(task->title,sizeof(task->title),"%s",title);
            page=tab=0;save();
        } else if(in->x==EDIT && editing>=0 && editing<count) {
            snprintf(tasks[editing].title,sizeof(tasks[editing].title),"%s",title);editing=-1;save();
        }
        return 1;
    }
    if(in->kind==INPUT_TAP && in->y>=74 && in->y<118) {
        if(in->x>=528 && in->x<572 && count<LIMIT)
            lumen_request("input",word("Neue Aufgabe","New task","Nouvelle tâche"),"",ADD);
        else if(in->x>=468 && in->x<512){board^=1;page=tab=0;}
    }
    return 1;
}
int main(void){load();lumen_run(draw,input,NULL);return 0;}
