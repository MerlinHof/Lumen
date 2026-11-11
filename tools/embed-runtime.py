from pathlib import Path
import json
from package import runtime_mapping
root=Path(__file__).resolve().parents[1]
paths=runtime_mapping()
text='/* Generated runtime ownership list; user data and installed apps excluded. */\nstatic const char *runtime_files[]={\n'+''.join(json.dumps(p)+',\n' for p in sorted(paths))+'};\n'
p=root/'lumen/system/runtime_files.h'
if not p.exists() or p.read_text()!=text:p.write_text(text)

icons={Path(destination).stem:source for destination,source in paths.items()
       if destination.startswith("lumen/home/icons/")}
text='/* App-owned artwork paths for host previews. */\nstatic const struct { const char *id,*path; } host_icons[]={\n'
text+=''.join('{'+json.dumps(key)+','+json.dumps(source)+'},\n' for key,source in sorted(icons.items()))+'};\n'
p=root/'lumen/system/host_icons.h'
if not p.exists() or p.read_text()!=text:p.write_text(text)
