/* Device alarms outlive the Clock foreground process. */
static int package_clock_service(const char *operation,const char *data,int action) {
    if(strcmp(operation,"state")) {
        char *end;long value=strtol(data,&end,10);
        if(!*data || *end)return package_acknowledge(action,0);
        if(!strcmp(operation,"minutes") && value>=1 && value<=1440) {
            timer_minutes=(int)value;
            if(!timer_due && !timer_paused)timer_total_seconds=timer_minutes*60;
        } else if(!strcmp(operation,"timer") && (value==0 || value==1))timer_action((int)value);
        else if(!strcmp(operation,"hour") && value>=0 && value<24)alarm_hour=(int)value;
        else if(!strcmp(operation,"minute") && value>=0 && value<60)alarm_minute=(int)value;
        else if(!strcmp(operation,"armed") && (value==0 || value==1))alarm_enabled=(int)value;
        else return package_acknowledge(action,0);
        extra_save();
    }
    int64_t remaining=timer_due ? timer_due-extra_millis() : timer_paused;
    char *message=NULL;size_t length=0;
    FILE *out=package_reply(&message,&length,action,1);
    if(!out)return 0;
    fprintf(out,",\"minutes\":%d,\"total\":%d,\"remaining\":%lld,\"running\":%d,"
                "\"paused\":%d,\"hour\":%d,\"minute\":%d,\"armed\":%d",
        timer_minutes,timer_total_seconds,(long long)(remaining>0 ? remaining : 0),
        timer_due>0,timer_paused>0,alarm_hour,alarm_minute,alarm_enabled);
    package_reply_end(out,&message);
    return 1;
}
