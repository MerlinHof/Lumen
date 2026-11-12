#include "lumen_app.h"

static int page,count,loaded;
static char files[5][256],selected[256];

static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void list(void) {
    char index[16];snprintf(index,sizeof(index),"%d",page);
    lumen_service("study_material","list",index,1);
}
static void draw(void) {
    ui_paint_heading(word("Lernen","Study","Étude"),1);
    ui_paint_icon_button(528,74,IC_CLOCK);
    lumen_wrapped(30,140,540,94,word(
        "Lies dein Material, schließe es und erkläre aus dem Gedächtnis, was du verstanden hast.",
        "Read the material, close it and explain what you understood from memory.",
        "Lisez le document, fermez-le et expliquez de mémoire ce que vous avez compris."),PAINT_BODY,3);
    lumen_button(28,255,260,48,word("25 min Fokus","25 min focus","25 min de concentration"),0);
    lumen_button(312,255,260,48,word("Abrufübung","Recall exercise","Rappel actif"),0);
    if(loaded && !count)lumen_wrapped(30,375,540,130,word(
        "Noch kein Material. Lade Dateien über Dateiübertragung hoch.",
        "No material yet. Upload documents with File Transfer.",
        "Aucun document. Ajoutez vos fichiers avec Transfert de fichiers."),PAINT_HEADING,4);
    for(int i=0;i<5 && page*5+i<count;i++) {
        int y=332+i*67;
        ui_paint_hitbox(28,y,544,62);
        ui_paint_icon_button(30,y+8,IC_BOOK);
        lumen_text(90,y+10,470,40,files[i],PAINT_BODY,-1,1);
        ui_paint_line(28,y+61,572,y+61,1,190);
    }
    lumen_pager(745,page,(count+4)/5);
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN || in->kind==INPUT_RESUME || in->kind==INPUT_REFRESH){list();return 1;}
    if(in->kind==INPUT_PAGE){page=in->x;list();return 1;}
    if(in->kind==INPUT_SERVICE) {
        if(in->x==1 && in->y) {
            count=lumen_number("count");page=lumen_number("page");loaded=1;
            for(int i=0;i<5;i++) {
                char key[16];snprintf(key,sizeof(key),"name%d",i);lumen_string(key,files[i],sizeof(files[i]));
            }
        } else if(in->x==2 && in->y) {
            lumen_request("notify","",word("Fokuszeit gestartet.","Focus timer started.","Minuteur démarré."),0);
        } else if(in->x==3) {
            lumen_request("notify","",in->y ? word("In Notizen → Learning gespeichert.","Saved in Notes → Learning.","Enregistré dans Notes → Learning.") :
                word("Speichern fehlgeschlagen.","Could not save.","Enregistrement impossible."),0);
        }
        return 1;
    }
    if(in->kind==INPUT_TEXT && in->x==4) {
        static char text[7850],note[8192];
        if(lumen_string("text",text,sizeof(text)) && *text) {
            char *newline=strpbrk(selected,"\r\n");if(newline)*newline=' ';
            snprintf(note,sizeof(note),"%s\n%s",selected,text);
            lumen_service("notes","create_learning",note,3);
        }
        return 1;
    }
    if(in->kind!=INPUT_TAP)return 0;
    int x=in->x,y=in->y;
    if(y>=74 && y<118 && x>=528 && x<572)lumen_open("lumen.clock");
    else if(y>=255 && y<303) {
        if(x>=28 && x<288)lumen_service("clock","start_timer","25",2);
        else if(x>=312 && x<572)lumen_multiline(word("Was weißt du noch?","What do you remember?","De quoi vous souvenez-vous ?"),"",4);
    } else if(x>=28 && x<572 && y>=332 && y<667) {
        int i=(y-332)/67;
        if(page*5+i<count && *files[i]) {
            snprintf(selected,sizeof(selected),"%s",files[i]);
            lumen_storage_write("material.txt",selected,strlen(selected));
            lumen_service("study_material","open",selected,0);
        }
    }
    return 1;
}
int main(void) {
    size_t n=0;
    if(lumen_storage_read("material.txt",selected,sizeof(selected)-1,&n))selected[n]=0;
    lumen_run(draw,input,NULL);
    return 0;
}
