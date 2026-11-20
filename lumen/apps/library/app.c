#include "lumen_app.h"

enum {SHELF,DETAIL,PREFERENCES};
static struct {char id[17],title[384],author[256];int pin,progress;} books[7],item;
static char query[128],selected[17],filename[256],raw_author[192],format[16];
static int view,page,pages,sort=-1,has_recent,query_api=1,detail_page,detail_total,kilobytes,supported,confirm;
static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void set_back(void){lumen_back=view!=SHELF || confirm || *query;}
static const char *catalog_service(void){return query_api ? "books" : "library";}
static void load(void) {
    if(view==DETAIL) {
        lumen_service(catalog_service(),"detail",selected,2);
    } else if(query_api) {
        int offset=has_recent && page>0 ? 3+(page-1)*6 : page*6;
        char data[280];
        query[strcspn(query,"\r\n")]=0;
        snprintf(data,sizeof(data),"%d %d %d 3\n%s\nLUMEN Lesetest.epub\nLUMEN – Willkommen.txt",
                 offset,page==0 && has_recent ? 3 : 6,sort,query);
        lumen_service("books","query",data,1);
    } else {
        char data[160];snprintf(data,sizeof(data),"%d\n%s",page,query);
        lumen_service("library","shelf",data,1);
    }
}
static void action(const char *operation,const char *value) {
    char data[448];snprintf(data,sizeof(data),"%s%s%s",selected,value ? "\n" : "",value ? value : "");
    lumen_service(catalog_service(),operation,data,!strcmp(operation,"read") ? 4 : 3);
}
static void cover(int x,int y,int w,int h,const char *id){lumen_book_cover_outline(x,y,w,h,id,198);}
static int shelf_row(int index,int top,int featured) {
    const typeof(*books) *book=&books[index];
    if(!*book->id)return top;
    int height=featured ? 221 : 98;
    int w=featured ? 135 : 59,h=featured ? 184 : 80;
    int x=28+w+(featured ? 23 : 15);
    cover(28,top+(featured ? 11 : 8),w,h,book->id);
    if(featured) {
        lumen_text(x,top+14,572-x,27,word("Weiterlesen","Continue reading","Reprendre la lecture"),PAINT_SECONDARY,-1,1);
        lumen_book_progress(x,top+62,572-x,book->title,book->author,book->progress);
    } else {
        lumen_text(x,top+12,(book->pin ? 465 : 505)-x,39,book->title,PAINT_BODY,-1,1);
        lumen_text(x,top+50,505-x,29,book->author,PAINT_SECONDARY,-1,1);
        if(book->pin)ui_paint_icon_button(469,top+26,IC_PIN);
        lumen_progress_ring(546,top+48,25,book->progress);
    }
    char id[16];snprintf(id,sizeof(id),"book-%d",index);
    lumen_rounded_action(id,28,top,544,height-3,9);
    return top+height;
}
static void shelf(void) {
    const int featured=*books[6].id!=0;
    ui_paint_heading(word("Bibliothek","Library","Bibliothèque"),2);
    lumen_icon_action("search",468,74,IC_SEARCH);
    lumen_icon_action("preferences",528,74,IC_SETTINGS);
    if(*query)lumen_text(28,119,544,25,query,PAINT_SECONDARY,-1,1);
    int top=*query ? 148 : 131;
    if(featured)top=shelf_row(6,top,1);
    for(int i=0;i<(featured ? 3 : 6);i++)top=shelf_row(i,top,0);
    if(!featured && !*books[0].id)lumen_wrapped(28,180,544,115,
        word("Deine Bücher erscheinen hier.","Your books will appear here.","Vos livres apparaîtront ici."),
        PAINT_TITLE,3);
    lumen_pager(745,page,pages);
}
static void draw(void) {
    if(view==PREFERENCES) {
        const char *modes[]={word("Titel","Title","Titre"),word("Autor","Author","Auteur"),
            word("Zuletzt gelesen","Last read","Dernière lecture"),word("Zuletzt hinzugefügt","Recently added","Ajout récent"),
            word("Dateigröße","Size","Taille"),word("Fortschritt","Progress","Progression")};
        lumen_form(word("Bibliothek","Library","Bibliothèque"));
        lumen_form_choice("sort",word("Sortierung","Sort by","Trier par"),modes[sort<0 ? 0 : sort]);
    } else if(view==DETAIL) {
        ui_paint_heading(word("Buchdetails","Book details","Détails du livre"),0);
        cover(28,145,120,164,selected);
        lumen_book_label(176,151,396,item.title,item.author);
        char details[64];snprintf(details,sizeof(details),"%s · %.1f MB",format,kilobytes/1024.0);
        lumen_text(30,329,540,36,details,PAINT_SECONDARY,-1,1);
        lumen_button_action("read",30,385,540,48,supported ? word("Lesen","Read","Lire") :
            word("Im Kindle lesen","Read in Kindle","Lire sur Kindle"),1);
        lumen_button_action("pin",30,451,260,48,item.pin ? word("Loslösen","Unpin","Désépingler") : word("Anheften","Pin","Épingler"),0);
        lumen_button_action("rename",310,451,260,48,word("Umbenennen","Rename file","Renommer"),0);
        lumen_button_action("title",30,517,260,48,word("Titel bearbeiten","Edit title","Modifier le titre"),0);
        lumen_button_action("author",310,517,260,48,word("Autor bearbeiten","Edit author","Modifier l’auteur"),0);
        lumen_button_action("delete",30,583,260,48,word("Buch löschen","Delete book","Supprimer le livre"),0);
        if(detail_total>0) {
            snprintf(details,sizeof(details),"%d / %d",detail_page+1,detail_total);
            lumen_text(30,661,540,35,details,PAINT_SECONDARY,-1,1);
        }
    } else shelf();
    if(confirm) {
        ui_paint_surface(20,274,560,218,16,255,100);
        ui_paint_text(30,294,540,90,word("Dieses Buch dauerhaft löschen?","Permanently delete this book?",
            "Supprimer définitivement ce livre ?"),PAINT_HEADING);
        lumen_button_action("cancel",36,414,250,52,word("Abbrechen","Cancel","Annuler"),0);
        lumen_button_action("confirm",312,414,250,52,word("Löschen","Delete","Supprimer"),1);
    }
}
static void read_row(int row,int field) {
    char key[32];
    snprintf(key,sizeof(key),"id%d",field);lumen_string(key,books[row].id,sizeof(books[row].id));
    snprintf(key,sizeof(key),"title%d",field);lumen_string(key,books[row].title,sizeof(books[row].title));
    snprintf(key,sizeof(key),"author%d",field);lumen_string(key,books[row].author,sizeof(books[row].author));
    snprintf(key,sizeof(key),"pinned%d",field);books[row].pin=lumen_number(key);
    snprintf(key,sizeof(key),"progress%d",field);books[row].progress=lumen_number(key);
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN) {
        if(lumen_number("api"))query_api=!!lumen_number("catalog_query");
        if(query_api)lumen_state_register("shelf-sort",0,&sort,sizeof(sort));
        if(sort<-1 || sort>=6)sort=-1;
    }
    if(in->kind==INPUT_OPEN || in->kind==INPUT_RESUME || in->kind==INPUT_REFRESH){load();return 1;}
    if(in->kind==INPUT_INTENT) {
        char operation[24];
        if(lumen_string("operation",operation,sizeof(operation))) {
            if(!strcmp(operation,"search") && lumen_string("text",query,sizeof(query))) {
                view=SHELF;page=0;set_back();load();
            } else if(!strcmp(operation,"book-detail") && lumen_string("text",selected,sizeof(selected))) {
                view=DETAIL;set_back();load();
            }
        }
        return 1;
    }
    if(in->kind==INPUT_BACK) {
        if(confirm)confirm=0;
        else if(view!=SHELF){view=SHELF;load();}
        else if(*query){query[0]=0;page=0;load();}
        set_back();return 1;
    }
    if(in->kind==INPUT_PAGE && !confirm){page=in->x>=0 && in->x<342 ? in->x : 0;load();return 1;}
    if(in->kind==INPUT_SELECT) {
        if(in->x==2 && in->y>=0 && in->y<6) {
            if(query_api) {
                sort=in->y;
                lumen_state_queue("shelf-sort",&sort,sizeof(sort));
                page=0;load();
            } else {
                char value[2]={(char)('0'+in->y),0};lumen_service("library","sort",value,3);
            }
        }
        if(in->x==3 && in->y>=0 && in->y<3) {
            if(in->y==0)action("pin",NULL);
            if(in->y==1){view=DETAIL;load();}
            if(in->y==2)confirm=1;
            set_back();
        }
        return 1;
    }
    if(in->kind==INPUT_TEXT) {
        char value[384];
        if(lumen_string("text",value,sizeof(value))) {
            if(in->x==1){snprintf(query,sizeof(query),"%.*s",(int)sizeof(query)-1,value);page=0;set_back();load();}
            if(in->x>=4 && in->x<=6 && *value)action((const char *[]){"rename","title","author"}[in->x-4],value);
        }
        return 1;
    }
    if(in->kind==INPUT_SERVICE) {
        if(in->x==1 && in->y) {
            if(!query_api || sort<0)sort=lumen_number("sort");
            if(sort<0 || sort>=6)sort=0;
            if(query_api) {
                int recent=!!lumen_number("has_recent"),old=has_recent;
                int count=lumen_number("count");
                has_recent=recent;
                pages=recent ? 1+(count>3 ? (count-3+5)/6 : 0) : count ? (count+5)/6 : 1;
                if(page>=pages){page=pages-1;load();return 1;}
                if(old!=recent && page){load();return 1;}
                memset(books,0,sizeof(books));
                for(int i=0;i<(recent && !page ? 3 : 6);i++)read_row(i,i);
                if(recent && !page)read_row(6,16);
            } else {
                page=lumen_number("page");pages=lumen_number("pages");
                for(int i=0;i<7;i++)read_row(i,i);
            }
        } else if(in->x==2 && in->y && view==DETAIL) {
            typeof(item) original=books[0];read_row(0,0);item=books[0];books[0]=original;
            lumen_string("filename",filename,sizeof(filename));lumen_string("raw_author",raw_author,sizeof(raw_author));
            lumen_string("format",format,sizeof(format));supported=lumen_number("supported");
            detail_page=lumen_number("page");detail_total=lumen_number("total");kilobytes=lumen_number("kilobytes");
        } else if(in->x==4 && !in->y && view==SHELF){view=DETAIL;set_back();load();}
        else if(in->x==3) {
            if(!in->y)lumen_request("notify","Library",word("Aktion fehlgeschlagen.","Action failed.","Échec de l’action."),0);
            else load();
        }
        return 1;
    }
    char id[64];
    int clicked=lumen_action(in,id,"clicked");
    int held=!clicked && lumen_action(in,id,"long_pressed");
    if(!clicked && !held)return 0;
    if(confirm) {
        if(clicked && !strcmp(id,"confirm")){action("delete",NULL);view=SHELF;}
        else if(!clicked || strcmp(id,"cancel"))return 0;
        confirm=0;set_back();
        return 1;
    }
    if(view==DETAIL && clicked) {
        if(!strcmp(id,"read"))action("read",NULL);
        else if(!strcmp(id,"pin"))action("pin",NULL);
        else if(!strcmp(id,"rename"))lumen_request("input",word("Datei umbenennen","Rename file","Renommer"),filename,4);
        else if(!strcmp(id,"title"))lumen_request("input",word("Titel","Title","Titre"),item.title,5);
        else if(!strcmp(id,"author"))lumen_request("input",word("Autor","Author","Auteur"),raw_author,6);
        else if(!strcmp(id,"delete")){confirm=1;set_back();}
        return 1;
    }
    if(view==PREFERENCES && clicked && !strcmp(id,"sort")) {
        const char *modes[]={word("Titel","Title","Titre"),word("Autor","Author","Auteur"),
            word("Zuletzt gelesen","Last read","Dernière lecture"),word("Zuletzt hinzugefügt","Recently added","Ajout récent"),
            word("Dateigröße","Size","Taille"),word("Fortschritt","Progress","Progression")};
        lumen_select(modes,6,sort<0 ? 0 : sort,2);return 1;
    }
    if(view!=SHELF)return 0;
    if(clicked && !strcmp(id,"preferences")){view=PREFERENCES;set_back();return 1;}
    if(clicked && !strcmp(id,"search")) {
        lumen_request("input",word("Bücher suchen","Search books","Chercher des livres"),query,1);
        return 1;
    }
    if((!clicked && !held) || strncmp(id,"book-",5) || strlen(id)!=6 || id[5]<'0' || id[5]>'6')return 0;
    int index=id[5]-'0';
    if(!*books[index].id)return 0;
    snprintf(selected,sizeof(selected),"%s",books[index].id);
    if(held) {
        const char *modes[]={books[index].pin ? word("Loslösen","Unpin","Désépingler") :
            word("Anheften","Pin","Épingler"),word("Buchdetails","Book details","Détails"),
            word("Löschen","Delete","Supprimer")};
        lumen_select(modes,3,-1,3);
    } else action("read",NULL);
    return 1;
}
int main(void){lumen_run(draw,input,NULL);return 0;}
