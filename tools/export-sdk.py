#!/usr/bin/env python3
from pathlib import Path
import zipfile,json
root=Path(__file__).resolve().parents[1];version=(root/'VERSION').read_text().strip();out=root/'dist'/f'LUMEN-{version}-sdk.zip';out.parent.mkdir(exist_ok=True)
header=(root/'tools/sdk/lumen_app.h').read_text()
example='''#include "lumen_app.h"
static int count;
static void draw(void){char b[64];ui_paint_heading("Counter",0);snprintf(b,sizeof(b),"%d",count);ui_paint_text(28,260,544,100,b,PAINT_DISPLAY);lumen_button_action("increment",100,430,400,60,"+1",1);}
static int input(const Input*in){char id[16],gesture[16];if(in->kind==INPUT_COMPONENT&&lumen_string("id",id,sizeof(id))&&lumen_string("gesture",gesture,sizeof(gesture))&&!strcmp(id,"increment")&&!strcmp(gesture,"clicked"))count++;return 1;}
int main(void){lumen_run(draw,input,NULL);return 0;}
'''
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED)as z:
 for name,text in {'lumen_app.h':header,'lumen_http.h':(root/'tools/sdk/lumen_http.h').read_text(),'lumen_reading_activity.h':(root/'tools/sdk/lumen_reading_activity.h').read_text(),'lumen_text.h':(root/'tools/sdk/lumen_text.h').read_text(),'lumen_device_preferences.h':(root/'tools/sdk/lumen_device_preferences.h').read_text(),'lumen_json.h':(root/'tools/sdk/lumen_json.h').read_text(),'lumen_wire.h':(root/'tools/sdk/lumen_wire.h').read_text(),'lumen_storage.h':(root/'tools/sdk/lumen_storage.h').read_text(),'lumen_fsrs.h':(root/'tools/sdk/lumen_fsrs.h').read_text(),'FSRS-LICENSE.txt':(root/'lumen/assets/licenses/FSRS-LICENSE.txt').read_text(),'lumen_input.h':(root/'tools/sdk/lumen_input.h').read_text(),'lumen_icons.h':(root/'tools/sdk/lumen_icons.h').read_text(),'build-app.py':(root/'tools/build-app.py').read_text(),'main.c':example,'package.json':json.dumps(dict(format='lumen-app-1',api=1,id='example.counter',name='Counter',version='1.0.0',sources=['main.c']),indent=2),'LICENSE':(root/'LICENSE').read_text(),'README.md':'# LUMEN App SDK 1\n\nInstall Zig 0.14.1 (or set ZIG). Run `python3 build-app.py package.json`. Upload the resulting .lumen-app in File Transfer → Apps. This does not compile LUMEN.\n\n'+(root/'docs/app-packages.md').read_text()}.items():z.writestr('LUMEN-SDK-1/'+name,text)
print(out)
