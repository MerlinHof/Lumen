# Kindle import dependencies

The application-owned runner is `lumen/apps/library/import_kfx.py`.

- DeDRM `kfxdedrm.py`: noDRM/DeDRM_tools v10.0.9, commit
  b9bad26d4b6efa43efdafd8b060f33ea1d945f36, GPL v3; original notices retained.
  https://github.com/noDRM/DeDRM_tools/tree/v10.0.9/DeDRM_plugin
- On-device ARMv7 CPython, compatibility shim, Crypto, lxml, Pillow, dedrm.ion
  and John Howell's kfxlib: kaikozlov/kindle.koplugin v0.0.11 release.
  https://github.com/kaikozlov/kindle.koplugin/releases/tag/v0.0.11
  Plugin/wrapper MIT; kfxlib and DeDRM GPL v3. CPython PSF and dependency
  licenses remain in the runtime. Upstream source and build recipe:
  https://github.com/kaikozlov/kindle.koplugin/tree/v0.0.11

The downloaded runtime is checksum-pinned in `tools/config/amazon-runtime.json` and
kept under ignored `.local`. It is not source code from LUMEN and is never mixed
into app folders. `tools/prepare-amazon.py` creates the runtime-only archive.
No device serial, key, book or decrypted personal content belongs in the repo.
