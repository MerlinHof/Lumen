"""Full-runtime transaction recovery and independent app updates, no network."""
from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys,time,zipfile
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

# A real process sends nested request-looking data, malformed lines and a real request.
wire_app=base/'wire.lumen-app'
script=b"""#!/bin/sh
IFS= read -r line
echo '{"back" : true, "ui": [{"text":"request","nested":{"request":"input","back":false}}]}'
IFS= read -r line
echo '{"back":true garbage}'
echo '{"request":1}'
echo '{"back":"true","ui":[{"text":"still valid"}]}'
IFS= read -r line
printf '%s\n' '{"request" : "notify", "text":"hello"}' '{"request":"notify","text":"second"}'
IFS= read -r line
"""
archive(wire_app,{'app':script},format='lumen-app-1',api=1,id='test.wire',name='Wire fixture',version='1.0.0')
run('install',wire_app)
bad=base/'bad-app.zip'
archive(bad,{'app':script},format='lumen-app-2',api=2,id='test.other',name='Invalid provider',
        version='1.0.0',entry='app',surfaces=['foreground'],provides=['text-input'])
run('install',bad,ok=False)
overlay=base/'overlay-app.zip'
archive(overlay,{'app':script},format='lumen-app-2',api=2,id='test.overlay',name='Independent provider',
        version='1.0.0',entry='app',surfaces=['overlay'],provides=['text-input'])
run('install',overlay)
(data/'providers.conf').write_text('text-input=test.overlay\nstartup=test.overlay\n')
run('provider')
run('wire')

caller_app=base/'caller.lumen-app'
script=b"""#!/bin/sh
IFS= read -r open_event
echo '{"ui":[]}'
case "$open_event" in
 *service_call*reply_and_exit*) echo '{"request":"service_result","ok":true,"data":"finished"}'; exit 0;;
esac
while IFS= read -r event; do
 printf '{"ui":[%s]}\n' "$event"
done
"""
archive(caller_app,{'app':script},format='lumen-app-1',api=1,id='test.caller',name='Caller fixture',
        version='1.0.0',operations=['reply_and_exit'])
run('install',caller_app)
for key in ('lumen.study','lumen.notes'):
 run('install',ROOT/'build/apps/host'/key/(key+'.lumen-app'))
worker=base/'worker.lumen-app'
script=b"""#!/bin/sh
IFS= read -r event
case "$event" in
 *reply_and_exit*) echo '{"request":"service_result","ok":true,"data":"finished"}';;
esac
exit 0
"""
meta=dict(format='lumen-app-1',api=1,id='test.worker',name='Worker fixture',version='1.0.0')
archive(worker,{'app':script},**meta,operations=['exit','exit'])
run('install',worker,ok=False)
archive(worker,{'app':script},**meta,operations=['exit','reply_and_exit'])
run('install',worker)
run('service')
recalls=[p.read_bytes() for p in (data/'notes').glob('*.txt')]
assert recalls==['# Abrufübung: Physics\n\n'.encode()+b'A\n\t'*1600]

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

# Launch every package from its own executable with fresh private storage.
manifests=sorted((ROOT/'lumen/apps').glob('*/package.json'))
assert len(manifests)==53
default_launcher=sorted((definition['launcher']['order'],definition['id'])
                        for path in manifests for definition in [json.loads(path.read_text())]
                        if definition.get('launcher',{}).get('visible'))
default_ids=[key for _,key in default_launcher]
assert default_ids[default_ids.index('lumen.openai')+1]=='lumen.browser'
ids=set()
for manifest in manifests:
 definition=json.loads(manifest.read_text())
 key=definition['id']
 assert key not in ids and definition['api']==2 and definition['format']=='lumen-app-2',key
 ids.add(key)
 assert 'assets/icon.pgm' in definition['assets'],key
 for source in definition['sources']:
  assert (manifest.parent/source).is_file() and (manifest.parent/source).resolve().is_relative_to(ROOT/'lumen/apps'),key
 frames,_=app_session(key,[])
 assert any(isinstance(frame.get('ui'),list) for frame in frames),key
print(f'PASS startup frames and manifests from all {len(manifests)} standalone apps.')

removed=('games/manuals.h','notes/folder_state.h','notes/folders.h','notes/defaults.h',
         'notes/shell_state.h','settings/preferences.h','settings/shell_state.h')
for path in removed:
 assert not (ROOT/'lumen/apps'/path).exists(),path
assert 'INPUT_TAP' not in (ROOT/'lumen/apps/settings/app.c').read_text()
for name in ('study_services.h','openai_service.h'):
 assert not (ROOT/'lumen/system/ui'/name).exists(),name
print('PASS extracted collection and Settings controls stay outside retired host implementations.')

def tap(x,y):
 return dict(event='input',kind=1,x=x,y=y)

def component(id,gesture='clicked',text=None):
    event=dict(event='component',id=id,gesture=gesture,changed=int(text is not None))
    if text is not None:event['text']=text
    return event

