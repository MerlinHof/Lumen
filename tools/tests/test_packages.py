"""Full-runtime transaction recovery and independent app updates, no network."""
from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys,zipfile
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
from check import compile_test
binary=compile_test('package-runner','tools/tests/package_runner.c')
base=ROOT/'build/tests/package-system';shutil.rmtree(base,ignore_errors=True)
data=base/'lumen/data';data.mkdir(parents=True)
old={'lumen/bin/lumen':b'old executable','lumen/launch.sh':b'old launcher','lumen/boot.sh':b'old obsolete launcher','lumen/data/private.txt':b'personal','lumen/apps/user-app/custom.txt':b'independent app','documents/my-book.epub':b'book'}
for rel,b in old.items():p=base/rel;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b)
def run(op,*args,ok=True,env=None):
 p=subprocess.run([str(binary),str(data),op,*map(str,args)],env=env);assert (p.returncode==0)==ok,(op,p.returncode);return p.returncode
def archive(path,files,**meta):
 m=dict(format='lumen-system-1',version='9.1',files=[dict(path=p,size=len(b),sha256=hashlib.sha256(b).hexdigest())for p,b in files.items()]);m.update(meta)
 with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED)as z:
  z.writestr('manifest.json',json.dumps(m))
  for p,b in files.items():z.writestr('payload/'+p,b)
new={'lumen/bin/lumen':b'new executable','lumen/launch.sh':b'new launcher','lumen/fonts/new.ttf':b'new resource'}
a=base/'update.zip';archive(a,new)
run('stage',a);run('apply')
for rel,b in new.items():assert (base/rel).read_bytes()==b
assert not (base/'lumen/boot.sh').exists()
run('apply') # A healthy trial survives a normal launch.
assert (base/'lumen/bin/lumen').read_bytes()==new['lumen/bin/lumen']
run('rollback')
for rel,b in old.items():assert (base/rel).read_bytes()==b
assert not(base/'lumen/fonts/new.ttf').exists()
run('stage',a)
env=dict(os.environ,LUMEN_TEST_UPDATE_CRASH='2');assert run('apply',ok=False,env=env)==73
run('apply')
for rel,b in old.items():assert (base/rel).read_bytes()==b
run('stage',a);run('apply');run('keep');run('apply')
for rel,b in new.items():assert(base/rel).read_bytes()==b
for rel in ['lumen/data/private.txt','lumen/apps/user-app/custom.txt','documents/my-book.epub']:assert(base/rel).read_bytes()==old[rel]
for bad in ['../escape','lumen/data/private.txt','lumen/apps/user-app/custom.txt']:
 archive(a,{bad:b'bad'});run('stage',a,ok=False)
 assert(base/'lumen/data/private.txt').read_bytes()==b'personal'
app=ROOT/'build/apps/host/lumen.sliding-puzzle/lumen.sliding-puzzle.lumen-app'
core=hashlib.sha256((base/'lumen/bin/lumen').read_bytes()).hexdigest()
run('install',app)
legacy=data/'app-state/lumen.sliding-puzzle/keep.txt';legacy.parent.mkdir(parents=True,exist_ok=True);legacy.write_text('keep')
run('launch')
state=base/'lumen/appdata/lumen.sliding-puzzle/keep.txt'
assert not legacy.exists() and state.read_text()=='keep'
run('install',app)
assert state.read_text()=='keep' and hashlib.sha256((base/'lumen/bin/lumen').read_bytes()).hexdigest()==core
print('PASS full runtime assets, trial rollback, interrupted activation recovery, keep, private-path rejection and independent app replacement.')

