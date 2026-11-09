from pathlib import Path
import sys,subprocess,urllib.request,urllib.error,urllib.parse,string,json,shutil,hashlib,socket,time
sandbox=Path('build/tests/http-sandbox');shutil.rmtree(sandbox,ignore_errors=True)
root=sandbox/'data';(root/'notes').mkdir(parents=True)
(sandbox/'bin').mkdir();(sandbox/'bin/lumen').write_bytes(b'old executable')
command='x'
(root/'notes/123.txt').write_text('# Example\n\nA **bold** note.\n');(root/'notes/123-page-0.ink').write_bytes(bytes([255])*412800)
p=subprocess.Popen([sys.argv[1]],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=open('build/tests/http-server.log','w'),text=True)
try:
 url=p.stdout.readline().strip();code=p.stdout.readline().strip();assert len(code)==4 and all(c in string.ascii_uppercase+string.digits for c in code)
 def req(path,method='GET',body=None,auth=None):
  try:
   r=urllib.request.urlopen(urllib.request.Request(url+path,data=body,method=method,headers={'Authorization':'Bearer '+(auth or code)}),timeout=30)
   return r.status,r.read()
  except urllib.error.HTTPError as e:return e.code,e.read()
 address=urllib.parse.urlsplit(url)
 idle=[socket.create_connection((address.hostname,address.port),timeout=3) for _ in range(6)]
 partial=socket.create_connection((address.hostname,address.port),timeout=3)
 partial.sendall(('GET /api/books HTTP/1.1\r\nHost: '+address.netloc+'\r\nAuthorization: Bearer '+code+'\r\n').encode())
 started=time.monotonic()
 assert req('/api/books')[0]==200
 assert json.loads(req('/api/terminal')[1])=={'enabled':False}
 assert req('/api/terminal','POST',b'echo unsafe')[0]==403
 assert req('/api/upload?name=Arrival.txt','PUT',b'Live library test.')[0]==201
 assert Path('build/tests/http-sandbox/data/books-changed').is_file()
 assert any(book['title']=='Arrival' for book in json.loads(req('/api/books')[1])['books'])
 assert time.monotonic()-started<3, 'Idle browser connections blocked the server'
 partial.sendall(b'\r\n')
 assert b'200 OK' in partial.recv(1024)
 partial.close()
 for connection in idle:connection.close()

 status,state=req('/api/custom-app/status');assert status==200 and 'version' in json.loads(state)
 status,guide=req('/api/custom-app/guide');assert status==200 and b'lumen-app-1' in guide
 app_package=Path('build/apps/host/lumen.sliding-puzzle/lumen.sliding-puzzle.lumen-app').read_bytes()
 assert req('/api/apps/install','PUT',app_package)[0]==201
 status,catalog=req('/api/apps');assert status==200
 apps=json.loads(catalog)['apps'];assert len([a for a in apps if a['id']=='lumen.sliding-puzzle'])==1
 assert(sandbox/'bin/lumen').read_bytes()==b'old executable'
 assert not(sandbox/'.update/relaunch').exists()
 status,data=req('/api/notes');assert status==200 and len(json.loads(data)['notes'])==1
 assert req('/api/note?id=123')[1].startswith(b'# Example')
 status,pdf=req('/api/note?id=123&format=pdf');assert status==200 and pdf.startswith(b'%PDF')
 assert req('/api/note?id=123/../../etc/passwd')[0]==400
 assert req('/api/upload?study=1&name=Lesson.txt','PUT',b'Study material')[0]==201
 assert(root/'study/Lesson.txt').read_bytes()==b'Study material'
 assert req('/api/setup?key=llm-key.txt','PUT',b'test-key')[0]==200
 assert req('/api/setup?key=../outside','PUT',b'test')[0]==400
 assert req('/api/update','PUT',bytes(4096))[0]==400
 binary=bytearray(8192)
 binary[:7]=b'\x7fELF\x01\x01\x01'
 binary[16]=2
 binary[18]=40
 status,receipt=req('/api/custom-app','PUT',bytes(binary))
 assert status==201 and json.loads(receipt)['sha256']==hashlib.sha256(binary).hexdigest()
 assert (sandbox/'bin/lumen').read_bytes()==b'old executable'
 assert req('/api/custom-app/install?sha256='+'0'*64,'POST')[0]==409
 assert req('/api/custom-app/install?sha256='+hashlib.sha256(binary).hexdigest(),'POST')[0]==200
 assert (sandbox/'.update/relaunch').is_file()
 command='u'
 for i in range(5):assert req('/api/books',auth='WRNG')[0]==401
 assert req('/api/books',auth='WRNG')[0]==429
 print('PASS HTTP: pairing, notes without phantom entries, TXT/PDF exports, study upload, secret setup allowlist, traversal rejection and rate limit.')
finally:
 p.stdin.write(command);p.stdin.flush();assert p.wait(timeout=10)==0
if command=='u':
 assert (sandbox/'bin/lumen').read_bytes()==binary
 assert (sandbox/'.update/previous').read_bytes()==b'old executable'
 print('PASS wireless update: reject invalid ELF, stage an ARM binary, verify its checksum, apply only after approval and retain the previous binary.')
