#!/usr/bin/env python3
"""Run the focused current checks, or add --http for a local transfer test."""
import argparse,os,subprocess,sys,shutil,tempfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import build
ROOT=build.ROOT
OUT=ROOT/'build/tests'
def compile_test(name,source,ui=False):
    mupdf,_,libs=build.dependencies('host');obj=OUT/(name+'.o');binary=OUT/name
    subprocess.run(['clang','-DLUMEN_HOST','-g','-fsanitize=address,undefined','-I'+str(mupdf/'include'),'-c',str(ROOT/source),'-o',str(obj)],check=True)
    objects=[build.object_path('host',s) for s in build.sources() if s not in ('main.c','system/ui/ui.c')]
    if not ui:
        ui_objects = {build.object_path('host', name) for name in
                      ('apps/frog/app.c', 'apps/sliding_puzzle/app.c')}
        objects = [obj for obj in objects if obj not in ui_objects]
    subprocess.run(['clang++','-fsanitize=address,undefined',str(obj)]+[str(p) for p in objects+libs]+['-lm','-lpthread','-o',str(binary)],check=True)
    return binary
def run(http=False,native_only=False):
    os.chdir(ROOT);OUT.mkdir(parents=True,exist_ok=True)
    shutil.rmtree(OUT/'test-data',ignore_errors=True)
    shutil.rmtree(OUT/'apps',ignore_errors=True)
    binary=compile_test('integration','tools/tests/test_integration.c',True);subprocess.run([str(binary)],check=True)
    binary=compile_test('fresh-settings','tools/tests/test_fresh_settings.c');subprocess.run([str(binary)],check=True)
    sdk=OUT/'sdk'
    subprocess.run(['clang','-O1','-g','-fsanitize=address,undefined','tools/tests/test_sdk.c','-o',str(sdk)],check=True)
    with tempfile.TemporaryDirectory(dir=OUT) as data:
        subprocess.run([str(sdk),data],check=True)
    refresh=OUT/'refresh'
    subprocess.run(['clang','-O1','-fsanitize=address,undefined','tools/tests/test_refresh.c','-o',str(refresh)],check=True)
    subprocess.run([str(refresh)],check=True)
    signal=OUT/'signal';subprocess.run(['clang','-O1','-g','-fsanitize=address,undefined','tools/tests/test_signal.c','lumen/apps/sliding_puzzle/engine.c','-lm','-o',str(signal)],check=True);subprocess.run([str(signal)],check=True)
    binary=compile_test('release-client','tools/tests/test_release_update.c');subprocess.run([str(binary)],check=True)
    subprocess.run([sys.executable,'tools/tests/test_packages.py'],check=True)
    if not native_only: subprocess.run([sys.executable,'tools/tests/test_installation.py'],check=True)
    if http:
        binary=compile_test('http','tools/tests/http_server.c');subprocess.run([sys.executable,'tools/tests/test_http.py',str(binary)],check=True)
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--http',action='store_true');p.add_argument('--native-only',action='store_true');a=p.parse_args();run(a.http,a.native_only)
