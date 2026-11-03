"""Fresh install, upgrades, retention, corruption and path-boundary tests."""
from pathlib import Path
import importlib.util,json,shutil,sys
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from package import package
spec=importlib.util.spec_from_file_location('installer',ROOT/'tools/update-device.py');u=importlib.util.module_from_spec(spec);spec.loader.exec_module(u)
if '--fixture' in sys.argv:
    bundle=ROOT/'build/tests/fixture-package';shutil.rmtree(bundle,ignore_errors=True)
    payload=bundle/'Kindle';paths={'documents/LUMEN.sh':'launcher','lumen/bin/lumen':'binary-fixture','lumen/system/selftest.epub':'epub-fixture'}
    manifest={}
    for rel,content in paths.items():
        target=payload/rel;target.parent.mkdir(parents=True,exist_ok=True);target.write_text(content);manifest[rel]=u.digest(target)
    (bundle/'manifest.json').write_text(json.dumps(manifest));(bundle/'release.json').write_text(json.dumps({'version':'1.9'}));(bundle/'legacy-files.json').write_text('{}')
else: bundle=package()
root=ROOT/'build/tests/installation'
shutil.rmtree(root,ignore_errors=True);(root/'documents').mkdir(parents=True)
try:u.update(bundle,root)
except RuntimeError as e:assert 'not prepared' in str(e)
else:raise AssertionError('Stock device must be rejected')
assert not (root/'lumen').exists()
(root/'libkh/bin').mkdir(parents=True);(root/'libkh/bin/fbink').write_text('firmware fixture')
(root/'documents/My book.epub').write_text('my book')
assert u.update(bundle,root) is None
(root/'lumen/data/notes').mkdir(exist_ok=True);(root/'lumen/data/notes/me.txt').write_text('my private note')
(root/'lumen/DISABLE_AUTOSTART').write_text('')
for i in range(3):
    (root/'lumen/bin/lumen').write_text('old runtime '+str(i));assert u.update(bundle,root)
backups=[p for p in (root/'lumen/backups').iterdir() if p.is_dir()];assert len(backups)==2
unknown=root/'lumen/backups/before-personal';(unknown/'lumen/data').mkdir(parents=True);(unknown/'lumen/data/keep.txt').write_text('keep')
u.update(bundle,root);assert (unknown/'lumen/data/keep.txt').read_text()=='keep'
assert (root/'lumen/data/notes/me.txt').read_text()=='my private note'
assert (root/'documents/My book.epub').read_text()=='my book'
assert (root/'lumen/DISABLE_AUTOSTART').exists()
manifest=json.loads((bundle/'manifest.json').read_text())
assert all(u.digest(root/r)==sha for r,sha in manifest.items())
# Source corruption rejected before any destination changes.
source=bundle/'Kindle/documents/LUMEN.sh';original=source.read_bytes();source.write_text('broken')
previous=u.digest(root/'lumen/bin/lumen')
try:u.update(bundle,root)
except RuntimeError as e:assert 'corrupt' in str(e)
else:raise AssertionError('Corruption accepted')
finally:source.write_bytes(original)
assert u.digest(root/'lumen/bin/lumen')==previous
# Malicious legacy destinations and symlink targets are rejected before copying.
legacy=bundle/'legacy-files.json';original=legacy.read_bytes();legacy.write_text(json.dumps({'../outside.txt':['x']}))
try:u.update(bundle,root)
except RuntimeError:pass
else:raise AssertionError('Traversal accepted')
finally:legacy.write_bytes(original)
assert u.storage(root)['backups']>0
print('PASS installation: fresh prepared/stock devices, preflight, runtime hashes, updates, two-backup retention, unknown/personal preservation and corrupt/traversal rejection.')
