# Validation · LUMEN 1.9

## Checked locally

- Native host build and stripped static ARMv7 build using the reorganized source list.
- Fresh dependency setup reuses pinned MuPDF and rebuilds the libmobi converter;
  official MuPDF download URL verified. A full network-only clean-clone build has
  not been performed on this Mac. CI is configured to build host from fresh dependencies.
- ASan/UBSan UI integration: independent keyboard languages, numbers/accents,
  caret editing, editable-only clipboard, multiline tasks, forms, Notes navigation,
  reader chrome, journal, local calendar and puzzle models.
- Fresh-settings test: one consistent default set, on-disk initialization,
  independent language persistence, invalid option rejection and exact Reset parity.
- Real runtime package installed on a synthetic prepared Kindle filesystem.
  Stock prerequisites rejected before modification; all runtime hashes checked;
  books, settings and personal notes preserved across repeated updates.
- Two-backup retention and unknown/personal-backup preservation tested.
  Corrupt payloads and path traversal rejected before replacement.
- Actual target autostart shell script exercised against mocked firmware commands:
  EPUB preflight, firmware guard, mount restoration and foreign-job preservation.
- Launcher mock: restore the owned window manager, Pillow and power settings;
  session-lock cleanup and return to firmware.
- Actual local transfer server: four-character pairing, TXT/PDF exports, uploads,
  configuration allowlist, traversal rejection and bad-code throttling.
- Current native documentation renders inspected; all books are original demo content.

## Measured package size

The runtime allowlist currently contains **48 files**, approximately **27.6 MiB**
expanded and **13.2 MiB** zipped. Personal app data and rollback backups are not
included. Backup retention reduces accumulated runtime copies without reducing
image/font/book quality.

## Not claimed

No Kindle was connected during this cleanup. A complete physical fresh-device
installation, new autostart behavior, sleep/wake, touch latency and reclaimed
on-device space have not been measured. The reported previous 400 MB usage cannot
be attributed precisely without the device. Host renders are not hardware photos.
Private services and paid OpenAI requests were not exercised.
