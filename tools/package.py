#!/usr/bin/env python3
"""Package only explicit runtime files; never sweep a workspace into a release."""
from pathlib import Path
import hashlib,json,shutil,zipfile
ROOT=Path(__file__).resolve().parents[1]
def package():
    version=(ROOT/'VERSION').read_text().strip();dist=ROOT/'dist';dist.mkdir(exist_ok=True)
    bundle=dist/('LUMEN-'+version);payload=bundle/'Kindle'
    if bundle.exists():shutil.rmtree(bundle)
    mapping=json.loads((ROOT/'packaging/runtime.json').read_text());manifest={}
    for rel,local in mapping.items():
        source=ROOT/local
        if not source.is_file():raise RuntimeError('Missing runtime file: '+local+'; build Kindle first.')
        dest=payload/rel;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,dest)
        manifest[rel]=hashlib.sha256(dest.read_bytes()).hexdigest()
    (bundle/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    (bundle/'release.json').write_text(json.dumps({'version':version,'firmware':'5.12.2.2','architecture':'armv7','runtime_files':len(manifest)},indent=2)+'\n')
    shutil.copy2(ROOT/'packaging/legacy.json',bundle/'legacy-files.json')
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
    archive=dist/('LUMEN-'+version+'.zip')
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for p in sorted(bundle.rglob('*')):
            if p.is_file():z.write(p,Path(bundle.name)/p.relative_to(bundle))
    (archive.with_suffix('.zip.sha256')).write_text(hashlib.sha256(archive.read_bytes()).hexdigest()+'  '+archive.name+'\n')
    size=sum((payload/r).stat().st_size for r in manifest)
    print(f'{len(manifest)} runtime files; {size/1048576:.1f} MiB installed; {archive.stat().st_size/1048576:.1f} MiB download.\n{archive}')
    return bundle
if __name__=='__main__':package()
