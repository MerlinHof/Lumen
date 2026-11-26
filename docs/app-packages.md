# LUMEN app packages · Protocol 2

The SDK uses a device-independent 600 × 800 portrait reference surface.
Use the shared layout controls for ordinary screens and
`lumen_fraction_rect` from `lumen_surface.h` for proportional custom regions.
Packages never receive a physical framebuffer size or PPI.

Surface API 3 encodes the width and height as 10,000 units per axis on the
package wire. Drawing commands, overlays, capture regions and touch events
scale to this same surface. The C SDK handles conversion for existing layout
calls and the host continues to accept older package frames.


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
  "format": "lumen-app-2",
  "api": 2,
  "entry": "app",
  "surfaces": ["foreground"],
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

`surfaces` declares where the package can run. Use `foreground` for a normal
app, `overlay` for system overlays and `background` for exported operations.
LUMEN can still install and run Protocol 1 apps. Protocol 2 packages
require 6.5 or later. From 6.6, packages can declare a `launcher` object with
`order` and `visible` defaults. Saved launcher preferences use package IDs.

## System providers

A package may declare a role in `provides`. The startup role requires a
foreground surface, as do `document` and `books`. The other four roles require an overlay surface and
receive their existing role-specific event stream. To override a shipped
provider, install a package declaring that role and set the respective entry
in `lumen/data/providers.conf` before restarting LUMEN.

```ini
startup=lumen.home
text-input=lumen.keyboard
status=lumen.status-bar
notification=lumen.notifications
sleep=lumen.lock-screen
document=lumen.reader
books=lumen.library
```

The runtime uses the shipped provider if an override is unavailable.

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

## App configuration

Declare up to eight filenames in a Protocol 2 package's `configuration` array
to let the paired File Transfer page write them. Send up to 2048 bytes in
`PUT /api/apps/config?id=<package-id>&key=<filename>`. LUMEN checks the
installed manifest and writes to that package's private state. Each app
validates the data before using it.

## Overlays

From 6.6, a foreground app can call `lumen_overlay_open` with an installed
package declaring an overlay surface, a positive caller-supplied handle, a
presentation policy and an anchor. The overlay receives `overlay_open` and its
bounds in screen coordinates, followed by pointer events. It can use the shared
drawing commands. Call `lumen_overlay_close` to close the overlay by handle.
The runtime closes nonpersistent overlays when their caller leaves. The supported
presentation policies are `persistent`, `floating`, `modal`, and `fullscreen`.

## System interface

The process receives newline JSON events on stdin and writes newline JSON frames
or service requests to stdout. Keep diagnostics off stdout. Protocol 2 has open,
input, pause, resume and text events. The open event supplies language, dimensions,
the package protocol number and the private data directory. SDK Input numeric
values are append-only. Use `lumen_page` with the `lumen_component_*` functions
for coordinate-free controls and `lumen_dispatch_click` to bind IDs to callbacks
in the input handler. Use `lumen_storage_checkpoint` from a save handler to
write changed state and retry failed writes on the next checkpoint.

Use `lumen_run` for a simple Input-driven app or `lumen_next` for a custom event
loop. `lumen_begin` / `lumen_end` send one complete frame. `ui_paint_*` describe
system-rendered text, headings, icons, hitboxes, rounded surfaces, ellipses, lines
and rings. `lumen_button` uses the shared button component. All ordinary app
content is below the 56px status bar. Touch feedback, rounding, icons, typography,
keyboard and E-Ink presentation remain owned by LUMEN. Use `lumen_text` for
left-aligned labels or when text needs contrasting ink on a game tile. The
6.2 SDK adds `lumen_text_right` for numbers and other right-aligned labels.
A frame's first heading is used as its current view title in the status bar
when that title slot is enabled. Use
`lumen_mono` for fixed-column output and `lumen_hex_cell` for an outlined hex-grid cell.
Those two drawing commands require LUMEN 4.6 or later. LUMEN 5.8 adds
`lumen_points_begin`, `lumen_point` and `lumen_points_end` for grayscale
scatter plots and dense maps. Each batch accepts at most 3,000 visible points
with radii from one to four pixels.

