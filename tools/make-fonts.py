from PIL import Image, ImageDraw, ImageFont
from pathlib import Path
import os
base=Path(__file__).resolve().parents[1]
root=Path(os.environ['LIBERATION_FONTS'])
extra=list(map(ord,'π√×÷−∞↵←→θΣ≠'))
extra+=sorted({ord(c) for f in ['i18n_catalog.h','system/ui/ui.c','apps/tool_views.h','apps/board_games.h'] for c in (base/'src'/f).read_text() if ord(c)>=256}-set(extra))
out=['static const unsigned ui_codepoints[]={'+','.join(map(str,extra))+'};','/* Rasterized from Liberation fonts (SIL Open Font License). */','typedef struct {int w,h; const unsigned char *pixels; const unsigned char *advance,*top,*bottom;} Font;']
for name,size,file in [('small',18,'LiberationSans-Regular.ttf'),('body',25,'LiberationSans-Regular.ttf'),('title',42,'LiberationSerif-Regular.ttf'),('editorial',32,'LiberationSerif-Regular.ttf'),('caption',15,'LiberationSans-Regular.ttf')]:
 f=ImageFont.truetype(str(root/file),size);cjk=ImageFont.truetype(str(Path(os.environ['MUPDF'])/'resources/fonts/droid/DroidSansFallbackFull.ttf'),size);w=size+8;h=size+12;data=bytearray();adv=[];tops=[];bottoms=[]
 for c in range(256+len(extra)):
  ch=chr(c) if 32<=c<256 else (chr(extra[c-256]) if c>=256 else ' ');im=Image.new('L',(w,h),255);ImageDraw.Draw(im).text((1,0),ch,font=cjk if ord(ch)>=0x2e80 else f,fill=0);bounds=im.point(lambda v:255 if v<160 else 0).getbbox();tops.append(bounds[1] if bounds else h);bottoms.append(bounds[3]-1 if bounds else 0);data.extend(im.tobytes());adv.append(min(w,round((cjk if ord(ch)>=0x2e80 else f).getlength(ch))))
 out += [f'static const unsigned char {name}_pixels[]={{'+','.join(map(str,data))+'};',f'static const unsigned char {name}_advance[]={{'+','.join(map(str,adv))+'};',f'static const unsigned char {name}_top[]={{'+','.join(map(str,tops))+'};',f'static const unsigned char {name}_bottom[]={{'+','.join(map(str,bottoms))+'};',f'static const Font {name}={{{w},{h},{name}_pixels,{name}_advance,{name}_top,{name}_bottom}};']
(base/'src/fonts.h').write_text('\n'.join(out))
