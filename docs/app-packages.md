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
manifest are relative paths beside the app source. `icon.pgm` is a binary P5
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
`lumen/data/app-state/<id>` and exposed as `LUMEN_APP_DATA`. The read-only-by-convention assets directory is exposed as `LUMEN_APP_ASSETS`. App replacement and
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
left-aligned labels or when text needs contrasting ink on a game tile.

A frame is bounded to 256 KiB and 2,048 commands. Input events are queued without
blocking the system's touch loop. An idle app blocks on stdin, rather than
polling or animating. LUMEN retains one active app process and pauses it when
another tool opens; app state must be saved after meaningful changes. A closed
or crashed app does not exit LUMEN. This is process isolation, **not a security
sandbox**: install only trusted native code.

Service requests use `lumen_request(kind,title,text,action)`:

- `input`: opens the actual system keyboard; the response is a text event.
- `reader`: opens Markdown in the shared Reader and retains Back navigation.
- `notify`: uses the shared notification surface.

Use the open event's language (0 German, 1 English, 2 French) for app-owned text.
Do not draw a private keyboard, status bar or refresh loop. Do not manipulate
firmware services. Prefer clear typography and finite, paper-native tools.

## Existing app implementations

Frog Solitaire, Sliding Puzzle, Tic Tac Toe, Terminal, 2048 and Sudoku ship as
independent packages and use this same protocol. Their sources and build manifests live in their app folders. Other
existing views still have linked compatibility implementations. They are entries
in the same catalog and can be replaced individually by a package using the same
ID (`lumen.calculator`, `lumen.notes`, `lumen.home`, etc.), without changing the
system. They have not all been ported into independent executable artifacts yet.
Do not misrepresent that migration as complete. New app development must use the
package contract, rather than add another linked view or numeric route.
