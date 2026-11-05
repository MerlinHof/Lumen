#!/usr/bin/env python3
"""Build the pinned, compressed, on-demand Kindle import runtime."""
from pathlib import Path
import hashlib
import io
import json
import tarfile
import urllib.request
import zipfile
ROOT=Path(__file__).resolve().parents[1]
def prepare():
    lock=json.loads((ROOT/'tools/config/amazon-runtime.json').read_text())
    archive=ROOT/'.local/deps/kindle-import/kindle-koplugin-armv7.zip'
    archive.parent.mkdir(parents=True,exist_ok=True)
    if not archive.exists():
        with urllib.request.urlopen(lock['url'],timeout=60) as source:archive.write_bytes(source.read())
    if hashlib.sha256(archive.read_bytes()).hexdigest()!=lock['sha256']:raise RuntimeError('Import runtime checksum mismatch')
    output=ROOT/'build/kindle/amazon-runtime.tar.gz';output.parent.mkdir(parents=True,exist_ok=True)
    def add(tar,name,data,mode=0o644):
        info=tarfile.TarInfo(name);info.size=len(data);info.mode=mode;info.mtime=0;tar.addfile(info,io.BytesIO(data))
    with zipfile.ZipFile(archive) as z,tarfile.open(output,'w:gz',compresslevel=9) as tar:
        for member in sorted(z.namelist()):
            if not member.startswith('kindle.koplugin/') or member.endswith('/'):continue
            name=member[len('kindle.koplugin/'):]
            if '..' in Path(name).parts or name.startswith('/'):raise RuntimeError('Unsafe upstream runtime path')
            if name=='libsyscall_wrapper.so' or name.startswith('dist/bin/') or name.startswith('dist/lib/') or name.startswith('dist/kfxlib/') or name.startswith('dist/dedrm/'):
                if '__pycache__' in name or name.endswith(('.pyc','.pyo','.jar')) or 'crypto_hook' in name:continue
                add(tar,name,z.read(member),0o755 if name.startswith('dist/bin/') or name.endswith('.so') or '/runtime/' in name else 0o644)
        for source,name in [('lumen/apps/library/import_kfx.py','dist/import_kfx.py'),('tools/vendor/amazon/kfxdedrm.py','dist/kfxdedrm.py'),('tools/vendor/amazon/COPYING','COPYING'),('tools/vendor/amazon/README.md','SOURCES.md')]:
            add(tar,name,(ROOT/source).read_bytes())
    print('On-demand Kindle import runtime:',round(output.stat().st_size/1048576,1),'MiB compressed')
    return output
if __name__=='__main__':prepare()
