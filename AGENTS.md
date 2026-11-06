# Working on LUMEN

## Start here

LUMEN is a small, calm, personal computer for paper-like screens. Its purpose is
to give the user time back through deliberate, local tools. Home should offer
a quiet sense of the day and easy access to things worth doing. Keep the date,
illustration and app grid in a clear hierarchy, with generous room to breathe.
The shell is native C on Kindle firmware.

The repository belongs to Merlin Hof. Work only inside this project directory.
Read `README.md`, `docs/architecture.md` and the relevant source before editing.
The current version is in `VERSION`; installation requirements are in
`docs/installation.md`. Public repository: https://github.com/MerlinHof/Lumen.

The original project was developed in late 2026 by GPT-6.1 Sol under the
supervision, prompting and license of Merlin Hof. Preserve the requested
attribution in About and the existing licenses. Do not invent testing claims,
release dates, supported devices, API capabilities or implementation status.

## The feeling we are building

Think of a good book, a beautifully made calculator, a well-kept notebook and an
old monochrome computer that still feels delightful to use. Quiet, tactile,
deliberate. A little charming. Clear enough that the user can stop thinking
about the interface and return to reading, writing or playing.

“Premium” here does not mean gradients, animation, a wall of cards or tiny gray
hairlines. It means excellent spacing, confident typography, recognizable
shapes, immediate feedback and a flow that respects the person's attention.
There should be care in a chess piece, a crate in Sokoban, an empty folder and
a loading state. Avoid generic placeholders when a small thoughtful detail
would make the interaction understandable.

We are not trying to make a miniature phone on an E-Ink screen. Do not copy
phone conventions that depend on constant animation, color, fast scrolling or
precise fingers. Borrow the clarity of a familiar interaction, not its baggage.

## Non-negotiable product principles

- Reading is the center. The library and Continue Reading should feel welcoming.
- Every app should be lovely on a slow monochrome screen.
- Prefer finite, purposeful experiences over feeds and endless scrolling.
- Keep personal content local by default. Network features are explicit.
- Preserve books, photos, notes, drawings, quotes, settings and reading progress.
- Do not add a feature when making the existing flow excellent would help more.
- Do not call a prototype production-ready just because it builds.
- Never replace a working interaction with a visual mockup or a no-op button.
- The firmware's power, Wi-Fi and original reader services must remain usable.
- A clean exit or failure should return to the Kindle UI, not trap the user.

## Design language

### Paper, ink and hierarchy

Use white backgrounds, black text and deliberate gray secondary text. Typography
should do much of the organizing: clear title, useful subheading, compact
metadata. Serif where it lends a book-like quality; readable sans for controls.
Do not scatter unnecessary headings, repeated app names or explanatory labels
through content screens.

Whitespace is intentional, not empty padding left by a removed feature. Align
icons, text, controls and baselines. Center button text by visible glyph metrics,
not by guessing a baseline. Use generous touch targets without making icons
oversized. Inspect the actual 600 × 800 render.

Use black primary actions with white text sparingly (for example calculator =),
white outlined secondary controls, and crisp offset shadows on floating surfaces.
Contrast and tactile depth are intentional; avoid a black tile on every control. Dividers
are lighter and quieter than button outlines, but still survive E-Ink. Avoid
super-thin pale lines. Gray text is for hierarchy, not an excuse for low contrast.

Use semantic type roles from system/ui/theme.h and typography.h: title, heading,
primary, body, secondary, display, control and inverse. Apps select roles, not
independent font sizes, arbitrary greys or ad-hoc bold offsets.

Most UI geometry is square for clarity and speed. Cover corners, switches and
app-icon squircles are intentional exceptions. Preserve anti-aliasing on icons
and those intentional curves. Do not add expensive rounding everywhere.

### Icons and controls

Use the central semantic icon library. A new icon should be recognizable at the
size it is used: real chess silhouettes, distinct game identities, a proper
folder, an actual undo arrow. Do not reuse unrelated icons because they exist.
Home Assistant and Wikipedia identities should remain recognizable and credited.
The Wi-Fi and battery icons are already established; do not casually redraw them.

