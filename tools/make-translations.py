from pathlib import Path
import json,re
base=Path(__file__).resolve().parents[1]
rows=json.loads((base/'lumen/system/translations.json').read_text())
fmt=re.compile(r'%(?!%)(?:[-+ #0]*\d*(?:\.\d+)?(?:ll|l|z)?[diuoxXfFeEgGcs])')
out=['/* Generated complete UI catalogue. */','typedef struct {const char *key;const char *values[3];} Translation;','static const Translation translations[]={']
for key,values in sorted(rows.items()):
 assert len(values)==3 and all(values),key
 assert all(fmt.findall(v)==fmt.findall(values[0]) for v in values),key
 out.append('{'+json.dumps(key,ensure_ascii=False)+',{'+','.join(json.dumps(v,ensure_ascii=False) for v in values)+'}},')
out.append('};')
(base/'lumen/system/i18n_catalog.h').write_text('\n'.join(out)+'\n')
