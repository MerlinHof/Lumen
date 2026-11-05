"""Exercise reversible firmware UI ownership with fake process identities."""
from pathlib import Path
import os,subprocess
base=Path(__file__).resolve().parents[2];root=Path(os.environ['LAUNCH_TEST_WORK']).resolve();root.mkdir(parents=True,exist_ok=True)
for folder in ['mnt/us/lumen/bin','var/tmp','tmp/lumen-session','tools','proc/999999','proc/999998']:(root/folder).mkdir(parents=True,exist_ok=True)
for pid,name in [('999999','awesome'),('999998','cvm')]:
 (root/f'proc/{pid}/status').write_text('State:\tS (sleeping)\n')
 (root/f'proc/{pid}/stat').write_text(' '.join([pid,'('+name+')']+['0']*19+['12345'])+'\n')
 (root/f'proc/{pid}/comm').write_text(name+'\n')
(root/'tmp/lumen-session').rmdir()
(root/'mnt/us/lumen/bin/lumen').write_text('#!/bin/sh\nexit 0\n');(root/'mnt/us/lumen/bin/lumen').chmod(0o755)
for name,body in {'pidof':'case "$1" in awesome) echo 999999;; cvm) echo 999998;; esac','sleep':'exit 0','lipc-get-prop':'echo active','lipc-set-prop':'echo "lipc $*" >> "$LAUNCH_ACTIONS"','ip':'exit 0','iptables':'exit 0','ps':'exit 0'}.items():
 f=root/'tools'/name;f.write_text('#!/bin/sh\n'+body+'\n');f.chmod(0o755)
def rewritten(s):
 for path in ['/mnt/us','/var/tmp','/tmp/lumen-session','/proc/']:s=s.replace(path,str(root)+path)
 return s.replace('#!/bin/sh','#!/bin/sh\nkill() { echo "kill $*" >> "$LAUNCH_ACTIONS"; }',1)
helper=root/'mnt/us/lumen/ui-session.sh';helper.write_text(rewritten((base/'tools/device/ui-session.sh').read_text()))
s=(base/'tools/device/launch.sh').read_text();assert 'stop lab126_gui' not in s
f=root/'launch.sh';f.write_text(rewritten(s))
env=dict(os.environ,PATH=str(root/'tools')+':/usr/bin:/bin',LAUNCH_ACTIONS=str(root/'actions'))
r=subprocess.run(['/bin/sh',str(f)],env=env,timeout=10);assert r.returncode==0
log=(root/'actions').read_text()
assert 'kill -STOP 999999' in log and 'kill -CONT 999999' in log
assert '999998' not in log
assert 'disableEnablePillow disable'in log and 'disableEnablePillow enable'in log
assert 'preventScreenSaver 0'in log
assert not(root/'tmp/lumen-session').exists()
# Existing pauses are not ours; reused PIDs must never be resumed.
(root/'tmp/lumen-session').mkdir();(root/'actions').write_text('')
(root/'proc/999998/status').write_text('State:\tT (stopped)\n')
subprocess.run(['/bin/sh',str(helper),'pause'],env=env,check=True)
(root/'proc/999999/stat').write_text(' '.join(['999999','(awesome)']+['0']*19+['77777'])+'\n')
subprocess.run(['/bin/sh',str(helper),'resume'],env=env,check=True)
log=(root/'actions').read_text();assert '999998' not in log and 'kill -CONT' not in log
(root/'tmp/lumen-session').rmdir()
print('PASS: reversible UI pause, backend preservation, existing pauses and PID reuse guards.')