Use the shared dropdown, checkbox, switch, search action, pager and segmented
control. A dropdown is a small anchored popup, not a new full-screen selection
page. Prefer a label on the left and selector on the right in the same row.
Connected tab selectors should look like one component. Do not duplicate widget
implementations in an app.

The selected bottom tab should be obvious but restrained: a small strong marker,
not a giant black tile. Status and bottom bars belong to the system. An app's
route policy decides whether the bottom bar is visible. Settings and glanceable
utilities usually keep it; games and the reader usually need the working space.

### Navigation and feedback

- Show Back only when there is somewhere to go. Give it a forgiving hit target.
- Use system Back for folder navigation; do not add a second redundant Up bar.
- A content tap that dismisses reader controls must not also turn the page.
- Reader controls belong above content; the system tabs appear below when open.
- A pager exists only when multiple pages exist. At an endpoint its unavailable
  arrow stays visible but gray. Horizontal swiping uses the same paging state.
- Every interactive control needs immediate touch-down feedback and normal
  touch-up appearance. Do not wait for an inversion animation before doing work.
- A fast app should open directly. A slow app should show feedback before doing
  the heavy work. Do not impose a loading delay on every app.
- A notification should be a compact floating surface with a close control,
  not a mysterious line covering content.
- An empty state should explain the situation and the next action briefly.
- Errors should say what failed and how to recover, without exposing secrets.

## Text input is a system responsibility

System language and keyboard language are independent persisted preferences.
Supported UI languages are German, English and French. Chinese UI was removed;
the CJK font remains for book content. Translate new user-facing strings in all
three UI languages and regenerate the catalog. Do not translate user content.

The keyboard has a fixed number row, staggered letter placement, one-shot Shift,
letter accents on long press, symbols, a language control, Enter and Close.
Enter means newline in a multiline field and submit/go in a single-line field.
Do not silently turn Shift into Caps Lock.

All editable fields must support a visible aligned caret, UTF-8-safe insertion
and deletion, placement by touch and appropriate copy/paste. Keycaps and ordinary
labels are not text selection targets. Password treatment must remain deliberate.
Never make rapid typing lose input because a render or disk write blocks events.

`text_input` describes field capacity and behavior, `text_edit` owns byte-boundary
operations, and the system keyboard/editor render them. Do not invent a second
editor in an app. Multiline tasks use measured dynamic row height; completion
strikes each actual text line through its middle.

## Architecture and ownership

```
lumen/
  system/       preferences, storage, network, device, reader, app runtime
    ui/         shell, chrome, keyboard, editor, widgets, icons, image helpers
  apps/<name>/  app-owned models, state, views and adapters
  apps/host/    explicit unity composition and shared dispatch adapters
  vendor/       clearly credited embedded third-party source
```

Apps currently share a process. Some older views remain grouped in header
fragments included by the shell; do not pretend they are sandboxed plugins.
Improve boundaries incrementally and preserve existing app IDs, file formats
and navigation. New code should be named by responsibility, not release number.

Use `AppIntent` for navigation/data delivery: copied text, stable book IDs and
value-based reading positions. Do not retain pointers to another app's temporary
query buffer. App definitions own route/chrome policy; system bars are not apps'
private drawing code. Use lifecycle boundaries to save app state.

The Settings app edits the central model. The Reader obtains a
`preferences_reader()` snapshot and reflows only when it changes. No app should
read another app's widget state. Factory values and import bounds live in
`system/defaults`, not in positional arrays copied into several functions.
First boot, reset and export/import must agree. Updating must preserve explicit
user choices.

The shared Reader is used for Wikipedia, news, recipes, Markdown, model answers
and game instructions. Keep typography, image support, paragraph spacing and
back navigation coherent. Do not build a second stripped-down article reader.

## E-Ink performance and power

The adaptive refresh engine is established and liked by the user. Preserve it.
It detects large changed areas for a full clear. Do not add a periodic full
refresh every N taps or per-app hardcoded clears without a demonstrated bug.

Avoid rebuilding an entire underlying app for every keystroke. The keyboard
caches its unchanged keybed and background; input still reaches current hit
regions. Use bounded caches and explicit invalidation. Drawing must not slow down
with the number of past strokes; use the existing fixed raster approach.

