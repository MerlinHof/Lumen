/* The status package selects fields and geometry. LUMEN supplies device state,
 * draws shared symbols, and executes navigation from the declared hit regions. */
static const char *status_app_id="lumen.status-bar";
static const char *status_title(void) {
    if(*package_view_title && app_process_owner()==(int)app_for_route(ui_route()))
        return tr(package_view_title);
    const AppDefinition *app=app_definition(app_for_route(ui_route()));
    return app ? tr(app->name) : "";
}
static void status_send(void) {
    static char previous[1024];
    static int64_t attempted;
    int alive=app_overlay_fd(APP_OVERLAY_STATUS)>=0;
    int64_t now_ms=extra_millis();
    if(!alive && attempted && now_ms-attempted<1000)return;
    time_t now=time(NULL);
    struct tm local;localtime_r(&now,&local);
    char date[32],clock[16],timer[32],alarm[16],timer_label[64];
    strftime(date,sizeof(date),"%d.%m.",&local);
    strftime(clock,sizeof(clock),"%H:%M",&local);
    extra_timer_label(timer,sizeof(timer));extra_alarm_label(alarm,sizeof(alarm));
    if(*timer) {
        int minutes=0,seconds=0;
        sscanf(timer,"%d:%d",&minutes,&seconds);
        snprintf(timer,sizeof(timer),"%d min",minutes+(seconds>0));
    }
    snprintf(timer_label,sizeof(timer_label),"%s%s%s",timer,*timer && *alarm ? " · " : "",alarm);
    char *message=NULL;size_t length=0;
    FILE *out=open_memstream(&message,&length);
    if(!out)return;
    fprintf(out,"{\"event\":\"system_state\",\"battery\":%d,\"charging\":%d,\"offline\":%d,\"back\":%d,"
        "\"show_battery\":%d,\"show_percent\":%d,\"show_wifi\":%d,\"show_clock\":%d,\"show_date\":%d",
        atoi(status_ui.battery),status_ui.battery_charging,!strcmp(status_ui.wifi,"Offline"),can_back(),
        options[OPT_BATTERY],options[OPT_BATTERY_PERCENT],options[OPT_WIFI],options[OPT_CLOCK],options[OPT_DATE]);
    package_field(out,"name",status_title());
    package_field(out,"date",date);package_field(out,"clock",clock);package_field(out,"timer",timer_label);
    fputc('}',out);fclose(out);
    if(message && (!alive || strcmp(previous,message))) {
        attempted=now_ms;
        if(app_overlay_send(APP_OVERLAY_STATUS,status_app_id,message))
            snprintf(previous,sizeof(previous),"%s",message);
    }
    free(message);
}
static int status_overlay_poll(void) {return app_overlay_poll(APP_OVERLAY_STATUS);}
static void status_battery(int x,int percent,int charging) {
    control_surface(x,19,30,18,9,0);
    control_surface(x+2,21,26,14,7,255);
    control_surface(x+30,26,2,4,1,0);
    int fill=22*percent/100;
    if(fill>22)fill=22;
    if(fill>0)control_surface(x+4,23,fill,10,5,0);
    if(charging) {
        stroke_color(x+18,20,x+11,29,5,255);
        stroke_color(x+11,29,x+18,28,5,255);
        stroke_color(x+18,28,x+13,35,5,255);
        stroke(x+18,20,x+11,29,2);
        stroke(x+11,29,x+18,28,2);
        stroke(x+18,28,x+13,35,2);
    }
}
static void header(void) {
    refresh_status(0);
    status_send();
    rect(0,0,WIDTH,STATUS_HEIGHT,255);
    status_ui.status_wifi_x=status_ui.status_battery_x=status_ui.status_timer_x=-1;
    status_ui.status_date_w=status_ui.status_clock_w=0;
    if(can_back()){system_region(0,0,62,STATUS_HEIGHT);icon_sized(9,6,IC_BACK,44);}
    Json frame;JsonToken tokens[512];
    int left=can_back() ? 68 : 24, title_width=195, title_right=WIDTH;
    if(!json_parse(&frame,app_overlay_frame(APP_OVERLAY_STATUS),tokens,512)) {
        label_ellipsis(left,0,title_width,STATUS_HEIGHT,status_title(),&small,0);
        return;
    }
    int items=json_get(&frame,0,"ui"), bind_title=0;
    for(int index=0;index<12;index++) {
        int item=json_at(&frame,items,index);
        if(item<0)break;
        char id[24],text[128];
        json_string(&frame,json_get(&frame,item,"id"),id,sizeof(id));
        json_string(&frame,json_get(&frame,item,"text"),text,sizeof(text));
        int x=package_value(&frame,item,"x"),width=package_value(&frame,item,"w");
        if(x<0 || x>=WIDTH || width<1 || width>WIDTH-x)continue;
        if(!strcmp(id,"back"))continue;
        if(strcmp(id,"name") && x>=left && x<title_right)title_right=x;
        if(!strcmp(id,"battery")) {
            status_ui.status_battery_x=x;status_ui.status_battery_right=x+width;
            status_battery(x,package_value(&frame,item,"value"),package_value(&frame,item,"flag"));
            system_region(x-5,0,width+10,STATUS_HEIGHT);
        } else if(!strcmp(id,"wifi")) {
            status_ui.status_wifi_x=x;wifi_symbol(x,6);
            if(package_value(&frame,item,"flag"))aa_stroke(x+2,15,x+27,38,2);
            system_region(x-5,0,width+10,STATUS_HEIGHT);
        } else if(!strcmp(id,"name")) {
            bind_title=1;
            if(package_value(&frame,item,"value")==1)title_width=width;
        } else if(!strcmp(id,"timer")) {
            int start=x+(width-textwidth_raw(text,&caption)-24)/2;
            if(start<x)continue;
            icon_sized(start,15,IC_CLOCK,25);
            label_ellipsis(start+27,0,x+width-start-27,STATUS_HEIGHT,text,&caption,0);
            status_ui.status_timer_x=x;status_ui.status_timer_width=width;
            system_region(x,0,width,STATUS_HEIGHT);
        } else {
            int length=textwidth_raw(text,&small);
            if(length>width)length=width;
            label_ellipsis(x+(width-length)/2,0,length,STATUS_HEIGHT,text,&small,0);
            if(!strcmp(id,"percent")) {
                if(status_ui.status_battery_x<0)status_ui.status_battery_right=x+width;
                status_ui.status_battery_x=x;
            } else if(!strcmp(id,"clock")) {
                status_ui.status_clock_x=x;status_ui.status_clock_w=width;
            } else if(!strcmp(id,"date")) {
                status_ui.status_date_x=x;status_ui.status_date_w=width;
            } else continue;
            system_region(x,0,width,STATUS_HEIGHT);
        }
    }
    if(title_width>title_right-left-8)title_width=title_right-left-8;
    if(bind_title && title_width>0)label_ellipsis(left,0,title_width,STATUS_HEIGHT,status_title(),&small,0);
}