# The ports exercise the same system input and paint protocol as user packages.
def app_session(key,events):
 folder=base/'lumen/data/app-state'/key;folder.mkdir(parents=True,exist_ok=True)
 app=ROOT/'build/apps/host'/key/'app'
 wire=''.join(json.dumps(e,separators=(',',':'))+'\n' for e in [dict(event='open',language=1),*events])
 result=subprocess.run([str(app)],input=wire,env=dict(os.environ,LUMEN_APP_DATA=str(folder)),
                       capture_output=True,text=True,check=True,timeout=12)
 return [json.loads(line) for line in result.stdout.splitlines()],folder

def tap(x,y):
 return dict(event='input',kind=1,x=x,y=y)

for app,x,y,help_x in [
 ('bingo',70,220,490),('connect-four',70,220,490),('hex',60,155,490),
 ('nonograms',177,291,490),('solitaire',50,165,368),('wordle',290,685,550),
 ('periodic-table',21,150,None)
]:
 events=[tap(x,y)]
 if app=='wordle': events.append(dict(event='text',action=1,text='PAPER'))
 if help_x: events.append(tap(help_x,92))
 frames,folder=app_session('lumen.'+app,events)
 assert any(frame.get('ui') for frame in frames),app
 assert any(folder.glob('*.bin')),app
 if help_x: assert any(frame.get('request')=='reader' for frame in frames),app

commands=['cd /tmp','export LUMEN_SESSION_TEST=works','pwd',
          'printf "%s\\n" "$LUMEN_SESSION_TEST"','cd /','pwd','ls /','false',r"printf '\033[31mRED\033[0m\n'",'  clear  ']
frames,folder=app_session('lumen.terminal',[dict(event='text',action=1,text=c) for c in commands])
lines=[c.get('text') for f in frames for c in f.get('ui',[]) if c.get('type')=='mono']
assert '/tmp' in lines or '/private/tmp' in lines
assert '/' in lines and any('bin' in line.split() for line in lines) and 'works' in lines and '[exit 1]' in lines
assert 'history.bin' not in lines and (folder/'history.bin').is_file()
assert 'RED' in lines and not any('\x1b' in line for line in lines if line)
assert len([c for c in frames[-1]['ui'] if c.get('type')=='mono'])==1
print('PASS standalone app ports, Reader help, private saves and persistent Terminal shell.')

# Read the wall runs actually published by the executable and prove the exit is reachable.
frames,_=app_session('lumen.maze',[tap(490,92)])
walls=set()
for cmd in frames[0]['ui']:
 if cmd['type']=='surface' and cmd['y']>=140 and cmd['y']<700 and cmd['gray']==38:
  row=(cmd['y']-140)//16
  start=(cmd['x']-36)//16
  walls.update((col,row) for col in range(start,start+cmd['w']//16))
seen={(1,1)}
queue=[(1,1)]
for x,y in queue:
 for spot in ((x+1,y),(x-1,y),(x,y+1),(x,y-1)):
  if 0<=spot[0]<33 and 0<=spot[1]<35 and spot not in walls and spot not in seen:
   seen.add(spot)
   queue.append(spot)
assert walls and (31,33) in seen
assert any(f.get('request')=='intent' and f.get('operation')=='open_document' for f in frames)
print('PASS packaged Maze renders a solvable board and opens help through a Reader intent.')

frames,folder=app_session('lumen.knights-tour',[tap(130,350),tap(430,92)])
assert (folder/'tour.bin').read_bytes()[17]==2
assert any(f.get('request')=='intent' for f in frames)
print('PASS packaged Knight’s Tour legal move, private save and Reader help.')

frames,_=app_session('lumen.quotes',[
 dict(event='service',action=1,ok=1,count=1,page=0,quote0='A quoted passage',
      book0='Example book',ref0='1234567890abcdef:2'),
 tap(100,160)])
assert any(f.get('operation')=='list' for f in frames)
assert any(f.get('operation')=='open' and f.get('text')=='1234567890abcdef:2' for f in frames)
assert any(c.get('text')=='A quoted passage' for f in frames for c in f.get('ui',[]))
print('PASS packaged Quotes renders annotations and uses stable book references.')
