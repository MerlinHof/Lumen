#include "lumen_app.h"
#include <dirent.h>
#include <strings.h>

static int page,count,loaded;
static char files[5][256],selected[256];

static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static int material_compare(const void *a,const void *b) {
    return strcasecmp(a,b);
}
static void list(void) {
    const char *legacy=getenv("LUMEN_LEGACY_DATA");
    char path[4096];
    int n=legacy ? snprintf(path,sizeof(path),"%s/study",legacy) : -1;
    int fd=n>0 && (size_t)n<sizeof(path) ? open(path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC) : -1;
    DIR *folder=fd>=0 ? fdopendir(fd) : NULL;
    if(fd>=0 && !folder)close(fd);
    count=0;
    struct dirent *entry;
    static char names[512][256];
    while(folder && count<512 && (entry=readdir(folder))) {
        struct stat info;
        if(entry->d_name[0]=='.' || strchr(entry->d_name,'\\') ||
           fstatat(fd,entry->d_name,&info,AT_SYMLINK_NOFOLLOW) || !S_ISREG(info.st_mode))continue;
        snprintf(names[count++],sizeof(names[0]),"%s",entry->d_name);
    }
    if(folder)closedir(folder);
    qsort(names,(size_t)count,sizeof(names[0]),material_compare);
    if(page*5>=count)page=count ? (count-1)/5 : 0;
    for(int i=0;i<5 && page*5+i<count;i++)strcpy(files[i],names[page*5+i]);
    loaded=1;
}
static void serve(const char *operation,const char *data) {
    if(strcmp(operation,"material_count") || *data){lumen_app_service_result(0,"");return;}
    list();
    char result[16];snprintf(result,sizeof(result),"%d",count);
    lumen_app_service_result(1,result);
}
static void draw(void) {
    ui_paint_heading(word("Lernen","Study","Étude"),1);
    lumen_icon_action("clock",528,74,IC_CLOCK);
    lumen_wrapped(30,140,540,94,word(
        "Lies dein Material, schließe es und erkläre aus dem Gedächtnis, was du verstanden hast.",
        "Read the material, close it and explain what you understood from memory.",
        "Lisez le document, fermez-le et expliquez de mémoire ce que vous avez compris."),PAINT_BODY,3);
    lumen_button_action("focus",28,255,260,48,word("25 min Fokus","25 min focus","25 min de concentration"),0);
    lumen_button_action("recall",312,255,260,48,word("Abrufübung","Recall exercise","Rappel actif"),0);
    if(loaded && !count)lumen_wrapped(30,375,540,130,word(
        "Noch kein Material. Lade Dateien über Dateiübertragung hoch.",
        "No material yet. Upload documents with File Transfer.",
        "Aucun document. Ajoutez vos fichiers avec Transfert de fichiers."),PAINT_HEADING,4);
    for(int i=0;i<5 && page*5+i<count;i++) {
        int y=332+i*67;
        char id[16];snprintf(id,sizeof(id),"material-%d",i);
        lumen_list_action(id,y,62,files[i],"",IC_BOOK);
    }
    lumen_pager(745,page,(count+4)/5);
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN || in->kind==INPUT_RESUME || in->kind==INPUT_REFRESH){list();return 1;}
    if(in->kind==INPUT_PAGE){page=in->x;list();return 1;}
    if(in->kind==INPUT_SERVICE) {
        if(in->x==2 && in->y) {
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
            char title[sizeof(selected)];
            snprintf(title,sizeof(title),"%s",selected);
            for(char *line=title; *line; line++)if(*line=='\r' || *line=='\n')*line=' ';
            snprintf(note,sizeof(note),"%s\n%s",title,text);
            lumen_app_service("lumen.notes","create_learning",note,3);
        }
        return 1;
    }
    if(in->kind!=INPUT_COMPONENT)return 0;
    char id[32],gesture[32];
    if(!lumen_string("id",id,sizeof(id)) || !lumen_string("gesture",gesture,sizeof(gesture)) ||
       strcmp(gesture,"clicked"))return 0;
    if(!strcmp(id,"clock"))lumen_open("lumen.clock");
    else if(!strcmp(id,"focus"))lumen_service("clock","start_timer","25",2);
    else if(!strcmp(id,"recall"))
        lumen_multiline(word("Was weißt du noch?","What do you remember?","De quoi vous souvenez-vous ?"),"",4);
    else if(strlen(id)==10 && !strncmp(id,"material-",9) && id[9]>='0' && id[9]<'5') {
        int i=id[9]-'0';
        if(page*5+i<count && *files[i]) {
            snprintf(selected,sizeof(selected),"%s",files[i]);
            lumen_storage_write("material.txt",selected,strlen(selected));
            lumen_reader_document(selected,"study");
        }
    }
    return 1;
}
int main(void) {
    size_t n=0;
    if(lumen_storage_read("material.txt",selected,sizeof(selected)-1,&n))selected[n]=0;
    lumen_service_handler=serve;
    lumen_run(draw,input,NULL);
    return 0;
}
