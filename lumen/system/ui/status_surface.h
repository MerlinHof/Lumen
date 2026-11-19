/* The status package selects fields and geometry. LUMEN supplies device state,
 * draws shared symbols, and executes navigation from the declared hit regions. */
static const char *status_title(void) {
    if(*package_view_title && app_process_owner()==(int)active_app())
        return tr(package_view_title);
    const AppDefinition *app=app_definition(active_app());
    return app ? tr(app->name) : "";
}
static void status_send(void) {
    static char previous[1024];
    static int64_t attempted;
    int alive=app_overlay_fd(app_provider_overlay_slot(PROVIDER_STATUS))>=0;
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
        if(app_provider_overlay_send(PROVIDER_STATUS,message))
            snprintf(previous,sizeof(previous),"%s",message);
    }
    free(message);
}
static int status_overlay_poll(void) {return app_overlay_poll(app_provider_overlay_slot(PROVIDER_STATUS));}
static OverlayAction status_actions[12];
static int status_action_count;

static void header(void) {
    refresh_status(0);
    status_send();
    status_overlay_poll();
    rect(0,0,WIDTH,STATUS_HEIGHT,255);
    const char *frame=app_overlay_frame(app_provider_overlay_slot(PROVIDER_STATUS));
    const OverlayAction *actions=overlay_slot_actions(app_provider_overlay_slot(PROVIDER_STATUS),&status_action_count);
    if(status_action_count>12)status_action_count=12;
    if(status_action_count)memcpy(status_actions,actions,(size_t)status_action_count*sizeof(*actions));
    const char *title=status_title();
    overlay_title_x=overlay_title_width=0;
    if(*frame)overlay_paint(frame,(OverlayBounds){0,0,WIDTH,STATUS_HEIGHT},title);
    int back=can_back();
    int left=back ? 68 : 24;
    int width=overlay_title_width>0 ? overlay_title_x+overlay_title_width-left : 195;
    if(width>195)width=195;
    if(width>0)label_ellipsis(left,0,width,STATUS_HEIGHT,title,&small,0);
    for(int i=0;i<status_action_count;i++) {
        OverlayAction *action=&status_actions[i];
        OverlayBounds box=action->bounds;
        if(box.y+box.h>STATUS_HEIGHT || !strcmp(action->command,"back"))continue;
        system_region(box.x,box.y,box.w,box.h);
    }
    if(back) {
        system_region(0,0,62,STATUS_HEIGHT);
        icon_sized(9,6,IC_BACK,44);
    }
}
static int status_action(const Input *input) {
    for(int i=status_action_count-1;i>=0;i--) {
        const OverlayAction *action=&status_actions[i];
        OverlayBounds box=action->bounds;
        if(box.y+box.h>STATUS_HEIGHT || input->x<box.x || input->x>=box.x+box.w ||
           input->y<box.y || input->y>=box.y+box.h)continue;
        if(!strcmp(action->command,"battery")) {
            battery_popup=1;
            return 1;
        }
        if(!strcmp(action->command,"open")) {
            int id=app_package_find(action->app);
            if(!app_package_foreground(id))return 0;
            AppIntent request={.app=id,.kind=INTENT_OPEN};
            snprintf(request.text,sizeof(request.text),"%s",action->intent);
            return app_send(&request);
        }
    }
    return 0;
}
