"""Exercise Browser and Home Assistant against a loopback HTTP server."""
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from tempfile import TemporaryDirectory
from threading import Thread
from urllib.parse import parse_qs, urlsplit
import json
import os
import queue
import struct
import subprocess
import time
import zlib

ROOT = Path(__file__).resolve().parents[2]
requests = queue.Queue()
toggles = queue.Queue()


def png_chunk(tag, data):
    return struct.pack(">I",len(data))+tag+data+struct.pack(">I",zlib.crc32(tag+data))


def fixture_image():
    header=struct.pack(">IIBBBBB",2,2,8,0,0,0,0)
    return (b"\x89PNG\r\n\x1a\n"+png_chunk(b'IHDR',header)+
            png_chunk(b'IDAT',zlib.compress(b'\x00\xe0\x10'*2))+png_chunk(b'IEND',b''))


class Fixture(BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_POST(self):
        assert self.path=='/api/services/switch/toggle'
        assert self.headers.get('Authorization')=='Bearer fixture-token'
        body=self.rfile.read(int(self.headers['Content-Length']))
        toggles.put(json.loads(body))
        self.send_response(200)
        self.end_headers()
        self.wfile.write(b'[]')

    def do_GET(self):
        request = urlsplit(self.path)
        if request.path == '/go':
            self.send_response(302)
            self.send_header('Location', '/article/start')
            self.end_headers()
            return
        pages = {
            '/article/start': ('text/html', b'''<!doctype html><html><head>
                <title>Browser fixture</title><link rel="stylesheet" href="base.css">
                <style>.story {font-style:italic}
                .cards {display:block} @media screen and (max-width: 600px) {
                .cards {display:grid;grid-template-columns:repeat(2,1fr);gap:10px}
                .story {background-color:#ddd;width:80vw}}
                @media (min-width: 900px) {.story {color:#008}}</style></head>
                <body><p id="headline" class="story" style="padding:7px">Reading on Kindle</p>
                <img src="picture.png" alt="Photo"><a href="next">Next page</a>
                <div class="cards"><section>Left column</section><section>Right column</section></div>
                <button id="count">Count</button><div id="result"></div><p id="async"></p>
                <script src="fixture.js"></script>
                <script>let count=0; document.querySelector('#headline').textContent='JavaScript ran';
                document.getElementById('count').addEventListener('click',()=>{
                    document.querySelector('#result').innerHTML='<strong>Count '+(++count)+'</strong>';
                });</script><script>while(true){}</script></body></html>'''),
            '/article/picture.png': ('image/png', fixture_image()),
            '/article/fixture.js': ('application/javascript', b'''
                document.addEventListener('DOMContentLoaded',()=>setTimeout(async()=>{
                    const response=await fetch('message.json');
                    document.querySelector('body #async').textContent=(await response.json()).message;
                },0));'''),
            '/article/message.json': ('application/json', b'{"message":"Async update"}'),
            '/article/base.css': ('text/css', b'.story { color:#555; margin:20px; }'),
            '/article/next': ('text/html', b'<title>Next fixture</title><p>A second page.</p>'),
            '/api/states': ('application/json', json.dumps([
                dict(entity_id='sensor.room', state='23', attributes=dict(unit_of_measurement='C')),
                dict(entity_id='sensor.mode', state='idle', attributes={}),
                dict(entity_id='switch.office', state='off', attributes={}),
            ]).encode())
        }
        if request.path.startswith('/api/history/period/'):
            if self.headers.get('Authorization') != 'Bearer fixture-token':
                self.send_error(401)
                return
            params = parse_qs(request.query, keep_blank_values=True)
            requests.put((request.path, params))
            now = time.time()
            if params['filter_entity_id']==['sensor.mode']:
                if 'minimal_response' in params:
                    self.send_error(400)
                    return
                response=[[dict(entity_id='sensor.mode',state='heating',last_updated=now-1200),
                           dict(state='idle',last_changed=now-60)]]
            else:
                response=[[dict(s='20',lu=(now-1200)*1000),dict(s='23',lu=now-60)]]
            page = ('application/json', json.dumps(response).encode())
        else:
            page = pages.get(request.path)
        if page is None:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header('Content-Type', page[0])
        self.send_header('Content-Length', str(len(page[1])))
        self.end_headers()
        try:
            self.wfile.write(page[1])
        except (BrokenPipeError, ConnectionResetError):
            pass


class App:
    def __init__(self, key, folder):
        self.frames = queue.Queue()
        self.process = subprocess.Popen(
            [str(ROOT/'build/apps/host'/key/'app')], stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, text=True,
            env=dict(os.environ, LUMEN_APP_DATA=str(folder),
                     LUMEN_DOCUMENT_TOOL=str(ROOT/'build/host/lumen-host')))
        Thread(target=self.read, daemon=True).start()
        self.send('open', language=1)

    def read(self):
        for line in self.process.stdout:
            self.frames.put(json.loads(line))

    def send(self, event, **params):
        self.process.stdin.write(json.dumps(dict(event=event, **params))+'\n')
        self.process.stdin.flush()

    def click(self, target):
        self.send('component', id=target, gesture='clicked')

    def wait(self, predicate):
        until = time.monotonic()+8
        while time.monotonic()<until:
            self.send('tick')
            try:
                commands = self.frames.get(timeout=.03).get('ui', [])
                if predicate(commands):
                    return commands
            except queue.Empty:
                pass
            time.sleep(.025)
        raise AssertionError('The expected app frame did not arrive')

    def close(self):
        self.process.stdin.close()
        try:
            assert self.process.wait(timeout=3)==0
        finally:
            if self.process.poll() is None:
                self.process.kill()
                self.process.wait()


def label(commands, text):
    return any(c.get('text')==text for c in commands)


with ThreadingHTTPServer(('127.0.0.1',0),Fixture) as server, TemporaryDirectory() as tmp:
    Thread(target=server.serve_forever, daemon=True).start()
    origin=f'http://127.0.0.1:{server.server_port}'
    base=Path(tmp)
    browser=base/'browser'
    browser.mkdir()
    app=App('lumen.browser',browser)
    try:
        app.send('text',action=1,text=origin+'/go')
        commands=app.wait(lambda ui: label(ui,'Browser fixture'))
        assert any(c.get('id')=='link:0' for c in commands), commands
        clean=(browser/'browser-0.html').read_text()
        assert 'color:#555;' in clean and 'font-style:italic;' in clean
        assert 'padding:7px;' in clean and 'while(true)' not in clean
        assert 'Async update' in clean
        assert 'JavaScript ran' in clean and 'Reading on Kindle' not in clean
        assert 'width:435.2px;' in clean and 'background-color:#ddd;' in clean and 'color:#008;' not in clean
        assert 'Left column</section></td><td' in clean and 'padding:5px;' in clean
        assert 'lumen-control:' in clean
        assert 'data:image/png;base64,' in clean
        assert f'href="{origin}/article/next"' in clean
        pixels=(browser/'browser-raster-0').read_bytes()
        assert len(pixels)==544*560 and min(pixels)<128 and max(pixels)==255
        app.click('link:1')
        app.wait(lambda ui: any(c.get('text')=='browser-raster-1' for c in ui))
        assert '<strong>Count 1</strong>' in (browser/'browser-1.html').read_text()
        app.click('link:1')
        app.wait(lambda ui: any(c.get('text')=='browser-raster-0' for c in ui))
        assert '<strong>Count 2</strong>' in (browser/'browser-0.html').read_text()
        app.click('link:0')
        app.wait(lambda ui: label(ui,'Next fixture'))
        app.click('previous')
        app.wait(lambda ui: label(ui,'Browser fixture'))
    finally:
        app.close()
    print('PASS standalone Browser renders HTML and CSS, runs JavaScript and responsive layout, follows redirects and keeps navigation history.')

    assistant=base/'home-assistant'
    assistant.mkdir()
    (assistant/'connection.json').write_text(json.dumps(dict(url=origin,token='fixture-token')))
    (assistant/'ha-favorites.txt').write_text('sensor.room\nsensor.mode\nswitch.office\n')
    app=App('lumen.home-assistant',assistant)
    try:
        app.wait(lambda ui: any(c.get('id')=='device:0' for c in ui))
        app.click('device:0')
        app.wait(lambda ui: any(
            c.get('type')=='line' and c.get('weight')==2 and
            c.get('y')!=c.get('y2') and c.get('x2',0)>c.get('x',0)
            for c in ui))
        app.send('component',id='span',gesture='changed',changed=1,text='0')
        end=time.monotonic()+4
        spans=set()
        while 1 not in spans and time.monotonic()<end:
            path,params=requests.get(timeout=2)
            if params['filter_entity_id']!=['sensor.room'] or params.get('significant_changes_only')!=['0']:
                continue
            start=path.rsplit('/',1)[1]
            fmt='%Y-%m-%dT%H:%M:%SZ'
            duration=datetime.strptime(params['end_time'][0],fmt)-datetime.strptime(start,fmt)
            spans.add(duration.total_seconds()/3600)
        assert {1,24}<=spans
        app.send('back')
        app.wait(lambda ui: any(c.get('id')=='power:2' for c in ui))
        app.click('device:1')
        app.wait(lambda ui: label(ui,'heating'))
        app.send('back')
        app.wait(lambda ui: any(c.get('id')=='power:2' for c in ui))
        app.click('device:2')
        app.wait(lambda ui: any(c.get('id')=='span' for c in ui))
        while True:
            _,params=requests.get(timeout=3)
            if params['filter_entity_id']==['switch.office']:
                break
        app.send('back')
        commands=app.wait(lambda ui: any(c.get('id')=='power:2' for c in ui))
        assert any(c.get('id')=='power:2' and c.get('nested') for c in commands)
        app.send('component',id='power:2',gesture='changed',changed=1,text='1')
        assert toggles.get(timeout=3)=={'entity_id':'switch.office'}
    finally:
        app.close()
    print('PASS Home Assistant draws fullscreen compact numeric history and fetches the selected time range.')

status=subprocess.run(
    [str(ROOT/'build/apps/host/lumen.status-bar/app')], text=True, capture_output=True, check=True,
    input='\n'.join(map(json.dumps, [
        dict(event='open', role='overlay', surface_api=2, language=1),
        dict(event='system_state',back=1,show_battery=1,show_percent=1,show_wifi=1,
             show_clock=1,show_date=1,battery=100,clock='12:45',date='19.11.',timer='12 min')
    ]))+'\n')
commands=json.loads(status.stdout)['ui']
timer=next(c for c in commands if c.get('text')=='12 min')
timer_hit=next(c for c in commands if c.get('intent')=='clock-timer')
icon=next(c for c in commands if c.get('type')=='symbol' and c.get('x')==timer_hit['x'])
title=next(c for c in commands if c.get('live'))
assert timer['center']==0 and timer['x']-(icon['x']+icon['w'])==4
assert title['x']+title['w']<=timer_hit['x']
print('PASS status timer text sits next to its icon and leaves room for the app title.')
