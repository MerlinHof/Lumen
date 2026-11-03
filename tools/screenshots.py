#!/usr/bin/env python3
"""Render real native app views with original demo books, never device data."""
import html,os,shutil,subprocess,sys,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
import build
from check import compile_test,OUT
from PIL import Image,ImageDraw,ImageFont
os.chdir(ROOT);folder=ROOT/'build/gallery';folder.mkdir(parents=True,exist_ok=True)
shutil.rmtree(folder/'data',ignore_errors=True);(folder/'data').mkdir();(folder/'books').mkdir(exist_ok=True)
books=[('The Quiet Hours','LUMEN Studio'),('Field Notes','LUMEN Studio'),('A Walk Through Light','LUMEN Studio'),('Small Worlds','LUMEN Studio')]
for i,(title,author) in enumerate(books):
    cover=f'''<svg xmlns="http://www.w3.org/2000/svg" width="360" height="492" viewBox="0 0 360 492"><rect width="360" height="492" fill="#eee"/><circle cx="{95+i*48}" cy="265" r="68" fill="#aaa"/><path d="M0 410L120 240L240 390L360 285V492H0Z" fill="#555"/><path d="M0 458L150 355L260 450L360 365V492H0Z" fill="#222"/><text x="30" y="68" font-family="serif" font-size="30">{title.split(' ')[0]}</text><text x="30" y="104" font-family="serif" font-size="30">{' '.join(title.split(' ')[1:])}</text><text x="30" y="465" fill="white" font-size="12" letter-spacing="3">LUMEN STUDIO</text></svg>'''
    text='''<h1>The Quiet Hours</h1><p>There is a kind of morning that asks very little of us. A window, a cup of something warm, a page waiting to be turned.</p><p>We have grown used to the idea that everything must compete for our attention. The brighter screen. The newer notification. The story that never quite ends.</p><p>A book makes another offer. Stay here for a while. Follow one thought to its end. Let the room become quiet around you.</p><h2>A little room to think</h2><p>The best tools leave space for the thing we came to do. They make the ordinary action comfortable, the next step clear, and the interruption rare.</p>'''
    with zipfile.ZipFile(folder/'books'/f'{i:02d}.epub','w') as z:
        z.writestr('mimetype','application/epub+zip')
        z.writestr('META-INF/container.xml','<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0"><rootfiles><rootfile full-path="book.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')
        z.writestr('book.opf',f'<package xmlns="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="id"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="id">lumen-demo-{i}</dc:identifier><dc:title>{title}</dc:title><dc:creator>{author}</dc:creator><dc:language>en</dc:language><meta name="cover" content="cover"/></metadata><manifest><item id="cover" href="cover.svg" media-type="image/svg+xml"/><item id="coverpage" href="cover.xhtml" media-type="application/xhtml+xml"/><item id="text" href="text.xhtml" media-type="application/xhtml+xml"/></manifest><spine><itemref idref="coverpage"/><itemref idref="text"/></spine></package>')
        z.writestr('cover.svg',cover);z.writestr('cover.xhtml','<html xmlns="http://www.w3.org/1999/xhtml"><head><title>Cover</title><style>body{margin:0;padding:0}img{width:100%;height:auto}</style></head><body><img src="cover.svg"/></body></html>');z.writestr('text.xhtml','<html xmlns="http://www.w3.org/1999/xhtml"><head><title>'+title+'</title></head><body>'+text.replace('The Quiet Hours',title)*5+'</body></html>')
OUT.mkdir(parents=True,exist_ok=True)
binary=compile_test('gallery','tests/render_gallery.c',True);subprocess.run([str(binary)],check=True)
images=ROOT/'docs/images';images.mkdir(exist_ok=True)
for p in folder.glob('*.pgm'):Image.open(p).save(images/(p.stem+'.png'),optimize=True)
# A restrained documentation contact sheet. Every panel is an actual native render.
canvas=Image.new('RGB',(1920,1150),'#ebe9e3');draw=ImageDraw.Draw(canvas)
font=ImageFont.truetype(next(p for p in ['/System/Library/Fonts/Supplemental/Georgia.ttf','/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf'] if Path(p).exists()),70)
small=ImageFont.truetype(next(p for p in ['/System/Library/Fonts/Supplemental/Arial.ttf','/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'] if Path(p).exists()),25)
draw.text((80,48),'LUMEN',font=font,fill='#171717');draw.text((82,139),'A quieter kind of Kindle.',font=small,fill='#555')
for i,name in enumerate(['library','reader','apps']):
    x=80+i*604;y=222
    draw.rounded_rectangle((x-10,y-10,x+560,y+744),radius=26,fill='#252525')
    im=Image.open(images/(name+'.png')).convert('RGB').resize((550,734),Image.Resampling.LANCZOS)
    canvas.paste(im,(x,y));draw.text((x,y+775),{'library':'READ','reader':'FOCUS','apps':'EXPLORE'}[name],font=small,fill='#555')
canvas.save(images/'hero.png',optimize=True)
print('Native screenshots regenerated:',images)