for app,x,y,help_x in [
 ('bingo',70,220,490),('connect-four',70,220,490),('hex',60,155,490),
 ('nonograms',177,291,490),('solitaire',50,165,368),('wordle',290,685,550),
 ('periodic-table',21,150,None)
]:
 first={'wordle':'guess','periodic-table':'element:0','bingo':'cell:0',
        'connect-four':'column:0','nonograms':'cell:0','solitaire':'draw'}.get(app)
 events=[component(first) if first else tap(x,y)]
 if app=='wordle': events.append(dict(event='text',action=1,text='PAPER'))
 if help_x:
     events.append(component('help'))
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
for key in ('lumen.frog-solitaire','lumen.sliding-puzzle'):
 frames,_=app_session(key,[component('restart'),component('help')],legacy=data)
 assert any(f.get('request')=='reader' and f.get('text') for f in frames),key
print('PASS standalone app ports, Reader help, private saves and persistent Terminal shell.')

# Read the wall runs actually published by the executable and prove the exit is reachable.
frames,_=app_session('lumen.maze',[component('help'),component('down')])
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

frames,folder=app_session('lumen.knights-tour',[component('cell:17'),component('help')])
assert (folder/'tour.bin').read_bytes()[17]==2
assert any(f.get('request')=='intent' for f in frames)
print('PASS packaged Knight’s Tour legal move, private save and Reader help.')

frames,_=app_session('lumen.quotes',[
 dict(event='service',action=1,ok=1,count=1,page=0,quote0='A quoted passage',
      book0='Example book',ref0='1234567890abcdef:2'),
 dict(event='component',id='quote-0',gesture='clicked')])
assert any(f.get('operation')=='list' for f in frames)
assert any(f.get('operation')=='open' and f.get('text')=='1234567890abcdef:2' for f in frames)
assert any(c.get('text')=='A quoted passage' for f in frames for c in f.get('ui',[]))
print('PASS packaged Quotes renders annotations and uses stable book references.')

frames,_=app_session('lumen.minesweeper',[
    component('cell:1','long_pressed'),component('cell:1'),component('cell:32'),component('help')])
assert any(c.get('text')=='1 / 12' for f in frames for c in f.get('ui',[]))
assert any(f.get('operation')=='open_document' for f in frames)
frames,_=app_session('lumen.hangman',[
    dict(event='service',action=1,ok=1,word='INK'),
    *[component(f'letter:{i}') for i in (0,1,2,3,4,5)]])
assert any(f.get('service')=='dictionary' for f in frames)
assert any('INK' in c.get('text','') for f in frames for c in f.get('ui',[]))
for key in ('lumen.tetris','lumen.snake'):
    frames,_=app_session(key,[component('pause'),dict(event='tick'),dict(event='pause'),dict(event='resume')])
    assert not frames[0]['tick'] and 20<=frames[1]['tick']<=650 and frames[-1]['tick']==0
    assert frames[1]['ui']!=frames[2]['ui']
print('PASS packaged Minesweeper, Hangman, Tetris and Snake input, dictionary, timing and pause.')

frames,folder=app_session('lumen.flashcards',[
    component('new'),dict(event='text',action=1,text='Physics'),
    dict(event='text',action=2,text='Force?'),dict(event='text',action=3,text='Mass times acceleration.'),
    component('reveal'),component('review:2')])
assert (folder/'cards.bin').is_file() and (folder/'reviews.tsv').read_text().split('\t')[2]=='3'
assert any(c.get('text')=='Mass times acceleration.' for f in frames for c in f.get('ui',[]))
print('PASS packaged Flashcards creates cards, reveals answers and records FSRS reviews.')

frames,folder=app_session('lumen.sokoban',[component('help'),component('left')])
assert (folder/'warehouse.bin').is_file()
assert any(c.get('type')=='sprite' and c.get('transparent')==1 for f in frames for c in f.get('ui',[]))
assert any(f.get('operation')=='open_document' for f in frames)
frames,folder=app_session('lumen.crossword',[component('clue:0'),dict(event='text',action=10,text='X'),component('help')])
assert any(f.get('operation')=='clues' for f in frames)
assert any(f.get('request')=='input' and f.get('action')==10 for f in frames)
assert any(f.get('request')=='notify' and 'letters' in f.get('text','') for f in frames)
assert any(f.get('operation')=='open_document' for f in frames)
print('PASS packaged Sokoban assets, save and help, and Crossword clues, editing, validation and help.')


frames,folder=app_session('lumen.braintrain',[component('exercise0'),component('continue'),component('memory:0'),component('continue')])
assert len([c for c in frames[0]['ui'] if c['type']=='component_card'])==6
assert (folder/'training.bin').is_file()
frames,folder=app_session('lumen.languages',[
    component('pair','selected','1'),component('practice','selected','2'),component('write'),
    dict(event='text',action=1,text='Translation'),
    component('glossary'),dict(event='page',page=2),dict(event='back')])
assert any(c.get('type')=='component_segments' for f in frames for c in f.get('ui',[]))
assert any(f.get('request')=='input' for f in frames)
assert not any(f.get('service')=='text' for f in frames)
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
(legacy/'apps-v1.bin').write_bytes(struct.pack('=8I',0x4c415050,1,1260,0,0,0,0,0)
    +struct.pack('=3I',int(today),1260,0)+bytes(30*12))
record=struct.pack('=iii',1,int(today),4)+b'Legacy journal'.ljust(6*8192,b'\0')
(legacy/'journals17'/f'{today}.bin').write_bytes(struct.pack('=II',0x4c554d16,len(record))+record)
frames,folder=app_session('lumen.journal',[component('day:0'),component('edit'),
    dict(event='text',action=10,text='Two lines\nSecond line'),dict(event='back')],legacy)
