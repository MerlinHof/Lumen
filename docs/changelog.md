# Changelog

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
