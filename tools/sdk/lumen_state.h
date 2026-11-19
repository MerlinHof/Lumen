#ifndef LUMEN_STATE_H
#define LUMEN_STATE_H
#include "lumen_storage.h"
#include <stdint.h>
#include <time.h>

enum { LUMEN_STATE_IDLE_MS=750, LUMEN_STATE_MAX_BYTES=262144 };
static struct {
    const char *name;
    unsigned char *saved;
    void *data;
    size_t size;
    uint32_t version;
    int64_t due;
    int dirty;
} lumen_state;

static inline int64_t lumen_state_now(void) {
    struct timespec now;
    if(clock_gettime(CLOCK_MONOTONIC,&now))return 0;
    return (int64_t)now.tv_sec*1000+now.tv_nsec/1000000;
}
/* The app supplies initial defaults or imports legacy data before registering.
   Version zero keeps an existing raw file format. Positive versions use a header.
   Return -1 on failure, 0 for defaults, or 1 for a restored record. */
static inline int lumen_state_register(const char *name,uint32_t version,void *data,size_t size) {
    if(!name || !*name || !data || !size || size>LUMEN_STATE_MAX_BYTES || lumen_state.saved)return -1;
    size_t header=version ? sizeof(uint32_t)*2 : 0,length=0;
    unsigned char *record=malloc(size+header);
    if(!record)return -1;
    int restored=lumen_storage_read(name,record,size+header,&length) && length==size+header;
    if(version && restored) {
        uint32_t values[2];
        memcpy(values,record,header);
        restored=values[0]==0x4c535431 && values[1]==version;
    }
    if(!restored && lumen_storage_exists(name)){free(record);return -1;}
    if(restored)memcpy(data,record+header,size);
    else memcpy(record+header,data,size);
    lumen_state.name=name;lumen_state.saved=record;lumen_state.data=data;
    lumen_state.size=size;lumen_state.version=version;
    return restored;
}
static inline void lumen_state_dirty(void) {
    if(!lumen_state.saved)return;
    lumen_state.dirty=1;
    lumen_state.due=lumen_state_now()+LUMEN_STATE_IDLE_MS;
}
/* Queue updates to an existing raw or versioned record. Callers can keep
   their existing file name and fall back to a direct write after import errors. */
static inline int lumen_state_queue(const char *name,const void *data,size_t size) {
    if(lumen_state.saved && lumen_state.data==data && lumen_state.size==size &&
       !strcmp(lumen_state.name,name)) {
        lumen_state_dirty();
        return 1;
    }
    return lumen_storage_write(name,data,size);
}
static inline int lumen_state_checkpoint(void) {
    if(!lumen_state.dirty)return 1;
    size_t header=lumen_state.version ? sizeof(uint32_t)*2 : 0;
    unsigned char *old=lumen_state.saved+header;
    if(memcmp(old,lumen_state.data,lumen_state.size)) {
        unsigned char *record=malloc(header+lumen_state.size);
        if(!record)return 0;
        if(header) {
            uint32_t values[]={0x4c535431,lumen_state.version};
            memcpy(record,values,header);
        }
        memcpy(record+header,lumen_state.data,lumen_state.size);
        int ok=lumen_storage_write(lumen_state.name,record,header+lumen_state.size);
        free(record);
        if(!ok)return 0;
        memcpy(old,lumen_state.data,lumen_state.size);
    }
    lumen_state.dirty=0;
    return 1;
}
static inline int lumen_state_wait(void) {
    if(!lumen_state.dirty)return 0;
    int64_t remain=lumen_state.due-lumen_state_now();
    return remain>=20 ? (int)remain : 20;
}
static inline void lumen_state_idle(void) {
    if(lumen_state.dirty && lumen_state_now()>=lumen_state.due && !lumen_state_checkpoint())
        lumen_state.due=lumen_state_now()+LUMEN_STATE_IDLE_MS;
}
static inline void lumen_state_release(void) {
    free(lumen_state.saved);
    memset(&lumen_state,0,sizeof(lumen_state));
}
#endif
