#!/usr/bin/env python3
"""Check the package boundary and list the remaining host ports."""
import argparse
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--reading', action='store_true',
                    help='Enforce the Library and Reader package boundary for 6.6.')
parser.add_argument('--complete', action='store_true',
                    help='Require every app-specific host dependency to be removed.')
parser.add_argument('--sdk', action='store_true',
                    help='Enforce semantic controls and shared HTTP in bundled packages.')
args = parser.parse_args()

manifests = [json.loads(path.read_text()) for path in
             (root/'lumen/apps').glob('*/package.json')]
package_ids = {app['id'] for app in manifests}
if len(manifests) != 52 or len(package_ids) != 52:
    raise SystemExit('Expected 52 unique bundled app IDs')

legacy = re.compile(r'\b(?:app_for_route|app_route_valid|app_factory_definition|ui_route|route_to)\s*\(')
imports = re.compile(r'^\s*#\s*include\s*"[^"]*apps/')
direct_identity = re.compile(r'"(lumen\.[a-z0-9._-]+)"')
host_dependencies = []
reading_dependencies = []
violations = []
for file in sorted((root/'lumen/system').rglob('*')):
    if file.suffix not in ('.c', '.h') or not file.is_file():
        continue
    for number, line in enumerate(file.read_text().splitlines(), 1):
        location = f'{file.relative_to(root)}:{number}'
        if legacy.search(line):
            violations.append(location)
        if re.search(r'["/]apps/(?:library|reader)/|app_package_find\("lumen\.(?:library|reader)"\)',line):
            reading_dependencies.append(location)
        if imports.search(line) or any(name in package_ids for name in direct_identity.findall(line)):
            host_dependencies.append(location)
for source in (root/'tools/config/sources.txt').read_text().splitlines():
    if source.startswith('apps/'):
        host_dependencies.append(f'compiled host source: lumen/{source}')

if violations:
    raise SystemExit('Numeric app routes returned:\n'+'\n'.join(violations))
print('PASS 52 distinct packages and no numeric app routing.')
if args.reading:
    if reading_dependencies:
        raise SystemExit('Remaining Library and Reader imports or identities:\n'+'\n'.join(reading_dependencies))
    print('PASS Library and Reader package boundary.')
if args.complete:
    host_dependencies.extend(str(path.relative_to(root)) for path in (root/'lumen/apps/host').rglob('*')
                             if path.is_file())
    if host_dependencies:
        raise SystemExit('Remaining app-specific core references:\n'+'\n'.join(host_dependencies))
    print('PASS Core has no bundled app identities, linked app code, or legacy host assembly.')


if args.sdk:
    # These interactions use position, shapes, or whole-view gestures as input.
    spatial = {
        'calculator': 'graph cursor', 'hex': 'hexagonal board',
        'home_assistant': 'history graph', 'maze': 'maze field',
        'notes': 'ink canvas', 'photos': 'full-screen photo dismissal',
        'planetarium': 'star coordinates', 'reader': 'document page gestures',
        'sokoban': 'warehouse field', 'solitaire': 'overlapping card stacks',
        'timer': 'full-screen clock dismissal',
    }
    raw = re.compile(r'\b(?:INPUT_(?:TAP|HOLD|DOWN)|ui_paint_hitbox)\b')
    network = re.compile(r'"curl"')
    pending = []
    for manifest in sorted((root/'lumen/apps').glob('*/package.json')):
        for source in manifest.parent.rglob('*'):
            if not source.is_file() or source.suffix not in ('.c','.h'):
                continue
            for number,line in enumerate(source.read_text().splitlines(),1):
                if network.search(line) or (raw.search(line) and manifest.parent.name not in spatial):
                    pending.append(f'{source.relative_to(root)}:{number}')
    if pending:
        raise SystemExit('Raw controls or private HTTP outside the SDK boundary:\n'+'\n'.join(pending))
    print(f'PASS SDK action and HTTP audit across all {len(manifests)} bundled packages.')
