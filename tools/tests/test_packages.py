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
run('apply') # An unkept trial rolls back on next launch.
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
run('install',app);run('launch')
state=data/'app-state/lumen.sliding-puzzle/keep.txt';state.parent.mkdir(parents=True,exist_ok=True);state.write_text('keep')
run('install',app)
assert state.read_text()=='keep' and hashlib.sha256((base/'lumen/bin/lumen').read_bytes()).hexdigest()==core
print('PASS full runtime assets, trial rollback, interrupted activation recovery, keep, private-path rejection and independent app replacement.')
