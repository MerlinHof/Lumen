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
def app_session(key,events,legacy=None):
 folder=base/'lumen/data/app-state'/key;folder.mkdir(parents=True,exist_ok=True)
 app=ROOT/'build/apps/host'/key/'app'
 wire=''.join(json.dumps(e,separators=(',',':'))+'\n' for e in [dict(event='open',language=1),*events])
 env=dict(os.environ,LUMEN_APP_DATA=str(folder))
 if legacy is not None:env['LUMEN_LEGACY_DATA']=str(legacy)
 result=subprocess.run([str(app)],input=wire,env=env,
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

frames,_=app_session('lumen.minesweeper',[
    dict(event='input',kind=12,x=110,y=210),tap(110,210),tap(340,340),tap(490,92)])
assert any(c.get('text')=='1 / 12' for f in frames for c in f.get('ui',[]))
assert any(f.get('operation')=='open_document' for f in frames)
frames,_=app_session('lumen.hangman',[
    dict(event='service',action=1,ok=1,word='INK'),
    *[tap(50+54*i,470) for i in (0,1,2,3,4,5)]])
assert any(f.get('service')=='dictionary' for f in frames)
assert any('INK' in c.get('text','') for f in frames for c in f.get('ui',[]))
for key in ('lumen.tetris','lumen.snake'):
    frames,_=app_session(key,[tap(480,145),dict(event='tick'),dict(event='pause'),dict(event='resume')])
    assert not frames[0]['tick'] and 20<=frames[1]['tick']<=650 and frames[-1]['tick']==0
    assert frames[1]['ui']!=frames[2]['ui']
print('PASS packaged Minesweeper, Hangman, Tetris and Snake input, dictionary, timing and pause.')

frames,folder=app_session('lumen.flashcards',[
    tap(550,92),dict(event='text',action=1,text='Physics'),
    dict(event='text',action=2,text='Force?'),dict(event='text',action=3,text='Mass times acceleration.'),
    tap(100,640),tap(340,744)])
assert (folder/'cards.bin').is_file() and (folder/'reviews.tsv').read_text().split('\t')[2]=='3'
assert any(c.get('text')=='Mass times acceleration.' for f in frames for c in f.get('ui',[]))
print('PASS packaged Flashcards creates cards, reveals answers and records FSRS reviews.')

frames,folder=app_session('lumen.sokoban',[tap(430,92)])
assert (folder/'warehouse.bin').is_file()
assert any(c.get('type')=='sprite' and c.get('transparent')==1 for f in frames for c in f.get('ui',[]))
assert any(f.get('operation')=='open_document' for f in frames)
frames,folder=app_session('lumen.crossword',[tap(100,545),dict(event='text',action=10,text='X'),tap(430,92)])
assert any(f.get('operation')=='clues' for f in frames)
assert any(f.get('request')=='input' and f.get('action')==10 for f in frames)
assert any(f.get('request')=='notify' and 'letters' in f.get('text','') for f in frames)
assert any(f.get('operation')=='open_document' for f in frames)
print('PASS packaged Sokoban assets, save and help, and Crossword clues, editing, validation and help.')

def component(id,gesture='clicked',text=None):
    event=dict(event='component',id=id,gesture=gesture,changed=int(text is not None))
    if text is not None:event['text']=text
    return event

frames,folder=app_session('lumen.braintrain',[component('exercise0'),tap(100,735),tap(150,300),tap(100,735)])
assert len([c for c in frames[0]['ui'] if c['type']=='component_card'])==6
assert (folder/'training.bin').is_file()
frames,folder=app_session('lumen.languages',[
    component('pair','selected','1'),component('practice','selected','2'),tap(100,425),
    dict(event='text',action=1,text='Translation'),dict(event='service',action=1,ok=1,match=1),
    tap(545,92),dict(event='page',page=2),dict(event='back')])
assert any(c.get('type')=='component_segments' for f in frames for c in f.get('ui',[]))
assert any(f.get('service')=='text' and f.get('operation')=='equivalent' for f in frames)
assert any(f.get('back') for f in frames) and not frames[-1].get('back')
assert (folder/'progress.bin').is_file() and (folder/'pair.bin').is_file()
print('PASS packaged BrainTrain practice cards and Languages segments, grading, glossary and saves.')

frames,folder=app_session('lumen.insights',[
    dict(event='service',action=1,ok=1,goal=30,day0=900,total=4500,sessions=3,remaining=-1),
    tap(440,500),dict(event='text',action=3,text='25'),
    component('stats','selected','1'),dict(event='service',action=2,ok=1,page=0,count=1,
         title0='Reading sample',seconds0=900,pages0=12,sessions0=3,position0=12,length0=60)])
assert (folder/'goal.bin').is_file()
assert any(c.get('text')=='15 / 30 min' for f in frames for c in f.get('ui',[]))
assert any(c.get('text')=='Reading sample' for f in frames for c in f.get('ui',[]))
assert any(f.get('operation')=='books' for f in frames)
print('PASS packaged Reading Statistics uses reading summaries, goal persistence and paged book activity.')


import datetime,struct
today=datetime.date.today().strftime('%Y%m%d')
legacy=base/'legacy-dates'
(legacy/'calendar').mkdir(parents=True)
(legacy/'journals17').mkdir()
(legacy/'calendar'/f'{today}.txt').write_text('Old appointment')
record=struct.pack('=iii',1,int(today),4)+b'Legacy journal'.ljust(6*8192,b'\0')
(legacy/'journals17'/f'{today}.bin').write_bytes(struct.pack('=II',0x4c554d16,len(record))+record)
frames,folder=app_session('lumen.journal',[tap(100,170),tap(100,250),
    dict(event='text',action=10,text='Two lines\nSecond line'),dict(event='back')],legacy)
assert any(f.get('request')=='multiline' and f.get('text')=='Legacy journal' for f in frames)
assert b'Two lines\nSecond line' in (folder/f'{today}.bin').read_bytes()
frames,folder=app_session('lumen.calendar',[tap(545,95),dict(event='text',action=1,text='Updated appointment'),
    component('view','selected','1'),component('view','selected','0'),component('view','selected','3')],legacy)
assert any(f.get('request')=='multiline' and f.get('text')=='Old appointment' for f in frames)
assert any(f.get('operation')=='month' for f in frames)
assert (folder/f'{today}.txt').read_text()=='Updated appointment'
assert (legacy/'calendar'/f'{today}.txt').read_text()=='Old appointment'
assert {c.get('selected') for f in frames for c in f.get('ui',[]) if c.get('type')=='component_segments'}=={0,1,2,3}
print('PASS packaged Journal and Calendar import legacy data, edit using multiline input and save privately.')

frames,folder=app_session('lumen.chess',[component('opponent','selected','0'),
    tap(333,615),tap(333,485),dict(event='tick'),tap(425,760)])
saved=(folder/'game.bin').read_bytes()
assert saved[52]==0 and saved[36]==1 and struct.unpack_from('=i',saved,64)[0]==1
assert any(f.get('tick')==80 for f in frames)
assert any(c.get('rotated')==1 for f in frames for c in f.get('ui',[]))
frames,_=app_session('lumen.chess',[tap(80,760)])
assert (folder/'game.bin').read_bytes()[52]==1
print('PASS packaged Chess legal pawn move, computer reply, piece rotation and persisted undo.')

(legacy/'weather-city.txt').write_text('Rosenheim')
frames,folder=app_session('lumen.weather',[
    dict(event='service',action=1,ok=1,forecast=1,temp=19,code=2,daylight_now=1,place='Rosenheim'),
    tap(490,92),dict(event='text',action=2,text='Paris')],legacy)
assert any(f.get('operation')=='open' and f.get('text')=='Rosenheim' for f in frames)
assert any(f.get('operation')=='refresh' and f.get('text')=='Paris' for f in frames)
assert any(c.get('type')=='weather_symbol' and c.get('code')==2 for f in frames for c in f.get('ui',[]))
assert (folder/'city.txt').read_text()=='Paris'
print('PASS packaged Weather renders cached forecasts, imports location and requests refresh.')

catalog=dict(event='service',action=1,ok=1,data='\n'.join(['lumen.library']*10))
frames,folder=app_session('lumen.home',[catalog,dict(event='page',page=1)])
assert any(c.get('type')=='pager' and c.get('page')==1 for c in frames[-1]['ui'])
frames,_=app_session('lumen.home',[catalog])
assert any(c.get('type')=='pager' and c.get('page')==1 for c in frames[-1]['ui'])
frames,_=app_session('lumen.home',[dict(catalog,data='lumen.library')])
assert any(c.get('type')=='pager' and c.get('page')==0 for c in frames[-1]['ui'])
print('PASS Home restores its page after a fresh process and clamps it after catalog changes.')

frames,folder=app_session('lumen.todo',[
    dict(event='text',action=1,text='Older task'),
    dict(event='text',action=1,text='New task\nSecond line'),
    component('0','pinned'),component('0','toggled'),component('0'),
    dict(event='text',action=2,text='Edited task'),
    tap(490,92)])
lists=[c for f in frames for c in f.get('ui',[]) if c.get('type')=='flow_list']
assert any(c['rows'] and c['rows'][0]['title']=='Older task' and c['rows'][0]['checked'] for c in lists)
assert any(f.get('request')=='input' and f.get('text')=='Older task' for f in frames)
assert lists[-1]['columns']==2 and lists[-1]['rows'][0]['title']=='Edited task'
assert (folder/'tasks.bin').is_file()
print('PASS packaged ToDo preserves multiline rows, flags, completion, editing and board grouping.')

frames,folder=app_session('lumen.study',[
    dict(event='service',action=1,ok=1,page=0,count=1,name0='Lesson.txt'),
    tap(100,350),tap(100,275),tap(425,275),
    dict(event='text',action=4,text='Recall\nSecond paragraph')])
assert any(f.get('service')=='study_material' and f.get('operation')=='open' and f.get('text')=='Lesson.txt' for f in frames)
assert any(f.get('service')=='clock' and f.get('operation')=='start_timer' and f.get('text')=='25' for f in frames)
assert any(f.get('service')=='notes' and f.get('text')=='Lesson.txt\nRecall\nSecond paragraph' for f in frames)
assert (folder/'material.txt').read_text()=='Lesson.txt'
print('PASS packaged Study routes material, focus timer and multiline recall through shared services.')

frames,_=app_session('lumen.recipes',[
    dict(event='service',action=1,ok=1,page=0,count=1,id0='wiki-de-5',title0='Offline meal'),
    tap(110,215),tap(550,95),dict(event='text',action=2,text='soup'),
    dict(event='service',action=1,ok=1,page=0,count=1,id0='wiki-en-2',title0='Soup',source0='Wikibooks'),
    tap(110,215),dict(event='back')])
assert any(f.get('operation')=='open' and f.get('text')=='wiki-de-5' for f in frames)
assert any(f.get('operation')=='search' and f.get('text')=='soup' for f in frames)
assert any(f.get('operation')=='fetch' and f.get('text')=='wiki-en-2' for f in frames)
assert any(f.get('back') for f in frames) and not frames[-1].get('back')
assert any(f.get('operation')=='archive' for f in frames)
print('PASS packaged Recipes archive, search, document selection and Back behavior.')
