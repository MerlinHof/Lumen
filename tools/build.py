#!/usr/bin/env python3
"""Build explicit source/object lists; never link stray files from an output directory."""
import argparse,json,os,shlex,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sources():
    return [s.strip() for s in (ROOT/'scripts/sources.txt').read_text().splitlines() if s.strip() and not s.startswith('#')]
def dependencies(target):
    mupdf=Path(os.environ.get('MUPDF',ROOT/'.local/deps/mupdf')).resolve()
    zig=Path(os.environ.get('ZIG',ROOT/'.local/deps/zig/zig')).resolve()
    libdir=mupdf/'build'/('lumen-host' if target=='host' else 'lumen-arm')
    libs=[libdir/'libmupdf.a',libdir/'libmupdf-third.a']
    if not all(p.is_file() for p in libs):raise SystemExit('MuPDF libraries missing. Run python3 tools/setup.py '+target)
    if target=='kindle' and not zig.is_file():raise SystemExit('Zig missing. Run python3 tools/setup.py kindle')
    return mupdf,zig,libs
def object_path(target,source):return ROOT/'build'/target/'obj'/Path(source).with_suffix('.o')
def build(target):
    mupdf,zig,libs=dependencies(target)
    env=dict(os.environ,ZIG_GLOBAL_CACHE_DIR=str(ROOT/'.local/cache/zig'),ZIG_LOCAL_CACHE_DIR=str(ROOT/'.local/cache/zig-local'))
    compiler=['clang'] if target=='host' else [str(zig),'cc','-target','arm-linux-musleabi','-mcpu=cortex_a9']
    flags=['-O2' if target=='host' else '-Os','-Wall','-Wextra','-I'+str(mupdf/'include')]
    if target=='host':flags+=['-DLUMEN_HOST']
    objects=[]
    for source in sources():
        obj=object_path(target,source);obj.parent.mkdir(parents=True,exist_ok=True);dep=obj.with_suffix('.d');signature=obj.with_suffix('.command.json')
        cmd=compiler+flags+['-MMD','-MF',str(dep),'-c',str(ROOT/'src'/source),'-o',str(obj)]
        stale=not obj.exists() or not dep.exists() or not signature.exists()
        if not stale:
            try:
                paths=shlex.split(dep.read_text().replace('\\\n',' ').split(':',1)[1])
                stale=json.loads(signature.read_text())!=cmd or any(not Path(p).exists() or Path(p).stat().st_mtime_ns>obj.stat().st_mtime_ns for p in paths)
            except (OSError,ValueError):stale=True
        if stale:subprocess.run(cmd,env=env,check=True);signature.write_text(json.dumps(cmd))
        objects.append(obj)
    binary=ROOT/'build'/target/('lumen-host' if target=='host' else 'lumen')
    link=['clang++'] if target=='host' else [str(zig),'c++','-target','arm-linux-musleabi','-mcpu=cortex_a9','-static','-s','-Wl,--gc-sections']
    subprocess.run(link+[str(p) for p in objects+libs]+['-lm']+(['-lpthread'] if target=='host' else [])+['-o',str(binary)],env=env,check=True)
    print(binary)
    return binary
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('target',choices=['host','kindle'],default='host',nargs='?');build(p.parse_args().target)
