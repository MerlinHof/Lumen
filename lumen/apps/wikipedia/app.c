#include "lumen_app.h"
#include "summary.h"
#include "lumen_http.h"
#include "lumen_text.h"
#include "archive.h"
#include "fetch.h"

static int page,count,busy;
static char query[256],deleting[17];
static struct {char id[17],title[256],date[64];} rows[7];

static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void list(void) {
    refresh_archive();
    count=archive_count;
    if(page<0 || page*7>=count)page=0;
    memset(rows,0,sizeof(rows));
    for(int i=0;i<7 && page*7+i<count;i++) {
        const ArchiveRow *article=&archive[page*7+i];
        snprintf(rows[i].id,sizeof(rows[i].id),"%016llx",(unsigned long long)article->id);
        snprintf(rows[i].title,sizeof(rows[i].title),"%s",article->title);
        time_t seen=(time_t)article->seen;
        struct tm date;
        if(localtime_r(&seen,&date))strftime(rows[i].date,sizeof(rows[i].date),"%d.%m.%Y · %H:%M",&date);
    }
}
static void unavailable(void) {
    lumen_request("notify","Wikipedia",word("Artikel nicht verfügbar.","Article unavailable.","Article indisponible."),0);
}
static void search(void) {
    if(!*query)return;
    busy=start_article(query);
    if(!busy)unavailable();
}
static void draw(void) {
    ui_paint_heading("Wikipedia",1);
    lumen_icon_action("search",528,74,IC_SEARCH);
    for(int i=0;i<7 && page*7+i<count;i++) {
        int y=151+i*78;
        char action[20];
        if(!*deleting && !busy) {
            snprintf(action,sizeof(action),"open-%d",i);
            lumen_custom_action(action,28,y,484,78);
        }
        lumen_text(36,y+10,465,34,rows[i].title,PAINT_BODY,-1,1);
        lumen_text(36,y+44,465,24,rows[i].date,PAINT_SECONDARY,-1,1);
        if(!*deleting && !busy) {
            snprintf(action,sizeof(action),"delete-%d",i);
            lumen_icon_action(action,522,y+17,IC_TRASH);
        }
        lumen_list_divider(28,y+77,572);
    }
    if(!count)
        lumen_wrapped(30,175,540,120,word("Gelesene Artikel bleiben hier offline verfügbar.",
            "Articles you read stay available offline.","Les articles lus restent disponibles hors ligne."),
            PAINT_HEADING,4);
    if(fetch_worker>0)lumen_schedule(200);
    if(*deleting || busy) {
        ui_paint_surface(30,272,540,210,14,255,95);
        ui_paint_text(45,290,510,74,busy ?
            word("Artikel wird geladen …","Loading article…","Chargement de l’article…") :
            word("Artikel löschen?","Delete article?","Supprimer l’article ?"),PAINT_HEADING);
        if(!busy) {
            lumen_button_action("cancel",48,400,240,54,word("Abbrechen","Cancel","Annuler"),0);
            lumen_button_action("confirm",312,400,240,54,word("Löschen","Delete","Supprimer"),1);
        }
    } else lumen_pager(746,page,(count+6)/7);
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN || in->kind==INPUT_RESUME || in->kind==INPUT_REFRESH) {
        list();
        return 1;
    }
    if(in->kind==INPUT_INTENT) {
        char operation[24];
        if(lumen_string("operation",operation,sizeof(operation)) && !strcmp(operation,"search") &&
           lumen_string("text",query,sizeof(query)))search();
    }
    if(in->kind==INPUT_BACK && *deleting){deleting[0]=0;lumen_back=0;return 1;}
    if(in->kind==INPUT_PAGE && !busy && !*deleting){page=in->x;list();return 1;}
    if(in->kind==INPUT_TEXT && in->x==1 && lumen_string("text",query,sizeof(query)))search();
    if(in->kind==INPUT_PAUSE){accept_result=0;return 1;}
    if(in->kind==INPUT_TICK) {
        uint64_t key;
        if(finish_article(&key)) {
            busy=0;
            list();
            if(accept_result && (!key || !open_article(key,0)))unavailable();
        }
        return 1;
    }
    char id[24],gesture[24];
    if(in->kind!=INPUT_COMPONENT || busy || !lumen_string("id",id,sizeof(id)) ||
       !lumen_string("gesture",gesture,sizeof(gesture)) || strcmp(gesture,"clicked"))return 0;
    if(*deleting) {
        if(!strcmp(id,"confirm")) {
            if(!remove_article(strtoull(deleting,NULL,16)))unavailable();
            list();
        }
        if(!strcmp(id,"confirm") || !strcmp(id,"cancel")){deleting[0]=0;lumen_back=0;}
    } else if(!strcmp(id,"search")) {
        lumen_request("input",word("Wikipedia durchsuchen","Search Wikipedia","Rechercher dans Wikipédia"),query,1);
    } else {
        int remove=!strncmp(id,"delete-",7);
        if(!remove && strncmp(id,"open-",5))return 0;
        const char *number=id+(remove ? 7 : 5);
        if(strlen(number)!=1 || *number<'0' || *number>'6')return 0;
        int row=*number-'0';
        if(page*7+row<count && *rows[row].id) {
            if(remove) {
                snprintf(deleting,sizeof(deleting),"%s",rows[row].id);
                lumen_back=1;
            } else {
                const ArchiveRow *article=&archive[page*7+row];
                if(!open_article(article->id,article->legacy))unavailable();
            }
        }
    }
    return 1;
}
int main(void){lumen_service_handler=serve_summary;lumen_run(draw,input,NULL);return 0;}
