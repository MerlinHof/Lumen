/* All installed app packages use the same system controls and input bridge. */
static JsonToken package_tokens[32768];
static int package_value(Json*j,int obj,const char*key){double v=json_number(j,json_get(j,obj,key));return isfinite(v)&&v>=-100000&&v<=100000?(int)v:0;}
static int package_input_action, package_select_action;
static char package_options[48][128];
static const char *package_labels[48];
static void package_ui_choice(int choice){char event[128];snprintf(event,sizeof(event),"{\"event\":\"select\",\"action\":%d,\"value\":%d}",package_select_action,choice);app_process_send(event);}
static void package_ui_draw(void) {
    if(!*app_process_error())app_process_open(app_for_route(ui_route()));
    static unsigned char status_bar[600*STATUS_HEIGHT];
    memcpy(status_bar,ui_pixels,sizeof(status_bar));
    const char*text=app_process_frame();
    if(!*text) {heading(app_definition(app_for_route(ui_route()))->name);centered(28,340,544,60,*app_process_error()?tr(app_process_error()):tr("App wird geöffnet …"),ui_font(UI_BODY));return;}
    Json j;if(!json_parse(&j,text,package_tokens,32768))return;
    int commands=json_get(&j,0,"ui");if(commands<0 || j.tokens[commands].type!='[' || j.tokens[commands].count>2048)return;
    size_t cost=0;
    for(int i=0;i<j.tokens[commands].count;i++) {
        int item=json_at(&j,commands,i);char type[32],label[2048];
        json_string(&j,json_get(&j,item,"type"),type,sizeof(type));json_string(&j,json_get(&j,item,"text"),label,sizeof(label));
        int x=package_value(&j,item,"x"),y=package_value(&j,item,"y"),w=package_value(&j,item,"w"),h=package_value(&j,item,"h");
        /* Content coordinates may never paint or intercept the status bar. */
        if(y<STATUS_HEIGHT || y>800 || x < -600 || x>600 || w<0 || w>1200 || h<0 || h>800)continue;
        size_t estimate=(size_t)w*h;
        if(!strcmp(type,"ellipse"))estimate=4u*(unsigned)abs(package_value(&j,item,"rx"))*(unsigned)abs(package_value(&j,item,"ry"));
        if(!strcmp(type,"ring")){int r=package_value(&j,item,"radius");estimate=4u*(unsigned)abs(r)*(unsigned)abs(r);}
        if(!strcmp(type,"line"))estimate=(size_t)(abs(x-package_value(&j,item,"x2"))+32)*(abs(y-package_value(&j,item,"y2"))+32);
        if(estimate>4000000 || cost+estimate>4000000)break;cost+=estimate;
        if(!strcmp(type,"heading"))heading_actions(label,package_value(&j,item,"actions"));
        else if(!strcmp(type,"text")){int role=package_value(&j,item,"role");ui_paint_text(x,y,w,h,label,role>=0&&role<=PAINT_SECONDARY?role:PAINT_BODY);}
        else if(!strcmp(type,"button"))button_h(x,y,w,h,label,package_value(&j,item,"primary"));
        else if(!strcmp(type,"checkbox"))checkbox(x,y,package_value(&j,item,"value"));
        else if(!strcmp(type,"toggle"))toggle(x,y,package_value(&j,item,"value"));
        else if(!strcmp(type,"dropdown"))dropdown_button(x,y,w,h,label);
        else if(!strcmp(type,"select")){char value[256];json_string(&j,json_get(&j,item,"value"),value,sizeof(value));select_row17(y,label,value);}
        else if(!strcmp(type,"icon"))icon_button(x,y,package_value(&j,item,"icon"));
        else if(!strcmp(type,"hit"))region(x,y,w,h);
        else if(!strcmp(type,"surface"))ui_paint_surface(x,y,w,h,package_value(&j,item,"radius"),package_value(&j,item,"gray"),package_value(&j,item,"outline"));
        else if(!strcmp(type,"ellipse")){int rx=package_value(&j,item,"rx"),ry=package_value(&j,item,"ry");if(rx>=0&&rx<=300&&ry>=0&&ry<=400)ui_paint_ellipse(x,y,rx,ry,package_value(&j,item,"gray"));}
        else if(!strcmp(type,"line")){int x2=package_value(&j,item,"x2"),y2=package_value(&j,item,"y2"),weight=package_value(&j,item,"weight");if(y2>=56&&y2<=800&&x2>=-600&&x2<=1200&&weight>0&&weight<=32)ui_paint_line(x,y,x2,y2,weight,package_value(&j,item,"gray"));}
        else if(!strcmp(type,"ring")){int r=package_value(&j,item,"radius"),weight=package_value(&j,item,"weight");if(r>0&&r<=300&&weight>0&&weight<=24)ui_paint_ring(x,y,r,weight);}
    }

    memcpy(ui_pixels,status_bar,sizeof(status_bar));
}
static int package_ui_input(const Input*in) {
    if(in->kind!=INPUT_TAP && in->kind!=INPUT_SWIPE && in->kind!=INPUT_HOLD)return 2;
    char event[256];snprintf(event,sizeof(event),"{\"event\":\"input\",\"kind\":%d,\"x\":%d,\"y\":%d,\"dx\":%d,\"dy\":%d}",in->kind,in->x,in->y,in->dx,in->dy);
    app_process_open(app_for_route(ui_route()));app_process_send(event);return 2;
}
static int package_ui_commit(int target,const char*text) {
    if(target!=2000)return 0;
    char*event=NULL;size_t n=0;FILE*f=open_memstream(&event,&n);fprintf(f,"{\"event\":\"text\",\"action\":%d,\"text\":",package_input_action);
    json_escape16(f,text);fputc('}',f);fclose(f);app_process_send(event);free(event);return 1;
}
static int package_ui_poll(void) {
    int changed=app_process_poll();
    if(changed && *app_process_error())ui_message(app_process_error());
    const char*message=app_process_request();if(!*message)return changed;
    Json j;JsonToken t[512];if(!json_parse(&j,message,t,512))return changed;
    char kind[32],text[8192],title[128];json_string(&j,json_get(&j,0,"request"),kind,sizeof(kind));json_string(&j,json_get(&j,0,"text"),text,sizeof(text));json_string(&j,json_get(&j,0,"title"),title,sizeof(title));
    if(!strcmp(kind,"input")){package_input_action=package_value(&j,0,"action");open_keyboard(2000,title,text);}
    else if(!strcmp(kind,"select")) {
        int array=json_get(&j,0,"options");if(array>=0 && j.tokens[array].type=='[' && j.tokens[array].count>0 && j.tokens[array].count<=48) {
            int count=j.tokens[array].count;for(int i=0;i<count;i++){json_string(&j,json_at(&j,array,i),package_options[i],128);package_labels[i]=package_options[i];}
            package_select_action=package_value(&j,0,"action");open_dropdown(2001,package_labels,count,package_value(&j,0,"selected"),220,180,350);
        }
    }
    else if(!strcmp(kind,"notify"))ui_message(text);
    else if(!strcmp(kind,"reader"))reader_markdown17(title,text,ui_route());
    else if(!strcmp(kind,"open")){char key[96];json_string(&j,json_get(&j,0,"app"),key,sizeof(key));int id=-1;for(int i=0;i<APP_LIMIT;i++)if(!strcmp(key,app_package_key(i))){id=i;break;}if(id>=0){AppIntent intent={.app=id,.kind=*text?INTENT_SEARCH:INTENT_OPEN};snprintf(intent.text,sizeof(intent.text),"%s",text);app_send(&intent);}}
    return 1;
}
