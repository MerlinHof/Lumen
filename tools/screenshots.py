#!/usr/bin/env python3
"""Render real native app views with original demo books, never device data."""
import html,os,shutil,subprocess,sys,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
import build
from check import compile_test,OUT
from PIL import Image,ImageDraw,ImageFont
os.chdir(ROOT);folder=ROOT/'build/gallery';folder.mkdir(parents=True,exist_ok=True)
shutil.rmtree(folder/'apps',ignore_errors=True);shutil.rmtree(folder/'data',ignore_errors=True);(folder/'data').mkdir();(folder/'books').mkdir(exist_ok=True)
books=[('The Quiet Hours','LUMEN Studio'),('Field Notes','LUMEN Studio'),('A Walk Through Light','LUMEN Studio'),('Small Worlds','LUMEN Studio')]
serif=next(p for p in ['/System/Library/Fonts/Supplemental/Georgia.ttf','/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf'] if Path(p).exists())
sans=next(p for p in ['/System/Library/Fonts/Supplemental/Arial.ttf','/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'] if Path(p).exists())
for i,(title,author) in enumerate(books):
    # Original book jackets, rendered at twice the demo cover's logical size.
    cover=Image.new('RGB',(720,984),'white')
    art=Image.open(ROOT/'lumen/apps/home/scene.pgm').convert('RGB')
    if i%2:art=art.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
    art=art.resize((720,485),Image.Resampling.LANCZOS)
    cover.paste(art,(0,370))
    jacket=ImageDraw.Draw(cover)
    jacket.text((360,65),'L U M E N   E D I T I O N S',anchor='mt',
                font=ImageFont.truetype(sans,18),fill='#666')
    words=title.split()
    split=1 if len(words)<=3 else 2
    for line,text_line in enumerate([' '.join(words[:split]),' '.join(words[split:])]):
        jacket.text((360,147+line*91),text_line,anchor='mt',
                    font=ImageFont.truetype(serif,74),fill='#111')
    jacket.line((280,906,440,906),fill='#555',width=2)
    jacket.text((360,935),'L U M E N   S T U D I O',anchor='mt',
                font=ImageFont.truetype(sans,18),fill='#333')
    import io
    cover_bytes=io.BytesIO()
    cover.save(cover_bytes,format='PNG',optimize=True)
    text='''<h1>The Quiet Hours</h1><p>There is a kind of morning that asks very little of us. A window, a cup of something warm, a page waiting to be turned.</p><p>We have grown used to the idea that everything must compete for our attention. The brighter screen. The newer notification. The story that never quite ends.</p><p>A book makes another offer. Stay here for a while. Follow one thought to its end. Let the room become quiet around you.</p><h2>A little room to think</h2><p>The best tools leave space for the thing we came to do. They make the ordinary action comfortable, the next step clear, and the interruption rare.</p>'''
    with zipfile.ZipFile(folder/'books'/f'{i:02d}.epub','w') as z:
        z.writestr('mimetype','application/epub+zip')
        z.writestr('META-INF/container.xml','<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0"><rootfiles><rootfile full-path="book.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')
        z.writestr('book.opf',f'<package xmlns="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="id"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="id">lumen-demo-{i}</dc:identifier><dc:title>{title}</dc:title><dc:creator>{author}</dc:creator><dc:language>en</dc:language><meta name="cover" content="cover"/></metadata><manifest><item id="cover" href="cover.png" media-type="image/png"/><item id="coverpage" href="cover.xhtml" media-type="application/xhtml+xml"/><item id="text" href="text.xhtml" media-type="application/xhtml+xml"/></manifest><spine><itemref idref="coverpage"/><itemref idref="text"/></spine></package>')
        z.writestr('cover.png',cover_bytes.getvalue());z.writestr('cover.xhtml','<html xmlns="http://www.w3.org/1999/xhtml"><head><title>Cover</title><style>body{margin:0;padding:0}img{width:100%;height:auto}</style></head><body><img src="cover.png"/></body></html>');z.writestr('text.xhtml','<html xmlns="http://www.w3.org/1999/xhtml"><head><title>'+title+'</title></head><body>'+text.replace('The Quiet Hours',title)*5+'</body></html>')
OUT.mkdir(parents=True,exist_ok=True)
binary=compile_test('gallery','tools/tests/render_gallery.c',True);subprocess.run([str(binary)],check=True)
images=ROOT/'docs/images';images.mkdir(exist_ok=True)
for p in folder.glob('*.pgm'):
    if not p.stem.startswith('face-'):Image.open(p).save(images/(p.stem+'.png'),optimize=True)
