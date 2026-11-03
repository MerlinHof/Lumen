# Validation · LUMEN 1.10

## Checked locally

- Native host build and stripped static ARMv7 build using the reorganized source list.
- Fresh dependency setup reuses pinned MuPDF and rebuilds the libmobi converter;
  official MuPDF download URL verified. The fresh GitHub-hosted host build and its native/installer checks passed.
  A full network-only Kindle cross-build has not been repeated.
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

The prepared update was subsequently installed on the connected Kindle with
all runtime hashes verified and personal files/books preserved. Accumulated
backups were confirmed as the main storage cost; macOS metadata sidecars are now
recognized safely. A complete physical fresh-device reinstall, new autostart
behavior, sleep/wake and touch latency still need hardware/user verification.
Host renders are not hardware photos.
Private services and paid OpenAI requests were not exercised.

## 1.10 checks

Native sanitizer checks cover the calculator width threshold, deleted routes,
erasing to white and removing empty ink files, dragged text selection before
copy actions, chat file round trips, multi-turn request JSON and recipe Back.
Installer tests perform twelve updates, an identical reinstall and explicit
package-path rejection. The refresh engine was not changed.

The OpenAI chat implementation follows the official conversation-state guide:
https://developers.openai.com/api/docs/guides/conversation-state . Previous user
and assistant text is sent as input with `store:false`; transcripts are kept
locally. Private API keys and paid live requests were not used in these tests.

The connected Kindle subsequently received 1.10: all runtime hashes, personal
files and books were verified. Backup retention left exactly two rollback
directories. This is file-copy/storage validation, not a claim of hands-on
touch, eraser or live API testing on the device.
