# LUMEN app packages · Protocol 1

One catalog and one package identity are used for every app. Packages do not
carry a “system” or “custom” privilege flag. Installing a new version with an
existing ID replaces that app's entry, icon and executable; it does not create a
second launcher tile. The native executable runs in its own process.

## Build an app without building LUMEN

Download the SDK ZIP from the release, install **Zig 0.14.1**, and run:

```sh
python3 build-app.py package.json
```

In a source checkout the command is:

```sh
python3 tools/build-app.py my-app/package.json --target kindle
```

Only the listed app sources and the small header-only SDK are compiled. MuPDF,
LUMEN's executable and its system source are not linked or rebuilt.

```json
{
  "format": "lumen-app-1",
  "api": 1,
  "id": "example.counter",
  "name": "Counter",
  "version": "1.0.0",
  "sources": ["main.c"]
}
```

The result is a `.lumen-app` ZIP containing `manifest.json` and regular files
under `payload/`. The manifest lists each file's length and SHA-256. The entry
point is `app`, a static ARMv7 Linux executable. Optional `assets` in the build
manifest are relative paths beside the app source. `assets/icon.pgm` is a binary P5
76×76 grayscale engraved icon; otherwise the existing identity/fallback is used.

## Install or replace

Use File Transfer → Apps in the paired web page, or send:

```text
PUT /api/apps/install
Authorization: Bearer <current four-character pairing code>
Content-Type: application/octet-stream
<body: .lumen-app bytes>
```

HTTP 201 means installed. `GET /api/apps` returns the unified catalog, stable
package IDs, names and versions. App installation never replaces the LUMEN
executable or restarts it. Apps appear in Home and the existing app-order editor.

Installed code/assets are in `lumen/apps/<id>`. Personal state is separate in
`lumen/appdata/<id>` and exposed as `LUMEN_APP_DATA`. The read-only-by-convention assets directory is exposed as `LUMEN_APP_ASSETS`. App replacement and
system updates preserve it. Save validated bounded state atomically. Shipped
package updates are applied only when their app version is newer; explicit web
installs may replace the same version during development. Home removal preserves app state and prevents automatic reseeding until an explicit reinstall.

## System interface

The process receives newline JSON events on stdin and writes newline JSON frames
or service requests to stdout. Keep diagnostics off stdout. Protocol 1 has open,
input, pause, resume and text events. The open event supplies language, dimensions
and the private data directory. SDK Input numeric values are append-only.

Use `lumen_run` for a simple Input-driven app or `lumen_next` for a custom event
loop. `lumen_begin` / `lumen_end` send one complete frame. `ui_paint_*` describe
system-rendered text, headings, icons, hitboxes, rounded surfaces, ellipses, lines
and rings. `lumen_button` uses the shared button component. All ordinary app
content is below the 56px status bar. Touch feedback, rounding, icons, typography,
keyboard and E-Ink presentation remain owned by LUMEN. Use `lumen_text` for
left-aligned labels or when text needs contrasting ink on a game tile. Use
`lumen_mono` for fixed-column output and `lumen_hex_cell` for an outlined hex-grid cell.
Those two drawing commands require LUMEN 4.6 or later. LUMEN 5.8 adds
`lumen_points_begin`, `lumen_point` and `lumen_points_end` for grayscale
scatter plots and dense maps. Each batch accepts at most 3,000 visible points
with radii from one to four pixels.

A frame is bounded to 256 KiB and 2,048 commands. Input events are queued without
blocking the system's touch loop. An idle app blocks on stdin, rather than
polling or animating. LUMEN retains one foreground process and pauses it when
another tool opens. Independent overlays run alongside foreground content. Apps should save state after meaningful changes. A closed
or crashed app does not exit LUMEN. This is process isolation, **not a security
sandbox**: install only trusted native code.

Service requests use `lumen_request(kind,title,text,action)`:

- `input`: opens the actual system keyboard; the response is a text event.
- `reader`: opens Markdown in the shared Reader and retains Back navigation.
- `notify`: calls the installed `lumen.notifications` app, which owns the banner and its lifetime.

