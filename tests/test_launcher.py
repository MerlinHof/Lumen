"""Exercise launcher ownership/cleanup using fake firmware commands and fake process IDs."""
from pathlib import Path
import os,subprocess
base=Path(__file__).resolve().parents[1];root=Path(os.environ['LAUNCH_TEST_WORK']).resolve();root.mkdir(parents=True,exist_ok=True)
for folder in ['mnt/us/lumen/bin','var/tmp','tmp','tools','proc/999999']:(root/folder).mkdir(parents=True,exist_ok=True)
(root/'proc/999999/status').write_text('State:\tS (sleeping)\n')
(root/'mnt/us/lumen/bin/lumen').write_text('#!/bin/sh\nexit 0\n');(root/'mnt/us/lumen/bin/lumen').chmod(0o755)
for name,body in {'pidof':'echo 999999','sleep':'exit 0','lipc-get-prop':'echo active','lipc-set-prop':'echo "lipc $*" >> "$LAUNCH_ACTIONS"','ip':'exit 0','iptables':'exit 0','ps':'exit 0'}.items():
 f=root/'tools'/name;f.write_text('#!/bin/sh\n'+body+'\n');f.chmod(0o755)
s=(base/'device/launch.sh').read_text();assert 'stop lab126_gui'not in s and 'kill -STOP "$wm_pid"'in s
for path in ['/mnt/us','/var/tmp','/tmp/lumen-session','/proc/']:s=s.replace(path,str(root)+path)
s=s.replace('#!/bin/sh','#!/bin/sh\nkill() { echo "kill $*" >> "$LAUNCH_ACTIONS"; }',1)
f=root/'launch.sh';f.write_text(s)
env=dict(os.environ,PATH=str(root/'tools')+':/usr/bin:/bin',LAUNCH_ACTIONS=str(root/'actions'))
r=subprocess.run(['/bin/sh',str(f)],env=env,timeout=10);assert r.returncode==0
log=(root/'actions').read_text();assert 'kill -STOP 999999'in log and 'kill -CONT 999999'in log
assert 'disableEnablePillow disable'in log and 'disableEnablePillow enable'in log
assert 'preventScreenSaver 0'in log
assert not(root/'tmp/lumen-session').exists()
print('PASS: keep firmware backend alive, pause/resume owned window manager, restore pillow/power and release session lock.')
