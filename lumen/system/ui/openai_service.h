static int package_openai(const char *operation,const char *data,int action) {
    if(app_for_route(ui_route())!=APP_LLM)return 0;
    if(!strcmp(operation,"key")) {
        int ok=*data && strlen(data)<=2048 && !strpbrk(data,"\r\n\"\\ ");
        if(ok)extra_write_text("llm-key.txt",data);
        return package_acknowledge(action,ok);
    }
    if(!strcmp(operation,"models_refresh"))return package_acknowledge(action,job_start16(3,""));
    if(!strcmp(operation,"send")) {
        if(strlen(data)!=16 || strspn(data,"0123456789abcdef")!=16 || job_pid16>0)
            return package_acknowledge(action,0);
        char key[2049]="";config16("llm-key.txt",key,sizeof(key));int valid=!!*key;
        memset(key,0,sizeof(key));
        if(!valid)return package_acknowledge(action,0);
        snprintf(llm_pending_id,sizeof(llm_pending_id),"%s",data);
        int ok=job_start16(2,"");
        if(!ok)llm_pending_id[0]=0;
        return package_acknowledge(action,ok);
    }
    if(strcmp(operation,"state") && strcmp(operation,"models"))return 0;
    char *message=NULL;size_t length=0;
    FILE *out=package_reply(&message,&length,action,1);
    if(!out)return 0;
    char key[2049]="",model[160]="";
    config16("llm-key.txt",key,sizeof(key));config16("llm-model.txt",model,sizeof(model));
    package_field(out,"legacy_model",model);
    fprintf(out,",\"key_set\":%d,\"pending\":%d,\"models_busy\":%d",!!*key,
        job_pid16>0 && job_kind16==2,job_pid16>0 && job_kind16==3);
    memset(key,0,sizeof(key));
    if(!strcmp(operation,"models")) {
        models_parse17();
        int page=atoi(data);
        if(page<0 || page>32 || page*24>=model_count17)page=0;
        fprintf(out,",\"page\":%d,\"count\":%d",page,model_count17);
        for(int i=0;i<24 && page*24+i<model_count17;i++) {
            char field[24];snprintf(field,sizeof(field),"model%d",i);
            package_field(out,field,model_ids17[page*24+i]);
        }
    }
    package_reply_end(out,&message);
    return 1;
}
