#!/usr/bin/env python3
"""Convert pinned FreeDict TEI sources to a compact, random-access index."""
import hashlib
import json
from pathlib import Path
import re
import struct
import tarfile
import unicodedata
import urllib.request
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "lumen/assets/dictionaries"
CACHE = ROOT / ".local/deps/freedict"
NS = {"tei": "http://www.tei-c.org/ns/1.0"}

def normalize(word):
    return "".join(c for c in unicodedata.normalize("NFKD", word.upper())
                   if not unicodedata.combining(c))

def convert(pair, release):
    archive = CACHE / (pair + ".tar.xz")
    if not archive.exists():
        with urllib.request.urlopen(release["URL"], timeout=45) as response:
            archive.write_bytes(response.read())
    if hashlib.sha512(archive.read_bytes()).hexdigest() != release["checksum"]:
        raise ValueError(f"Invalid checksum for {pair}")
    with tarfile.open(archive) as source:
        root = ET.parse(source.extractfile(f"{pair}/{pair}.tei"))
        license_data = source.extractfile(f"{pair}/COPYING").read()
        header = root.find(".//tei:teiHeader", NS)
    words = {}
    for entry in root.findall(".//tei:entry", NS):
        targets = []
        for quote in entry.findall(".//tei:cit[@type='trans']/tei:quote", NS):
            text = " ".join("".join(quote.itertext()).split())
            if text and text not in targets:
                targets.append(text)
        if not targets:
            continue
        for orth in entry.findall("./tei:form/tei:orth", NS):
            key = normalize("".join(orth.itertext()))
            if re.fullmatch(r"[A-Z]{2,63}", key):
                values = words.setdefault(key, [])
                values.extend(target for target in targets if target not in values)
    records = []
    for word, definitions in sorted(words.items()):
        text = " · ".join(definitions[:5])
        while len(text.encode()) > 350:
            text = text[:-1]
        records.append(f"{word}\t{text}\n".encode())
    offset = 4 + 4 * len(records)
    with (ASSETS / (pair + ".idx")).open("wb") as out:
        out.write(struct.pack("<I", len(records)))
        for record in records:
            out.write(struct.pack("<I", offset))
            offset += len(record)
        out.writelines(records)
    (ASSETS / (pair + "-COPYING.txt")).write_bytes(license_data)
    (ASSETS / (pair + "-credits.txt")).write_text(
        f"FreeDict {pair}, version {release['version']}\n{release['URL']}\n"
        f"SHA-512: {release['checksum']}\n"
        "LUMEN adaptation: extracted single-word forms and up to five translations.\n\n"
        + "\n".join(line.strip() for line in header.itertext() if line.strip()) + "\n")
    print(pair, len(records), "headwords")

if __name__ == "__main__":
    CACHE.mkdir(parents=True, exist_ok=True)
    for pair, release in json.loads((ASSETS / "sources.json").read_text()).items():
        convert(pair, release)
