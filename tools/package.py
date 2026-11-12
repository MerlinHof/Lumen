#!/usr/bin/env python3
"""Package only explicit runtime files; never sweep a workspace into a release."""
from pathlib import Path
import hashlib,json,shutil,zipfile,re
ROOT=Path(__file__).resolve().parents[1]
def runtime_mapping():
    mapping=json.loads((ROOT/'tools/config/runtime.json').read_text())
    for manifest in sorted((ROOT/'lumen/apps').glob('*/package.json')):
        app_id=json.loads(manifest.read_text())['id']
        if not re.fullmatch(r'[a-z0-9][a-z0-9.-]{0,62}',app_id):
            raise RuntimeError('Invalid app ID in '+str(manifest))
        filename=app_id+'.lumen-app'
        destination='lumen/app-packages/'+filename
        if destination in mapping:
            raise RuntimeError('Duplicate shipped app: '+app_id)
        mapping[destination]=f'build/apps/kindle/{app_id}/{filename}'
    return mapping

def validate_runtime_mapping(mapping):
    from pathlib import PurePosixPath
    for destination,source in mapping.items():
        parts=PurePosixPath(destination)
        if parts.is_absolute() or '..' in parts.parts or any(p.startswith('._') for p in parts.parts):raise RuntimeError('Unsafe package destination')
        if destination!='documents/LUMEN.sh' and (not destination.startswith('lumen/') or (destination in ('lumen/data','lumen/appdata','lumen/backups','lumen/apps') or destination.startswith(('lumen/data/','lumen/appdata/','lumen/backups/','lumen/apps/','lumen/.update/')))):raise RuntimeError('Personal data or backups must never enter a release')
        path=(ROOT/source).resolve()
        if not path.is_relative_to(ROOT):raise RuntimeError('Unsafe package source')
        relative=path.relative_to(ROOT);parts=relative.parts
        if not (relative.is_relative_to('lumen/assets') or relative.is_relative_to('tools/device')
                or (len(parts)==5 and parts[:2]==('lumen','apps') and parts[3:]==('assets','icon.pgm'))
                or parts[0] in {'docs', 'build', 'LICENSE'}):
            raise RuntimeError('Non-runtime package source')
        if parts[0]=='build' and not relative.is_relative_to('build/apps/kindle') and relative.as_posix() not in {'build/kindle/lumen','build/kindle/mobitool','build/kindle/amazon-runtime.tar.gz'}:raise RuntimeError('Build/test data must never enter a release')
        if any(p in {'.git','.local','dist','backups','data'} for p in parts):raise RuntimeError('Unsafe package source')

def validate_version(version):
    if not re.fullmatch(r'[1-9][0-9]*\.[0-9]',version):
        raise RuntimeError('Use one minor digit: 2.9 is followed by 3.0, never 2.10.')
    return version

def validate_app_coverage(mapping,version):
    """Require packages for active apps, except the two known 5.9 holdouts."""
    if tuple(map(int,version.split('.'))) < (5,9):
        return
    catalog=(ROOT/'lumen/system/app_packages.c').read_text()
    keys=re.search(r'keys\[APP_COUNT\]\s*=\s*\{([^}]+)\}',catalog)
    if not keys:
        raise RuntimeError('Cannot read the active app catalog')
    active=set(re.findall(r'"(lumen\.[a-z0-9.-]+)"',keys[1]))|{'lumen.notifications'}
    shipped={Path(path).stem for path in mapping if path.startswith('lumen/app-packages/')}
    allowed={'lumen.notes','lumen.settings'} if version=='5.9' else set()
    missing=sorted(active-shipped-allowed)
    if missing:
        raise RuntimeError('Standalone app migration incomplete: '+', '.join(missing))

def package():
    version=validate_version((ROOT/'VERSION').read_text().strip());dist=ROOT/'dist';dist.mkdir(exist_ok=True)
    mapping=runtime_mapping();validate_runtime_mapping(mapping);validate_app_coverage(mapping,version)
    bundle=dist/('LUMEN-'+version);payload=bundle/'Kindle'
    if bundle.exists():shutil.rmtree(bundle)
    manifest={}
    for rel,local in mapping.items():
        source=ROOT/local
        if not source.is_file():raise RuntimeError('Missing runtime file: '+local+'; build Kindle first.')
        dest=payload/rel;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,dest)
        manifest[rel]=hashlib.sha256(dest.read_bytes()).hexdigest()
    (bundle/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    (bundle/'release.json').write_text(json.dumps({'version':version,'firmware':'5.12.2.2','architecture':'armv7','runtime_files':len(manifest)},indent=2)+'\n')
    shutil.copy2(ROOT/'tools/config/legacy.json',bundle/'legacy-files.json')
    shutil.copy2(ROOT/'tools/update-device.py',bundle/'Install-LUMEN.py')
    command=bundle/'Install-LUMEN.command'
    command.write_text('''#!/bin/sh
base=$(CDPATH='' cd "$(dirname "$0")" && pwd)
/usr/bin/python3 "$base/Install-LUMEN.py" "$@"
result=$?
printf '\\nPress Enter to close.\\n'
read answer
exit "$result"
''');command.chmod(0o755)
    shutil.copy2(ROOT/'docs/installation.md',bundle/'INSTALL.md')
    shutil.copy2(ROOT/'docs/validation.md',bundle/'VALIDATION.md')
    shutil.copy2(ROOT/'docs/wireless-updates.md',bundle/'wireless-updates.md')
    archive=dist/('LUMEN-'+version+'.zip')
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for p in sorted(bundle.rglob('*')):
            if p.is_file():z.write(p,Path(bundle.name)/p.relative_to(bundle))
    (archive.with_suffix('.zip.sha256')).write_text(hashlib.sha256(archive.read_bytes()).hexdigest()+'  '+archive.name+'\n')
    system_archive=dist/f'LUMEN-{version}.lumen-system'
    system_manifest={'format':'lumen-system-1','version':version,'files':[
        {'path':rel,'size':(payload/rel).stat().st_size,'sha256':sha} for rel,sha in sorted(manifest.items())]}
    with zipfile.ZipFile(system_archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        z.writestr('manifest.json',json.dumps(system_manifest))
        for rel in sorted(manifest):z.write(payload/rel,'payload/'+rel)
    wireless={'format':'lumen-system-1','version':version,
              'url':f'https://github.com/MerlinHof/Lumen/releases/download/v{version}/{system_archive.name}',
              'size':system_archive.stat().st_size,'installed_size':sum((payload/r).stat().st_size for r in manifest),
              'sha256':hashlib.sha256(system_archive.read_bytes()).hexdigest()}
    (dist/f'LUMEN-{version}.wireless.json').write_text(json.dumps(wireless,indent=2)+'\n')
    size=sum((payload/r).stat().st_size for r in manifest)
    print(f'{len(manifest)} runtime files; {size/1048576:.1f} MiB installed; {archive.stat().st_size/1048576:.1f} MiB download.\n{archive}')
    return bundle
if __name__=='__main__':package()
