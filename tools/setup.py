#!/usr/bin/env python3
"""Fetch pinned official build dependencies into .local; no repository binaries."""
import argparse,hashlib,json,os,platform,subprocess,sys,tarfile,urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def download(url,dest,sha):
    if not dest.exists() or hashlib.sha256(dest.read_bytes()).hexdigest()!=sha:
        temp=dest.with_suffix(dest.suffix+'.part')
        with urllib.request.urlopen(url,timeout=90) as response,temp.open('wb') as out:
            import shutil;shutil.copyfileobj(response,out)
        if hashlib.sha256(temp.read_bytes()).hexdigest()!=sha:temp.unlink();raise RuntimeError('Dependency checksum mismatch')
        temp.replace(dest)
def extract(archive,dest):
    import shutil
    temporary=dest.parent/(dest.name+'-extract');temporary.mkdir(exist_ok=True)
    with tarfile.open(archive) as t:
        for m in t.getmembers():
            p=Path(m.name)
            if p.is_absolute() or '..' in p.parts:raise RuntimeError('Unsafe archive path')
        t.extractall(temporary)
    entries=list(temporary.iterdir());assert len(entries)==1
    entries[0].rename(dest);temporary.rmdir()
def setup(target):
    lock=json.loads((ROOT/'tools/config/dependencies.json').read_text());deps=ROOT/'.local/deps';cache=ROOT/'.local/cache'
    deps.mkdir(parents=True,exist_ok=True);cache.mkdir(parents=True,exist_ok=True)
    mupdf=deps/'mupdf'
    if not mupdf.exists():
        data=lock['mupdf'];archive=cache/('mupdf-'+data['version']+'-source.tar.gz');download(data['url'],archive,data['sha256']);extract(archive,mupdf)
    zig=deps/'zig'
    if target=='kindle' and not zig.exists():
        system={'Darwin':'macos','Linux':'linux'}[platform.system()];arch={'arm64':'aarch64','aarch64':'aarch64','x86_64':'x86_64'}[platform.machine()]
        with urllib.request.urlopen(lock['zig']['index'],timeout=60) as f:data=json.load(f)[lock['zig']['version']][arch+'-'+system]
        archive=cache/data['tarball'].split('/')[-1];download(data['tarball'],archive,data['shasum']);extract(archive,zig)
    env=dict(os.environ,MUPDF=str(mupdf),ZIG=str(zig/'zig'),ZIG_GLOBAL_CACHE_DIR=str(cache/'zig'),ZIG_LOCAL_CACHE_DIR=str(cache/'zig-local'))
    lib=mupdf/'build'/('lumen-host' if target=='host' else 'lumen-arm')/'libmupdf.a'
    if not lib.exists():subprocess.run(['sh',str(ROOT/'tools/vendor/build-mupdf.sh')]+(['host'] if target=='host' else []),env=env,check=True)
    if target=='kindle':
        # libmobi's small pinned source archive is included for reproducibility.
        mobi=deps/'libmobi'
        if not mobi.exists():extract(ROOT/'tools/vendor/libmobi-0.12.tar.gz',mobi)
        if not (mobi/'config.h').exists() or '#define USE_ENCRYPTION 1' not in (mobi/'config.h').read_text():subprocess.run(['sh','configure','--with-libxml2=no','--with-zlib=no','--enable-encryption','--disable-shared','--enable-tools-static'],cwd=mobi,check=True)
        env.update(LIBMOBI=str(mobi),MOBI_BUILD=str(ROOT/'build/kindle/mobi-obj'))
        subprocess.run(['sh',str(ROOT/'tools/vendor/build-libmobi.sh')],env=env,check=True)
        subprocess.run([sys.executable,str(ROOT/'tools/prepare-amazon.py')],check=True)
    print('Dependencies ready:',target)
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('target',choices=['host','kindle']);setup(p.parse_args().target)
