from pathlib import Path
import sys,subprocess,urllib.request,urllib.error,string,json,shutil
root=Path('build/tests/http-data');shutil.rmtree(root,ignore_errors=True);(root/'notes').mkdir(parents=True)
(root/'notes/123.txt').write_text('# Example\n\nA **bold** note.\n');(root/'notes/123-page-0.ink').write_bytes(bytes([255])*412800)
p=subprocess.Popen([sys.argv[1]],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=open('build/tests/http-server.log','w'),text=True)
try:
 url=p.stdout.readline().strip();code=p.stdout.readline().strip();assert len(code)==4 and all(c in string.ascii_uppercase+string.digits for c in code)
 def req(path,method='GET',body=None,auth=None):
  try:
   r=urllib.request.urlopen(urllib.request.Request(url+path,data=body,method=method,headers={'Authorization':'Bearer '+(auth or code)}),timeout=30)
   return r.status,r.read()
  except urllib.error.HTTPError as e:return e.code,e.read()
 assert req('/api/books')[0]==200
 status,data=req('/api/notes');assert status==200 and len(json.loads(data)['notes'])==1
 assert req('/api/note?id=123')[1].startswith(b'# Example')
 status,pdf=req('/api/note?id=123&format=pdf');assert status==200 and pdf.startswith(b'%PDF')
 assert req('/api/note?id=123/../../etc/passwd')[0]==400
 assert req('/api/upload?study=1&name=Lesson.txt','PUT',b'Study material')[0]==201
 assert(root/'study/Lesson.txt').read_bytes()==b'Study material'
 assert req('/api/setup?key=llm-key.txt','PUT',b'test-key')[0]==200
 assert req('/api/setup?key=../outside','PUT',b'test')[0]==400
 for i in range(5):assert req('/api/books',auth='WRNG')[0]==401
 assert req('/api/books',auth='WRNG')[0]==429
 print('PASS HTTP: pairing, notes without phantom entries, TXT/PDF exports, study upload, secret setup allowlist, traversal rejection and rate limit.')
finally:
 p.stdin.write('x');p.stdin.flush();p.wait(timeout=10)
