#!/usr/bin/env python3
from pathlib import Path
import zipfile,json
root=Path(__file__).resolve().parents[1];version=(root/'VERSION').read_text().strip();out=root/'dist'/f'LUMEN-{version}-sdk.zip';out.parent.mkdir(exist_ok=True)
header=(root/'tools/sdk/lumen_app.h').read_text().replace('../../lumen/system/device.h','device.h').replace('../../lumen/system/ui/icon_ids.h','icon_ids.h')
example='''#include "lumen_app.h"
static int count;
static void draw(void){char b[64];ui_paint_heading("Counter",0);snprintf(b,sizeof(b),"%d",count);ui_paint_text(28,260,544,100,b,PAINT_DISPLAY);lumen_button(100,430,400,60,"+1",1);}
static int input(const Input*in){if(in->kind==INPUT_TAP&&in->y>=430&&in->y<490)count++;return 1;}
int main(void){lumen_run(draw,input,NULL);return 0;}
'''
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED)as z:
 for name,text in {'lumen_app.h':header,'device.h':(root/'lumen/system/device.h').read_text(),'icon_ids.h':(root/'lumen/system/ui/icon_ids.h').read_text(),'build-app.py':(root/'tools/build-app.py').read_text(),'main.c':example,'package.json':json.dumps(dict(format='lumen-app-1',api=1,id='example.counter',name='Counter',version='1.0.0',sources=['main.c']),indent=2),'LICENSE':(root/'LICENSE').read_text(),'README.md':'# LUMEN App SDK 1\n\nInstall Zig 0.14.1 (or set ZIG). Run `python3 build-app.py package.json`. Upload the resulting .lumen-app in File Transfer → Apps. This does not compile LUMEN.\n\n'+(root/'docs/app-packages.md').read_text()}.items():z.writestr('LUMEN-SDK-1/'+name,text)
print(out)
