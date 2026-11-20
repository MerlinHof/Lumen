#!/usr/bin/env python3
"""Build one independent app; the LUMEN system is never compiled or linked."""
import argparse,hashlib,json,os,re,shlex,shutil,subprocess,zipfile
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
HERE=Path(__file__).resolve().parent
ROOT=HERE.parent

def changed(signature,command):
    try:
        return json.loads(signature.read_text())!=command
    except (OSError,ValueError):
        return True

def stale(obj,dependency,signature,command):
    if not obj.is_file() or changed(signature,command):
        return True
    try:
        pattern=dependency.read_text().replace('\\\n',' ').split(':',1)[1]
        stamp=obj.stat().st_mtime_ns
        return any(Path(path).stat().st_mtime_ns>stamp for path in shlex.split(pattern))
    except (OSError,ValueError,IndexError):
        return True

def build_app(manifest,target='kindle',output=None):
    manifest=Path(manifest).resolve();meta=json.loads(manifest.read_text());folder=manifest.parent
    api=meta.get('api')
    if api not in (1,2) or meta.get('format')!=f'lumen-app-{api}':
        raise ValueError('Unsupported app protocol')
    if not re.fullmatch(r'[a-z0-9][a-z0-9_.-]{2,62}',meta.get('id','')) or '..' in meta['id']:
        raise ValueError('Invalid package ID')
    if api==2:
        surfaces=meta.get('surfaces')
        providers=meta.get('provides',[])
        valid_surfaces={'foreground','overlay','background'}
        valid_providers={'startup','text-input','status','notification','sleep','document','books','device-control','file-transfer','alarm','agenda','written-documents'}
        if meta.get('entry')!='app':
            raise ValueError('Protocol 2 entry must be app')
        if (not isinstance(surfaces,list) or not surfaces or not all(
                isinstance(item,str) and item in valid_surfaces for item in surfaces)
                or len(surfaces)!=len(set(surfaces))):
            raise ValueError('Invalid app surfaces')
        if (not isinstance(providers,list) or not all(
                isinstance(item,str) and item in valid_providers for item in providers)
                or len(providers)!=len(set(providers))):
            raise ValueError('Invalid system provider roles')
        foreground_providers={'startup','document','books','device-control','file-transfer','written-documents'}
        background_providers={'agenda'}
        if set(providers) & background_providers and 'background' not in surfaces:
            raise ValueError('Background providers require a background surface')
        if set(providers) & foreground_providers and 'foreground' not in surfaces:
            raise ValueError('Foreground providers require a foreground surface')
        if set(providers)-foreground_providers-background_providers and 'overlay' not in surfaces:
            raise ValueError('System overlays require an overlay surface')
        if meta.get('operations') and 'background' not in surfaces:
            raise ValueError('Exported operations require a background surface')
        periodic=meta.get('periodic')
        if periodic is not None and (not isinstance(periodic,dict)
                or periodic.get('operation') not in meta.get('operations',[])
                or type(periodic.get('interval')) is not int
                or not 900 <= periodic['interval'] <= 86400):
            raise ValueError('Periodic work requires an exported operation and a 900–86400 second interval')
    sdk=HERE/'sdk' if (HERE/'sdk').is_dir() else HERE
    out=Path(output or (ROOT/'build/apps'/target/meta['id'] if (ROOT/'lumen').exists() else folder/'build'/target)).resolve();out.mkdir(parents=True,exist_ok=True)
    compiler=shutil.which('clang') if target=='host' else os.environ.get('ZIG') or str(ROOT/'.local/deps/zig/zig')
    if target!='host' and not Path(compiler).is_file():compiler=shutil.which('zig')
    if not compiler:raise RuntimeError('Install Zig or set ZIG to its executable')
    command=[compiler] if target=='host' else [compiler,'cc','-target','arm-linux-musleabi','-mcpu=cortex_a9']
    flags=['-Os','-DLUMEN_PACKAGED',f'-DLUMEN_APP_PROTOCOL={api}','-ffunction-sections',
           '-fdata-sections','-I'+str(sdk)]
    env=dict(os.environ,ZIG_GLOBAL_CACHE_DIR=str(ROOT/'.local/cache/zig'),
             ZIG_LOCAL_CACHE_DIR=str(ROOT/'.local/cache/zig-local'))
    meta.pop('legacy',None)
    meta.pop('required',None)
    source=meta.pop('sources')
    if not source or not all(isinstance(path,str) for path in source):
        raise ValueError('An app needs at least one source file')
    native=meta.pop('native_dependencies',[])
    if not isinstance(native,list) or set(native)-{'quickjs','litehtml'}:
        raise ValueError('Unknown native app dependency')
    deps=ROOT/'.local/deps'
    cflags=[]
    if 'quickjs' in native:
        quickjs=deps/'quickjs'
        if not (quickjs/'VERSION').is_file():
            raise RuntimeError('QuickJS missing. Run python3 tools/setup.py '+target)
        version=(quickjs/'VERSION').read_text().strip()
        cflags+=['-I'+str(quickjs),'-D_GNU_SOURCE',f'-DCONFIG_VERSION="{version}"']
        source.extend(str(quickjs/name) for name in ('quickjs.c','dtoa.c','libregexp.c','libunicode.c','cutils.c'))
    if 'litehtml' in native:
        litehtml=deps/'litehtml'
        stb=deps/'stb'
        if not (litehtml/'CMakeLists.txt').is_file() or not (stb/'stb_truetype.h').is_file():
            raise RuntimeError('Browser layout dependencies missing. Run python3 tools/setup.py '+target)
        gumbo=litehtml/'src/gumbo'
        flags+=['-I'+str(path) for path in (litehtml/'include',litehtml/'include/litehtml',
                                            litehtml/'src',gumbo/'include',gumbo/'include/gumbo',stb)]
        # These are the pinned upstream CMake target source lists.
        for directory,variable in ((litehtml,'SOURCE_LITEHTML'),(gumbo,'SOURCE_GUMBO')):
            configuration=(directory/'CMakeLists.txt').read_text()
            listing=re.search(r'set\('+variable+r'\s+([^)]+)\)',configuration).group(1)
            source.extend(str(directory/name) for name in listing.split())
    cache=out/'.objects';cache.mkdir(exist_ok=True)
    binary=out/'app'
    cxx=['clang++'] if target=='host' else [compiler,'c++',*command[2:]]
    def compile_unit(listing):
        index,path=listing
        src=(folder/path).resolve()
        obj=cache/f'{index}.o';dep=obj.with_suffix('.d');signature=obj.with_suffix('.json')
        cpp=src.suffix in ('.cc','.cpp','.cxx')
        argv=(cxx if cpp else command)+flags+(['-std=c++17'] if cpp else cflags)+[
            '-MMD','-MF',str(dep),'-c',str(src),'-o',str(obj)]
        if stale(obj,dep,signature,argv):
            signature.unlink(missing_ok=True)
            subprocess.run(argv,check=True,env=env)
            signature.write_text(json.dumps(argv))
        return obj
    if 'litehtml' in native:
        with ThreadPoolExecutor(max_workers=6) as pool:objects=list(pool.map(compile_unit,enumerate(source)))
    else:
        objects=list(map(compile_unit,enumerate(source)))
    link=cxx if any(Path(path).suffix in ('.cc','.cpp','.cxx') for path in source) else command
    argv=link+(['-static','-s','-Wl,--gc-sections'] if target=='kindle' and link==cxx else
               ['-static','-s'] if target=='kindle' else [])+list(map(str,objects))+['-lm','-o',str(binary)]
    link_signature=cache/'link.json'
    if not binary.is_file() or changed(link_signature,argv) or any(
            obj.stat().st_mtime_ns>binary.stat().st_mtime_ns for obj in objects):
        link_signature.unlink(missing_ok=True)
        subprocess.run(argv,check=True,env=env)
        link_signature.write_text(json.dumps(argv))
    files={'app':out/'app'}
    for path in meta.pop('assets',[]):
        p=Path(path)
        if p.is_absolute() or '..' in p.parts:raise ValueError('Unsafe asset path')
        files[path]=folder/path
    meta['files']=[dict(path=name,size=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for name,p in sorted(files.items())]
    package=out/(meta['id']+'.lumen-app')
    manifest_text=json.dumps(meta,sort_keys=True)
    try:
        with zipfile.ZipFile(package) as existing:
            if existing.read('manifest.json').decode()==manifest_text:
                print(package);return package
    except (OSError,KeyError,UnicodeError,zipfile.BadZipFile):
        pass
    temporary=package.with_suffix('.tmp')
    try:
        with zipfile.ZipFile(temporary,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
            z.writestr('manifest.json',manifest_text)
            for name,p in files.items():z.write(p,'payload/'+name)
        temporary.replace(package)
    finally:
        temporary.unlink(missing_ok=True)
    print(package);return package
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('manifest');p.add_argument('--target',choices=['kindle','host'],default='kindle');p.add_argument('--output');a=p.parse_args();build_app(a.manifest,a.target,a.output)
