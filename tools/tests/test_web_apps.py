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


def png_chunk(tag, data):
    return struct.pack(">I",len(data))+tag+data+struct.pack(">I",zlib.crc32(tag+data))


def fixture_image():
    header=struct.pack(">IIBBBBB",2,2,8,0,0,0,0)
    return (b"\x89PNG\r\n\x1a\n"+png_chunk(b'IHDR',header)+
            png_chunk(b'IDAT',zlib.compress(b'\x00\xe0\x10'*2))+png_chunk(b'IEND',b''))


class Fixture(BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

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
                <style>.story {font-style:italic}</style></head>
                <body><p class="story" style="padding:7px">Reading on Kindle</p>
                <img src="picture.png" alt="Photo"><a href="next">Next page</a><script>hidden script</script></body></html>'''),
            '/article/picture.png': ('image/png', fixture_image()),
            '/article/base.css': ('text/css', b'.story { color:#555; margin:20px; }'),
            '/article/next': ('text/html', b'<title>Next fixture</title><p>A second page.</p>'),
            '/api/states': ('application/json', json.dumps([
                dict(entity_id='sensor.room', state='23', attributes=dict(unit_of_measurement='C'))
            ]).encode())
        }
        if request.path.startswith('/api/history/period/'):
            params = parse_qs(request.query, keep_blank_values=True)
            requests.put((request.path, params))
            now = time.time()
            response = [[dict(s='20', lu=now-1200), dict(s='23', lu=now-60)]]
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
        assert 'padding:7px;' in clean and 'hidden script' not in clean
        assert 'data:image/png;base64,' in clean
        assert f'href="{origin}/article/next"' in clean
        pixels=(browser/'browser-raster-0').read_bytes()
        assert len(pixels)==544*560 and min(pixels)<128 and max(pixels)==255
        app.click('link:0')
        app.wait(lambda ui: label(ui,'Next fixture'))
        app.click('previous')
        app.wait(lambda ui: label(ui,'Browser fixture'))
    finally:
        app.close()
    print('PASS standalone Browser renders HTML and CSS, follows redirects and links, and preserves navigation history.')

    assistant=base/'home-assistant'
    assistant.mkdir()
    (assistant/'connection.json').write_text(json.dumps(dict(url=origin,token='fixture-token')))
    (assistant/'ha-favorites.txt').write_text('sensor.room\n')
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
            assert params['filter_entity_id']==['sensor.room']
            assert 'significant_changes_only' not in params
            start=path.rsplit('/',1)[1]
            fmt='%Y-%m-%dT%H:%M:%SZ'
            duration=datetime.strptime(params['end_time'][0],fmt)-datetime.strptime(start,fmt)
            spans.add(duration.total_seconds()/3600)
        assert {1,24}<=spans
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