Do network, parsing and expensive loading through bounded background work.
Avoid synchronous calls, filesystem writes or sleeps on the touch path. No
unbounded history processes, polling loops or new background jobs per redraw.
Preserve firmware sleep/wake ownership. A visual timer does not wake deep sleep;
do not document that capability unless it is implemented and hardware-verified.

Storage: the runtime is roughly tens of MB, not the whole `lumen` directory.
Distinguish runtime, user data, caches and rollback backups. Keep two recognized
runtime-only backups after a successful update. Never prune unknown folders or
personal data to make a storage number look better. Do not sacrifice image,
cover, font or book quality just to shrink the package.

## Build, test and release

Read `docs/building.md`. Dependency versions and hashes are pinned under
`scripts`; dependencies and archives stay in ignored `.local`. Build products
and test data stay in ignored `build`; packages in ignored `dist`.

Use explicit source/object lists from `tools/config/sources.txt`. Never link every
object matching a wildcard: stale objects and duplicate names caused clutter.
Normal incremental builds do not download dependencies.

Run checks appropriate to the change. Use focused tests and native screenshots;
do not spend all the work budget repeating the same tests. Verify migrations,
filesystem boundaries, fresh defaults and package contents for installer changes.
Report the practical limits: host renders are not evidence of touch latency,
physical sleep/wake or private account integrations on real hardware.

`tools/package.py` copies only the allowlisted runtime. It must not include a
workspace archive, a serial number, personal documents, API keys or test data.
The installer verifies the payload before changes and keeps a usable rollback.
No fresh install may silently claim to jailbreak stock firmware. Current support
is KT2, 5.12.2.2, with the documented `.sh` launcher/FBInk prerequisite.

Versions use one decimal digit: 2.0, 2.1, ... 2.9, then 3.0 (never 2.10).
The user explicitly designated this quality/performance release 2.0. Keep
`VERSION`, displayed version, package metadata and release notes
consistent. Commit each finished update with its documentation and validation, then push it to `origin/main`. Verify that the remote contains the new commit before reporting completion.

## Git and publication

`main` is the clean public source history. Legacy local commits and private
import/device reports must never be pushed. If a local archival branch exists,
keep it local: do not use `git push --all` or `--mirror`. Do not rewrite another
person's published commits or force-push casually.

Before publication inspect the exact staged file list, secret scan and release
payload. Keep the public repository lean: source, tests, documentation, required
assets and small pinned source archives. No compilers, object files, build trees,
old screenshot collections, generated test fixtures or copied release folders.

The README should be attractive but honest: real native screenshots with demo
content, useful installation/build links and clear limitations. Never imply a
mockup is a hardware photo or that a live service was tested when it was not.

## Working with Merlin

Be direct, thoughtful and practical. Carry authorized work through to a concrete
result. Ask when information or a real external action requires it, not for every
reversible implementation choice. Explain meaningful tradeoffs plainly.

Merlin cares deeply about details and how the whole thing feels. “It works” is
the beginning, not the finish. Read the screen like a person would: what do I tap
next, what just happened, can I get back, is the text comfortable, does this
button look like its purpose? Prefer one coherent interaction over three clever
but inconsistent variants. Leave the project easier for the next agent to
understand than you found it.

## 1.10 maintenance notes

Files and Paper Quest IDs are retired tombstones; never renumber persisted app IDs.
ToDo has only ToDo/Done and is fullscreen. Clipboard actions appear after the
selection drag is released, not at the beginning of a long press. Notes ink is
a fixed raster; erasing clears pixels and empty pages remove the ink file.
Chat transcripts belong to user data and must survive updates.

Package validators reject data/backups in payloads. Reinstalling identical files
must not produce a rollback copy. Keep the twelve-update regression test and
macOS AppleDouble handling: FAT can remove sidecars while a directory traversal
still contains their names. Ignore only that FileNotFound case, never arbitrary
cleanup errors. Documentation previews keep native pixels at 1:1.

## 2.0 quality contracts

Use `system/ui/list.h` for paper lists: visible-ink primary/secondary alignment,
quiet divider, leading icon and a separate trailing action column. Apps retain
row input semantics and pagination. `titlebar.h` reserves action columns before
fitting a translated heading. `widgets.h` owns UTF-8-safe last-line ellipsis and
vertically centered form labels. Do not return to raw font-cell offsets for
row subtitles or guess an icon's position independently of its title.

