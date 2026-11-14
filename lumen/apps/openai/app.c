#define _GNU_SOURCE
#include "lumen_app.h"
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

static const char *data_root,*legacy_root;
static char llm_prompt16[8192],model[160]="gpt-4.1-mini";
static void chat_json(FILE *f,const char *text) {
    fputc('"',f);
    for(const unsigned char *p=(const unsigned char *)text;*p;p++) {
        if(*p=='"' || *p=='\\')fputc('\\',f);
        if(*p<32)fprintf(f,"\\u%04x",*p);
        else fputc(*p,f);
    }
    fputc('"',f);
}
#include "conversation.h"
#include "provider.h"

enum {CHAT,ARCHIVE,SETUP,MODELS};
static int view,busy,key_set,models_busy,model_page,model_count;
static unsigned long long pending;
static char models[24][160];
static int message_rows[6],message_y[6],message_clipped[6],row_count,pages;
static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void state(void) {
    char *key=provider_key();
    key_set=key!=NULL;
    if(key){memset(key,0,strlen(key));free(key);}
    if(!lumen_storage_exists("model.txt")) {
        char *previous=provider_read("llm-model.txt",sizeof(model)-1,1);
        if(previous && *previous) {
            snprintf(model,sizeof(model),"%s",previous);
            lumen_storage_write("model.txt",model,strlen(model));
        }
        free(previous);
    }
}
static void request_models(void) {
    char *response=provider_read("models.json",PROVIDER_LIMIT,0);
    if(!response)response=provider_read("llm-models17.json",PROVIDER_LIMIT,1);
    char (*ids)[160]=calloc(512,sizeof(*ids));
    model_count=response && ids ? parse_models(response,ids) : 0;
    if(model_page<0 || model_page*24>=model_count)model_page=0;
    memset(models,0,sizeof(models));
    for(int i=0;i<24 && model_page*24+i<model_count;i++)
        memcpy(models[i],ids[model_page*24+i],sizeof(models[i]));
    free(response);free(ids);
}
static void provider_error(void) {
    lumen_request("notify","OpenAI",word("Anfrage fehlgeschlagen. Schlüssel, Modell und Verbindung prüfen.",
        "Request failed. Check the key, model and connection.",
        "La requête a échoué. Vérifiez la clé, le modèle et la connexion."),0);
}
static void view_set(int next){view=next;lumen_back=view!=CHAT;}
static void new_chat(void){chat_clear();llm_prompt16[0]=0;view_set(CHAT);}
static void restore_reply(void) {
    if(!pending)return;
    char file[64],*text=malloc(131073);
    if(!text)return;
    snprintf(file,sizeof(file),"reply-%016llx.txt",pending);
    size_t n;
    if(lumen_storage_read(file,text,131072,&n)) {
        text[n]=0;
        if(n && chat_id==pending && chat_append(1,text)) {
            llm_prompt16[0]=0;chat_page=1000;
        }
        lumen_storage_delete(file);lumen_storage_delete("pending.bin");
        pending=0;busy=0;
    }
    free(text);
}
static void send(void) {
    if(busy || !*llm_prompt16)return;
    if(!key_set){view_set(SETUP);state();return;}
    if((!chat_count || chat_messages[chat_count-1].role ||
        strcmp(chat_messages[chat_count-1].text,llm_prompt16)) && !chat_append(0,llm_prompt16))return;
    char *bytes=NULL;size_t size=0;FILE *file=open_memstream(&bytes,&size);
    if(!file)return;
    chat_write_request(file,model);fclose(file);
    if(size<=1024*1024 && lumen_storage_write("request.json",bytes,size)) {
        pending=chat_id;
        if(app_state_save("pending.bin",&pending,sizeof(pending))) {
            char id[24];snprintf(id,sizeof(id),"%016llx",pending);
            busy=1;chat_page=1000;lumen_app_service_background("lumen.openai","send",id,3);
        }
    } else lumen_request("notify","OpenAI",word("Chat ist zu lang.","This chat is too long.","Cette conversation est trop longue."),0);
    free(bytes);
}
static int estimate_lines(const char *text,int limit,int *clipped) {
    int lines=1,columns=0;
    for(;*text;text++) {
        if(((unsigned char)*text&192)==128)continue;
        if(*text=='\n' || ++columns>35) {
            if(lines==limit){*clipped=1;break;}
            columns=0;lines++;
        }
    }
    return lines;
}
static void layout(void) {
    int all_pages=1,y=153;
    int index[CHAT_LIMIT],ys[CHAT_LIMIT],group[CHAT_LIMIT],clipped[CHAT_LIMIT];
    for(int i=0;i<chat_count;i++) {
        clipped[i]=0;
        int lines=estimate_lines(chat_messages[i].text,chat_messages[i].role ? 5 : 3,&clipped[i]);
        int height=44+lines*26;
        if(y+height>676){all_pages++;y=153;}
        index[i]=i;ys[i]=y;group[i]=all_pages-1;y+=height+10;
    }
    pages=all_pages;
    if(chat_page>=pages)chat_page=pages-1;
    row_count=0;
    for(int i=0;i<chat_count && row_count<6;i++)if(group[i]==chat_page) {
        message_rows[row_count]=index[i];message_y[row_count]=ys[i];
        message_clipped[row_count++]=clipped[i];
    }
}
static void draw(void) {
    ui_paint_heading(view==ARCHIVE ? "Chats" : "OpenAI",3);
    if(view==SETUP || view==MODELS) {
        if(view==SETUP) {
            lumen_icon_action("refresh-models",528,74,IC_RESTART);
            lumen_custom_action("models",30,155,540,48);
            lumen_select_row(155,word("Modell","Model","Modèle"),model);
            lumen_button_action("key",28,282,544,54,key_set ? word("API-Schlüssel ändern","Change API key","Modifier la clé API") : "API key",0);
            lumen_wrapped(30,380,540,130,word("API-Schlüssel und Modell für dieses Konto wählen.",
                "Set the API key and choose a model for this account.","Définissez la clé API et choisissez un modèle."),PAINT_BODY,4);
            if(models_busy)lumen_text(30,522,540,45,word("Modelle werden geladen …","Loading models…","Chargement…"),PAINT_SECONDARY,-1,1);
        } else {
            int current=model_count-model_page*24;if(current>24)current=24;
            for(int i=0;i<current;i++) {
                int x=28+i%2*274,y=139+i/2*48;
                char action[24];snprintf(action,sizeof(action),"model-%d",i);
                lumen_button_action(action,x,y,266,43,models[i],!strcmp(model,models[i]));
            }
            lumen_pager(745,model_page,(model_count+23)/24);
        }
        return;
    }
    if(view==ARCHIVE) {
        lumen_icon_action("new",528,74,IC_PLUS);
        for(int i=0;i<7 && chat_archive_page*7+i<chat_archive_count;i++) {
            int index=chat_archive_page*7+i,y=149+i*76;
            char action[24];snprintf(action,sizeof(action),"chat-%d",i);
            lumen_custom_action(action,28,y,544,76);
            lumen_text(30,y+7,540,34,chat_archive[index].title,PAINT_BODY,0,1);
            char date[50];struct tm local;localtime_r(&chat_archive[index].modified,&local);
            strftime(date,sizeof(date),"%d.%m.%Y · %H:%M",&local);
            lumen_text(30,y+42,540,23,date,PAINT_SECONDARY,-1,1);
            ui_paint_line(28,y+75,572,y+75,1,191);
        }
        lumen_pager(745,chat_archive_page,(chat_archive_count+6)/7);
        return;
    }
    lumen_icon_action("new",408,74,IC_PLUS);
    lumen_icon_action("archive",468,74,IC_CHAT);
    lumen_icon_action("settings",528,74,IC_SETTINGS);
    if(!chat_count)lumen_wrapped(44,315,512,148,word("Was möchtest du verstehen?",
        "What would you like to understand?","Que souhaitez-vous comprendre ?"),PAINT_TITLE,4);
    layout();
    for(int r=0;r<row_count;r++) {
        int i=message_rows[r],y=message_y[r],bot=chat_messages[i].role,x=bot ? 28 : 92;
        int clipped=0,lines=estimate_lines(chat_messages[i].text,bot ? 5 : 3,&clipped);
        ui_paint_surface(x,y,480,44+lines*26,16,255,bot ? 160 : 112);
        lumen_text(x+13,y+6,409,26,bot ? "OpenAI" : word("Du","You","Vous"),PAINT_SECONDARY,-1,1);
        char preview[512];const char *p=chat_messages[i].text;
        size_t count=strlen(p);if(count>460){count=460;clipped=1;}
        while(count && ((unsigned char)p[count]&192)==128)count--;
        snprintf(preview,sizeof(preview),"%.*s%s",(int)count,p,clipped ? "…" : "");
        lumen_wrapped(x+13,y+34,454,lines*26,preview,PAINT_BODY,lines);
        if(clipped || message_clipped[r]) {
            char action[24];snprintf(action,sizeof(action),"read-%d",r);
            lumen_icon_action(action,x+426,y,IC_BOOK);
        }
    }
    lumen_pager(686,chat_page,pages);
    lumen_button_action("prompt",28,738,480,50,*llm_prompt16 ? llm_prompt16 :
        busy ? word("Antwort wird geschrieben …","Writing reply…","Réponse en cours…") :
        word("Nachricht schreiben …","Write a message…","Écrire un message…"),0);
    lumen_icon_action("send",525,741,IC_SEND);
}
static int action_index(const char *action,const char *prefix,int limit) {
    size_t length=strlen(prefix);
    if(strncmp(action,prefix,length) || action[length]<'0' || action[length]>'9')return -1;
    char *end;
    long index=strtol(action+length,&end,10);
    return !*end && index<limit ? (int)index : -1;
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN) {
        data_root=getenv("LUMEN_APP_DATA");if(!data_root)data_root=".";
        legacy_root=getenv("LUMEN_LEGACY_DATA");
        size_t n;char stored[160];
        if(lumen_storage_read("model.txt",stored,sizeof(stored)-1,&n)){stored[n]=0;snprintf(model,sizeof(model),"%s",stored);}
        if(app_state_load("pending.bin",&pending,sizeof(pending)) && pending)chat_load(pending);
    }
    if(in->kind==INPUT_OPEN || in->kind==INPUT_RESUME) {
        int action=lumen_number("background_action");
        busy=pending && action==3;
        models_busy=action==5;
        restore_reply();
        if(pending && !busy){pending=0;lumen_storage_delete("pending.bin");}
    }
    if(in->kind==INPUT_OPEN || in->kind==INPUT_RESUME || in->kind==INPUT_REFRESH) {
        restore_reply();if(view==MODELS)request_models();else state();return 1;
    }
    if(in->kind==INPUT_BACK) {view_set(view==MODELS ? SETUP : CHAT);return 1;}
    if(in->kind==INPUT_PAGE) {
        if(view==ARCHIVE)chat_archive_page=in->x;
        else if(view==MODELS){model_page=in->x;request_models();}
        else chat_page=in->x;
        return 1;
    }
    if(in->kind==INPUT_TEXT) {
        if(in->x==1)lumen_string("text",llm_prompt16,sizeof(llm_prompt16));
        else if(in->x==2) {
            char key[2049];
            if(wire_bounded_string("text",key,sizeof(key)) && provider_key_valid(key) &&
                provider_save_key(key))state();
            else provider_error();
            memset(key,0,sizeof(key));
        }
        return 1;
    }
    if(in->kind==INPUT_SERVICE) {
        if(in->x==3) {
            if(in->y)restore_reply();
            if(pending || !in->y) {
                busy=0;pending=0;lumen_storage_delete("pending.bin");provider_error();
            }
        } else if(in->x==5) {
            models_busy=0;
            if(in->y)request_models();
            else provider_error();
        }
        return 1;
    }
    char id[32],gesture[16];
    if(in->kind!=INPUT_COMPONENT || !lumen_string("id",id,sizeof(id)) ||
       !lumen_string("gesture",gesture,sizeof(gesture)) || strcmp(gesture,"clicked"))return 0;
    if(view==SETUP) {
        if(!strcmp(id,"refresh-models") && !models_busy && !busy) {
            models_busy=1;lumen_app_service_background("lumen.openai","models_refresh","",5);
        } else if(!strcmp(id,"models")) {
            view_set(MODELS);model_page=0;request_models();
        } else if(!strcmp(id,"key"))lumen_request("secret","API key","",2);
    } else if(view==MODELS) {
        int i=action_index(id,"model-",24);
        if(i>=0 && model_page*24+i<model_count && *models[i]) {
            snprintf(model,sizeof(model),"%s",models[i]);
            lumen_storage_write("model.txt",model,strlen(model));
            view_set(SETUP);
        }
    } else if(view==ARCHIVE && !busy) {
        if(!strcmp(id,"new"))new_chat();
        else {
            int i=action_index(id,"chat-",7);
            if(i>=0 && chat_archive_page*7+i<chat_archive_count &&
               chat_load(chat_archive[chat_archive_page*7+i].id))view_set(CHAT);
        }
    } else if(view==CHAT) {
        if(!strcmp(id,"settings")){view_set(SETUP);state();}
        else if(!strcmp(id,"archive") && !busy){chat_scan();chat_archive_page=0;view_set(ARCHIVE);}
        else if(!strcmp(id,"new") && !busy)new_chat();
        else if(!strcmp(id,"send"))send();
        else if(!strcmp(id,"prompt"))lumen_multiline(word("Nachricht","Message","Message"),llm_prompt16,1);
        else {
            int r=action_index(id,"read-",row_count);
            if(r<0)return 0;
            int i=message_rows[r];
            if((message_clipped[r] || strlen(chat_messages[i].text)>460) &&
               lumen_storage_write("reader.md",chat_messages[i].text,strlen(chat_messages[i].text)))
                lumen_reader_file(chat_title,"reader.md");
        }
    }
    return 1;
}
int main(void){lumen_service_handler=provider_serve;lumen_run(draw,input,NULL);chat_clear();return 0;}
