#!/usr/bin/env python3
"""Generate provider default preferences and the old launcher ID migration table."""
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
defaults = {}
legacy = {}
required = []
for manifest in sorted((root / 'lumen/apps').glob('*/package.json')):
    app = json.loads(manifest.read_text())
    for role in app.get('provides', []):
        if role in defaults:
            raise ValueError(f'Multiple shipped providers declare {role}')
        defaults[role] = app['id']
    if app.get('required') is True:
        required.append(app['id'])
    if 'legacy' in app:
        slot = app['legacy']['slot']
        if not isinstance(slot,int) or not 0<=slot<55 or slot in legacy:
            raise ValueError(f'Invalid old preference index for {manifest}')
        legacy[slot] = app['id']

roles = ['startup', 'text-input', 'status', 'notification', 'sleep', 'document', 'books', 'device-control', 'file-transfer', 'alarm', 'agenda', 'written-documents']
header = root / 'build/generated/lumen_package_defaults.h'
header.parent.mkdir(parents=True, exist_ok=True)
lines = ['/* Generated from package manifests. */',
         'static const char *const legacy_keys[55] = {',
         *(f'    [{slot}] = {json.dumps(key)},' for slot,key in sorted(legacy.items())),
         '};',
         'static const char *const builtin_providers[PROVIDER_COUNT] = {',
         *(f'    {json.dumps(defaults[role])},' for role in roles),
         '};', 'static const char *const builtin_required[] = {',
         *(f'    {json.dumps(name)},' for name in required), '};', '']
contents = '\n'.join(lines)
if not header.exists() or header.read_text() != contents:
    header.write_text(contents)
