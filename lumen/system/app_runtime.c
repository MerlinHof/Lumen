#include "app_runtime.h"
#include <string.h>

void app_runtime_init(AppRuntime *runtime,void (*leave)(AppId,void *),void *context) {
    memset(runtime,0,sizeof(*runtime));
    runtime->active=-1;
    runtime->leave=leave;
    runtime->context=context;
}
void app_runtime_activate(AppRuntime *runtime,AppId app) {
    if(runtime->active==app)return;
    if(runtime->active>=0 && runtime->leave)runtime->leave(runtime->active,runtime->context);
    runtime->active=app;
    runtime->generation++;
}
