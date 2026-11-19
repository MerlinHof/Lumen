#define _GNU_SOURCE
#include "lumen_app.h"
#include <ctype.h>
#include <signal.h>
#include <sys/resource.h>
#include <time.h>
#include "catalog.h"
#include "download.h"

enum { FEATURED, RESULTS, DETAIL, SOURCES };
static int view,page,count,busy,depth,previous_view,previous_page,saved_view,saved_page;
static int download_index=-1,row_indices[6];
static char query[256],selected[24],summary[2048];
static struct {char id[24],title[256],author[512];int flag;} rows[6];
static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void store_notice(const char *de,const char *en,const char *fr) {
    lumen_request("notify","Store",word(de,en,fr),0);
}
#include "provider.h"
static void load(void) {
    if(view==SOURCES)u15_init();
    else if(!*query && view==FEATURED && !store_ui.store_federated)store_seed();
    depth=store_ui.opds_depth;
    busy=discover_busy();
    int size=view==SOURCES ? 6 : view==FEATURED ? 4 : 5;
    count=view==SOURCES ? source15_count : opds_count;
    if(page<0 || page*size>=count)page=0;
    memset(rows,0,sizeof(rows));
    summary[0]=0;
    if(view==DETAIL) {
        if(download_index<0 || download_index>=opds_count){count=0;return;}
        count=1;
        snprintf(summary,sizeof(summary),"%s",opds_items[download_index].summary);
    }
    int first=count ? (unsigned)time(NULL)/86400u%(unsigned)count : 0;
    for(int i=0;i<(view==DETAIL ? 1 : size) && page*size+i<count;i++) {
        int index=view==DETAIL ? download_index : view==FEATURED ? (first+i)%count : page*size+i;
        row_indices[i]=index;
        if(view==SOURCES) {
            snprintf(rows[i].title,sizeof(rows[i].title),"%s",source15[index].name);
            snprintf(rows[i].author,sizeof(rows[i].author),"%s",source15[index].url);
            rows[i].flag=source15[index].enabled;
        } else {
            const OPDSItem *book=&opds_items[index];
            snprintf(rows[i].title,sizeof(rows[i].title),"%s",book->title);
            const char *comma=strchr(book->author,',');
            if(comma && !strchr(comma+1,',')) {
                const char *first=comma+1;
                while(*first==' ')first++;
                snprintf(rows[i].author,sizeof(rows[i].author),"%s %.*s",first,(int)(comma-book->author),book->author);
            } else snprintf(rows[i].author,sizeof(rows[i].author),"%s",book->author);
            rows[i].flag=!!book->acquisition;
        }
        snprintf(rows[i].id,sizeof(rows[i].id),"%d",index);
    }
    lumen_back=view!=FEATURED || depth>0;
}
static void cover(int row,int x,int y,int width,int height) {
    int index=row_indices[row];
    const OPDSItem *item=&opds_items[index];
    const char *part=strstr(item->url,"/ebooks/");
    int skip=8;
    if(!part){part=strstr(item->url,"/files/");skip=7;}
    if(!part){part=strstr(item->url,"/cache/epub/");skip=12;}
    char asset[128]="";
    if(part) {
        unsigned long id=strtoul(part+skip,NULL,10);
        if(id)snprintf(asset,sizeof(asset),"assets/cover-%012lx.pgm",id);
    }
    lumen_cover_asset(x,y,width,height,asset,item->title);
}
static void request(const char *operation,const char *data) {
    if(!strcmp(operation,"search")) {
        snprintf(store_ui.store_query,sizeof(store_ui.store_query),"%s",data);
        store_search();
    } else if(!strcmp(operation,"cancel")) {
        discover_cancel();store_ui.store_stage=0;
    } else if(!strcmp(operation,"parent")) {
        if(store_ui.opds_depth) {
            discover_cancel();store_ui.store_stage=0;
            const char *parent=store_ui.opds_history[--store_ui.opds_depth];
            if(!strcmp(parent,"lumen:search"))store_restore();
            else if(!discover_start(parent,NULL))store_ui.opds_depth++;
        }
    } else if(!strcmp(operation,"browse") || !strcmp(operation,"download")) {
        int index=atoi(data),download=!strcmp(operation,"download");
        if(index>=0 && index<opds_count && !!opds_items[index].acquisition==download &&
           (download || store_ui.opds_depth<16) && !import_pending) {
            discover_cancel();store_ui.store_stage=0;
            if(!download)snprintf(store_ui.opds_history[store_ui.opds_depth],2048,"%s",
                store_ui.store_federated ? "lumen:search" : discover_url());
            if(discover_start(opds_items[index].url,download ? &opds_items[index] : NULL) && !download) {
                store_ui.opds_depth++;
                store_ui.store_federated=0;
            }
        }
    } else if(!strcmp(operation,"toggle_source")) {
        int index=atoi(data);
        if(index>=0 && index<source15_count) {
            source15[index].enabled=!source15[index].enabled;
            sources15_save();
        }
    } else if(!strcmp(operation,"add_source") && source15_count<10 &&
              (!strncmp(data,"https://",8) || !strncmp(data,"http://",7)) &&
              !strpbrk(data,"\r\n\"\\<> ")) {
        int index=source15_count++;
        snprintf(source15[index].name,sizeof(source15[index].name),"Catalog %d",index-4);
        snprintf(source15[index].url,sizeof(source15[index].url),"%s",data);
        source15[index].enabled=1;
        sources15_save();
    }
    load();
}
static void set_view(int next,int next_page) {
    view=next;page=next_page;count=0;
    lumen_back=view!=FEATURED || depth>0;
}
static void draw(void) {
    if(job>0)lumen_schedule(250);
    if(view==DETAIL) {
        ui_paint_heading(word("Buch entdecken","Discover a book","Découvrir un livre"),0);
        if(count)cover(0,28,145,120,164);
        lumen_wrapped(176,145,396,128,rows[0].title,PAINT_TITLE,3);
        lumen_wrapped(176,273,396,60,rows[0].author,PAINT_BODY,2);
        lumen_wrapped(28,366,544,275,summary,PAINT_BODY,8);
        lumen_button(28,665,544,48,busy ? word("Download abbrechen","Cancel download","Annuler") :
            word("Buch herunterladen","Download book","Télécharger"),0);
        return;
    }
    ui_paint_heading(view==SOURCES ? word("Store · Quellen","Store · Sources","Librairie · Sources") : "Store",2);
    ui_paint_icon_button(528,74,view==SOURCES ? IC_PLUS : IC_SEARCH);
    if(view!=SOURCES)ui_paint_icon_button(468,74,IC_SETTINGS);
    if(view!=SOURCES)
        lumen_text(30,125,540,36,busy ?
            word("Kataloge werden durchsucht …","Searching catalogs…","Recherche dans les catalogues…") :
            *query ? query : word("Aus den Katalogen","From the catalogs","Dans les catalogues"),PAINT_SECONDARY,-1,1);
    int limit=view==FEATURED ? 4 : view==SOURCES ? 6 : 5;
    for(int i=0;i<limit && page*limit+i<count;i++) {
        if(view==FEATURED) {
            int x=36+i%2*284,y=174+i/2*250;
            cover(i,x+57,y,130,178);
            lumen_wrapped(x,y+185,244,57,rows[i].title,PAINT_BODY,2);
            ui_paint_hitbox(x,y,244,243);
        } else if(view==SOURCES) {
            int y=145+i*82;
            lumen_text(30,y+6,425,37,rows[i].title,PAINT_BODY,0,1);
            lumen_text(30,y+42,425,25,rows[i].author,PAINT_SECONDARY,-1,1);
            lumen_toggle(492,y+10,rows[i].flag);
            lumen_list_divider(28,y+75,572);
        } else {
            int y=174+i*87;
            cover(i,30,y+7,50,69);
            lumen_wrapped(100,y+4,419,51,rows[i].title,PAINT_BODY,2);
            lumen_text(100,y+57,419,24,rows[i].author,PAINT_SECONDARY,-1,1);
            ui_paint_icon_button(528,y+20,IC_NEXT);
            ui_paint_hitbox(28,y,544,87);
            lumen_list_divider(28,y+86,572);
        }
    }
    if(view!=FEATURED)lumen_pager(745,page,(count+limit-1)/limit);
    if(view!=SOURCES && !count && !busy)
        lumen_wrapped(28,225,544,110,word("Keine Treffer.","No results.","Aucun résultat."),PAINT_HEADING,3);
}
static void search(void) {
    set_view(*query ? RESULTS : FEATURED,0);
    busy=1;depth=0;
    request("search",query);
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN || in->kind==INPUT_RESUME || in->kind==INPUT_REFRESH || in->kind==INPUT_TICK) {
        int completed=discover_complete();
        if(store_ui.store_stage && completed && completed!=2)store_complete(completed);
        else if(completed<0)store_notice("Verbindung oder Download fehlgeschlagen.",
            "Connection or download failed.","Échec de la connexion ou du téléchargement.");
        load();return 1;
    }
    if(in->kind==INPUT_PAUSE && import_pending) {
        import_pending=0;
        lumen_storage_delete(job_file);
        return 1;
    }
    if(in->kind==INPUT_INTENT) {
        if(lumen_string("text",query,sizeof(query)))search();
        return 1;
    }
    if(in->kind==INPUT_BACK) {
        if(view==DETAIL)set_view(previous_view,previous_page);
        else if(view==SOURCES)set_view(saved_view,saved_page);
        else if(depth){page=0;request("parent","");return 1;}
        else {query[0]=0;search();return 1;}
        load();return 1;
    }
    if(in->kind==INPUT_PAGE){page=in->x;load();return 1;}
    if(in->kind==INPUT_TEXT && in->x==1 && lumen_string("text",query,sizeof(query)))search();
    if(in->kind==INPUT_TEXT && in->x==2) {
        char url[2048];if(lumen_string("text",url,sizeof(url)))request("add_source",url);
    }
    if(in->kind==INPUT_SERVICE && in->x==30) {
        import_pending=0;
        lumen_storage_delete(job_file);
        if(!in->y)store_notice("Buch konnte nicht hinzugefügt werden.",
            "Could not add the book to the library.","Impossible d’ajouter le livre à la bibliothèque.");
        load();
        return 1;
    }
    if(in->kind!=INPUT_TAP)return 0;
    int x=in->x,y=in->y;
    if(view==DETAIL) {
        if(y>=665 && y<713)request(busy ? "cancel" : "download",selected);
        return 1;
    }
    if(y>=74 && y<118) {
        if(x>=528) {
            if(view==SOURCES)lumen_request("input","OPDS URL ({q})","",2);
            else lumen_request("input",word("Bücher suchen","Search books","Chercher des livres"),query,1);
        } else if(x>=468 && view!=SOURCES) {
            saved_view=view;saved_page=page;set_view(SOURCES,0);load();
        }
        return 1;
    }
    if(x<28 || x>=572)return 0;
    if(view==SOURCES) {
        if(y>=145 && y<637 && x>=460) {
            int i=(y-145)/82;
            if(*rows[i].id)request("toggle_source",rows[i].id);
        }
        return 1;
    }
    int i=-1;
    if(view==FEATURED && y>=174 && y<659)i=(y-174)/250*2+(x>=300);
    if(view==RESULTS && y>=174 && y<609)i=(y-174)/87;
    if(i>=0 && *rows[i].id) {
        if(rows[i].flag) {
            previous_view=view;previous_page=page;
            snprintf(selected,sizeof(selected),"%s",rows[i].id);
            download_index=row_indices[i];
            set_view(DETAIL,0);load();
        } else {
            set_view(RESULTS,0);
            request("browse",rows[i].id);
        }
    }
    return 1;
}
int main(void){lumen_run(draw,input,NULL);discover_cancel();return 0;}
