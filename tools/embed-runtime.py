from pathlib import Path
import json
root=Path(__file__).resolve().parents[1]
paths=json.loads((root/'tools/config/runtime.json').read_text())
text='/* Generated runtime ownership list; user data and installed apps excluded. */\nstatic const char *runtime_files[]={\n'+''.join(json.dumps(p)+',\n' for p in sorted(paths))+'};\n'
p=root/'lumen/system/runtime_files.h'
if not p.exists() or p.read_text()!=text:p.write_text(text)
