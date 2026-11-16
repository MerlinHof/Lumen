/* Preserve launcher preferences by package key. The binary reader is only
 * used to import locally saved orders from past versions. */
enum { OLD_BUILTIN_COUNT=55, OLD_SLOT_COUNT=87 };
static int launcher_save(void) {
    char output[APP_LIMIT*68],file[2048];
    size_t used=0;
    for(int i=0;i<APP_LIMIT;i++) {
        int id=launcher_order[i];
        const char *key=app_package_key(id);
        if(!*key)continue;
        int n=snprintf(output+used,sizeof(output)-used,"%d %s\n",launcher_visible[id],key);
        if(n<0 || (size_t)n>=sizeof(output)-used)return 0;
        used+=(size_t)n;
    }
    local_path(file,sizeof(file),"launcher-order.txt");
    return atomic_write(file,output,used);
}
static void launcher_apply(const int *ordered,int count) {
    int seen[APP_LIMIT]={0},next[APP_LIMIT],used=0;
    for(int i=0;i<count;i++) {
        int id=ordered[i];
        if(id>=0 && !seen[id]){next[used++]=id;seen[id]=1;}
    }
    for(int i=0;i<APP_LIMIT;i++) {
        int id=launcher_order[i];
        if(!seen[id])next[used++]=id;
    }
    memcpy(launcher_order,next,sizeof(next));
}
static int launcher_load(void) {
    char file[2048],key[64];
    local_path(file,sizeof(file),"launcher-order.txt");
    FILE *input=fopen(file,"r");
    if(!input)return 0;
    int ordered[APP_LIMIT],count=0,visible;
    while(count<APP_LIMIT && fscanf(input,"%d %63s",&visible,key)==2) {
        int id=app_package_find(key);
        if(id<0)continue;
        launcher_visible[id]=!!visible;
        ordered[count++]=id;
    }
    fclose(input);
    launcher_apply(ordered,count);
    return 1;
}
static int launcher_import(const char *name,int count) {
    int config[OLD_SLOT_COUNT*2],seen[OLD_SLOT_COUNT]={0},mapped[OLD_SLOT_COUNT],order[OLD_SLOT_COUNT];
    if(!app_state_load(name,config,sizeof(int)*count*2))return 0;
    for(int i=0;i<count;i++)
        if(config[i]<0 || config[i]>=count || seen[config[i]]++)return 0;
    for(int i=0;i<count;i++)mapped[i]=app_package_find(app_legacy_key(i));
    if(count>OLD_BUILTIN_COUNT) {
        char path[2048],key[64];
        snprintf(path,sizeof(path),"%s/../apps/slots.txt",data_root);
        FILE *file=fopen(path,"r");
        if(file) {
            int slot;
            while(fscanf(file,"%d %63s",&slot,key)==2)
                if(slot>=OLD_BUILTIN_COUNT && slot<count)mapped[slot]=app_package_find(key);
            fclose(file);
        }
    }
    for(int i=0;i<count;i++) {
        order[i]=mapped[config[i]];
        if(mapped[i]>=0)launcher_visible[mapped[i]]=!!config[i+count];
    }
    launcher_apply(order,count);
    return 1;
}
static void v1_init(void) {
    u15_init();
    if(v1_ready)return;
    v1_ready=1;
    launcher_defaults();
    if(!launcher_load()) {
        int imported=launcher_import("launcher-packages.bin",OLD_SLOT_COUNT) ||
                     launcher_import("launcher-36.bin",OLD_BUILTIN_COUNT) ||
                     launcher_import("launcher-31.bin",OLD_BUILTIN_COUNT);
        for(int count=OLD_BUILTIN_COUNT;count>=52 && !imported;count--)
            imported=launcher_import("launcher-16.bin",count);
        if(imported)launcher_save();
    }
    media_scan();
}
static void v1_save(void) {
    if (!v1_ready || !v1_dirty)
        return;
    if(launcher_save())v1_dirty=0;
}
#include "../../system/ui/chess_icons.h"
