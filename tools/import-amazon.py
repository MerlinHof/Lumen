#!/usr/bin/env python3
# Local personal-library conversion helper. Upstream engines are supplied separately under their GPL licenses.
from pathlib import Path
import sys,types,re,zipfile,contextlib,io,importlib,struct,unicodedata,json,subprocess,xml.etree.ElementTree as ET,gc
import os,argparse
a=argparse.ArgumentParser();a.add_argument('--device',type=Path,default=Path('/Volumes/Kindle'));a.add_argument('--output',type=Path,required=True);a.add_argument('--dedrm',type=Path,required=True);a.add_argument('--mobitool',type=Path,required=True);a.add_argument('--device-report',type=Path);args=a.parse_args()
root=args.dedrm.resolve();pkg=types.ModuleType('personal_dedrm');pkg.__path__=[str(root)];sys.modules['personal_dedrm']=pkg;sys.path.insert(0,str(root))
compat=types.ModuleType('Crypto.Util.py3compat');compat.bchr=lambda n:bytes([n]);sys.modules['Crypto.Util.py3compat']=compat;sys.modules['ion']=importlib.import_module('personal_dedrm.ion')
from personal_dedrm import mobidedrm,kgenpids,kfxdedrm
from kfxlib import YJ_Book
serial=os.environ.get('LUMEN_KINDLE_SERIAL','')
if not serial and args.device_report:serial=re.search(r'Serial\s*:\s*"([^"]+)"',args.device_report.read_text()).group(1)
if not serial:raise SystemExit('Set LUMEN_KINDLE_SERIAL or pass --device-report; never publish your serial.')
device=args.device;raw=(device/'lumen/data/index-v3.bin').read_bytes();magic,size,count=struct.unpack_from('<III',raw);assert magic==0x4c554d32 and size==2312
lookup={}
for i in range(count):
 r=raw[12+i*size:12+(i+1)*size];ident=struct.unpack_from('<Q',r)[0];name=r[40:1576].split(b'\0')[0].decode();lookup[unicodedata.normalize('NFC',name[18:])]=(ident,name,struct.unpack_from('<q',r,16)[0])
base=args.output;(base/'imports').mkdir(parents=True,exist_ok=True);(base/'books').mkdir(exist_ok=True)
report=[]
for src in device.joinpath('documents').iterdir():
 if src.suffix.lower() not in ['.kfx','.azw3','.azw','.mobi']:continue
 key=unicodedata.normalize('NFC',src.name)
 if key not in lookup:print('SKIP: not in device index',src.name,flush=True);continue
 ident,devicepath,device_mtime=lookup[key];stem=f'{ident:016x}';out=base/'imports'/f'{stem}.epub';st=src.stat()
 try:
  if not out.exists():
   with contextlib.redirect_stdout(io.StringIO()),contextlib.redirect_stderr(io.StringIO()):
    if src.suffix.lower()=='.kfx':
     files=[src]+[f for f in (src.with_suffix('.sdr')/'assets').rglob('*') if f.is_file()]
     archive=base/'source.kfx-zip'
     with zipfile.ZipFile(archive,'w')as z:
      for f in files:z.write(f,str(f.relative_to(src.parent)))
     b=kfxdedrm.KFXZipBook(str(archive));b.processBook([serial]);b.getFile(str(base/'clear.kfx-zip'))
     out.write_bytes(YJ_Book(str(base/'clear.kfx-zip')).convert_to_epub());del b
    else:
     b=mobidedrm.MobiBook(str(src));rec,token=b.getPIDMetaInfo();pids=kgenpids.getPidList(rec,token,[serial],[]);b.processBook(pids);clear=base/'clear.azw3';b.getFile(str(clear))
     dest=base/'mobi';dest.mkdir(exist_ok=True)
     for f in dest.glob('*'):f.unlink()
     subprocess.run([str(args.mobitool),'-e','-o',str(dest),str(clear)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)
     ep=next(dest.glob('*.epub'));out.write_bytes(ep.read_bytes());del b
   gc.collect()
  (base/'imports'/f'{stem}.source').write_text(f'{st.st_size} {device_mtime}\n')
  with zipfile.ZipFile(out)as z:
   container=ET.fromstring(z.read('META-INF/container.xml'));opf=next(x for x in container.iter()if x.tag.endswith('rootfile')).attrib['full-path'];tree=ET.fromstring(z.read(opf));titles=[x.text for x in tree.iter()if x.tag.endswith('}title')];authors=[x.text for x in tree.iter()if x.tag.endswith('}creator')];title=next(filter(None,titles),src.stem);author=', '.join(filter(None,authors))
  (base/'books'/f'{stem}.meta').write_text(title.replace('\n',' ')+'\n'+author.replace('\n',' ')+'\n')
  report.append({'file':src.name,'id':stem,'status':'converted','title':title,'epub_bytes':out.stat().st_size});print('OK',len(report),src.name,flush=True)
 except Exception as e:
  out.unlink(missing_ok=True);report.append({'file':src.name,'id':stem,'status':'failed','error':str(e)[:250]});print('FAILED',src.name,str(e)[:160],flush=True)
 (base/'report.json').write_text(json.dumps(report,indent=2,ensure_ascii=False))
print('FINISHED',sum(r['status']=='converted'for r in report),'of',len(report),flush=True)
