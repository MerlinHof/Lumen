#!/usr/bin/env python3
from pathlib import Path
import zipfile,json
root=Path(__file__).resolve().parents[1];version=(root/'VERSION').read_text().strip();out=root/'dist'/f'LUMEN-{version}-sdk.zip';out.parent.mkdir(exist_ok=True)
header=(root/'tools/sdk/lumen_app.h').read_text()
example='''#include "lumen_app.h"
#include "lumen_storage.h"

static int count, dirty;

static void increment(void) {
    if(count < 1000000) { count++; dirty=1; }
}
static void draw(void) {
    char label[64];
    snprintf(label,sizeof(label),"Count: %d",count);
    lumen_page("Counter");
    lumen_component_button("increment",label,1);
}
static int input(const Input *event) {
    static const LumenAction actions[]={{"increment",increment}};
    return lumen_dispatch_click(event,actions,sizeof(actions)/sizeof(*actions));
}
static void save(void) {
    lumen_storage_checkpoint("counter",&count,sizeof(count),&dirty);
}
int main(void) {
    int saved;
    size_t length;
    if(lumen_storage_read("counter",&saved,sizeof(saved),&length) &&
       length==sizeof(saved) && saved>=0 && saved<=1000000)count=saved;
    lumen_run(draw,input,save);
    return 0;
}
'''
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED)as z:
 for name,text in {'lumen_app.h':header,'lumen_geometry.h':(root/'tools/sdk/lumen_geometry.h').read_text(),'lumen_markup.h':(root/'tools/sdk/lumen_markup.h').read_text(),'lumen_game.h':(root/'tools/sdk/lumen_game.h').read_text(),'lumen_state.h':(root/'tools/sdk/lumen_state.h').read_text(),'lumen_worker.h':(root/'tools/sdk/lumen_worker.h').read_text(),'lumen_http.h':(root/'tools/sdk/lumen_http.h').read_text(),'lumen_reading_activity.h':(root/'tools/sdk/lumen_reading_activity.h').read_text(),'lumen_text.h':(root/'tools/sdk/lumen_text.h').read_text(),'lumen_device_preferences.h':(root/'tools/sdk/lumen_device_preferences.h').read_text(),'lumen_json.h':(root/'tools/sdk/lumen_json.h').read_text(),'lumen_wire.h':(root/'tools/sdk/lumen_wire.h').read_text(),'lumen_storage.h':(root/'tools/sdk/lumen_storage.h').read_text(),'lumen_fsrs.h':(root/'tools/sdk/lumen_fsrs.h').read_text(),'FSRS-LICENSE.txt':(root/'lumen/assets/licenses/FSRS-LICENSE.txt').read_text(),'lumen_input.h':(root/'tools/sdk/lumen_input.h').read_text(),'lumen_icons.h':(root/'tools/sdk/lumen_icons.h').read_text(),'build-app.py':(root/'tools/build-app.py').read_text(),'main.c':example,'package.json':json.dumps(dict(format='lumen-app-2',api=2,entry='app',surfaces=['foreground'],id='example.counter',name='Counter',version='1.0.0',sources=['main.c']),indent=2),'LICENSE':(root/'LICENSE').read_text(),'README.md':'# LUMEN App SDK 2\n\nInstall Zig 0.14.1 (or set ZIG). Run `python3 build-app.py package.json`. Upload the resulting .lumen-app in File Transfer → Apps. This does not compile LUMEN.\n\n'+(root/'docs/app-packages.md').read_text()}.items():z.writestr('LUMEN-SDK-2/'+name,text)
print(out)