# Preserve every screenshot pixel when laying out the public overview.
images=ROOT/'docs/images'
canvas=Image.new('RGB',(2100,1220),'#f2f0e9');draw=ImageDraw.Draw(canvas)
serif=next(p for p in ['/System/Library/Fonts/Supplemental/Georgia.ttf','/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf'] if Path(p).exists())
sans=next(p for p in ['/System/Library/Fonts/Supplemental/Arial.ttf','/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'] if Path(p).exists())
font=ImageFont.truetype(serif,82);small=ImageFont.truetype(sans,26)
draw.text((88,47),'LUMEN',font=font,fill='#181818')
draw.text((1860,87),(ROOT/'VERSION').read_text().strip(),font=small,fill='#55534d')
draw.text((94,157),'A calm personal computer for your Kindle.',font=small,fill='#55534d')
for i,name in enumerate(['home','library','calculator']):
    x=88+i*660;y=258
    draw.rounded_rectangle((x-8,y-8,x+615,y+815),radius=16,fill='#d7d3c8')
    draw.rounded_rectangle((x-8,y-8,x+607,y+807),radius=16,fill='#262624')
    canvas.paste(Image.open(images/(name+'.png')).convert('RGB'),(x,y))
    draw.text((x,y+842),{'home':'01   HOME','library':'02   LIBRARY','calculator':'03   CALCULATOR'}[name],font=small,fill='#55534d')
draw.text((94,1161),'Native software renders / original demo content / 600 x 800 per screen',font=ImageFont.truetype(sans,20),fill='#747168')
canvas.save(images/'hero.png',optimize=True)

# Contact sheet of the actual 76px artwork, using the same default app order.
import json,re,textwrap
enum=(ROOT/'lumen/system/app_runtime.h').read_text().split('typedef enum {',1)[1].split('APP_COUNT',1)[0]
ids=re.findall(r'APP_[A-Z0-9_]+',enum)
catalog=(ROOT/'lumen/apps/catalog.c').read_text().split('static const AppRoute',1)[0]
names=dict(re.findall(r'\{(APP_[A-Z0-9_]+), "([^"]+)", \d+\}',catalog))
order_source=(ROOT/'lumen/apps/launcher/order.h').read_text()
def app_group(name):
    return re.findall(r'APP_[A-Z0-9_]+',order_source.split(name+'[] = {',1)[1].split('};',1)[0])
first,games=app_group('launcher_first'),app_group('launcher_games')
order=first+[id for id in ids if id not in first and id not in games]+games
order=[id for id in order if id in names]
translations=json.loads((ROOT/'lumen/system/translations.json').read_text())
package_source=(ROOT/'lumen/system/app_packages.c').read_text()
keys=re.findall(r'"([^"]+)"',package_source.split('keys[APP_COUNT]={',1)[1].split('};',1)[0])
runtime=json.loads((ROOT/'tools/config/runtime.json').read_text())
packaged={json.loads(p.read_text())['id']: p.parent/'assets/icon.pgm'
          for p in (ROOT/'lumen/apps').glob('*/package.json')}
plate=Image.new('RGB',(1120,1070),'white');ink=ImageDraw.Draw(plate)
ink.text((36,26),'50 engraved app icons',font=ImageFont.truetype(serif,39),fill='#191919')
ink.text((38,85),'Actual LUMEN artwork, shown at its native 76 x 76 pixels.',font=ImageFont.truetype(sans,17),fill='#686868')
label_font=ImageFont.truetype(sans,14)
for position,id in enumerate(order):
    x=position%8*140;y=140+position//8*130
    key=keys[ids.index(id)]
    artwork=packaged.get(key)
    if artwork is None:
        artwork=ROOT/runtime[f'lumen/home/icons/{key}.pgm']
    plate.paste(Image.open(artwork).convert('RGB'),(x+32,y))
    label=translations.get(names[id],[names[id]]*3)[1]
    for j,line in enumerate(textwrap.wrap(label,18)):
        ink.text((x+70,y+88+j*18),line,font=label_font,fill='#333',anchor='mt')
plate.save(images/'icons.png',optimize=True)
print('Native screenshots regenerated:',images)

# Representative faces, actual native render crops, arranged for comparison.
faces=[4,7,8,1,5,9,12,13,6,2,10,11,3,0]
labels=['Classic numbers','Roman','Railway','Minimal','Orbit','Bauhaus','Sundial · 24 h','Compass','Petal dial','Typography','Editorial','Hour by hour','Night','Classic']
sheet=Image.new('RGB',(1200,((len(faces)+3)//4)*300),'white')
draw=ImageDraw.Draw(sheet)
for i,(face,label)in enumerate(zip(faces,labels)):
    crop=Image.open(folder/f'face-{face:02d}.pgm').crop((35,200,565,690)).convert('RGB')
    crop.thumbnail((270,250),Image.Resampling.LANCZOS)
    x=(i%4)*300+(300-crop.width)//2;y=(i//4)*300
    sheet.paste(crop,(x,y))
    draw.text(((i%4)*300+150,y+268),label,anchor='mm',font=ImageFont.truetype(sans,18),fill='#171717')
sheet.save(images/'clock-faces.png',optimize=True)
