#include "lumen_app.h"

enum { FEATURED, RESULTS, DETAIL, SOURCES };
static int view,page,count,busy,depth,previous_view,previous_page,saved_view,saved_page;
static char query[256],selected[24],summary[2048];
static struct {char id[24],title[256],author[512];int flag;} rows[6];
static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void load(void) {
    char number[16];snprintf(number,sizeof(number),"%d",page);
    const char *operations[]={"featured","list","detail","sources"};
    lumen_service("store",operations[view],view==DETAIL ? selected : number,10+view);
}
static void request(const char *operation,const char *data) {
    lumen_service("store",operation,data,1);
}
static void set_view(int next,int next_page) {
    view=next;page=next_page;count=0;
    lumen_back=view!=FEATURED || depth>0;
}
static void draw(void) {
    if(view==DETAIL) {
        ui_paint_heading(word("Buch entdecken","Discover a book","Découvrir un livre"),0);
        lumen_catalog_cover(28,145,120,164,selected);
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
            lumen_catalog_cover(x+57,y,130,178,rows[i].id);
            lumen_wrapped(x,y+185,244,57,rows[i].title,PAINT_BODY,2);
            ui_paint_hitbox(x,y,244,243);
        } else if(view==SOURCES) {
            int y=145+i*82;
            lumen_text(30,y+6,425,37,rows[i].title,PAINT_BODY,0,1);
            lumen_text(30,y+42,425,25,rows[i].author,PAINT_SECONDARY,-1,1);
            lumen_toggle(492,y+10,rows[i].flag);
            ui_paint_line(28,y+75,572,y+75,1,190);
        } else {
            int y=174+i*87;
            lumen_catalog_cover(30,y+7,50,69,rows[i].id);
            lumen_wrapped(100,y+4,419,51,rows[i].title,PAINT_BODY,2);
            lumen_text(100,y+57,419,24,rows[i].author,PAINT_SECONDARY,-1,1);
            ui_paint_icon_button(528,y+20,IC_NEXT);
            ui_paint_hitbox(28,y,544,87);
            ui_paint_line(28,y+86,572,y+86,1,190);
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
    if(in->kind==INPUT_OPEN || in->kind==INPUT_RESUME || in->kind==INPUT_REFRESH){load();return 1;}
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
    if(in->kind==INPUT_SERVICE) {
        if(in->x==1){load();return 1;}
        if(in->x!=10+view || !in->y)return 0;
        count=lumen_number("count");page=lumen_number("page");depth=lumen_number("depth");busy=lumen_number("busy");
        lumen_back=view!=FEATURED || depth>0;
        if(view==DETAIL)lumen_string("summary",summary,sizeof(summary));
        for(int i=0;i<6;i++) {
            char field[20];
            snprintf(field,sizeof(field),"id%d",i);lumen_string(field,rows[i].id,sizeof(rows[i].id));
            snprintf(field,sizeof(field),"title%d",i);lumen_string(field,rows[i].title,sizeof(rows[i].title));
            snprintf(field,sizeof(field),"author%d",i);lumen_string(field,rows[i].author,sizeof(rows[i].author));
            snprintf(field,sizeof(field),"flag%d",i);rows[i].flag=lumen_number(field);
        }
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
            set_view(DETAIL,0);load();
        } else {
            set_view(RESULTS,0);
            request("browse",rows[i].id);
        }
    }
    return 1;
}
int main(void){lumen_run(draw,input,NULL);return 0;}
