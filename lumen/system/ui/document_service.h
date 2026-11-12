/* Session-scoped document engine. The Reader app owns navigation and controls. */
static int reader_value(int id) {
    switch(id) {
    case 0:return options[OPT_FONT];
    case 1:return settings.size;
    case 2:return settings.leading;
    case 3:return settings.margin;
    case 4:return options[OPT_ALIGN];
    case 5:return options[OPT_PARAGRAPH];
    case 6:return options[OPT_LETTERSPACE];
    case 7:return options[OPT_SWIPE];
    case 8:return options[OPT_TAP];
    case 9:return options[OPT_PROGRESS] ? reader_ui.reader_progress_mode : 5;
    default:return 0;
    }
}
static int reader_set_value(int id,int value) {
    static const int low[]={0,16,110,0,0,0,0,0,0,0};
    static const int high[]={4,34,190,30,1,24,20,1,1,6};
    if(id<0 || id>=10 || value<low[id] || value>high[id])return 0;
    switch(id) {
    case 0:options[OPT_FONT]=value;break;
    case 1:settings.size=value;break;
    case 2:settings.leading=value;break;
    case 3:settings.margin=value;break;
    case 4:options[OPT_ALIGN]=value;break;
    case 5:options[OPT_PARAGRAPH]=value;break;
    case 6:options[OPT_LETTERSPACE]=value;break;
    case 7:options[OPT_SWIPE]=value;break;
    case 8:options[OPT_TAP]=value;break;
    case 9:{
        char setting[8];snprintf(setting,sizeof(setting),"%d",value);
        reader_ui.reader_progress_mode=value;
        options[OPT_PROGRESS]=value!=5;
        extra_write_text("reader-progress15.txt",setting);
        break;
    }}
    if(id<=6)apply_reading_settings();
    else library_options_save();
    return 1;
}
static int package_document(const char *operation,const char *data,int action) {
    if(app_for_route(ui_route())!=APP_READER)return 0;
    if(!strcmp(operation,"setting")) {
        int field,value;char tail;
        if(sscanf(data,"%d %d %c",&field,&value,&tail)!=2 || !reader_set_value(field,value))
            return package_acknowledge(action,0);
    } else if(!strcmp(operation,"resume_book")) {
        int last=featured();
        if(last<0)return package_acknowledge(action,0);
        return ui_open(last) || package_acknowledge(action,0);
    } else if(strcmp(operation,"state") && strcmp(operation,"contents")) {
        if(selected<0 || current<0 || !engine_count())return package_acknowledge(action,0);
        if(!strcmp(operation,"bookmark"))toggle_bookmark();
        else {
            char *end;long value=strtol(data,&end,10);
            if(!*data || *end)return package_acknowledge(action,0);
            if(!strcmp(operation,"turn") && (value==-1 || value==1))turn((int)value);
            else if(!strcmp(operation,"goto") && value>=0 && value<engine_count())turn((int)value-page);
            else if(!strcmp(operation,"annotation") && value>=0 && value<reader_ui.annotation_count)
                turn(engine_restore(&reader_ui.annotations[value].position)-page);
            else if(!strcmp(operation,"remove_annotation") && value>=0 && value<reader_ui.annotation_count) {
                int i=(int)value;Annotation saved=reader_ui.annotations[i];
                memmove(reader_ui.annotations+i,reader_ui.annotations+i+1,
                    (size_t)(reader_ui.annotation_count-i-1)*sizeof(saved));
                reader_ui.annotation_count--;
                if(!annotations_write()) {
                    memmove(reader_ui.annotations+i+1,reader_ui.annotations+i,
                        (size_t)(reader_ui.annotation_count-i)*sizeof(saved));
                    reader_ui.annotations[i]=saved;reader_ui.annotation_count++;
                    return package_acknowledge(action,0);
                }
            } else return package_acknowledge(action,0);
        }
    }
    char *message=NULL;size_t length=0;
    FILE *out=package_reply(&message,&length,action,1);
    if(!out)return 0;
    int total=selected>=0 && current>=0 ? engine_count() : 0;
    package_field(out,"title",total>0 ? library[selected].title : "");
    fprintf(out,",\"page\":%d,\"total\":%d",page,total);
    for(int i=0;i<10;i++)fprintf(out,",\"setting%d\":%d",i,reader_value(i));
    if(!strcmp(operation,"contents") && total>0) {
        int category,list_page;char tail;
        if(sscanf(data,"%d %d %c",&category,&list_page,&tail)==2 && category>=0 &&
           category<=2 && list_page>=0 && list_page<=128) {
            int count=category==0 ? reader_ui.chapter_count : category==1 ?
                library[selected].bookmark_count : reader_ui.annotation_count;
            if(list_page*8>=count)list_page=count ? (count-1)/8 : 0;
            fprintf(out,",\"count\":%d,\"list_page\":%d",count,list_page);
            for(int row=0;row<8 && list_page*8+row<count;row++) {
                int i=list_page*8+row,position=category==0 ? reader_ui.chapters[i].page :
                    category==1 ? library[selected].bookmarks[i] : i;
                char key[24],label[40];snprintf(key,sizeof(key),"label%d",row);
                snprintf(label,sizeof(label),"%d",position+1);
                package_field(out,key,category==0 ? reader_ui.chapters[i].title : category==1 ?
                    label : reader_ui.annotations[i].text);
                fprintf(out,",\"position%d\":%d",row,position);
            }
        }
    }
    package_reply_end(out,&message);
    return 1;
}
