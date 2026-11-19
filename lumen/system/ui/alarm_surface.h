static int alarm_surface_started,alarm_surface_pressed;
static void alarm_surface_event(const char *agenda) {
    char *event=NULL;
    size_t size=0;
    FILE *stream=open_memstream(&event,&size);
    if(!stream)return;
    fprintf(stream,"{\"event\":\"intent\",\"operation\":\"alarm-state\",\"alarm\":%d,\"language\":%d",alarm_ring,options[OPT_LANGUAGE]);
    package_field(stream,"agenda",agenda);
    fputc('}',stream);
    if(!fclose(stream))app_provider_overlay_send(PROVIDER_ALARM,event);
    free(event);
}
static void alarm_agenda_ready(const char *agenda) {
    if(alarm_ring)alarm_surface_event(agenda);
}
static void extra_overlay(void) {
    if(!alarm_ring)return;
    if(!alarm_surface_started || app_overlay_fd(app_provider_overlay_slot(PROVIDER_ALARM))<0) {
        alarm_surface_started=1;
        alarm_surface_event("");
        app_system_request(PROVIDER_AGENDA,"today",alarm_agenda_ready);
    }
    widget_count=0;
    const char *frame=app_overlay_frame(app_provider_overlay_slot(PROVIDER_ALARM));
    if(strstr(frame,"overlay_action"))overlay_paint(frame,(OverlayBounds){0,0,WIDTH,HEIGHT},NULL);
    else {
        rect(0,0,WIDTH,HEIGHT,255);
        centered(28,240,544,70,"Wecker",ui_font(UI_TITLE));
        button(80,620,440,"Weiterlesen",0);
    }
    if(alarm_surface_pressed) {
        for(int y=621;y<667;y++)
            for(int x=81;x<519;x++)ui_pixels[y*WIDTH+x]=255-ui_pixels[y*WIDTH+x];
    }
}
static int extra_intercept(const Input *in) {
    if(!alarm_ring)return -1;
    int inside=in->x>=80 && in->x<520 && in->y>=620 && in->y<668;
    int count,slot=app_provider_overlay_slot(PROVIDER_ALARM);
    overlay_slot_actions(slot,&count);
    if(count)inside=overlay_hit_action(slot,(OverlayBounds){0,0,WIDTH,HEIGHT},in->x,in->y,"dismiss")!=NULL;
    if(in->kind==INPUT_TAP && inside) {
        alarm_ring=alarm_surface_started=alarm_surface_pressed=0;
        app_overlay_close(app_provider_overlay_slot(PROVIDER_ALARM));
        app_process_send("{\"event\":\"refresh\"}");
        return 1;
    }
    if(in->kind==INPUT_DOWN){alarm_surface_pressed=inside;return 1;}
    if(in->kind==INPUT_CANCEL || in->kind==INPUT_TAP || in->kind==INPUT_SWIPE)alarm_surface_pressed=0;
    if(in->kind==INPUT_CANCEL || in->kind==INPUT_TAP || in->kind==INPUT_SWIPE ||
       in->kind==INPUT_HOLD || in->kind==INPUT_MOVE)return 1;
    return -1;
}
