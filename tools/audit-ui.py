#!/usr/bin/env python3
"""Render the native all-app quality atlas and CPU timings using demo data only.

Run tools/screenshots.py once first to generate the four original demo books.
Outputs are ignored build artifacts, never a Kindle payload or public screenshot.
"""
import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from check import ROOT, OUT, compile_test

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('label', nargs='?', default='current')
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9_-]+', args.label):
        parser.error('Use a simple label, such as before or after.')
    if not (ROOT / 'build/gallery/books/00.epub').is_file():
        parser.error('Generate demo books with tools/screenshots.py first.')
    folder = ROOT / 'build/audit' / args.label
    folder.mkdir(parents=True, exist_ok=True)
    shutil.rmtree(folder / 'data', ignore_errors=True)
    OUT.mkdir(parents=True, exist_ok=True)
    binary = compile_test('audit', 'tests/render_audit.c', True)
    result = subprocess.run([str(binary), str(folder)], cwd=ROOT, check=True, text=True, capture_output=True)
    (folder / 'timings.txt').write_text(result.stdout)
    timings = {line.split()[0]: float(line.split()[1]) for line in result.stdout.splitlines()}
    (folder / 'timings.json').write_text(json.dumps(timings, indent=2) + '\n')
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        print('PGM atlas ready; install Pillow for contact sheets:', folder)
        return
    files = sorted(folder.glob('*.pgm'))
    for start in range(0, len(files), 12):
        sheet = Image.new('RGB', (1200, 1275), '#ccc')
        draw = ImageDraw.Draw(sheet)
        for i, path in enumerate(files[start:start + 12]):
            x, y = i % 4 * 300, i // 4 * 425
            # Contact sheets are reduced overviews; original PGM files stay 600x800.
            with Image.open(path) as native:
                sheet.paste(native.resize((300, 400)), (x, y + 25))
            draw.text((x + 8, y + 5), path.stem, fill='black')
        sheet.save(folder / ('sheet-%d.png' % (start // 12)))
    print('%d native views and CPU timings: %s' % (len(files), folder))

if __name__ == '__main__':
    main()