Use the open event's language (0 German, 1 English, 2 French) for app-owned text.
Do not draw a private keyboard, status bar or refresh loop. Do not manipulate
firmware services. Prefer clear typography and finite, paper-native tools.

LUMEN 4.8 adds `lumen_service` for named host operations. Replies arrive as
`INPUT_SERVICE` with the action ID, a success flag and response fields.
The catalog and transfer services expose existing launcher state and server
controls. `lumen_pager` sends `INPUT_PAGE` for taps and horizontal swipes.
`lumen_schedule` asks for one future `INPUT_TICK` without idle polling inside
the app process. Package-owned grayscale PGM files can use `lumen_asset`.
`lumen_app_tile` and `lumen_qr` use the system artwork and QR renderer.

## Existing app implementations

Forty-one independent apps ship with LUMEN 5.8. Their build manifests are the
`lumen/apps/*/package.json` files. Legacy views without manifests still run from
the host and may be replaced by packages using the same stable identity.

Protocol 1 supports semantic layout through `lumen_page`, `lumen_component_button`,
`lumen_textfield`, `lumen_component_toggle` and `lumen_component_checkbox`.
A component event includes the stable `id` and a `gesture` of `clicked`,
`submitted`, `changed` or `long_pressed`. Toggle and checkbox events carry
the new value as the string `"0"` or `"1"` in `text`.

Use `lumen_app_open_with` to open an app, a library book by stable hexadecimal
ID or an inline Markdown document. To send a named action to another installed
package use `lumen_call`. A positive request ID asks for a result. The receiver
gets `INPUT_INTENT` with the ID and can read `operation` and `text` from the
event. It can return with `lumen_return`. The caller receives
`INPUT_INTENT_RESULT` with the original ID and a `status` of `success`,
`error`, `cancelled` or `unavailable`. Save any caller state needed when the
foreground process is reopened. Document handles and generic typed file
references are outside this protocol revision.

Use the `lumen_storage_*` helpers for package-owned files. Keyboard,
Notifications and Status Bar have independent overlay processes. Core handles
compositing and routes keyboard output through the shared text editor.

LUMEN 5.1 adds `lumen_sprite` for a vertical P5 sheet of square tiles, an
`INPUT_PAUSE` lifecycle event, and shared dictionary operations
`game_word` and `clues`. A package should pause its simulation when suspended.

LUMEN 5.2 adds `lumen_card` and `lumen_segments`. Cards emit `clicked` by ID.
Segmented controls emit `selected` with the zero-based option in `text`.
The `reading` service provides a bounded `summary` of the last 31 days and
paged `books` activity. The `text` service’s `equivalent` operation compares two
newline-separated answers after the same folding used in LUMEN search. For
local spaced repetition, include the exported `lumen_fsrs.h` helper.

LUMEN 5.3 adds `lumen_multiline(title, text, action)` for the shared editor.
The committed value uses the existing `INPUT_TEXT` response. The reading
service’s `month` operation returns daily activity for a `YYYYMM` key.

LUMEN 5.4 adds `lumen_chess_piece` with optional piece rotation and
`lumen_weather_symbol` for monochrome forecast graphics. The weather service
provides bounded `open`, `refresh` and `status` operations.

LUMEN 5.5 adds measured lists via `lumen_flow_list`, `lumen_flow_row` and
`lumen_flow_end`. Clients choose row order, stable IDs and one or two columns.
The renderer wraps text, lays out and paginates the rows, and emits `clicked`,
`toggled`, `pinned` and `deleted` events for row actions.

LUMEN 5.6 adds scoped `study_material` operations for paged material lists
and opening an uploaded document by name in Reader. Packages can also call
`clock/start_timer` with minutes or `notes/create_learning` with a title
followed by a newline and the recall text.

LUMEN 5.7 runs Lock Screen through a separate sleep overlay slot. The package
receives a `sleep_state` snapshot and returns image and typography choices. The
runtime handles power events and bounds how long it waits for a frame. The
`recipes` service exposes paged archive and results queries plus search and
Reader opening by recipe ID.
