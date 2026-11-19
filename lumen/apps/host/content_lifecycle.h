/* App-owned module; assembled by apps/host/compose.h. */
static int polish_tick(void) {
    int changed = 0, done = press_poll();
    if (done) {
        if(done<0)press_retry_at=time(NULL)+3600;
        if(active_app()==app_package_find("lumen.news")) {
            if(done==2 && press_open_pending)article_news_open();
            else {
                if(done<0)ui_message("Nicht geladen. Gespeicherte Inhalte bleiben verfügbar.");
                app_process_send("{\"event\":\"refresh\"}");
            }
            changed=1;
        }
        press_open_pending=0;
    }
    static int initialized;
    if(!initialized) {
        initialized=1;
        char *settings=service_read("press-daily-enabled.txt",8);
        if(settings) {
            press_daily_enabled=atoi(settings)!=0;
            free(settings);
        } else {
            char *previous=service_read("news-settings.txt",80);
            unsigned topics;
            int enabled;
            if(previous && sscanf(previous,"%u %d",&topics,&enabled)==2 && enabled) {
                press_daily_enabled=1;
                extra_write_text("press-daily-enabled.txt","1");
            }
            free(previous);
        }
    }
    if (press_daily_enabled && !press_busy() && time(NULL) >= press_retry_at && active_app()!=app_package_find("lumen.news")) {
        press_retry_at = time(NULL) + 60;
        char n[100], p[2048];
        snprintf(n, sizeof(n), "press/edition-%08d.bin", news_date());
        local_path(p, sizeof(p), n);
        if (access(p, R_OK)) {
            char ip[80];
            device_ip(ip, 80);
            if (device_wifi_enabled() && *ip) {
                press_retry_at = time(NULL) + 3600;
                press_start_day(news_date(), options[OPT_LANGUAGE]);
            }
        }
    }
    return changed;
}
static int polish_poll(void) {
    if (press_busy())
        return 250;
    if (press_daily_enabled)
        return 60000;
    return -1;
}
static void polish_close(void) {
    press_cancel();
}
