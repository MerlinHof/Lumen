#define _GNU_SOURCE
#include "lumen_app.h"
#include "lumen_worker.h"
#include "lumen_http.h"
#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include "archive.h"
#include "provider.h"

static int articles,page,edition_page,pending_article;
static char query[256];

static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void html_escape_n(FILE *out,const char *text,size_t size) {
    for(size_t i=0;i<size;i++) {
        switch(text[i]) {
        case '&':fputs("&amp;",out);break;
        case '<':fputs("&lt;",out);break;
        case '>':fputs("&gt;",out);break;
        case '"':fputs("&quot;",out);break;
        default:fputc((unsigned char)text[i],out);break;
        }
    }
}
static void html_escape(FILE *out,const char *text) {html_escape_n(out,text,strlen(text));}
#include "editions.h"

static void date_label(char *out,size_t size,int value,const char *suffix) {
    snprintf(out,size,"%02d.%02d.%04d%s",value%100,value/100%100,value/10000,suffix);
}
static void draw(void) {
    int busy=press_busy(),count=articles ? press_list.count : date_count,today=news_date();
    int per_page=articles ? 5 : 7,max_page=count ? (count-1)/per_page : 0;
    if(page<0 || page>max_page)page=0;
    if(busy)lumen_schedule(250);
    ui_paint_heading("News",1);
    lumen_icon_action("search",528,74,IC_SEARCH);
    const char *headline=word("Kuratierte Ausgaben","Curated editions","Éditions sélectionnées");
    char edition[64];
    if(articles) {
        date_label(edition,sizeof(edition),press_list.date,"");
        headline=busy ? word("Nachrichten werden geladen …","Loading news…","Chargement des actualités…") :
            *query ? query : edition;
    }
    lumen_text(30,128,540,35,headline,PAINT_SECONDARY,-1,1);
    int top=articles ? 169 : 181, height=articles ? 110 : 75;
    for(int i=0;i<per_page && page*per_page+i<count;i++) {
        int y=top+i*height;
        char id[16];snprintf(id,sizeof(id),"entry-%d",i);
        if(!busy)lumen_custom_action(id,28,y,544,height);
        if(articles) {
            PressItem *item=&press_list.item[page*per_page+i];
            char detail[180];
            snprintf(detail,sizeof(detail),"%s · %s",item->category,item->publisher);
            lumen_wrapped(36,y+9,522,66,item->title,PAINT_BODY,2);
            lumen_text(36,y+75,522,25,detail,PAINT_SECONDARY,-1,1);
        } else {
            int value=dates[page*per_page+i],current=value==today;
            date_label(edition,sizeof(edition),value,current ? word(" · Heute"," · Today"," · Aujourd’hui") : "");
            lumen_text(36,y+7,470,34,edition,PAINT_BODY,-1,1);
            lumen_text(36,y+41,470,25,current ?
                word("Nachrichten des Tages","Today’s news","Actualités du jour") :
                word("Gespeicherte Ausgabe","Saved edition","Édition enregistrée"),PAINT_SECONDARY,-1,1);
            ui_paint_icon_button(522,y+15,IC_NEXT);
        }
        lumen_list_divider(28,y+height-1,572);
    }
    if(articles && !count && !busy)
        lumen_wrapped(30,210,540,120,word("Keine Artikel verfügbar.","No articles available.",
            "Aucun article disponible."),PAINT_HEADING,3);
    lumen_pager(746,page,(count+per_page-1)/per_page);
}
static void failed(void) {
    lumen_request("notify","News",word("Nicht geladen. Gespeicherte Inhalte bleiben verfügbar.",
        "Could not load. Saved content is still available.",
        "Chargement impossible. Les contenus enregistrés restent disponibles."),0);
}
static void show_articles(int date,const char *term) {
    if(!articles)edition_page=page;
    press_cancel();
    pending_article=0;
    articles=lumen_back=1;
    page=0;
    memset(&press_list,0,sizeof(press_list));
    press_list.date=date;
    if(term) {
        if(!press_search(term,lumen_language))failed();
    } else if(!press_load_day(date)) {
        press_legacy(date);
        if(date==news_date() && !press_start_day(date,lumen_language))failed();
    }
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN) {
        lumen_storage_write("daily-enabled","1",1);
        press_archive_scan();
        return 1;
    }
    if(in->kind==INPUT_PAUSE){pending_article=0;return 1;}
    if(in->kind==INPUT_RESUME || in->kind==INPUT_REFRESH || in->kind==INPUT_TICK) {
        int result=press_poll();
        if(result==2 && pending_article)open_article();
        if(result<0 && articles)failed();
        if(result)pending_article=0;
        if(!articles && in->kind!=INPUT_TICK)press_archive_scan();
        return 1;
    }
    if(in->kind==INPUT_BACK && articles) {
        if(press_busy())press_cancel();
        articles=lumen_back=pending_article=0;
        query[0]=0;page=edition_page;
        press_archive_scan();
        return 1;
    }
    if(in->kind==INPUT_PAGE){page=in->x;return 1;}
    if(in->kind==INPUT_TEXT && in->x==1 && lumen_string("text",query,sizeof(query)) && *query) {
        show_articles(0,query);
        return 1;
    }
    char id[24];
    if(!lumen_action_read(in,id,sizeof(id),"clicked"))return 0;
    if(!strcmp(id,"search")) {
        lumen_request("input",word("News suchen","Search news","Rechercher des actualités"),query,1);
        return 1;
    }
    int per_page=articles ? 5 : 7;
    if(!press_busy() && strlen(id)==7 && !strncmp(id,"entry-",6) && id[6]>='0' && id[6]<'0'+per_page) {
        int index=page*per_page+id[6]-'0';
        if(!articles && index<date_count) {
            query[0]=0;
            show_articles(dates[index],NULL);
        } else if(articles && index<press_list.count) {
            int result=press_open(&press_list.item[index]);
            if(result==2)open_article();
            else if(result==1)pending_article=1;
            else failed();
        }
    }
    return 1;
}
int main(void) {
    lumen_service_handler=daily_service;
    lumen_run(draw,input,NULL);
    press_cancel();
    return 0;
}
