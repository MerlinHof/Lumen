/* Credentials and network workers stay on the host. The app receives typed data. */
static int ha_entity_index(const char *id) {
    for(int i=0;i<ha_count;i++)if(!strcmp(ha_entities[i].id,id))return i;
    return -1;
}
static void ha_load_config(void) {
    extra_load_text("ha-url.txt",ha_url,sizeof(ha_url));
    extra_load_text("ha-token.txt",ha_token,sizeof(ha_token));
}
static void ha_refresh(void) {
    if(!*ha_url || !*ha_token || service_busy() || time(NULL)<ha_refresh_at)return;
    ha_refresh_at=time(NULL)+5;
    service_start(SERVICE_HA,ha_url,ha_token,"");
}
static void ha_fetch_history(const char *ids) {
    if(!*ids || !*ha_url || !*ha_token || service_history_busy() ||
       (!strcmp(ids,ha_history_key) && time(NULL)-ha_history_at<300))return;
    if(service_start(SERVICE_HA_HISTORY,ha_url,ha_token,ids)) {
        snprintf(ha_history_key,sizeof(ha_history_key),"%s",ids);
        ha_history_at=time(NULL);
    }
}
static void ha_row(FILE *out,int row,const HAEntity *entity) {
    char field[24];
    snprintf(field,sizeof(field),"id%d",row);package_field(out,field,entity->id);
    snprintf(field,sizeof(field),"name%d",row);package_field(out,field,entity->name);
    snprintf(field,sizeof(field),"value%d",row);package_field(out,field,entity->state);
    snprintf(field,sizeof(field),"unit%d",row);package_field(out,field,entity->unit);
    fprintf(out,",\"sensor%d\":%d,\"saved%d\":%d",row,entity->sensor,row,ha_favorite(entity->id)>=0);
}
static void ha_points(FILE *out,int row,const char *id) {
    char field[24],points[4200];size_t used=0;
    points[0]=0;
    for(int i=0;i<8;i++)if(!strcmp(ha_history[i].id,id)) {
        const HAHistory *series=&ha_history[i];
        for(int j=0;j<series->count;j++) {
            int n=snprintf(points+used,sizeof(points)-used,"%lld %.7g\n",
                (long long)series->time[j],series->value[j]);
            if(n<=0 || (size_t)n>=sizeof(points)-used){points[used]=0;break;}
            used+=(size_t)n;
        }
        break;
    }
    snprintf(field,sizeof(field),"points%d",row);package_field(out,field,points);
}
static int package_home_assistant(const char *operation,const char *data,int action) {
    if(!strcmp(operation,"refresh") || !strcmp(operation,"config")) {
        ha_load_config();
        if(!strcmp(operation,"refresh")) {
            ha_parse();ha_history_parse();
            ha_refresh();
            return package_acknowledge(action,1);
        }
        char *message=NULL;size_t length=0;
        FILE *out=package_reply(&message,&length,action,1);
        if(!out)return 0;
        package_field(out,"url",ha_url);
        fprintf(out,",\"token_set\":%d",!!*ha_token);
        package_reply_end(out,&message);
        return 1;
    }
    if(!strcmp(operation,"url") || !strcmp(operation,"token")) {
        int url=!strcmp(operation,"url");
        if(!*data || strlen(data)>=sizeof(ha_token) || strpbrk(data,"\r\n\"\\") ||
           (url && strncmp(data,"http://",7) && strncmp(data,"https://",8)))
            return package_acknowledge(action,0);
        snprintf(url ? ha_url : ha_token,sizeof(ha_token),"%s",data);
        extra_write_text(url ? "ha-url.txt" : "ha-token.txt",data);
        ha_refresh_at=0;
        return package_acknowledge(action,1);
    }
    if(!strcmp(operation,"favorite") || !strcmp(operation,"toggle")) {
        int index=ha_entity_index(data);
        if(index<0)return package_acknowledge(action,0);
        int favorite=ha_favorite(data);
        if(!strcmp(operation,"toggle"))
            return package_acknowledge(action,favorite>=0 && !ha_entities[index].sensor &&
                *ha_url && *ha_token && service_start(SERVICE_TOGGLE,ha_url,ha_token,data));
        if(favorite>=0) {
            memmove(ha_favorites+favorite,ha_favorites+favorite+1,
                    (size_t)(ha_favorite_count-favorite-1)*sizeof(*ha_favorites));
            ha_favorite_count--;
        } else if(ha_favorite_count<128)snprintf(ha_favorites[ha_favorite_count++],160,"%s",data);
        else return package_acknowledge(action,0);
        char all[128*161]="";size_t used=0;
        for(int i=0;i<ha_favorite_count;i++)
            used+=(size_t)snprintf(all+used,sizeof(all)-used,"%s\n",ha_favorites[i]);
        extra_write_text("ha-favorites.txt",all);
        return package_acknowledge(action,1);
    }
    if(!strcmp(operation,"graph")) {
        int index=ha_entity_index(data);
        if(index<0 || !ha_entities[index].sensor)return package_acknowledge(action,0);
        ha_refresh();ha_fetch_history(data);
        char *message=NULL;size_t length=0;
        FILE *out=package_reply(&message,&length,action,1);
        if(!out)return 0;
        ha_row(out,0,&ha_entities[index]);ha_points(out,0,data);
        fprintf(out,",\"busy\":%d",service_history_busy());
        package_reply_end(out,&message);
        return 1;
    }
    int picking=!strcmp(operation,"devices");
    if(!picking && strcmp(operation,"dashboard"))return 0;
    char *end;long page=strtol(data,&end,10);
    if(end==data || *end!='\n' || page<0 || page>512 || strlen(end+1)>127)
        return package_acknowledge(action,0);
    int visible[512],per=picking ? 6 : 8,count=ha_visible(visible,picking,end+1);
    if(page*per>=count)page=count ? (count-1)/per : 0;
    if(!picking || !count)ha_refresh();
    char ids[1500]="";
    if(!picking) {
        for(int i=0;i<per && page*per+i<count;i++) {
            HAEntity *entity=&ha_entities[visible[page*per+i]];
            if(entity->sensor) {
                size_t used=strlen(ids);
                snprintf(ids+used,sizeof(ids)-used,"%s%s",used ? "," : "",entity->id);
            }
        }
        ha_fetch_history(ids);
    }
    char *message=NULL;size_t length=0;
    FILE *out=package_reply(&message,&length,action,1);
    if(!out)return 0;
    fprintf(out,",\"page\":%ld,\"count\":%d,\"configured\":%d,\"busy\":%d",
        page,count,!!*ha_url && !!*ha_token,service_busy());
    for(int i=0;i<per && page*per+i<count;i++) {
        HAEntity *entity=&ha_entities[visible[page*per+i]];
        ha_row(out,i,entity);
        if(!picking && entity->sensor)ha_points(out,i,entity->id);
    }
    package_reply_end(out,&message);
    return 1;
}
