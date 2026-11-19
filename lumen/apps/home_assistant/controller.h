#include "state.h"
#include "network.h"
#include "model.h"

static void ha_init(void) {
    ha_configure();
    char *favorites=ha_read("ha-favorites.txt",128*161);
    if(favorites) {
        char *save;
        for(char *id=strtok_r(favorites,"\n",&save);id && ha_favorite_count<128;
            id=strtok_r(NULL,"\n",&save)) {
            if(strlen(id)<sizeof(ha_favorites[0]))
                snprintf(ha_favorites[ha_favorite_count++],sizeof(ha_favorites[0]),"%s",id);
        }
        free(favorites);
    }
    ha_parse();
    ha_history_parse(NULL);
}
static void ha_refresh(void) {
    if(states_worker || time(NULL)<ha_refresh_at)return;
    ha_refresh_at=time(NULL)+5;
    states_worker=ha_worker("/api/states",NULL,"ha-states.json",2097153,8);
}
static void ha_update(void) {
    int result=ha_worker_done(&states_worker);
    if(result>0)ha_parse();
    if(result && refresh_pending) {
        refresh_pending=0;
        ha_refresh_at=0;
    }
    result=ha_worker_done(&history_worker);
    if(result>0) {
        ha_history_parse(ha_history_pending);
        snprintf(ha_history_key,sizeof(ha_history_key),"%s",ha_history_pending);
        ha_history_at=time(NULL);ha_history_span=ha_pending_span;
    }
    if(result<0)ha_history_retry=time(NULL)+12;
    ha_refresh();
}
static void ha_fetch_history(const char *ids,int span) {
    time_t now=time(NULL);
    if(!*ids || strlen(ids)>=sizeof(ha_history_key) ||
       strspn(ids,"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.,")!=strlen(ids) ||
       (!strcmp(ha_history_key,ids) && span==ha_history_span && now-ha_history_at<300) ||
       (!strcmp(ha_history_pending,ids) && span==ha_pending_span && now<ha_history_retry))return;
    if(history_worker) {
        if(!strcmp(ha_history_pending,ids) && span==ha_pending_span)return;
        lumen_worker_cancel(&history_worker);
    }
    time_t start=now-(span>0 ? span : 24)*3600;
    struct tm utc;
    char stamp[32],until[32],endpoint[1800];
    gmtime_r(&start,&utc);
    strftime(stamp,sizeof(stamp),"%Y-%m-%dT%H:%M:%SZ",&utc);
    gmtime_r(&now,&utc);
    strftime(until,sizeof(until),"%Y-%m-%dT%H:%M:%SZ",&utc);
    snprintf(endpoint,sizeof(endpoint),
        "/api/history/period/%s?filter_entity_id=%s&end_time=%s&minimal_response&no_attributes",
        stamp,ids,until);
    history_worker=ha_worker(endpoint,NULL,"ha-history.json",8388609,30);
    snprintf(ha_history_pending,sizeof(ha_history_pending),"%s",ids);
    ha_pending_span=span;
    ha_history_retry=history_worker ? 0 : now+12;
}
static void ha_device(Device *device,const HAEntity *entity) {
    *device=(Device){.sensor=entity->sensor,.saved=ha_favorite(entity->id)>=0};
    snprintf(device->id,sizeof(device->id),"%s",entity->id);
    snprintf(device->name,sizeof(device->name),"%s",entity->name);
    snprintf(device->value,sizeof(device->value),"%s",entity->state);
    snprintf(device->unit,sizeof(device->unit),"%s",entity->unit);
    for(int i=0;i<8;i++) {
        const HAHistory *history=&ha_history[i];
        if(strcmp(history->id,entity->id))continue;
        device->count=history->count;
        for(int k=0;k<history->count;k++) {
            device->t[k]=history->time[k];
            device->v[k]=history->value[k];
        }
        break;
    }
}
static int ha_save_favorites(void) {
    char text[128*161];
    size_t used=0;
    for(int i=0;i<ha_favorite_count;i++)
        used+=snprintf(text+used,sizeof(text)-used,"%s\n",ha_favorites[i]);
    return lumen_storage_write("ha-favorites.txt",text,used);
}
static void ha_toggle_favorite(const char *id) {
    int i=ha_favorite(id);
    if(i<0) {
        if(ha_favorite_count==128)return;
        snprintf(ha_favorites[ha_favorite_count++],sizeof(ha_favorites[0]),"%s",id);
        if(!ha_save_favorites())ha_favorite_count--;
    } else {
        char previous[160];
        memcpy(previous,ha_favorites[i],sizeof(previous));
        size_t bytes=(--ha_favorite_count-i)*sizeof(ha_favorites[0]);
        memmove(ha_favorites+i,ha_favorites+i+1,bytes);
        if(!ha_save_favorites()) {
            memmove(ha_favorites+i+1,ha_favorites+i,bytes);
            memcpy(ha_favorites[i],previous,sizeof(previous));
            ha_favorite_count++;
        }
    }
}
static void ha_toggle_device(const Device *device) {
    const char *id=device->id;
    if(states_worker || !device->saved || device->sensor ||
       strspn(id,"abcdefghijklmnopqrstuvwxyz0123456789_.")!=strlen(id))return;
    const char *dot=strchr(id,'.');
    if(!dot || dot-id>20)return;
    char domain[21],endpoint[80],payload[192];
    snprintf(domain,sizeof(domain),"%.*s",(int)(dot-id),id);
    if(strcmp(domain,"light") && strcmp(domain,"switch") &&
       strcmp(domain,"fan") && strcmp(domain,"input_boolean"))return;
    snprintf(endpoint,sizeof(endpoint),"/api/services/%s/toggle",domain);
    snprintf(payload,sizeof(payload),"{\"entity_id\":\"%s\"}",id);
    states_worker=ha_worker(endpoint,payload,NULL,2097153,8);
    refresh_pending=states_worker!=0;
}
