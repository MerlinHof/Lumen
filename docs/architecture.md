# LUMEN · Runtime and apps

LUMEN provides an origin-neutral app catalog. Every identity has a stable package
key, launcher entry and version. There is no package privilege flag for “system”
versus “custom”. A package using an existing identity replaces that app in place.

## Independent app processes

`system/app_packages` owns discovery, installation, process lifecycle and bounded
IPC. Packages live in `lumen/apps/<id>`; personal state lives in
`lumen/appdata/<id>`. `AppIntent` still owns navigation payloads and stable
book references. Existing numeric IDs remain stable; package slots do not
renumber built-in entries. Engraved artwork lives in each app’s assets folder.

An independent app is a static ARMv7 executable plus assets and a SHA-256 manifest.
It exchanges Protocol 1 newline JSON with LUMEN. The system renders its UI through
`system/ui/package_app.h`, using the existing controls, typography, icons,
rounding and refresh engine. The app receives bounded queued input and the system
keyboard's text results. Markdown requests open the same Reader used by books,
Wikipedia, notes and model answers. Apps can expose an internal Back state.

The SDK (`tools/sdk/lumen_app.h`) and `tools/build-app.py` compile only the app.
A standalone SDK ZIP is published with releases. No system rebuild or MuPDF
linking is required. App replacement is a directory transaction and does not
restart LUMEN. Crashes are confined to the app process; this is not a filesystem
security sandbox. Native packages must be trusted.

Thirty-nine standalone packages ship from their individual app folders,
including Home, File Transfer, Terminal, Periodic Table, Keyboard, Notifications, Status Bar, Quotes, Flashcards, Languages, BrainTrain, Reading Statistics, Journal, Calendar, Weather, ToDo, Study, Lock Screen, Recipes and twenty games.
Keyboard, Notifications, Status Bar and Lock Screen run in separate overlay processes while the foreground app remains active. Other existing views retain linked
compatibility adapters in `apps/host/compose.h`. Their IDs use the same catalog
and accept independent replacement packages, but those older implementations
have not all been extracted into executable artifacts. New apps must use the
package interface. Do not describe every old implementation as migrated.

[App-by-app migration inventory and SDK dependencies](app-migration.md)

### Remaining linked apps in 5.7

- Reading and content · Library, Reader, Store, Wikipedia and News.
- Personal tools · Clock, Notes, Photos, Home Assistant, OpenAI.
- Learning and reference · Calculator and Planetarium.
- Device surfaces · Settings.

Pending SDK work includes semantic lists, grids, menus and dialogs,
shared document/file handles, and migration of data still owned by these
linked apps. Standard text input, pagination, and E-Ink refresh are already
shared. Raw drawing remains available for specialized content.

## Shared system services

The system owns display/input/power, app navigation, storage and preferences,
network jobs, UI primitives and rich-document rendering. Shared components use
semantic text roles, visible-ink baselines, measured list geometry, dropdowns,
checkboxes and consistent superellipse masks. The status bar belongs to the
system and is preserved when rendering ordinary app content.

`text_input` / `text_edit` / the keyboard provide UTF-8-safe editing and clipboard
behavior. System and keyboard languages are independent (DE/EN/FR). Keyboard
background/keybed caching and queued touch input are retained. Large display
text is rendered from font outlines at final size with a bounded tile cache.
Calculator boxes use a mathematical axis; note ink shares its text viewport.

The Reader uses a preferences snapshot and reflows only on changes. CommonMark
and GFM use the pinned cmark-gfm implementation. Image contrast and sampling are
shared. Cover resampling has a bounded RAM cache and no new persistent copies.

Kindle and emulator use the same refresh planner. It selects DU, GL16 or GC16
from screen coverage, change intensity and old ink removal. Firmware
power, Wi-Fi, USB and app-manager services remain available.

## Complete system updates

`system/release_update` checks the public GitHub latest release asynchronously.
It downloads `.lumen-system` using `.wireless.json`, verifies its overall digest,
and delegates to `system/packages` and `system/system_update`. Packages are
bounded, regular-file ZIP archives; traversal, symlink targets, duplicates,
unexpected files and incorrect lengths/digests are rejected.

A complete update includes program, fonts, artwork, dictionaries, scripts and
shipped app archives. It cannot overwrite books, app state, separately installed
app directories or recovery control paths. Changed originals are copied before
a durable write-ahead journal commits. A helper outside the replaced binary
performs activation. Interrupted activation and failed trial startup restore originals, including
resources. The launcher stays alive across activation. The first rendered screen
confirms the update automatically by removing the journal durably before deleting
rollback files.

`runtime_files.h` is generated from the package allowlist and identifies obsolete
managed files. Unknown/user files are preserved. One transaction rollback is
retained until Keep, so GitHub updates do not accumulate complete runtime copies.
The USB installer remains available for first installation and recovery.

## Releases

`tools/config/sources.txt` is explicit; dependencies are pinned under
`tools/config`. Runtime packaging is allowlisted. Generated tools, build files,
private content and device reports are ignored. Public screenshots use demo data.

A versioned push to main runs Native checks. After success, Publish Kindle update
builds and creates the missing GitHub release with USB, full-runtime, metadata,
SDK and app assets. Existing releases are never overwritten. Version digits roll
from x.9 to (x+1).0. See [wireless updates](wireless-updates.md) and
[app packages](app-packages.md) for the operational contracts.
