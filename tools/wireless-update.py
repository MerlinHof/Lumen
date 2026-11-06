#!/usr/bin/env python3
"""Export a binary-only wireless update from two verified LUMEN bundles."""
import argparse
import hashlib
import json
import re
from pathlib import Path

BINARY = 'lumen/bin/lumen'

def load(bundle):
    release = json.loads((bundle / 'release.json').read_text())
    manifest = json.loads((bundle / 'manifest.json').read_text())
    binary = (bundle / 'Kindle' / BINARY).read_bytes()
    if hashlib.sha256(binary).hexdigest() != manifest.get(BINARY):
        raise ValueError('Bundle binary does not match its manifest')
    assets = {key: digest for key, digest in manifest.items() if key != BINARY}
    return release, assets, binary

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('installed', type=Path, help='Unpacked USB bundle installed on the Kindle')
    parser.add_argument('target', type=Path, help='Unpacked USB bundle for the target version')
    args = parser.parse_args()
    try:
        base, previous_assets, _ = load(args.installed)
        target, assets, binary = load(args.target)
        if not all(re.fullmatch(r'[1-9][0-9]*\.[0-9]', r['version']) for r in (base,target)):
            raise ValueError('Invalid release version')
        base_version = tuple(map(int,base['version'].split('.')))
        target_version = tuple(map(int,target['version'].split('.')))
        if base_version < (3,7) or target_version < base_version:
            raise ValueError('Wireless updates require 3.7 or later, with no downgrade')
        if any(base[k] != target[k] for k in ('architecture', 'firmware')):
            raise ValueError('This update requires different firmware or hardware')
        if assets != previous_assets:
            raise ValueError('Assets or launcher changed. Install this release over USB.')
        path = args.target.parent / ('LUMEN-' + target['version'] + '.lumen-update')
        path.write_bytes(binary)
        digest = hashlib.sha256(binary).hexdigest()
        path.with_suffix('.lumen-update.sha256').write_text(digest + '  ' + path.name + '\n')
        print(path)
        print('SHA-256', digest)
    except (KeyError, OSError, ValueError) as error:
        parser.exit(1, str(error) + '\n')

if __name__ == '__main__':
    main()
