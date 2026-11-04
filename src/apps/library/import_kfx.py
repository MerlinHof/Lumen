#!/usr/bin/env python3
"""Offline conversion of a device-owned KFX book; originals are never changed."""
import contextlib
import os
from pathlib import Path
import sys
import tempfile
import zipfile

def convert(source, destination):
    root = Path(__file__).resolve().parent
    sys.path.insert(0, str(root / 'kfxlib/calibre-plugin-modules'))
    from dedrm import ion
    sys.modules['ion'] = ion
    from kfxdedrm import KFXZipBook
    from kfxlib import YJ_Book
    serial = os.environ.get('LUMEN_KINDLE_SERIAL', '').strip()
    if not serial:
        raise ValueError('Device identity is unavailable')
    source = Path(source).resolve()
    sidecars = source.with_suffix('.sdr')
    files = [source]
    if sidecars.is_dir():
        files += [p for p in sidecars.rglob('*') if p.is_file() and not p.is_symlink()]
    with tempfile.TemporaryDirectory(prefix='book-', dir=root.parent) as work:
        work = Path(work)
        archive, clear = work / 'source.kfx-zip', work / 'clear.kfx-zip'
        with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_STORED) as z:
            for path in files:
                z.write(path, str(path.relative_to(source.parent)))
        # Engines are verbose; book text, keys and serials never enter session logs.
        with open(os.devnull, "w") as quiet, contextlib.redirect_stdout(quiet), contextlib.redirect_stderr(quiet):
            book = KFXZipBook(str(archive))
            book.processBook([serial])
            book.getFile(str(clear))
            output = YJ_Book(str(clear)).convert_to_epub()
        destination = Path(destination)
        temp = destination.with_name(destination.name + '.part')
        try:
            temp.write_bytes(output)
            os.replace(temp, destination)
        finally:
            temp.unlink(missing_ok=True)

if __name__ == '__main__':
    try:
        convert(sys.argv[1], sys.argv[2])
    except Exception:
        # Detailed third-party exception text can contain private document data.
        sys.exit(1)
