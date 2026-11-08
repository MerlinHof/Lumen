#!/usr/bin/env python3
"""Build one independent app; the LUMEN system is never compiled or linked."""
import argparse,hashlib,json,os,shutil,subprocess,zipfile
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=HERE.parent

def build_app(manifest,target='kindle',output=None):
    manifest=Path(manifest).resolve();meta=json.loads(manifest.read_text());folder=manifest.parent
    if meta.get('api')!=1 or meta.get('format')!='lumen-app-1':raise ValueError('App protocol 1 required')
    sdk=HERE/'sdk' if (HERE/'sdk').is_dir() else HERE
    out=Path(output or (ROOT/'build/apps'/target/meta['id'] if (ROOT/'lumen').exists() else folder/'build'/target)).resolve();out.mkdir(parents=True,exist_ok=True)
    compiler=shutil.which('clang') if target=='host' else os.environ.get('ZIG') or str(ROOT/'.local/deps/zig/zig')
    if target!='host' and not Path(compiler).is_file():compiler=shutil.which('zig')
    if not compiler:raise RuntimeError('Install Zig or set ZIG to its executable')
    command=[compiler] if target=='host' else [compiler,'cc','-target','arm-linux-musleabi','-mcpu=cortex_a9','-static','-s']
    command+=['-Os','-DLUMEN_PACKAGED','-ffunction-sections','-fdata-sections','-I'+str(sdk)]
    source=meta.pop('sources');command += [str(folder/p) for p in source]+['-lm','-o',str(out/'app')]
    env=dict(os.environ,ZIG_GLOBAL_CACHE_DIR=str(ROOT/'.local/cache/zig'),ZIG_LOCAL_CACHE_DIR=str(ROOT/'.local/cache/zig-local'))
    subprocess.run(command,check=True,env=env)
    files={'app':out/'app'}
    for path in meta.pop('assets',[]):
        p=Path(path)
        if p.is_absolute() or '..' in p.parts:raise ValueError('Unsafe asset path')
        files[path]=folder/path
    meta['files']=[dict(path=name,size=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for name,p in sorted(files.items())]
    package=out/(meta['id']+'.lumen-app')
    with zipfile.ZipFile(package,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        z.writestr('manifest.json',json.dumps(meta))
        for name,p in files.items():z.write(p,'payload/'+name)
    print(package);return package
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('manifest');p.add_argument('--target',choices=['kindle','host'],default='kindle');p.add_argument('--output');a=p.parse_args();build_app(a.manifest,a.target,a.output)
