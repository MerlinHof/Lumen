#!/usr/bin/env python3
"""Install or update LUMEN on a prepared Kindle; preserve all personal content."""
from pathlib import Path, PurePosixPath
import argparse,hashlib,json,os,re,shutil,sys,time

def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(1048576),b''):h.update(block)
    return h.hexdigest()

def destination(root,relative):
    p=PurePosixPath(relative)
    if p.is_absolute() or '..' in p.parts or not p.parts:raise RuntimeError('Invalid package path')
    target=root/relative
    if not target.resolve().is_relative_to(root):raise RuntimeError('Path escapes the Kindle')
    return target

def runtime_path(relative):
    return relative=='documents/LUMEN.sh' or (relative.startswith('lumen/') and not relative.startswith(('lumen/data/','lumen/backups/')))

def storage(root):
    sizes={}
    for label,path in [('runtime',root/'lumen'),('data',root/'lumen/data'),('backups',root/'lumen/backups')]:
        sizes[label]=sum(p.stat().st_size for p in path.rglob('*') if p.is_file() and not p.is_symlink()) if path.exists() else 0
    sizes['runtime']-=sizes['data']+sizes['backups']
    return sizes

def safe_backup(path,allowed):
    if not re.fullmatch(r'before-\d+\.\d+-\d{8}-\d{6}-\d{6}',path.name) or path.is_symlink():return False
    entries=list(path.rglob('*'))
    if any(p.is_symlink() for p in entries):return False
    files=[p for p in entries if p.is_file()]
    ordinary=0
    for p in files:
        rel=str(p.relative_to(path))
        if rel in allowed or rel=='.lumen-backup.json':ordinary+=1;continue
        # macOS writes AppleDouble xattrs to FAT volumes. Accept only genuine
        # sidecars of an existing recognized file or directory, never arbitrary
        # files merely named with an underscore prefix.
        if p.name.startswith('._'):
            peer=p.with_name(p.name[2:])
            with p.open('rb') as f:header=f.read(8)
            if header==bytes.fromhex('0005160700020000') and peer.exists() and (peer.is_dir() or str(peer.relative_to(path)) in allowed or peer.name=='.lumen-backup.json'):continue
        if p.name=='.DS_Store':
            with p.open('rb') as f:header=f.read(8)
            if header[4:8]==b'Bud1':continue
        return False
    return ordinary>0

def prune_backups(root,allowed,keep=2):
    parent=root/'lumen/backups'
    candidates=sorted((p for p in parent.iterdir() if p.is_dir() and safe_backup(p,allowed)),key=lambda p:tuple(p.name.rsplit('-',3)[1:]),reverse=True)
    reclaimed=0
    for path in candidates[keep:]:
        reclaimed+=sum(p.stat().st_size for p in path.rglob('*') if p.is_file())
        def missing_sidecar_only(function,filename,error):
            # macOS/FAT removes a file's AppleDouble peer automatically; a
            # directory traversal may still contain that now-missing entry.
            if not isinstance(error[1],FileNotFoundError):raise error[1]
        shutil.rmtree(path,onerror=missing_sidecar_only)
    return reclaimed

def update(bundle,kindle,keep_backups=2):
    kindle=kindle.resolve();bundle=bundle.resolve()
    if not (kindle/'documents').is_dir():raise RuntimeError('Kindle documents folder missing; select the mounted Kindle root.')
    if not (kindle/'libkh/bin/fbink').is_file():raise RuntimeError('This Kindle is not prepared: install a compatible jailbreak and Kindle .sh launcher (libkh/FBInk) first. LUMEN does not jailbreak stock firmware.')
    metadata=json.loads((bundle/'release.json').read_text());version=metadata['version']
    manifest=json.loads((bundle/'manifest.json').read_text());payload=bundle/'Kindle'
    legacy=json.loads((bundle/'legacy-files.json').read_text())
    for rel,expected in manifest.items():
        destination(kindle,rel)
        if not runtime_path(rel):raise RuntimeError('Package attempts to replace personal content')
        source=destination(payload,rel)
        if digest(source)!=expected:raise RuntimeError('Package incomplete or corrupt: '+rel)
    for rel in legacy:
        destination(kindle,rel)
        if not rel.startswith('documents/') or not rel.endswith(('.sh','.txt','.epub')):raise RuntimeError('Invalid legacy path')
    needed=sum((payload/r).stat().st_size for r in manifest)+sum((kindle/r).stat().st_size for r in manifest if (kindle/r).is_file())
    if shutil.disk_usage(kindle).free<needed+8*1048576:raise RuntimeError('Not enough space for the runtime and a rollback backup.')
    removals=[r for r,hashes in legacy.items() if (kindle/r).is_file() and digest(kindle/r) in hashes]
    changed=[r for r,expected in manifest.items() if not (kindle/r).is_file() or digest(kindle/r)!=expected]
    existing=([r for r in manifest if (kindle/r).is_file()]+removals) if changed or removals else []
    backup=None
    if existing:
        stamp=time.strftime('%Y%m%d-%H%M%S')+'-'+str((time.time_ns()//1000)%1000000).zfill(6)
        backup=kindle/'lumen/backups'/('before-'+version+'-'+stamp);backup.mkdir(parents=True)
        for rel in existing:
            saved=backup/rel;saved.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(kindle/rel,saved)
        (backup/'.lumen-backup.json').write_text(json.dumps({'version':version,'files':existing}))
    for rel in changed:
        dest=destination(kindle,rel);dest.parent.mkdir(parents=True,exist_ok=True);temp=dest.with_name(dest.name+'.lumen-update')
        with (payload/rel).open('rb') as source,temp.open('wb') as out:
            shutil.copyfileobj(source,out);out.flush();os.fsync(out.fileno())
        if rel.endswith('.sh') or rel in ('lumen/bin/lumen','lumen/bin/mobitool'):temp.chmod(0o755)
        os.replace(temp,dest)
    for rel,expected in manifest.items():
        if digest(kindle/rel)!=expected:raise RuntimeError('Copy verification failed: '+rel)
    for rel in removals:(kindle/rel).unlink()
    # Create only structural folders; user settings are initialized by the native app.
    for name in ['data','data/books','data/covers','data/notes','data/photos']:(kindle/'lumen'/name).mkdir(parents=True,exist_ok=True)
    reclaimed=prune_backups(kindle,set(manifest)|set(legacy)|{'lumen/ANFORDERUNGEN.md'},keep_backups) if (kindle/'lumen/backups').exists() else 0
    print(f'LUMEN {version}: {len(changed)} files copied; {len(manifest)} files verified.')
    print('Personal books, settings, notes and photos preserved.')
    if backup:print('Rollback backup:',backup)
    if reclaimed:print(f'Old managed runtime backups removed: {reclaimed/1048576:.1f} MiB reclaimed. Two newest retained.')
    print('Safely eject the Kindle, disconnect USB, then open LUMEN once to complete on-device setup.')
    return backup

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--kindle',default='/Volumes/Kindle');p.add_argument('--storage',action='store_true');p.add_argument('--keep-backups',type=int,default=2)
    a=p.parse_args()
    try:
        if a.keep_backups<1:raise RuntimeError('Keep at least one rollback backup')
        if a.storage:print(json.dumps(storage(Path(a.kindle)),indent=2));return 0
        update(Path(__file__).resolve().parent,Path(a.kindle),a.keep_backups)
    except (OSError,ValueError,RuntimeError) as error:print('Installation not completed:',error,file=sys.stderr);return 1
    return 0
if __name__=='__main__':raise SystemExit(main())