assert any(f.get('request')=='multiline' and f.get('text')=='Legacy journal' for f in frames)
assert b'Two lines\nSecond line' in (folder/f'{today}.bin').read_bytes()
frames,folder=app_session('lumen.calendar',[component('add-note'),dict(event='text',action=1,text='Updated appointment'),
    component('view','selected','1'),component('view','selected','0'),component('view','selected','3')],legacy)
assert any(f.get('request')=='multiline' and f.get('text')=='Old appointment' for f in frames)
assert any('21 min' in c.get('text','') for f in frames for c in f.get('ui',[]))
assert not any(f.get('operation')=='month' for f in frames)
assert (folder/f'{today}.txt').read_text()=='Updated appointment'
assert (legacy/'calendar'/f'{today}.txt').read_text()=='Old appointment'
assert {c.get('selected') for f in frames for c in f.get('ui',[]) if c.get('type')=='component_segments'}=={0,1,2,3}
print('PASS packaged Journal and Calendar import legacy data, edit using multiline input and save privately.')

frames,folder=app_session('lumen.chess',[component('opponent','selected','0'),
    component("square:52"),component("square:36"),dict(event='tick'),component('rotate')])
saved=(folder/'game.bin').read_bytes()
assert saved[52]==0 and saved[36]==1 and struct.unpack_from('=i',saved,64)[0]==1
assert any(f.get('tick')==80 for f in frames)
assert any(c.get('type')=='sprite' and c.get('text')=='assets/pieces-rotated.pgm' for f in frames for c in f.get('ui',[]))
frames,_=app_session('lumen.chess',[component('undo')])
assert (folder/'game.bin').read_bytes()[52]==1
print('PASS packaged Chess legal pawn move, computer reply, piece rotation and persisted undo.')

(legacy/'weather-city.txt').write_text('Rosenheim')
forecast_fixture=dict(current=dict(temperature_2m=19,weather_code=2,is_day=1),
                      daily=dict(time=['2026-11-12'],weather_code=[2]))
(legacy/'weather.json').write_text(json.dumps(forecast_fixture))
frames,folder=app_session('lumen.weather',[
    component('search'),dict(event='text',action=2,text='Paris')],legacy)
assert any(f.get('operation')=='refresh_forecast' and f.get('text')=='Paris' for f in frames)
assert any(c.get('type')=='sprite' and c.get('index')==2 for f in frames for c in f.get('ui',[]))
assert (folder/'city.txt').read_text()=='Paris'

fake_weather=base/'weather-bin';fake_weather.mkdir()
curl=fake_weather/'curl'
curl.write_text("""#!/bin/sh
case "$*" in
 *geocoding-api.open-meteo.com*) echo '{"results":[{"name":"Paris","latitude":48.8,"longitude":2.3}]}';;
 *) echo '"""+json.dumps(forecast_fixture)+"""';;
esac
""")
curl.chmod(0o755)
weather_app=ROOT/'build/apps/host/lumen.weather/app'
event=dict(event='service_call',language=1,operation='refresh_forecast',data='Paris')
result=subprocess.run([str(weather_app)],input=json.dumps(event)+'\n',capture_output=True,text=True,
    env=dict(os.environ,LUMEN_APP_DATA=str(folder),PATH=str(fake_weather)+':'+os.environ['PATH']),
    timeout=10,check=True)
assert json.loads(result.stdout)['ok']
assert (folder/'forecast-v1.bin').is_file()
frames,_=app_session('lumen.weather',[],legacy)
assert any(c.get('text')=='Paris' for f in frames for c in f.get('ui',[]))
assert not any(f.get('request')=='app_service' for f in frames)
print('PASS Weather owns geocoding, forecast parsing, caching, refresh and semantic controls.')

fake_openai=base/'openai-bin';fake_openai.mkdir()
openai_root=base/'openai-data';openai_root.mkdir()
(openai_root/'request.json').write_text('{"input":"hello"}')
(openai_root/'api-key.txt').write_text('fixture-secret')
openai_curl=fake_openai/'curl'
openai_curl.write_text("""#!/bin/sh
case "$*" in *fixture-secret*) exit 2;; esac
config=$(cat)
case "$config" in *'Authorization: Bearer fixture-secret'*) ;; *) exit 3;; esac
case "$*" in
 *'/v1/models') echo '{"data":[{"id":"z-model"},{"id":"a-model"}]}';;
 *'/v1/responses')
  case "$config" in *'data-binary = "{\\"input\\":\\"hello\\"}"'*) ;; *) exit 4;; esac
  test "$(cat "$LUMEN_APP_DATA/request.json")" = '{"input":"hello"}' || exit 5
  echo '{"output":[{"content":[{"type":"output_text","text":"Grüße ☀"}]}]}';;
 *) exit 6;;
esac
""")
openai_curl.chmod(0o755)
def openai_worker(operation,data):
    event=dict(event='service_call',language=1,operation=operation,data=data)
    binary=ROOT/'build/apps/host/lumen.openai/app'
    result=subprocess.run([str(binary)],input=json.dumps(event)+'\n',capture_output=True,text=True,
        env=dict(os.environ,LUMEN_APP_DATA=str(openai_root),PATH=str(fake_openai)+':'+os.environ['PATH']),
        timeout=10,check=True)
    return json.loads(result.stdout)['ok']
