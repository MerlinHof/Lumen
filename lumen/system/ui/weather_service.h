/* The network worker owns provider fetches and the last successful cache. */
static int package_weather(const char *operation,const char *city,int action) {
    int refresh=!strcmp(operation,"refresh"),automatic=!strcmp(operation,"open");
    if(!refresh && !automatic && strcmp(operation,"status"))return 0;
    int request=refresh;
#ifndef LUMEN_HOST
    if(automatic) {
        char path[2048];snprintf(path,sizeof(path),"%s/weather.json",data_root);
        struct stat cache;
        request=stat(path,&cache) || time(NULL)-cache.st_mtime>1800;
    }
#endif
    if(request && *city && strlen(city)<256 && !service_busy())
        service_start(SERVICE_WEATHER,city,"","");
    char *message=NULL;size_t length=0;
    FILE *out=package_reply(&message,&length,action,1);
    if(!out)return 0;
    fprintf(out,",\"busy\":%d",!!service_busy());
    char *raw=service_read("weather.json",100000);
    static JsonToken tokens[4096];Json weather;
    if(raw && json_parse(&weather,raw,tokens,4096)) {
        int current=json_get(&weather,0,"current"),daily=json_get(&weather,0,"daily");
        const char *numbers[]={"temperature_2m","apparent_temperature","wind_speed_10m","weather_code","is_day"};
        const char *names[]={"temp","feel","wind","code","daylight_now"};
        double temp=json_number(&weather,json_get(&weather,current,"temperature_2m"));
        if(isfinite(temp)) {
            fputs(",\"forecast\":1",out);
            for(int i=0;i<5;i++) {
                double value=json_number(&weather,json_get(&weather,current,numbers[i]));
                fprintf(out,",\"%s\":%d",names[i],isfinite(value) && fabs(value)<10000 ? (int)lround(value) : 0);
            }
            char stamp[64];json_string(&weather,json_get(&weather,current,"time"),stamp,sizeof(stamp));
            package_field(out,"stamp",stamp);
            const char *fields[]={"sunrise","sunset","time","weather_code","temperature_2m_max","temperature_2m_min","precipitation_probability_max"};
            const char *keys[]={"rise","set","date","code","high","low","rain"};
            for(int k=0;k<7;k++) {
                int array=json_get(&weather,daily,fields[k]);
                for(int d=0;d<(k<2 ? 1 : 5);d++) {
                    int token=json_at(&weather,array,d);char key[24];
                    snprintf(key,sizeof(key),"%s%d",keys[k],d);
                    if(k<3) {
                        char value[64];json_string(&weather,token,value,sizeof(value));
                        package_field(out,key,value);
                    } else {
                        double value=json_number(&weather,token);
                        fprintf(out,",\"%s\":%d",key,isfinite(value) && fabs(value)<10000 ? (int)lround(value) : 0);
                    }
                }
            }
            double light=json_number(&weather,json_at(&weather,json_get(&weather,daily,"daylight_duration"),0));
            fprintf(out,",\"light\":%d",isfinite(light) && light>=0 && light<=86400 ? (int)light : 0);
        }
    }
    free(raw);
    raw=service_read("weather-place.json",65536);
    if(raw && json_parse(&weather,raw,tokens,4096)) {
        int result=json_at(&weather,json_get(&weather,0,"results"),0);
        char place[256];json_string(&weather,json_get(&weather,result,"name"),place,sizeof(place));
        package_field(out,"place",place);
    }
    free(raw);
    package_reply_end(out,&message);
    return 1;
}
