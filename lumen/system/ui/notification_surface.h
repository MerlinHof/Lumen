/* Compose the installed Notifications app without blocking the foreground app. */
static const char notification_app[]="lumen.notifications";
static int notification_pending,notification_capture,notification_generation;
static int notification_x,notification_y,notification_w,notification_h;

static int receive_notification(const void *payload,size_t size,int64_t now) {
    (void)now;
    if(size!=sizeof(NotificationRequest))return 0;
    const NotificationRequest *request=payload;
    char *event=NULL;size_t n=0;
    FILE *out=open_memstream(&event,&n);
    if(!out)return 0;
    notification_generation=(notification_generation+1)%1000000;
    fprintf(out,"{\"event\":\"notify\",\"generation\":%d,\"duration\":%d,\"text\":",
            notification_generation,request->duration_ms);
    json_escape16(out,request->text);
    fputc('}',out);fclose(out);
    int sent=app_overlay_send(notification_app,event);
    free(event);
    notification_pending=sent;
    return sent;
}
static void notification_dismiss(void) {
    if(notification_w || notification_pending)app_overlay_send(notification_app,"{\"event\":\"dismiss\"}");
    notification_w=0;
    notification_pending=0;
    notification_generation=(notification_generation+1)%1000000;
}
static int notification_poll(void) {
    if(!app_overlay_poll())return 0;
    int before=notification_w;
    notification_w=0;
    notification_pending=0;
    Json j;JsonToken tokens[512];
    if(json_parse(&j,app_overlay_frame(),tokens,512) &&
       json_number(&j,json_get(&j,0,"generation"))==notification_generation) {
        int rect=json_get(&j,0,"capture");
        int values[4]={0};
        for(int i=0;i<4;i++) {
            double n=json_number(&j,json_at(&j,rect,i));
            if(n>=0 && n<=800)values[i]=(int)n;
        }
        int x=values[0],y=values[1],w=values[2],h=values[3];
        if(x>=0 && y>=STATUS_HEIGHT && y<180 && w>0 && h>0 && w<=WIDTH &&
           h<=124 && x<=WIDTH-w && y<=180-h) {
            notification_x=x;notification_y=y;notification_w=w;notification_h=h;
        }
    }
    return before || notification_w;
}
static int notification_visible(void) {
    return notification_w && !editor_ui.keyboard && !sleeping && !extra_ringing() && !power_menu;
}
static void notification_draw(void) {
    if(notification_visible())package_paint_frame(app_overlay_frame());
}
static int notification_input(const Input *in,int visible) {
    int inside=visible && in->x>=notification_x && in->x<notification_x+notification_w &&
               in->y>=notification_y && in->y<notification_y+notification_h;
    if(in->kind==INPUT_CANCEL){notification_capture=0;return 0;}
    if(in->kind==INPUT_DOWN) {
        notification_capture=inside;
        return inside;
    }
    if(in->kind==INPUT_MOVE || in->kind==INPUT_HOLD)return notification_capture || inside;
    if(in->kind!=INPUT_TAP && in->kind!=INPUT_SWIPE)return 0;
    int captured=notification_capture || inside;
    notification_capture=0;
    if(captured && visible) {
        char event[128];
        snprintf(event,sizeof(event),"{\"event\":\"input\",\"kind\":%d,\"x\":%d,\"y\":%d}",in->kind,in->x,in->y);
        app_overlay_send(notification_app,event);
    }
    return captured;
}
