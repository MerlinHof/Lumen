# Validation · LUMEN 4.2

## 3.9 verification

Host and static ARM builds passed. Focused checks cover simulation deadlines for
Snake/Tetris, a four-turn lossless photo rotation, shared Markdown emphasis,
UI/default/input flows, local HTTP pairing and custom-app staging, approval and
restart handoff. The public gallery is rendered from current native code and
original demo content. All 63 runtime files were verified during device installation,
with existing books and personal files unchanged.

Physical Wi-Fi behavior, live Home Assistant interactions and the custom-app
restart flow still need on-device review. Host views and sanitizer checks do
not measure E-Ink latency or battery life.

The 3.9 allowlist contains **63 files**, about **51.8 MiB installed** and
**34.2 MiB downloaded**, excluding personal data and rollback copies.

## Earlier 3D puzzle checks

Focused ASan/UBSan tests cover deterministic rendering, legal moves and the
victory state.
A native demo render is included. Antialiasing is lightweight spatial edge
filtering at native viewport resolution.
Physical input latency and E-Ink appearance still need on-device review.

## 2.1 scope and practical limits

Host and ARM builds, focused native sanitizer/default checks, launcher identity
mocks and package/install preservation checks cover this release. Native gallery
images use original demo books, not personal device content. The adaptive E-Ink
refresh algorithm is unchanged.

The new ARM KFX converter has been packaged and checksum-verified, but has not yet
been executed on this Kindle. Supported serial-based protection is attempted;
newer or unsupported variants may need the original-reader fallback. Conversion
is lazy on first opening, not an eager rewrite of every book. Existing imports
remain readable. Original book files are never replaced.

The firmware pause is limited to awesome and restored on exit; tests exercise
ownership, already-stopped processes and PID reuse. Real wake behavior and API
integrations still require hardware verification. Do not infer these from mocks.

The previous **80.9% median draw-CPU reduction** compares the same native host
views against 1.10, eight draws per view. It comes from exact glyph ink bounds,
clipped rectangle writes, faster ring geometry and bounded reusable cover tiles.
It is not an 81% Kindle speed or E-Ink refresh claim.

## Previous baseline checks


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

The earlier 2.x runtime allowlist contained **48 files**, approximately **27.6 MiB**
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

## 2.0 quality and performance checks

- Native host and ARMv7 builds completed.
- Sanitizer integration covers 100 pixel-identical clipped text cases across all
  five UI fonts, shared row geometry, exact cover-cache reuse and replacement
  invalidation, weather-snapshot lifecycle and distinct game action positions.
- The native atlas contains 92 views: 47 active app entries, populated list
  fixtures, Reader controls, secondary views and settings in DE/EN/FR.
- Existing input, editing, navigation, fresh defaults, twelve-update retention,
  unchanged reinstall and transfer HTTP checks pass. Release numbering rejects
  multi-digit minor versions such as 2.10.
- The adaptive refresh engine and firmware service lifecycle are unchanged.

A sequential comparison compiled the previous 1.10 UI from Git and the current
2.0 UI with the same macOS ARM64 host/sanitizer harness and demo fixtures. Each
view was drawn eight times; the median CPU draw-time reduction across 92 views
was **80.9%** in this run. This is a host rendering measurement, not a claim of
80.9% faster Kindle display, touch response, startup or battery life.

| Native view | 1.10 CPU draw | 2.0 CPU draw |
| --- | ---: | ---: |
| Library, demo covers | 22.50 ms | 2.47 ms |
| Settings | 2.87 ms | 0.54 ms |
| App launcher | 2.96 ms | 1.25 ms |
| Calculator | 1.46 ms | 0.46 ms |
| Timer | 2.50 ms | 0.65 ms |
| Wi-Fi list | 1.30 ms | 0.37 ms |
| Reader with controls | 1.22 ms | 0.27 ms |

Caches are bounded RAM allocations; they do not add persistent backups or
resampled cover files. The package remains an explicit runtime allowlist.
Physical usability, sleep/wake and account integrations still need user testing;
paid/private services were not exercised for this quality-only update.


## 2.5 control corners

Run `python3 tools/benchmark-corners.py` from the repository root. The tool
compiles an optimized host build, warms each corner mask and alternates the
three profiles across nine runs. Each run draws 280 frames per profile across
Calculator, Timer, Clock and a paginated dropdown, then draws 10,000 outlined
controls in a separate sample. The table reports medians of thread CPU time.

| Profile | UI frame | Relative to square | Outlined control |
| --- | ---: | ---: | ---: |
| Square | 0.123 ms | baseline | 0.21 µs |
| Circular | 0.127 ms | +2.9% | 1.05 µs |
| Superellipse | 0.125 ms | +1.7% | 1.08 µs |

The two round profiles share the same cached coverage raster path. The small
frame-level difference between them is within the spread of the samples.
The measurements cover raster CPU time on the Mac. Physical E-Ink refresh
and Kindle input latency require hardware measurements.

## 4.0 focused checks

Connect is tested with idle and incomplete browser connections alongside active requests. CommonMark/GFM rules, tables and nested lists and the one-time Photos seed have targeted checks. Physical Wi-Fi reliability still requires confirmation on the device.

## 4.1 focused checks

Native and ARM builds; existing native interaction suite plus fraction-axis pixels and note-ink viewport anchoring; fresh settings; solvable 2D puzzle movement; mock GitHub latest-release discovery and safe refusal when wireless resources are absent; HTTP idle/incomplete socket and transfer checks. Live GitHub activation on hardware is not claimed. Device installation records personal-file and book hashes before/after copying.

## 4.2 focused checks

Native/ARM builds; native interaction suite including all fourteen clock-face render paths and native-resolution serif display text; updated Clock/alarm screenshots and a face comparison sheet. These are software renders, not claims of a physical panel timing or wake test.