assert openai_worker('models_refresh','')
assert openai_worker('send','0000000000000042')
assert not openai_worker('send','../unscoped')
assert (openai_root/'reply-0000000000000042.txt').read_text()=='Grüße ☀\n'
assert json.loads((openai_root/'models.json').read_text())['data'][1]['id']=='a-model'
print('PASS OpenAI package owns model loading and UTF-8 replies, with keys passed only through worker input.')

wiki_worker=base/'wiki-bin';wiki_worker.mkdir()
wiki_curl=wiki_worker/'curl'
wiki_curl.write_text("""#!/bin/sh
case "$*" in
 *en.wikipedia.org*Darcy*) echo '{"query":{"pages":[{"title":"Mr. Darcy","extract":"A character in Pride and Prejudice."}]}}';;
 *) exit 2;;
esac
""")
wiki_curl.chmod(0o755)
wiki_request=dict(event='service_call',language=1,operation='summary',data='Darcy')
wiki_process=subprocess.run([str(ROOT/'build/apps/host/lumen.wikipedia/app')],
    input=json.dumps(wiki_request)+'\n',capture_output=True,text=True,check=True,timeout=10,
    env=dict(os.environ,PATH=str(wiki_worker)+':'+os.environ['PATH']))
assert json.loads(wiki_process.stdout)['data']=='Mr. Darcy\nA character in Pride and Prejudice.'
print('PASS standalone Wikipedia summary operation and bounded text response.')

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
    component('board')])
lists=[c for f in frames for c in f.get('ui',[]) if c.get('type')=='flow_list']
assert any(c['rows'] and c['rows'][0]['title']=='Older task' and c['rows'][0]['checked'] for c in lists)
assert any(f.get('request')=='input' and f.get('text')=='Older task' for f in frames)
assert lists[-1]['columns']==2 and lists[-1]['rows'][0]['title']=='Edited task'
assert (folder/'tasks-v2.bin').is_file()
frames,_=app_session('lumen.todo',[
    dict(event='text',action=1,text='Deadline later'),
    dict(event='text',action=1,text='Deadline sooner'),
    component('2','deadline'),dict(event='text',action=4,text='2099-02-07'),
    component('3','deadline'),dict(event='text',action=4,text='2020-03-01 09:30')])
due_rows=next(c['rows'] for c in frames[-1]['ui'] if c['type']=='flow_list')
assert [r['title'] for r in due_rows]==['Deadline sooner','Deadline later','New task\nSecond line']
assert due_rows[0]['warning']==1 and due_rows[0]['detail']=='2020-03-01 09:30'
assert due_rows[1]['warning']==0 and due_rows[1]['detail']=='2099-02-07'
print('PASS packaged ToDo preserves multiline rows, flags, completion, editing and board grouping.')

(data/'study').mkdir(exist_ok=True)
(data/'study'/'Lesson.txt').write_text('Test lesson')
frames,folder=app_session('lumen.study',[
    component('material-0'),component('focus'),component('recall'),
    dict(event='text',action=4,text='Recall\nSecond paragraph')],legacy=data)
assert any(f.get('document')=={'type':'document','path':'Lesson.txt','collection':'study'} for f in frames)
assert any(f.get('service')=='clock' and f.get('operation')=='start_timer' and f.get('text')=='25' for f in frames)
assert any(f.get('request')=='app_service' and f.get('app')=='lumen.notes' and
           f.get('operation')=='create_learning' and f.get('text')=='Lesson.txt\nRecall\nSecond paragraph' for f in frames)
assert (folder/'material.txt').read_text()=='Lesson.txt'
print('PASS packaged Study enumerates material locally and handles semantic actions for reading, focus and recall.')

recipe_legacy=base/'recipes-legacy'
old_recipes=recipe_legacy/'recipe-archive16'
old_recipes.mkdir(parents=True)
(old_recipes/'sample.txt').write_text('Offline meal\nOld recipe instructions')
(old_recipes/'sample.html').write_text('<html><body>Offline meal</body></html>')
frames,folder=app_session('lumen.recipes',[component('recipe-0'),component('search')],legacy=recipe_legacy)
assert any(f.get('operation')=='open_file' and f.get('title')=='Offline meal' for f in frames),frames
assert (folder/'recipe-archive16'/'sample.html').read_text()=='<html><body>Offline meal</body></html>'
assert all(f.get('service')!='recipes' for f in frames)
assert any(f.get('request')=='input' for f in frames)
print('PASS packaged Recipes opens offline archives and imports an older saved recipe.')

frames,folder=app_session('lumen.planetarium',[
    component('map','selected','1'),
    component('year-forward'),
    component('settings'),
    component('location'),
    dict(event='text',action=1,text='48.13, 11.58'),
    dict(event='back'),
    component('map','selected','0'),
    dict(event='text',action=2,text='12026-06-21 12:00')])
point_sets=[c['points'] for f in frames for c in f.get('ui',[]) if c['type']=='points']
assert point_sets and all(0<len(points)<=9000 and len(points)%3==0 for points in point_sets)
assert any(c.get('text')=='Neptune' for f in frames for c in f.get('ui',[]))
assert any(f.get('back') for f in frames) and not frames[-1].get('back')
assert any('12026' in c.get('text','') for c in frames[-1]['ui'])
assert struct.unpack('=dd',(folder/'sky-location-1.bin').read_bytes())==(48.13,11.58)
print('PASS packaged Planetarium sky/solar views, time travel, Back and private location.')

