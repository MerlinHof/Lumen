# Changelog

## 3.2

- Merge Home and the app launcher. The date page rotates through seven grayscale landscapes, followed by full-height app grids.
- Use the whole screen for apps. Manage Home grid order and visibility in Settings, and group appearance options into General, Status bar and Lock screen.
- Keep Notes on the same ruled page when typing. Capture keys on touch-down, tolerate release drift and restore press feedback.
- Keep grayscale transitions on a consistent waveform and complete press feedback before submitting its replacement.

## 3.1

- Start on Home with a live date, a lakeside pencil sketch and a paged app grid.
- Choose, reorder or hide the bottom tab bar. App names sit on the left of the status bar and time sits beside the connectivity indicators.
- Process keyboard input independently of pending e-ink refreshes and batch rapid edits into the latest frame.
- Flag ToDo items, with newest first inside each group. Notes use faint ruled lines.
- Crop the original cover to fill the lock screen when available and fall back to the sharpest cached cover.

## 3.0

- Fix shared pager dispatch in OpenAI conversations and archives. Long messages open in Reader from the sender row.
- Use the shared UTF-8 search folding in the Home Assistant device picker.
- Enlarge the lock-screen book cover and remove the blurred backdrop and its setting.
- Restore the white Weather background and extend German, English and French localization.

## 2.9

- Align Reader actions in the title bar and match the calculator equals typography to its other keys.
- Keep the keyboard accent menu open on release and dismiss battery details before forwarding touch to an app.
- Return from Home Assistant device selection to the dashboard and use larger dashboard card corners.
- Center ToDo rows, refine the minimal clock, and fit more conversation above a bottom-aligned OpenAI composer.
- Correct localization in shared labels, Planetarium history, and device status. Refine Slide 3D tile materials.

## 2.8

- Shared rounded outlines and press feedback, larger media and dropdown corners, and single-update segmented controls.
- Tap the status date or time to open Calendar or Clock. New analog faces and larger digital time.
- Cleaner Reader controls, rounded reflowable illustrations and independent navigation highlighting.
- Four-row photo gallery and folder assignment from the viewer. Photos settings return to the gallery.
- Refined weather, chat, drawing strokes, quick task entry and Slide 3D guidance.

## 2.7

- Round capsule silhouettes for the battery and toggles using the shared corner renderer.
- Softer cubic superellipses and 8×8 coverage for shared corners, including image clips.

## 2.6

- Photo folders with persistent multi-folder assignment and an All Photos view.
- Folder slideshow with duration, fit or fill, and portrait or landscape preferences.
- Global appearance switch for corner rounding. Covers, photos, launcher tiles, and shared controls use the same superellipse mask.
- Wireless update design using LUMEN Connect and launcher-managed activation and rollback.

## 2.5

- Superellipse corners for shared buttons, segmented controls, dropdowns, switches and floating surfaces.
- Segmented controls with one continuous outline and full-width active segments.
- Grayscale frog pieces, reflective eyes and shaded lily pads.
- Frog Solitaire and Slide 3D compile independently against the public drawing and app runtime APIs.
- App-owned state for reader, library, store, Wi-Fi, settings, Clock, editor, notes, tasks, status bar and Wikipedia. The app runtime is the sole owner of the active route.
- Existing save files use a dedicated system app-state storage service.

## 2.4

- Slide 3D uses the existing engine at native resolution. Frog Solitaire adds a 33-pad jumping puzzle with undo.
- Clock has analog and digital faces, full screen, timer and alarm tabs.
- Shared rounded controls, solid selected segments, dropdown pager and semantic option icons.
- Book long-press menu, pinned shelf order, reader preferences shared across both entry points.
- Offline FreeDict translations and a shared vocabulary for word games.
- Source and assets consolidated under `lumen/`. Build and release tooling live under `tools/`.
- Firmware `cvm` stays running to support normal power-button handling.

## 2.3

- Signal: an original local 3D exploration game with three beacons and a lighthouse objective.
- Custom CPU triangle rasterizer, near clipping, depth buffer, directional light, projected shadows, material hatching and contour outlines.
- Lightweight spatial antialiasing and filtered presentation; event-driven rendering with a cached display image.
- Touch/swipe movement, collision, orientation map, instructions in DE/EN/FR and persistent progress.
- Existing launcher order and visibility preserved when adding the app.

## 2.2

- Compact cached system icons with preserved touch targets; corrected Back and reload arrow geometry.
- Full-size launcher identities remain unchanged.
- Shared dropdown option icons and separate selected checkmarks.
- Lowered note/folder list icons, primary Timer action and slightly stronger photo contrast.

## 2.1

- Semantic typography and restrained black/depth accents; shared rounded icons and continuous launcher squircles.
- Library book tab, internal charging bolt and display-only photo contrast enhancement.
- Notification app with complete overlay gesture ownership.
- App-owned source folders, colocated APIs and removal of retired drawing/NumScratch implementation.
- On-demand, cached device-serial Amazon import with pinned converter runtime; originals preserved.
- Reversible UI-only firmware pause with process-identity checks and wake reassertion.
- Refreshed native screenshots, design/ownership guidance and dependency documentation.

## 2.0

- Quality and performance only; no new apps or user-facing features.
- Shared ink-aligned list rows, title/action columns, form labels and UTF-8 ellipsis.
- Consistent metadata spacing, quieter dividers and aligned icons across settings, network, notes, articles, recipes, history and app management.
- Corrected overlapping game header actions and clearer Reader title/chrome alignment.
- Exact glyph-bound rendering, faster rectangle/ring paths, bounded cover RAM tiles and note/weather snapshots.
- Coalesced device-status queries, optional frame logging and speed-oriented compilation for hot system units.
- One coherent responsive Connect stylesheet and connected desktop navigation.
- Reproducible all-app visual atlas, three-language settings review and pixel-equivalence checks.
- Version sequence now rolls from x.9 to (x+1).0.

## 1.10

- Measured calculator viewport and aligned function/input edges.
- Notes alignment, direct-child counts, natural gutter typography and true pixel erasing.
- Drag selection before copy actions, calmer keyboard numbers and fullscreen ToDo/Done.
- Recipe search Back behavior and denser source metadata.
- Local OpenAI chats, follow-up context, archive and long-answer Reader continuation.
- Retired Files and Paper Quest, removed unused text-capture work and stale test harnesses.
- Twelve-update retention checks, identical-install skipping and package backup/data exclusion.
- Native 1:1 documentation screenshots and a refreshed gallery.


## 1.9

- Lean public source tree; dependencies, build artifacts and local history kept private.
- Responsibility-based app filenames and explicit incremental object/dependency lists.
- Packaging independent of all previous release/work folders.
- Unified fresh-install/reset defaults and validated imported option ranges.
- Prepared-Kindle installation preflight, initialized storage folders and clearer steps.
- Manual launch remains available if guarded autostart installation cannot complete.
- Two recognized runtime-only rollback backups retained after successful updates.
- Visual README, native demo gallery, comprehensive agent/design guide and host CI.

## 1.8

Independent keyboard language; staggered keyboard and number row; accents on hold;
shared UTF-8 input/clipboard; multiline tasks; cached keyboard rendering and
system/app source separation.

Earlier working history remains in a local archival branch, not in the public
repository, because it contained device-specific reports and unnecessary build material.
