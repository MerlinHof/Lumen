# Build and develop

The source tree is intentionally small. Toolchains, dependencies and generated
files stay in ignored `.local`, `build` and `dist` folders.

## Requirements

macOS or Linux, Python **3.9+**, Clang/Clang++, Make, and standard C build tools.
On macOS install Apple Command Line Tools. On Ubuntu, install `clang make
build-essential pkg-config`. Kindle compilation uses Zig **0.14.1**. Reader
libraries use MuPDF **1.28.5**; libmobi uses the included **0.12** source archive.

## From a fresh clone

```sh
git clone https://github.com/MerlinHof/Lumen.git
cd Lumen
python3 tools/setup.py host
python3 tools/build.py host
python3 tools/setup.py kindle
python3 tools/build.py kindle
```

Setup downloads the pinned MuPDF archive from Artifex and checks its SHA-256.
Zig is selected for the host from the official version index and checksum-checked.
Only setup uses the network; subsequent builds are local and incremental.
MuPDF sources include their third-party libraries. The libmobi converter is
built separately from the included archive without DRM code.

Local source/dependency copies already present under `.local/deps` are reused.
The environment variables `MUPDF` and `ZIG` can override build dependency paths.

## Test

```sh
python3 tools/check.py          # focused native UI, input, defaults and installer
python3 tools/check.py --http   # also starts a local transfer test server
```

Build host **and Kindle** before the installer/package checks. Tests use synthetic
content under `build/tests`, never the mounted Kindle. ASan/UBSan cover the UI
integration executable. Only the current supported test harnesses are kept in the source tree.

## Preview and screenshots

```sh
build/host/lumen-host --library /path/to/demo-books --data build/preview-data \
  --screen 1 --preview build/library.pgm
```

For the public documentation gallery, install Pillow in your own development
environment and run `python3 tools/screenshots.py`. It creates original demo
books, renders the real native app views and writes only `docs/images` and
`build/gallery`. No private books or device account data are used.

## Package

```sh
python3 tools/embed-web.py        # after editing web/connect.html
python3 tools/make-translations.py # after editing the translation catalog
python3 tools/build.py kindle
python3 tools/package.py
```

The result is `dist/LUMEN-<version>.zip`, checksum and unpacked installer folder.
`scripts/sources.txt` is the explicit compile list; `packaging/runtime.json` is
the explicit runtime allowlist. There is no dependency on old release folders.

See [Architecture](architecture.md), [Installation](installation.md) and
[Agent guide](../AGENTS.md) before changing shared services or installation logic.