wiki_legacy=base/'wiki-legacy'
archive=wiki_legacy/'wiki-archive'
archive.mkdir(parents=True)
def article_record(title, body, stamp):
    return struct.pack('=Iiq256s2048s512s524288s',0x574b3131,1,stamp,title.encode(),b'',b'',body.encode())
(archive/'1234567890abcdef.bin').write_bytes(article_record('Cached article','A saved paragraph.',1700000001))
(archive/'1234567890abcdea.bin').write_bytes(article_record('Other','A second paragraph.',1700000000))
frames,wiki_data=app_session('lumen.wikipedia',[
    component('search'),component('open-0'),component('delete-0'),component('confirm')],wiki_legacy)
assert any(f.get('request')=='input' for f in frames)
assert any(f.get('operation')=='open_file' and f.get('title')=='Cached article' for f in frames)
assert any(c.get('text')=='Cached article' for f in frames for c in f.get('ui',[]))
assert (wiki_data/'wiki-archive/1234567890abcdef.deleted').read_text()=='1'
assert (archive/'1234567890abcdef.bin').is_file()
reopened,_=app_session('lumen.wikipedia',[],wiki_legacy)
assert all(c.get('text')!='Cached article' for c in reopened[-1]['ui'])
print('PASS Wikipedia reads and opens its legacy offline archive and preserves deletions across restarts.')
wiki_binary=ROOT/'build/apps/host/lumen.wikipedia/app'
with subprocess.Popen([str(wiki_binary)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,
                      stderr=subprocess.PIPE,text=True,
                      env=dict(os.environ,LUMEN_APP_DATA=str(wiki_data),
                               PATH=str(wiki_worker)+':'+os.environ['PATH'])) as wiki:
    wiki.stdin.write(json.dumps(dict(event='open',language=1))+'\n')
    wiki.stdin.write(json.dumps(dict(event='intent',operation='search',text='Darcy'))+'\n')
    wiki.stdin.flush()
    for _ in range(100):
        time.sleep(.02)
        if any((wiki_data/'wiki-archive').glob('*.bin')):break
    for _ in range(5):
        wiki.stdin.write('{"event":"tick"}\n')
        wiki.stdin.flush()
        time.sleep(.02)
    output,wiki_error=wiki.communicate(timeout=5)
    assert wiki.returncode==0,wiki_error
assert any(frame.get('title')=='Mr. Darcy' and frame.get('operation')=='open_file'
           for frame in map(json.loads,output.splitlines()))
assert any('Pride and Prejudice' in path.read_text() for path in (wiki_data/'wiki-archive').glob('*.html'))
print('PASS Wikipedia searches in its own worker and opens a newly cached article.')


news_legacy=base/'news-legacy'
(news_legacy/'news').mkdir(parents=True)
article=struct.pack('=i256s2300s1024s80s80s128s',1,b'Science today',
    b'An archived article that works offline.',b'https://example.org/article',
    b'Publisher',b'',b'')
(news_legacy/'news'/'20200101.bin').write_bytes(
    struct.pack('=IIIIi',0x4c4e4557,1,20200101,1,1)+article+bytes(9*len(article)))
news_bin=ROOT/'build/apps/host/lumen.news/app'
news_data=base/'news-private'
news_data.mkdir()
news_curl=base/'news-curl'
news_curl.mkdir()
(news_curl/'curl').write_text("""#!/bin/sh
for arg do url=$arg; done
case "$url" in
  *bing.com*) printf '%s' '<rss><channel><item><title>Energy forecast</title><link>https://example.org/energy</link><description>New energy report</description></item></channel></rss>' ;;
  *) exit 22 ;;
esac
""")
(news_curl/'curl').chmod(0o755)
env=dict(os.environ,LUMEN_APP_DATA=str(news_data),LUMEN_LEGACY_DATA=str(news_legacy),
    LUMEN_DOCUMENT_TOOL=str(ROOT/'build/host/lumen-host'),PATH=str(news_curl)+os.pathsep+os.environ['PATH'])
with subprocess.Popen([str(news_bin)],env=env,stdin=subprocess.PIPE,stdout=subprocess.PIPE,
                      stderr=subprocess.PIPE,text=True) as news:
    def news_event(event):
        news.stdin.write(json.dumps(event)+'\n')
        news.stdin.flush()
    for event in [dict(event='open',language=1),component('entry-1'),component('entry-0')]:
        news_event(event)
    for _ in range(100):
        if list((news_data/'press').glob('*.article')):break
        time.sleep(.025)
    news_event(dict(event='tick'))
    time.sleep(.1)
    news_event(dict(event='tick'))
    news_event(dict(event='back'))
    news_event(dict(event='text',action=1,text='Energy'))
    for _ in range(100):
        if (news_data/'press'/'search.bin').exists():break
        time.sleep(.025)
    news_event(dict(event='tick'))
    time.sleep(.1)
    news_event(dict(event='tick'))
    output,error=news.communicate(timeout=5)
    assert news.returncode==0,error
frames=list(map(json.loads,output.splitlines()))
assert any(f.get('operation')=='open_file' and f.get('title')=='Science today' for f in frames)
assert any('works offline' in path.read_text() for path in (news_data/'press').glob('*.html'))
assert any(c.get('text')=='Energy forecast' for f in frames for c in f.get('ui',[]))
assert all(f.get('service')!='news' for f in frames)
print('PASS News reads an old edition offline, searches RSS locally and opens a private Reader document.')

frames,folder=app_session('lumen.calculator',[
    dict(event='text',action=80,text='5/4*sin(3/4)*sqrt(4)'),
    component('equals'),component('equals'),component('history'),dict(event='back'),
    component('graph'),dict(event='back'),
    component('settings'),component('angle'),dict(event='select',action=1,value=1)])
assert any(c.get('type')=='formula' and 'sqrt' in c.get('text','') for f in frames for c in f.get('ui',[]))
assert (folder/'calculator-history.txt').read_text().splitlines()==['5/4*sin(3/4)*sqrt(4)']
assert any(c.get('type')=='line' for f in frames for c in f.get('ui',[]))
assert any(f.get('request')=='select' for f in frames)
assert (folder/'calculator-state.txt').read_text().split()[-1]=='0'
print('PASS packaged Calculator expression, evaluation, deduplicated history, plot and settings.')

frames,folder=app_session('lumen.clock',[
    *[dict(event='select',action=2,value=i) for i in range(14)],
    component("fullscreen"),tap(300,400),
    component('mode','selected','1'),
    dict(event='service',action=1,ok=1,minutes=20,total=1200,hour=7,minute=15,armed=0),
    component("start-pause"),component("reset"),
    component('mode','selected','2'),component('arm')])
assert any(c.get('id')=='faces' and c.get('type')=='dropdown_action' for f in frames for c in f.get('ui',[]))
assert any(f.get('fullscreen') for f in frames)
assert any(c.get('type')=='display_text' for f in frames for c in f.get('ui',[]))
assert any(f.get('operation')=='timer' and f.get('text')=='0' for f in frames)
assert any(f.get('operation')=='timer' and f.get('text')=='1' for f in frames)
assert any(f.get('operation')=='armed' and f.get('text')=='1' for f in frames)
assert struct.unpack('=i',(folder/'face.bin').read_bytes())==(3,)
print('PASS packaged Clock faces, fullscreen, timer controls and alarm service.')

store_bin=ROOT/'build/apps/host/lumen.store/app'
store_data=base/'store-private'
store_data.mkdir()
store_assets=base/'store-assets'
(store_assets/'assets').mkdir(parents=True)
(store_assets/'assets'/'gutenberg-de.opds').write_text(
    '<feed xmlns="http://www.w3.org/2005/Atom"><entry><title>Featured book</title>'
    '<author><name>Doe, Jane</name></author><summary>A story</summary>'
    '<link href="https://example.org/book.pdf" type="application/pdf" '
    'rel="http://opds-spec.org/acquisition"/></entry></feed>')
store_curl=base/'store-curl'
store_curl.mkdir()
(store_curl/'curl').write_text("""#!/bin/sh
for arg do url=$arg; done
case "$url" in
  *book.pdf) printf '%s' '%PDF-1.7 sample' ;;
  *) exit 22 ;;
esac
""")
(store_curl/'curl').chmod(0o755)
env=dict(os.environ,LUMEN_APP_DATA=str(store_data),LUMEN_APP_ASSETS=str(store_assets),
    LUMEN_DOCUMENT_TOOL=str(ROOT/'build/host/lumen-host'),PATH=str(store_curl)+os.pathsep+os.environ['PATH'])
with subprocess.Popen([str(store_bin)],env=env,stdin=subprocess.PIPE,stdout=subprocess.PIPE,
                      stderr=subprocess.PIPE,text=True) as store:
    def store_event(event):
        store.stdin.write(json.dumps(event)+'\n')
        store.stdin.flush()
    for event in (dict(event='open',language=1),component('book:0'),component('download')):
        store_event(event)
    for _ in range(100):
        if list(store_data.glob('*.pdf')):break
        time.sleep(.025)
    store_event(dict(event='tick'))
    time.sleep(.1)
    store_event(dict(event='tick'))
    store_event(dict(event='service',action=30,ok=1))
    store_event(dict(event='back'))
    store_event(component("sources"))
    store_event(component("source:0","changed","0"))
    output,error=store.communicate(timeout=5)
    assert store.returncode==0,error
frames=list(map(json.loads,output.splitlines()))
assert any(c.get('type')=='cover_asset' and c.get('text')=='Featured book' for f in frames for c in f.get('ui',[]))
assert any(c.get('text')=='A story' for f in frames for c in f.get('ui',[]))
assert any(f.get('request')=='book_import' and f.get('title')=='Featured book' for f in frames)
assert (store_data/'sources.bin').is_file()
assert all(f.get('service')!='store' for f in frames)
print('PASS Store owns OPDS parsing, covers, source settings and download, and requests a scoped Library import.')

frames,_=app_session('lumen.library',[
    dict(event='service',action=1,ok=1,count=6,has_recent=1,sort=2,id16='1234567890abcdee',
         title16='Continue book',author16='An Author',progress16=45,id0='1234567890abcdef',
         title0='Shelf book',author0='Another Author'),
    component('book-0','long_pressed'),dict(event='select',action=3,value=1),
    dict(event='service',action=2,ok=1,id0='1234567890abcdef',title0='Shelf book',supported=1),
    component('read'),dict(event='back'),dict(event='page',page=1)])
assert any(c.get('type')=='book_cover' for f in frames for c in f.get('ui',[]))
assert any(f.get('operation')=='detail' and f.get('text')=='1234567890abcdef' for f in frames)
assert any(f.get('operation')=='read' and f.get('text')=='1234567890abcdef' for f in frames)
assert any(f.get('operation')=='query' and f.get('text','').startswith('3 6 2 3\n') for f in frames)
print('PASS packaged Library cover grid, book details, Reader intent and pagination.')

home_assistant=base/'home-assistant-legacy'
home_assistant.mkdir()
(home_assistant/'ha-url.txt').write_text('http://127.0.0.1:1')
(home_assistant/'ha-token.txt').write_text('test-token')
(home_assistant/'ha-favorites.txt').write_text('sensor.room\n')
(home_assistant/'ha-states.json').write_text(json.dumps([
    {'entity_id':'sensor.room','state':'21.5','attributes':{'friendly_name':'Küche','unit_of_measurement':'°C'}},
    {'entity_id':'light.study','state':'off','attributes':{'friendly_name':'Study'}}
]))
(home_assistant/'ha-history.json').write_text(json.dumps([[
    {'entity_id':'sensor.room','state':'21','last_changed':datetime.datetime.fromtimestamp(time.time()-3600,datetime.timezone.utc).isoformat()},
    {'s':'22','lu':datetime.datetime.fromtimestamp(time.time()-1800,datetime.timezone.utc).isoformat()}
]]))
frames,private=app_session('lumen.home-assistant',[
    component('device:0'),dict(event='back'),component('devices'),
    component('search'),dict(event='text',action=1,text='KUCHE'),component('favorite:0'),dict(event='back')
],legacy=home_assistant)
assert any(c.get('text')=='Küche' for f in frames for c in f.get('ui',[]))
assert any(c.get('type')=='line' and c.get('weight')==2 and c.get('x2',0)>c.get('x',0)
           for f in frames for c in f.get('ui',[]))
assert (private/'ha-favorites.txt').read_text()==''
assert (home_assistant/'ha-favorites.txt').read_text()=='sensor.room\n'
assert not any(f.get('service')=='home_assistant' for f in frames)
assert not frames[-1]['back']
print('PASS Home Assistant owns device search, graphs, favorites and legacy import.')

(data/'llm-key.txt').write_text('fixture-key')
(data/'llm-models17.json').write_text('{"data":[{"id":"test-model"}]}')
frames,folder=app_session('lumen.openai',[
    dict(event='text',action=1,text='First question'),component('send'),
    dict(event='service',action=3,ok=0),
    dict(event='text',action=1,text='Follow-up'),component('send'),
    dict(event='service',action=3,ok=0),
    component('archive'),component('chat-0'),component('settings'),component('models'),
    component('model-0')],legacy=data)
assert any(f.get('operation')=='send' for f in frames)
assert any(c.get('text')=='Generating reply…' for f in frames for c in f.get('ui',[]))
reopened,_=app_session('lumen.openai',[],legacy=data)
assert any(c.get('text')=='Follow-up' for c in reopened[-1]['ui'])
request=json.loads((folder/'request.json').read_text())
assert [turn['content'] for turn in request['input']]==['First question','Follow-up']
assert len(list(folder.glob('*.chat')))==1
assert (folder/'model.txt').read_text()=='test-model'
print('PASS packaged OpenAI private chat archive, follow-up context and model selection.')

frames,photo_state=app_session('lumen.photos',[
    dict(event='service',action=1,ok=1,ids='1234567890abcdef 1234567890abcdea'),
    component('slideshow'),dict(event='tick'),tap(240,410),component('new-album'),dict(event='text',action=1,text='Travel'),
    component('albums'),dict(event='select',action=1,value=0),component('photo:0'),component('rotate'),component('move'),
    dict(event='select',action=2,value=0),dict(event='back')])
assert any(f.get('fullscreen') for f in frames)
assert any(c.get('type')=='photo' and c.get('text')=='1234567890abcdea' and c.get('full')
           for f in frames for c in f.get('ui',[]))
assert any(f.get('service')=='media' and f.get('operation')=='rotate' and f.get('text')=='1234567890abcdef' for f in frames)
assert b'Travel' in (photo_state/'albums.bin').read_bytes()
assert bytes.fromhex('efcdab9078563412') in (photo_state/'albums.bin').read_bytes()
print('PASS Photos gallery, fullscreen slideshow, rotation and private album assignment.')

frames,reader_state=app_session('lumen.reader',[
    dict(event='service',action=1,ok=1,title='Sample document',page=0,total=7,setting7=1),
    tap(400,380),tap(300,70),component("contents"),
    dict(event='service',action=2,ok=1,title='Sample document',page=1,total=7,count=1,
         position0=2,label0='Chapter 1'),
    component("entry-0"),tap(300,70),component("settings"),dict(event='page',page=1),
    dict(event='select',action=19,value=2)])
first_frame=next(f for f in frames if 'ui' in f)
assert not first_frame['ready'] and not first_frame['ui']
assert not any(c.get('id') in ('resume','library') for f in frames for c in f.get('ui',[]))
assert any(c.get('type')=='document_page' for f in frames for c in f.get('ui',[]))
assert any(f.get('operation')=='turn' and f.get('text')=='1' for f in frames)
assert any(f.get('operation')=='goto' and f.get('text')=='2' for f in frames)
assert struct.unpack('=iii',(reader_state/'reader-controls').read_bytes())==(1,0,2)
selection,_=app_session('lumen.reader',[
    dict(event='service',action=1,ok=1,title='Sample',total=7),
    dict(event='intent',operation='document-selection',text='Darcy'),
    component('selection-copy'),dict(event='service',action=4,ok=1)])
assert any(c.get('id')=='selection-mark' for f in selection for c in f.get('ui',[]))
assert any(f.get('operation')=='selection_copy' for f in selection)
print('PASS packaged Reader document surface, page navigation, contents and preferences.')

notes_data=base/'legacy-notes'
(notes_data/'notes').mkdir(parents=True)
(notes_data/'notes'/'123.txt').write_text('Legacy note\nA second line')
(notes_data/'notes-defaults16.txt').write_text('1')
folder_record=struct.pack('=ii96s',1,0,b'Notes')
tree=folder_record+bytes(511*104+1024*8)+struct.pack('=iii',1,0,0)
(notes_data/'note-tree-1.bin').write_bytes(struct.pack('=II',0x4c554d16,len(tree))+tree)
def note_pointer(kind,x,y):
    return dict(event='input',kind=kind,x=x,y=y,captured=1)

frames,preview_data=app_session('lumen.notes',[
    component("entry-1"),dict(event='service',action=5,ok=1,breaks='12 13 '),
    component("ink"),note_pointer(7,200,250),note_pointer(1,250,250),
    component("eraser"),note_pointer(7,225,250),note_pointer(1,225,250),
    dict(event='back'),component("markdown"),dict(event='back')],notes_data)
assert any(f.get('request')=='text_measure' for f in frames)
assert any(c.get('text')=='Legacy note' for f in frames for c in f.get('ui',[]))
assert not any(f.get('request')=='service' for f in frames)
assert any(c.get('type')=='pointer_area' for f in frames for c in f.get('ui',[]))
assert any(f.get('operation')=='open_file' and f.get('document')=={'type':'markdown','path':'preview.md'} for f in frames)
assert (preview_data/'preview.md').read_text()=='Legacy note\nA second line'
ink_file=notes_data/'notes'/'123-page-0.ink'
pixels=ink_file.read_bytes()
assert len(pixels)==600*688 and pixels[(250-54)*600+200]==0 and pixels[(250-54)*600+225]==255
frames,_=app_session('lumen.notes',[
    component("entry-1"),dict(event='service',action=5,ok=1,breaks='12 13 '),component("ink"),
    note_pointer(7,200,450),note_pointer(1,260,450),component("undo")],notes_data)
assert ink_file.read_bytes()==pixels
frames,_=app_session('lumen.notes',[
    component("new-note"),dict(event='text',action=4,text='A separate local note')],notes_data)
created=next(p for p in (notes_data/'notes').glob('*.txt') if p.name!='123.txt')
assert created.read_text()=='A separate local note'
created.unlink()
# Exercise collection changes in a fresh process against the existing format.
frames,_=app_session('lumen.notes',[
    component('new-folder'),dict(event='text',action=1,text='Project'),
    component('entry-1'),component('rename'),dict(event='text',action=2,text='Archive'),
    dict(event='back'),component('menu-2'),dict(event='select',action=1,value=0),
    component('entry-2')],notes_data)
assert not any(f.get('service')=='notes_editor' for f in frames)
assert (notes_data/'notes'/'123.txt').read_text()=='Legacy note\nA second line'
frames,_=app_session('lumen.notes',[component('entry-1')],notes_data)
assert any(c.get('text')=='Archive' for f in frames for c in f.get('ui',[]))
assert any(c.get('text')=='Legacy note' for c in frames[-1]['ui'])
(notes_data/'notes'/'123-page-0.ink').write_bytes(b'ink')
app_session('lumen.notes',[component('menu-1'),dict(event='select',action=1,value=1),
                          component('confirm')],notes_data)
assert not (notes_data/'notes'/'123.txt').exists()
assert not (notes_data/'notes'/'123-page-0.ink').exists()
assert (notes_data/'note-tree-1.bin').read_bytes()[:8]==struct.pack('=II',0x4c554d16,len(tree))
print('PASS standalone Notes text, drawing, erasing, undo, Markdown, folder mutations and legacy formats.')

frames,_=app_session('lumen.settings',[
    component('appearance'),component('tab','selected','2'),component('lock_mode'),dict(event='select',action=19,value=1),
    dict(event='back'),component('wifi'),dict(event='service',action=5,ok=1),
    dict(event='service',action=2,ok=1,enabled=1,count=1,name0='Wi-Fi fixture',locked0=1),
    component('network-0'),dict(event='text',action=1,text='test-password'),
    dict(event='back'),component('system'),component('check')])
assert any(f.get('service')=='device' and f.get('operation')=='preference' and f.get('text')=='lock_mode 5' for f in frames)
assert any(f.get('request')=='secret' for f in frames)
assert any(f.get('operation')=='wifi_connect' and f.get('text')=='Wi-Fi fixture\ntest-password' for f in frames)
assert any(f.get('operation')=='check' for f in frames)
print('PASS packaged Settings appearance, masked Wi-Fi connection and GitHub updates.')