A frame is bounded to 256 KiB and 2,048 commands. Input events are queued without
blocking the system's touch loop. An idle app blocks on stdin, rather than
polling or animating. LUMEN runs one foreground app and keeps up to three
recently used apps paused. Returning to a retained app delivers `resume` with
the current language and keeps its in-memory view. Older apps can be evicted.
Independent overlays run alongside foreground content. Apps should save their
important view and data state in private storage. A closed
or crashed app does not exit LUMEN. This is process isolation, **not a security
sandbox**: install only trusted native code.

Service requests use `lumen_request(kind,title,text,action)`:

- `input`: opens the actual system keyboard; the response is a text event.
- `reader`: opens Markdown in the shared Reader and retains Back navigation.
- `notify`: calls the configured notification provider, which owns the banner and its lifetime.

Drawing packages can use `LUMEN_APP_SURFACES` for transient raw grayscale
surfaces. `lumen_transient_bitmap` composites the file with white pixels
transparent. Its revision should change when pixels change, even if the rest
of the frame is identical. The runtime scopes the files to their foreground
process and removes them on exit. Maintain a fallback for older runtimes.

A flow row with `lumen_flow_row_detail` can show secondary text and an optional
warning, with a clock button producing a `deadline` component event.

The Wikipedia package offers a declared `summary` operation for a short
selected term. The result contains the article title and lead extract on
separate lines. It uses [MediaWiki TextExtracts](https://www.mediawiki.org/wiki/Extension%3ATextExtracts).

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

## Device preferences

LUMEN 6.1 uses `lumen_service("device","preference","invert 1",action)` for
named changes. `device/state` returns the values under those same names.
The public names are in `lumen_device_preferences.h`, and the shell permits
these privileged operations from the Settings package. The older
`settings/preference` and `settings/state` calls keep their positional contract
for existing Settings packages.

## Existing app implementations

All 53 active apps ship as independent packages. Their build
manifests are the `lumen/apps/*/package.json` files. A missing package displays a
missing-app screen. Home also offers access to Settings and File Transfer.

Protocol 2 supports semantic layout through `lumen_page`, `lumen_component_button`,
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

Use the `lumen_storage_*` helpers for package-owned files. Version 6.0
accepts relative subdirectories such as `drafts/note.txt`. Writes create missing
parents and atomically replace the final file. Hidden components, traversal and
symlinked directories are rejected by descriptor-relative access. On 6.1,
`lumen_storage_mkdir` creates a directory and its missing parents,
`lumen_storage_exists` checks for a regular file or directory, and
`lumen_storage_list` visits its public entries. Pass an empty path to list
the root of the current package's data directory.

`lumen_reader_file(title, path)` opens a UTF-8 Markdown file up to 128 KiB in
Reader using a typed document reference. The runtime resolves the path under
the calling package's private data directory. Inline Markdown and book-ID
intents remain supported. `lumen_list_action` and `lumen_icon_action` route
ordinary taps by component ID. The frame registers up to 64 semantic controls.
Incoming events are parsed as JSON and limited to 65,535 bytes. Keyboard,
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

LUMEN 6.2 opens a file in Reader with
`lumen_reader_document(relative_path, collection)`. Pass `NULL` for private
app storage or `"study"` for a filename in the shared Study uploads folder.
A private Markdown preview can use `lumen_reader_file(title, relative_path)`.
Packages can start a Clock timer with `clock/start_timer`.

LUMEN 5.7 runs Lock Screen through a separate sleep overlay slot. The package
receives a `sleep_state` snapshot and returns image and typography choices. The
runtime handles power events and bounds how long it waits for a frame. The
`recipes` service exposes paged archive and results queries plus search and
Reader opening by recipe ID.


## Background operations declared by a package

Add an optional `operations` array to the app manifest. It accepts up to 16
unique names with at most 47 lowercase ASCII letters, digits, underscores or
hyphens each. Publish the input and result contract with the app. A name is a
public API, so use a new name when an incompatible operation replaces it.

A caller uses `lumen_app_service(package_id, operation, data, action)`. The
recipient registers `lumen_service_handler` before calling `lumen_run` and
answers with `lumen_app_service_result(ok, data)`. Data is a UTF-8 string
of up to 8,191 decoded bytes in either direction on LUMEN 6.1. The recipient can read the caller's
package ID with `lumen_string("caller", ...)`. Both sides can use an empty
string for operations with no payload. Study declares `material_count`, which
accepts an empty string and returns the count of locally available documents.
Notes declares `create_learning`. It accepts a title (up to 255 UTF-8 bytes)
followed by a newline and the recall body. The formatted note must fit in
8,191 bytes. A successful result returns the existing decimal note ID format.
If the Learning folder has been removed, Notes places the note at the root.

The app starts in a separate process with its own data and assets, and receives
`service_call` without foreground initialization. The runtime closes the worker
after its reply, on failure, when the caller leaves, or after thirty seconds.
There is one in-flight worker at a time. Handlers should perform bounded work
and coordinate any shared writes with their foreground or overlay instance.

The caller receives the existing `INPUT_SERVICE` event with its action and
success flag, plus `data` and `status` fields for completed calls. Workers
cannot open overlays, route the UI, or send further service calls through this
channel. A missing package, undeclared operation or busy worker reports failure.
Leaving the app cancels a standard request and discards its result.

An app can call its own declared operation with
`lumen_app_service_background(package_id, operation, data, action)` and a
positive action ID. The same worker can continue after the app closes or
pauses and has a 130-second deadline. It shares the single worker slot with
foreground calls. An app gets a result event if it is active when the work ends.
Save durable results in app storage so reopening the app can recover them.
The open and resume events report an outstanding call's `background_action`
or zero.

These packages run as trusted native code and process separation does not
sandbox files or network.

Use `lumen_button_action` for a button that needs explicit placement and
`lumen_app_tile_action` for an interactive app tile. Both report stable
component IDs. The app tile also reports long presses. Use `lumen_toggle_action` for an
explicitly placed switch that emits a `changed` event with `"0"` or `"1"`.
Standard app controls
should handle those events instead of repeating hit geometry in input handlers.

`lumen_editor_action` emits a `caret` gesture with a UTF-8 byte offset.
`lumen_select_action` emits a named click from a shared selector row.
`lumen_custom_action` adds semantic interaction and feedback to artwork with
custom rendering, such as a calculator function key. Plot sampling and other
position-dependent content interactions can still receive raw coordinates.

`lumen_https_get` from `lumen_http.h` performs a bounded HTTPS GET from a
package worker using the device's curl and trusted CA bundle. Weather uses
this in its own `refresh_forecast` operation.


### Document and background helpers

A package can pass an EPUB or PDF from its private data directory to Library
with `lumen_book_import(path, title, action)`. On success Library consumes the
temporary file and delivers a service reply. The operation keeps an existing
book with the same filename and rejects paths outside the sender’s directory.

`lumen_cover_asset` paints a package-local PGM using the shared cover renderer.
`lumen_markup.h` exposes the installed document parser through
`markup_load(path, html, attributes, count)`. The caller chooses which
attributes to receive and releases the returned tree with `markup_free`.
Markup input is limited to 4 MiB. Packages can use
`lumen_reader_document_title` to open a private HTML file in Reader.
A package can render a trusted local HTML document into a grayscale bitmap in a
worker by running `LUMEN_DOCUMENT_TOOL --render-document INPUT PAGE WIDTH HEIGHT OUTPUT`.
The first stdout line is the page count, followed by tab-separated link
rectangles and destinations. Coordinates and page numbers are zero-based.
Use `lumen_bitmap` to paint the raster from private storage. The Browser owns
fetching and sanitizing its HTML before calling the renderer. The
`lumen_http_page` helper also returns the final URL after redirects so callers
can resolve relative links against the displayed page.

For an automatically sized navigation row inside a form, use
`lumen_form_link(id, label, detail)`. The row shows a forward arrow and
emits a named click using the same component handling as other form controls.
When a toggle sits inside another touch target, use `lumen_nested_toggle` so
the switch receives its own taps before the surrounding card.
Use `lumen_progress_ring(x, y, radius, percent)` for clockwise reading or
task progress with a centered percentage. `lumen_popup_y` in
`lumen_geometry.h` places a popup beside a visible selection.

Browser declares pinned QuickJS and litehtml build dependencies. The package
links both directly. Its JavaScript bindings and grayscale viewport live in the
app and run on Kindle. The shared document renderer remains available to other
apps through the SDK.
HTTP workers can specify a user agent when a service negotiates content.
`lumen_http_same_host_tls` checks for an exact HTTPS upgrade without sending
a token to the probe endpoint.

A Protocol 2 package can declare `periodic` with an exported background
`operation` and an `interval` of 900–86,400 seconds. LUMEN starts at most one
scheduled worker at a time while awake and connected. Workers get a standard
`service_call` and are responsible for checking their own user preferences and
whether there is work to do. The scheduler limits each worker to 210 seconds.


### Named controls and indexed targets

Use `lumen_page` and the form components when the SDK can handle layout.
For custom layouts, `lumen_button_action`, `lumen_icon_action`,
`lumen_icon_control`, `lumen_toggle_action`, and `lumen_select_action`
bind a name to the control when it is drawn. The input handler reads the
action with `lumen_action` and checks the gesture it expects. Use
`lumen_index_action("cell", index, x, y, w, h)` for repeated areas or dense
grids. The SDK creates IDs such as `cell:12` and `lumen_action_index`
validates the index against the app's item count.

### HTTP and workers

Include `lumen_http.h` and call `lumen_https_get(url, buffer, sizeof(buffer))`
from a package worker for a public text endpoint. For authenticated JSON,
pass `LumenHttpRequest` to `lumen_http_text` and set `bearer` and, for POST,
`json`. Credentials and request bodies go through a private stream.
Authenticated requests do not follow redirects. Plain HTTP is opt-in for
local or user-configured servers. Use `lumen_url_encode` for query values.
For web forms, set `form_post` to the URL-encoded form body. To maintain an
HTTP session, set `cookie_jar` to the absolute path of a private app-data
file. The transport reads and updates that jar across requests.

`lumen_http_download` writes a bounded response to an empty private file
descriptor. The caller owns the file and commits it only on success.
For longer jobs, use an exported app service or the local worker helpers
in `lumen_worker.h`. The local helpers separate worker I/O from the app
protocol and provide nonblocking polling and cancellation.


### Forms and state checkpoints

Use `lumen_form(title)` for a standard vertical form. Add `lumen_form_text`,
`lumen_form_choice`, or the existing button, field, checkbox and toggle
components. LUMEN sets spacing, wraps text and paginates when the controls
fill the screen. Selected values still flow through named component events.

Call `lumen_state_register(name, version, &state, sizeof(state))` once before
`lumen_run`. Registration loads a matching record into the supplied state.
Use version zero to retain an existing raw storage format. Mark mutations with
`lumen_state_dirty()`. The SDK checks the dirty state after 750 ms of idle
time and flushes it on Pause and graceful exit. Calling
`lumen_state_checkpoint()` is useful when an operation needs a saved record
immediately. Identical bytes never cause a write.

For network operations that need precise diagnostics, use
`lumen_http_text_result` or `lumen_http_download_result`. They distinguish
timeouts, redirect limits, size limits, interrupted transfers, and local I/O
errors. A failed download clears the partial file. The bearer convenience
helper uses HTTPS. Set `.allow_http=1` on a specific request only when using
an explicitly configured plain-HTTP service.
