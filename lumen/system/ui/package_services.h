/* Services are the host-side data and device boundary for app executables. */
static FILE *package_reply(char **message,size_t *length,int action,int success) {
    FILE *stream=open_memstream(message,length);
    if(stream)fprintf(stream,"{\"event\":\"service\",\"action\":%d,\"ok\":%d",action,success);
    return stream;
}
static void package_reply_end(FILE *stream,char **message) {
    fputc('}',stream);
    if(!fclose(stream))app_process_send(*message);
    free(*message);
}
static int package_acknowledge(int action,int ok) {
    char *message=NULL;size_t length=0;
    FILE *out=package_reply(&message,&length,action,ok);
    if(!out)return 0;
    package_reply_end(out,&message);
    return ok;
}
static void package_field(FILE *stream,const char *key,const char *value) {
    fprintf(stream,",\"%s\":",key);
    json_escape16(stream,value);
}
static int catalog_service(const char *operation,const char *data,int action) {
    if(strcmp(operation,"list")) {
        int id=app_package_find(data);
        if(id>=0 && !strcmp(operation,"hide")){
            launcher_visible[id]=0;v1_dirty=1;v1_save();
        } else if(id>=0 && !strcmp(operation,"remove")){
            if(!app_package_remove(data))ui_message("Löschen fehlgeschlagen.");
        } else return 0;
    }
    int ids[APP_LIMIT],count=v1_visible(ids);
    char list[APP_LIMIT*65],*message=NULL;size_t used=0,length=0;
    list[0]=0;
    for(int i=0;i<count;i++) {
        int n=snprintf(list+used,sizeof(list)-used,"%s\n",app_package_key(ids[i]));
        if(n<0 || (size_t)n>=sizeof(list)-used)break;
        used+=n;
    }
    FILE *stream=package_reply(&message,&length,action,1);
    if(!stream)return 0;
    package_field(stream,"data",list);
    package_reply_end(stream,&message);
    return 1;
}
static int transfer_service(const char *operation,const char *data,int action) {
    int ok=1;
    if(!strcmp(operation,"start"))ok=connect_start(options[OPT_PORT],options[OPT_TIMEOUT]);
    else if(!strcmp(operation,"stop")){
        connect_stop();
        if(library_ui.library_dirty)refresh_library();
    } else if(!strcmp(operation,"port") || !strcmp(operation,"timeout")) {
        char *end;long value=strtol(data,&end,10);
        int port=!strcmp(operation,"port");
        if(!*data || *end || value<(port ? 1024 : 1) || value>(port ? 65535 : 60))ok=0;
        else {options[port ? OPT_PORT : OPT_TIMEOUT]=(int)value;library_options_save();}
    } else if(strcmp(operation,"status"))return 0;
    char *message=NULL;size_t length=0;
    FILE *stream=package_reply(&message,&length,action,ok);
    if(!stream)return 0;
    int running=connect_running();
    char seen[2048];snprintf(seen,sizeof(seen),"%s/connect-peer16.txt",data_root);
    fprintf(stream,",\"running\":%d,\"paired\":%d,\"port\":%d,\"timeout\":%d",running,
        running && !access(seen,F_OK),options[OPT_PORT],options[OPT_TIMEOUT]);
    package_field(stream,"address",running ? connect_address() : "");
    package_field(stream,"code",running ? connect_code() : "");
    package_field(stream,"error",ok ? "" : !strcmp(operation,"start") ? tr(connect_error()) : tr("Eingabe prüfen."));
    package_reply_end(stream,&message);
    return 1;
}
#include "annotation_service.h"
#include "reading_service.h"
#include "study_services.h"
#include "clock_service.h"
#include "recipe_service.h"
#include "wikipedia_service.h"
#include "news_service.h"
#include "catalog_service.h"
#include "library_service.h"
#include "home_assistant_service.h"
#include "openai_service.h"
#include "photo_service.h"
#include "document_service.h"
#include "notes_service.h"
#include "settings_service.h"
static int package_service_request(const char *name,const char *operation,const char *data,int action) {
    if(!strcmp(name,"reading"))return package_reading_activity(operation,data,action);
    if(!strcmp(name,"study_material"))return package_study_material(operation,data,action);
    if(!strcmp(name,"notes") && !strcmp(operation,"create_learning"))return package_learning_note(data,action);
    if(!strcmp(name,"clock")) {
        if(!strcmp(operation,"start_timer"))return package_start_timer(data,action);
        return package_clock_service(operation,data,action);
    }
    if(!strcmp(name,"settings"))return package_settings(operation,data,action);
    if(!strcmp(name,"notes_editor"))return package_notes(operation,data,action);
    if(!strcmp(name,"document"))return package_document(operation,data,action);
    if(!strcmp(name,"photos"))return package_photos(operation,data,action);
    if(!strcmp(name,"openai"))return package_openai(operation,data,action);
    if(!strcmp(name,"home_assistant"))return package_home_assistant(operation,data,action);
    if(!strcmp(name,"library"))return package_library(operation,data,action);
    if(!strcmp(name,"store"))return package_store(operation,data,action);
    if(!strcmp(name,"news"))return package_news(operation,data,action);
    if(!strcmp(name,"wikipedia"))return package_wikipedia(operation,data,action);
    if(!strcmp(name,"recipes"))return package_recipes(operation,data,action);
    if(!strcmp(name,"transfer"))return transfer_service(operation,data,action);
    if(!strcmp(name,"dictionary") && !strcmp(operation,"game_word")) {
        char *end;
        unsigned long seed=strtoul(data,&end,10);
        if(!*data || *end)return 0;
        char word[64],*message=NULL;size_t length=0;
        dictionary_game_word(options[OPT_LANGUAGE],seed,word);
        FILE *out=package_reply(&message,&length,action,1);
        if(!out)return 0;
        package_field(out,"word",word);
        package_reply_end(out,&message);
        return 1;
    }
    if(!strcmp(name,"dictionary") && !strcmp(operation,"clues")) {
        char *message=NULL;size_t length=0;
        FILE *out=package_reply(&message,&length,action,1);
        if(!out)return 0;
        for(int i=0;i<5 && *data;i++) {
            size_t n=strcspn(data," ");
            if(!n || n>=24 || strspn(data,"ABCDEFGHIJKLMNOPQRSTUVWXYZ")!=n)break;
            char term[24],key[16];
            snprintf(term,sizeof(term),"%.*s",(int)n,data);
            int language=-1;
            const char *definition=dictionary_lookup(term,1,&language);
            if(definition && language==options[OPT_LANGUAGE]) {
                snprintf(key,sizeof(key),"clue%d",i);
                package_field(out,key,definition);
            }
            data+=n+(data[n]==' ');
        }
        package_reply_end(out,&message);
        return 1;
    }
    if(!strcmp(name,"annotations"))return annotation_service(operation,data,action);
    if(!strcmp(name,"catalog"))return catalog_service(operation,data,action);
    return 0;
}
