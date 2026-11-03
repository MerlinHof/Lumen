"""Run the actual installer script against an isolated, mocked Kindle filesystem."""
from pathlib import Path
import os, subprocess, shutil
base=Path(__file__).resolve().parents[1]
root=Path(os.environ['INSTALL_TEST_WORK']).resolve()
root.mkdir(parents=True,exist_ok=True)
for d in ['etc/upstart','tmp','mnt/us/lumen/bin','mnt/us/lumen/system','mnt/us/documents','tools']:(root/d).mkdir(parents=True,exist_ok=True)
(root/'etc/prettyversion.txt').write_text('Kindle 5.12.2.2 (test)\n')
(root/'etc/upstart/lab126_gui.conf').write_text('description "mock"\n')
shutil.copy(base/'device/system/lumen.conf',root/'mnt/us/lumen/system/lumen.conf')
shutil.copy(base/'build/host/lumen-host',root/'mnt/us/lumen/bin/lumen')
shutil.copy(base/'device/system/selftest.epub',root/'mnt/us/lumen/system/selftest.epub')
commands={'id':'echo 0','uname':'echo armv7l','mount':"echo '/dev/root on / type ext3 (ro,noatime)'",'mntroot':f'echo "$*" >> "{root}/mount-changes"','initctl':f'echo "$*" >> "{root}/init-changes"','lipc-wait-event':'exit 0','lipc-probe':'exit 0','sync':'exit 0'}
for name,body in commands.items():
 p=root/'tools'/name;p.write_text('#!/bin/sh\n'+body+'\n');p.chmod(0o755)
env=dict(os.environ,PATH=str(root/'tools')+':/usr/bin:/bin')
src=(base/'device/install-autostart.sh').read_text()
for path in ['/mnt/us','/etc/upstart','/etc/prettyversion.txt','/tmp/lumen-boot-started']:
 src=src.replace(path,str(root)+path)
script=root/'install.sh';script.write_text(src)
r=subprocess.run(['/bin/sh',str(script)],env=env);assert r.returncode==0
assert (root/'etc/upstart/lumen.conf').read_bytes()==(base/'device/system/lumen.conf').read_bytes()
assert (root/'mount-changes').read_text().splitlines()==['rw','ro']
assert (root/'mnt/us/lumen/autostart.enabled').exists()
assert 'SUCCESS' in (root/'mnt/us/lumen/autostart-install.log').read_text()
# A foreign job must be preserved, with no attempt to remount root.
(root/'etc/upstart/lumen.conf').write_text('unrelated job\n')
(root/'mount-changes').unlink()
r=subprocess.run(['/bin/sh',str(script)],env=env);assert r.returncode!=0
assert (root/'etc/upstart/lumen.conf').read_text()=='unrelated job\n'
assert not (root/'mount-changes').exists()
job=(base/'device/system/lumen.conf').read_text();assert 'respawn' not in '\n'.join(l for l in job.splitlines() if not l.strip().startswith('#'))
assert '/tmp/lumen-boot-started' in job and 'DISABLE_AUTOSTART' in job
print('PASS: on-device EPUB preflight, job install, read-only mount restoration, foreign-job rejection and boot guards.')