`cover_cache.h` retains at most eight resampled display tiles, each at most
80,000 bytes, in RAM. It keys source identity/time/size, crop, dimensions and
dither phase; rounding is applied against the current background afterward.
This does not add files, reduce image quality or change source covers. Clear
owned buffers on shutdown. Keep the pixel-equivalence and invalidation tests.
Text glyph ink bounds include every antialiasing fringe, not just dark pixels.

Use `tools/audit-ui.py` for the native all-app / three-language visual atlas.
Generated images and timings stay under `build/audit`, never in the payload.
Host render timings are CPU measurements, not Kindle display/touch benchmarks.
Only set `LUMEN_PROFILE=1` when per-frame session logging is actually needed.
Do not disable power, Wi-Fi, app manager or reader services for speculative speed.

## Overlay ownership

Notifications are an app module with a copied request payload and bounded lifetime.
They capture DOWN through release before app input, including release outside the
surface or expiration during a gesture. Never dispatch a close gesture beneath
an overlay. Named app endpoints support non-navigating component apps; navigation
continues to use stable AppIntent IDs. An app need not be a launcher tile.

## 2.1 ownership and design contracts

Every active app owns its source under `lumen/apps/<name>/`. Public app APIs live
beside their implementation (calculator/evaluate, flashcards/scheduler,
news/provider and archive). `apps/host/compose.h` is the explicit composition root
for the remaining unity-build view fragments. Shared dispatch adapters are still
coupled: do not claim plugin isolation. Move behavior and state together when
improving these boundaries; never introduce another release-number catch-all.
System public APIs live beside their implementations under `lumen/system`.

Use `theme.h` roles and `typography.h` for hierarchy and primary actions. Use
`icons.h` / `app_icons.h` semantic identities, not private app copies. Black is an
intentional accent, not a replacement background for all controls. Floating
surfaces have a crisp offset shadow; most geometry stays square. Launcher tiles
use the shared continuous squircle mask. Library navigation uses the book icon.
Photo contrast is display-only: never rewrite the user's original photo.

Amazon imports are a Library responsibility. Device-owned supported KFX and
MOBI-family files are prepared lazily and cached with source identity; originals
remain untouched. The pinned converter archive is runtime, imported EPUBs are
personal data. Never ship a device identity, decrypted book or import cache.
Unsupported protection must fail clearly and retain the original-reader fallback.
Do not claim all DRM variants work; ARM converter execution needs target testing.
Keep third-party notices and pinned source links with distributed dependencies.

`tools/device/ui-session.sh` may pause only `awesome` for exclusive display
ownership. Record PID and start tick, never take ownership of an existing pause,
reassert on wake and resume on exit. Never stop powerd, wifid, USB, D-Bus or
appmgrd; do not generalize this into a kill-all optimization. This frees UI CPU,
not all firmware RAM. Preserve sleep, original-reader return and recovery.

## Compact system actions

System icons render through the cached compact mask inside unchanged 44px touch
targets. Launcher identities use the full canonical geometry separately. Do not
enlarge toolbar icons when changing app identities. Keep arrow shafts joined to
arrowheads and arc endpoints coherent. Dropdowns share option icons and a separate
right-side selection tick; new action menus must provide meaningful semantics in
`system/ui/dropdown_icons.h`, rather than drawing private menu rows.

## Slide 3D game

`apps/slide_3d` owns its engine, model and view. Preserve event-driven rendering:
no animation timer, permanent repaint loop or refresh-engine changes. Geometry
uses near-plane clipping and perspective inverse depth; outlines and spatial AA
are bounded post-processes. Cache the presented frame between actions. Game
state is local personal data; launcher IDs stay append-only and old order files
must remain readable when adding apps. Never describe host render timing as
Kindle touch/display latency.

The canonical source tree is `lumen/`. Use `python3 compile.py` for Kindle or
`python3 compile.py host` for native preview. Device scripts, source manifests,
packaging and tests live under `tools/`. The package mapping explicitly
separates repository paths from on-device installation paths.
