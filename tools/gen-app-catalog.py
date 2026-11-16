#!/usr/bin/env python3
"""Generate the route bridge for old system screens from built-in manifests."""
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
retired = json.loads((root / 'tools/config/retired-apps.json').read_text())
apps = [(key, '', metadata) for key, metadata in retired.items()]
defaults = {}
for manifest in sorted((root / 'lumen/apps').glob('*/package.json')):
    app = json.loads(manifest.read_text())
    for role in app.get('provides', []):
        if role in defaults:
            raise ValueError(f'Multiple shipped providers declare {role}')
        defaults[role] = app['id']
    if 'legacy' in app:
        apps.append((app['id'], app['name'], app['legacy']))

keys, definitions, routes = [], [], []
seen_slots, seen_routes = set(), set()
for key, name, metadata in apps:
    slot = metadata['slot']
    entry = metadata['entry']
    aliases = metadata['aliases']
    if slot in seen_slots or set(aliases) & seen_routes or len(set(aliases)) != len(aliases):
        raise ValueError(f'Duplicate legacy slot or route for {key}')
    if entry >= 0 and entry not in aliases:
        raise ValueError(f'Missing legacy entry route for {key}')
    seen_slots.add(slot)
    seen_routes.update(aliases)
    keys.append(f'    [{slot}] = {json.dumps(key)},')
    definitions.append(f'    [{slot}] = {{{slot}, {json.dumps(name, ensure_ascii=False)}, {entry}}},')
    routes.extend(f'    {{{route}, {slot}}},' for route in aliases)

roles = ['startup', 'text-input', 'status', 'notification', 'sleep']
providers = [f'    {json.dumps(defaults[role])},' for role in roles]
header = root / 'build/generated/lumen_builtin_catalog.h'
header.parent.mkdir(parents=True, exist_ok=True)
lines = ['/* Generated from package manifests. */',
         'static const char *const builtin_keys[APP_COUNT] = {', *keys, '};',
         'static const AppDefinition definitions[APP_COUNT] = {', *definitions, '};',
         'static const AppRoute routes[] = {', *routes, '};',
         'static const char *const builtin_providers[PROVIDER_COUNT] = {', *providers, '};', '']
contents = '\n'.join(lines)
if not header.exists() or header.read_text() != contents:
    header.write_text(contents)
