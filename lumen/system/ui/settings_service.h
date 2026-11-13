/* Typed device/preferences operations. The Settings package chooses the UI. */
static const int appearance_options[]={
    OPT_LANGUAGE,OPT_KEYBOARD_LANGUAGE,OPT_INVERT,OPT_ROUNDING,OPT_DATE,OPT_CLOCK,OPT_WIFI,
    OPT_BATTERY,OPT_BATTERY_PERCENT,OPT_LOCK,OPT_LOCK_TIME,OPT_LOCK_DATE,OPT_LOCK_HINT,OPT_AUTO_TIME,OPT_ZONE
};
static int settings_autostart(int enabled) {
    char path[2048];local_path(path,sizeof(path),"../DISABLE_AUTOSTART");
#ifndef LUMEN_HOST
    if(enabled && access("/etc/upstart/lumen.conf",F_OK) &&
        system("/bin/sh /mnt/us/lumen/install-autostart.sh")!=0)return 0;
#endif
    if(enabled){if(unlink(path) && errno!=ENOENT)return 0;}
    else if(!atomic_write(path,"",0))return 0;
    options[OPT_AUTOSTART]=enabled;library_settings_save();return 1;
}
static int settings_wifi_operation(const char *operation,const char *data,int action) {
    if(!strcmp(operation,"wifi_start")){wifi_open();return package_acknowledge(action,1);}
    if(!strcmp(operation,"wifi_scan")) {
        wifi_ui.wifi_scanning=network_scan();return package_acknowledge(action,wifi_ui.wifi_scanning);
    }
    if(!strcmp(operation,"wifi_power")) {
        if(strcmp(data,"0") && strcmp(data,"1"))return package_acknowledge(action,0);
        int enabled=*data=='1';
        if(!device_wifi(enabled))return package_acknowledge(action,0);
        wifi_ui.wifi_enabled=enabled;
        if(enabled)wifi_open();
        else {
            connect_stop();discover_cancel();network_disconnect();
            wifi_ui.wifi_ssid[0]=0;wifi_count=0;wifi_ui.wifi_scanning=0;
        }
        return package_acknowledge(action,1);
    }
    if(!strcmp(operation,"wifi_connect") || !strcmp(operation,"wifi_forget")) {
        const char *end=strchr(data,'\n');size_t len=end ? (size_t)(end-data) : strlen(data);
        if(!len || len>128)return package_acknowledge(action,0);
        for(int i=0;i<wifi_count;i++)if(strlen(wifi_networks[i].ssid)==len && !memcmp(data,wifi_networks[i].ssid,len)) {
            int ok;
            if(!strcmp(operation,"wifi_forget")) {
                int connected=!strcmp(wifi_networks[i].ssid,wifi_ui.wifi_ssid);
                ok=network_forget(i);
                if(ok && connected){connect_stop();network_disconnect();wifi_ui.wifi_ssid[0]=0;}
            } else ok=network_connect(i,end ? end+1 : "");
            if(!ok)ui_message(network_error());
            return package_acknowledge(action,ok);
        }
        return package_acknowledge(action,0);
    }
    if(strcmp(operation,"wifi_list"))return 0;
    int page=atoi(data);if(page<0 || page>128 || page*6>=wifi_count)page=0;
    char *message=NULL;size_t length=0;FILE *out=package_reply(&message,&length,action,1);
    if(!out)return 0;
    fprintf(out,",\"enabled\":%d,\"busy\":%d,\"count\":%d,\"page\":%d",
        wifi_ui.wifi_enabled,wifi_ui.wifi_scanning,wifi_count,page);
    for(int i=0;i<6 && page*6+i<wifi_count;i++) {
        WifiNetwork *n=&wifi_networks[page*6+i];
        char field[24];snprintf(field,sizeof(field),"name%d",i);package_field(out,field,n->ssid);
        fprintf(out,",\"saved%d\":%d,\"locked%d\":%d,\"connected%d\":%d",
            i,!!n->saved,i,!!strstr(n->flags,"WPA"),i,!strcmp(n->ssid,wifi_ui.wifi_ssid));
    }
    package_reply_end(out,&message);return 1;
}
static int package_settings(const char *operation,const char *data,int action) {
    if(app_for_route(ui_route())!=APP_SETTINGS)return 0;
    if(!strncmp(operation,"wifi_",5))return settings_wifi_operation(operation,data,action);
    if(!strcmp(operation,"preference")) {
        const int max[]={2,2,1,1,1,1,1,1,1,5,1,1,1,1,4};
        int key,value;char tail;
        if(sscanf(data,"%d %d %c",&key,&value,&tail)!=2 || key<0 ||
           key>=(int)(sizeof(max)/sizeof(*max)) || value<0 || value>max[key])return package_acknowledge(action,0);
        options[appearance_options[key]]=value;
        if(key==14)extra_zone();
        if(key==13 && value && !service_busy())service_start(SERVICE_TIME,"","","");
        library_options_save();
        return package_acknowledge(action,1);
    }
    if(!strcmp(operation,"sleep")) {
        char *end;
        long value=strtol(data,&end,10);
        if(end==data || *end)return package_acknowledge(action,0);
        int valid=0;
        for(size_t i=0;i<sizeof(sleep_durations)/sizeof(*sleep_durations);i++)
            valid|=value==sleep_durations[i];
        if(!valid)return package_acknowledge(action,0);
        char canonical[12],path[2048];
        snprintf(canonical,sizeof(canonical),"%d",(int)value);
        local_path(path,sizeof(path),"auto-sleep16.txt");
        if(!atomic_write(path,canonical,strlen(canonical)))return package_acknowledge(action,0);
        auto_sleep_minutes=(int)value;
        return package_acknowledge(action,1);
    }
    if(!strcmp(operation,"time")) {
        int y,m,d,h,min;char tail;
        int ok=sscanf(data,"%d-%d-%d %d:%d%c",&y,&m,&d,&h,&min,&tail)==5 && system_set_time(y,m,d,h,min);
        if(ok)status_ui.status_time=0;
        return package_acknowledge(action,ok);
    }
    if(!strcmp(operation,"check"))return package_acknowledge(action,release_start(0));
    if(!strcmp(operation,"install")) {
        int state=release_state();
        if(state!=4 && (state!=3 || !release_start(1)))return package_acknowledge(action,0);
        connect_stop();device_hold_awake(1);updating=1;update_error=NULL;full=1;pressed=-1;
        return 1;
    }
    if(!strcmp(operation,"autostart")) {
        if(strcmp(data,"0") && strcmp(data,"1"))return package_acknowledge(action,0);
        return package_acknowledge(action,settings_autostart(*data=='1'));
    }
    if(!strcmp(operation,"export") || !strcmp(operation,"import")) {
        char path[2048];local_path(path,sizeof(path),"../LUMEN-Einstellungen.txt");
        int ok=!strcmp(operation,"export") ? library_config_export(path) : library_config_import(path);
        return package_acknowledge(action,ok);
    }
    if(!strcmp(operation,"amazon")){package_exit_requested=1;return 1;}
    if(!strcmp(operation,"reset")) {
        library_defaults();library_settings_save();settings_autostart(1);
        auto_sleep_minutes=10;extra_write_text("auto-sleep16.txt","10");
        return package_acknowledge(action,1);
    }
    if(!strcmp(operation,"app_change")) {
        char key[64],verb[12],tail;
        if(sscanf(data,"%11s %63s %c",verb,key,&tail)!=2)return package_acknowledge(action,0);
        int app=app_package_find(key),indices[APP_LIMIT],n=launcher_manage_indices(indices),pos=-1;
        for(int i=0;i<n;i++)if(launcher_order[indices[i]]==app){pos=i;break;}
        if(pos<0)return package_acknowledge(action,0);
        if(!strcmp(verb,"remove"))return package_acknowledge(action,app_package_can_remove(app) && app_package_remove(key));
        if(!strcmp(verb,"visible"))launcher_visible[app]^=1;
        else {
            int delta=!strcmp(verb,"up") ? -1 : !strcmp(verb,"down") ? 1 : 0;
            if(!delta || pos+delta<0 || pos+delta>=n)return package_acknowledge(action,0);
            launcher_order[indices[pos]]=launcher_order[indices[pos+delta]];
            launcher_order[indices[pos+delta]]=app;
        }
        v1_dirty=1;v1_save();return package_acknowledge(action,1);
    }
    int apps=!strcmp(operation,"apps"),storage=!strcmp(operation,"storage");
    if(!apps && !storage && strcmp(operation,"state"))return 0;
    char *message=NULL;size_t length=0;FILE *out=package_reply(&message,&length,action,1);
    if(!out)return 0;
    if(apps) {
        int indices[APP_LIMIT],count=launcher_manage_indices(indices),page=atoi(data);
        if(page<0 || page>APP_LIMIT || page*8>=count)page=count ? (count-1)/8 : 0;
        fprintf(out,",\"count\":%d,\"page\":%d",count,page);
        for(int r=0;r<8 && page*8+r<count;r++) {
            int app=launcher_order[indices[page*8+r]];char field[32];
            snprintf(field,sizeof(field),"id%d",r);package_field(out,field,app_package_key(app));
            snprintf(field,sizeof(field),"name%d",r);package_field(out,field,tr(app_definition(app)->name));
            snprintf(field,sizeof(field),"version%d",r);package_field(out,field,app_package_version(app));
            fprintf(out,",\"visible%d\":%d,\"remove%d\":%d",r,launcher_visible[app],r,app_package_can_remove(app));
        }
    } else if(storage) {
        storage_scan();
        uint64_t free=storage_free<storage_total ? storage_free : storage_total;
        uint64_t books=storage_books<storage_total-free ? storage_books : storage_total-free;
        uint64_t system=storage_system<storage_total-free-books ? storage_system : storage_total-free-books;
        uint64_t sizes[]={books,system,storage_firmware,storage_total-free-books-system,free};
        uint64_t total=storage_total+storage_firmware;char label[40];
        storage_size(label,sizeof(label),total);package_field(out,"total",label);
        for(int i=0;i<5;i++){
            char field[20];snprintf(field,sizeof(field),"size%d",i);
            storage_size(label,sizeof(label),sizes[i]);package_field(out,field,label);
            fprintf(out,",\"ratio%d\":%d",i,total ? (int)(1000.0*sizes[i]/total) : 0);
        }
    } else {
        release_poll();
        for(unsigned i=0;i<sizeof(appearance_options)/sizeof(*appearance_options);i++)
            fprintf(out,",\"p%u\":%d",i,options[appearance_options[i]]);
        fprintf(out,",\"sleep\":%d,\"autostart\":%d,\"release\":%d",auto_sleep_minutes,options[OPT_AUTOSTART],release_state());
        package_field(out,"version",LUMEN_VERSION);
        package_field(out,"release_version",release_version());
        package_field(out,"error",release_error());
        time_t checked=release_checked();struct tm t;char value[40]="";
        if(checked && localtime_r(&checked,&t))strftime(value,sizeof(value),"%d.%m.%Y · %H:%M",&t);
        package_field(out,"checked",value);
    }
    package_reply_end(out,&message);return 1;
}
