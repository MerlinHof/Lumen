#include "lumen_app.h"

enum { PER_PAGE=4, FIRST=143, ROW_HEIGHT=139 };
static int page,count,loaded;
static char quotes[PER_PAGE][2048],books[PER_PAGE][256],references[PER_PAGE][32];
static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void fetch(void) {
    char number[16];snprintf(number,sizeof(number),"%d",page);
    lumen_service("annotations","list",number,1);
}
static void draw(void) {
    ui_paint_heading(word("Zitate","Quotes","Citations"),0);
    if(!loaded)return;
    if(!count) {
        lumen_wrapped(30,170,540,120,word("Alle markierten Zitate deiner Bücher erscheinen hier.",
            "Highlighted passages from your books appear here.",
            "Les passages surlignés de vos livres apparaissent ici."),PAINT_TITLE,4);
        return;
    }
    for(int i=0;i<PER_PAGE && page*PER_PAGE+i<count;i++) {
        int y=FIRST+i*ROW_HEIGHT;
        char id[12];snprintf(id,sizeof(id),"quote-%d",i);
        lumen_custom_action(id,28,y,544,ROW_HEIGHT-10);
        lumen_wrapped(34,y,532,95,quotes[i],PAINT_BODY,3);
        lumen_text(34,y+94,532,29,books[i],PAINT_SECONDARY,95,1);
        ui_paint_line(28,y+ROW_HEIGHT-6,572,y+ROW_HEIGHT-6,1,190);
    }
    lumen_pager(745,page,(count+PER_PAGE-1)/PER_PAGE);
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN || in->kind==INPUT_RESUME || in->kind==INPUT_REFRESH)fetch();
    else if(in->kind==INPUT_PAGE){page=in->x;fetch();}
    else if(in->kind==INPUT_SERVICE && in->x==1 && in->y) {
        count=lumen_number("count");page=lumen_number("page");loaded=1;
        for(int i=0;i<PER_PAGE;i++) {
            char key[16];
            snprintf(key,sizeof(key),"quote%d",i);lumen_string(key,quotes[i],sizeof(quotes[i]));
            snprintf(key,sizeof(key),"book%d",i);lumen_string(key,books[i],sizeof(books[i]));
            snprintf(key,sizeof(key),"ref%d",i);lumen_string(key,references[i],sizeof(references[i]));
        }
    } else if(in->kind==INPUT_COMPONENT) {
        char id[16],gesture[16];
        if(!lumen_string("id",id,sizeof(id)) || !lumen_string("gesture",gesture,sizeof(gesture)) ||
           strcmp(gesture,"clicked") || strlen(id)!=7 || strncmp(id,"quote-",6) ||
           id[6]<'0' || id[6]>='0'+PER_PAGE)return 0;
        int row=id[6]-'0';
        int index=page*PER_PAGE+row;
        if(loaded && index<count && *references[row])
            lumen_service("annotations","open",references[row],2);
    }
    return 1;
}
int main(void) {
    lumen_run(draw,input,NULL);
    return 0;
}
