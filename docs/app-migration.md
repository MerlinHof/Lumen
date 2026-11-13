# App migration inventory

Baseline for this migration is LUMEN 5.1, commit `a0b71673`. The source
catalog contains 52 active app identities. Retired identities are excluded.
The 28 packages below have individual manifests and executables and are
included in both installers.

## Already packaged at baseline

- System and tools · Home, Keyboard, Notifications, Status Bar, File Transfer,
  Terminal, Quotes, Flashcards, Periodic Table.
- Games · 2048, Bingo, Connect Four, Crossword, Frog Solitaire, Hangman, Hex,
  Knight’s Tour, Maze, Minesweeper, Nonograms, Sliding Puzzle, Snake, Sokoban,
  Solitaire, Sudoku, Tetris, Tic Tac Toe, Wordle.

## Linked at baseline

| App | Migration challenge | Shared SDK extension |
| --- | --- | --- |
| BrainTrain | Preserve exercise state, results and progression across practice views. | Semantic segmented controls and private records for reusable training flows. |
| Calculator | Preserve mathematical-axis layout, graphing, history and editable expressions. | Formula and plot surfaces plus bounded editable fields and shared history lists. |
| Calendar | Preserve local notes, date arithmetic and four views. | Local date/time primitives, calendar grid and date-selection intents. |
| Chess | Preserve move legality, computer opponent, board orientation and saved games. | Cancellable app-local jobs and a reusable interactive board surface with shared piece artwork. |
| Clock | Keep alarms and timers firing after leaving the foreground view. | Persistent scheduled alarms with named delivery, timezone handling and progress surfaces. |
| Home Assistant | Keep credentials separate and refresh live values and history independently. | Named secrets, cancellable polling jobs and interactive charts with stable sample IDs. |
| Reading Statistics | Read shared reading events without owning Reader state. | Bounded activity queries, aggregated time series and shared chart components. |
| Journal | Preserve date-indexed drafts and multiline editing. | Date-keyed storage, inline multiline fields and paper-style prompt lists. |
| Languages | Keep FSRS progress, quiz options, answer checking and glossary. | Semantic segmented controls, Unicode normalization and reusable review persistence. |
| Library | Preserve imports, covers, metadata, pins and reading progress. | Stable book references, paged catalog queries, image handles and library change events. |
| Lock Screen | Compose promptly before suspend with current cover/photo and clock settings. | Lifecycle-aware sleep overlay role, bounded prepare deadline and shared image and clock surfaces. |
| News | Preserve editions, fetches, archive and rich articles. | Background content jobs, finite archive views and shared document handles. |
| Notes | Preserve folder tree, page-aligned ink, editable text and Markdown previews. | Hierarchical private storage, text/ink viewport anchoring, reusable editors and document intents. |
| OpenAI | Preserve chats, model selection, credentials and long formatted responses. | Secret references, cancellable network requests, message lists and shared document handles. |
| Photos | Keep orientation, albums, shared imports and slideshow timing. | Image handles, transforms, slideshow scheduling and photo-picker result intents. |
| Planetarium | Keep local calculations and fast interaction on Kindle. | A bounded interactive drawing surface and app-local rendering jobs. |
| Reader | Keep pagination, rich text, images, annotations and saved positions. | Session-scoped document rendering, shared selection, stable positions and progress services. |
| Recipes | Keep search, multilingual retrieval and offline archive. | Search jobs with result pages and shared rich-document archival. |
| Settings | Replace direct access to preferences, Wi-Fi, package and updater state. | Typed preference schemas, capability-specific device services and shared settings rows. |
| Store | Keep catalog searches, download feedback and Library import. | Download jobs with progress, provider pagination and verified import intents. |
| Study | Keep material uploaded by File Transfer and notes created from study prompts. | Scoped shared file handles, import subscriptions and typed Notes intents. |
| ToDo | Preserve flags, completion, board view and wrapped editable task text. | Measured multiline rows, semantic checkboxes and shared editor behavior. |
| Weather | Preserve saved locations, forecasts and day/night presentation. | Cancellable geocoding and forecast jobs, shared icons and forecast cards. |
| Wikipedia | Preserve cached articles, search, images and Reader navigation. | Offline document cache, image handles and shared search jobs. |

## Progress in 5.2

BrainTrain, Languages and Reading Statistics have moved from the linked list
to independent packages. This brings the total to **31 packaged / 21 linked**.
The SDK gained semantic cards and segmented controls, normalized text
comparison, shared FSRS-6 scheduling and a bounded reading-activity service.
The entries above retain the 5.1 baseline for comparison. The [architecture
guide](architecture.md) lists the current linked apps.

## Progress in 5.3

Journal and Calendar have independent packages, bringing the total to
**33 packaged / 19 linked**. Both use shared multiline editing. Calendar
also uses a bounded monthly reading-activity query.

## Progress in 5.4

Chess and Weather now run from independent packages, bringing the total to
**35 packaged / 17 linked**. Their migration adds reusable chess-piece
rotation, forecast symbols and bounded forecast requests.

## Progress in 5.5

ToDo is independently packaged, bringing the total to **36 packaged / 16
linked**. Its migration adds a reusable, measured multiline list with stable
row actions and one- or two-column layouts.

## Progress in 5.6

Study runs independently, bringing the total to **37 packaged / 15 linked**.
Its migration adds scoped Reader requests for uploaded material and shared
services for focus timers and recall notes.

## Progress in 5.7

Lock Screen and Recipes now have independent packages, bringing the total to
**39 packaged / 13 linked**. The runtime gained a sleep overlay slot and a
bounded recipe provider and archive service.

## Progress in 5.8

Planetarium owns the sky map, solar system, time controls and location in an
independent process. Batched point rendering keeps the dense sky chart within
the frame budget. Wikipedia owns search, pagination and offline history
interaction through a shared document provider. This brings the total to
**41 packaged / 11 linked**.

## Progress in 5.9

News, Calculator, Clock, Store, Library, Home Assistant, OpenAI, Photos and
Reader now run as standalone packages. This brings the total to **50 packaged /
2 linked**. The SDK gained document pages, book and catalog covers, photo
surfaces, measured text cursors, mathematical formula layout, fullscreen
presentation, arc drawing, display typography and masked input.

## Complete in 6.0

Notes and Settings now run in their own processes. Notes retains the existing
shared notes, folder and ink files so the editor, Study and web export see the
same documents. Settings uses the same saved preferences and device services.
The foreground fallback displays a missing-package message. The installer
requires all 52 active app identities.

## Decoupling after 6.0

Independent executables complete the packaging migration. The host still
implements app-specific service handlers and owns data that multiple features
access through those handlers. The following boundaries need to move before the
core can run without knowledge of individual app behavior.

| Boundary | Current ownership | Extraction needed |
| --- | --- | --- |
| Notes, Study recall and web export | Host note records and folder/ink storage | A Notes-owned document service with migration of existing IDs and scoped access for callers. |
| Reader, Library and Store | Host book records, imports and reading sessions | App-owned library/catalog services and generic document rendering behind scoped references. |
| Settings and Home management | Host preference and app-catalog handlers | Generic device and catalog operations with the app owning presentation and workflows. |
| Content providers and archives | Host service dispatch for app-specific provider requests | Package-owned jobs and archives using the shared network and document APIs. |
| Clock and activity | Host timer, alarm and reading checkpoints | Generic scheduling and event delivery with app-owned records and policy. |

The remaining release checks cover semantic controls in all 52 apps, persistence
and rollback, device-level input and overlay behavior, and a performance
comparison on the same Kindle against 6.0.

### Work on the 6.1 contracts

The runtime supports manifest-declared operations in temporary package workers.
Study's material enumeration and Languages' written grading run in their own
packages. Home, Study and Languages handle ordinary actions through semantic
components. Calculator, Clock, Reading Statistics, Weather and Reader also
use semantic controls for their standard actions. The Calculator caret uses
a shared editor control, and Weather owns its network job, forecast parsing and
cache. Calendar reads the existing reading activity snapshot and builds its
own monthly view. Calendar’s ordinary commands, Quotes, Recipes, Wikipedia,
News and File Transfer use named actions. The input and icon definitions are
public SDK headers shared with the core.

Notes builds its folder and note lists directly from existing on-disk records. Editing, mutation, ink, and export still use the host. Study's uploads are still stored in the existing transfer directory,
and opening an uploaded document still uses a host bridge. Notes, Reader,
Settings and the other host handlers in the table still need extraction.

## Completion checks

A migration counts when the app owns its behavior in an independently built
package and its old host view and dispatch are removed. Keep the same ID,
preserve existing data, exercise the normal entry points and verify the
package is present in both the USB and wireless installers. Keep Protocol 1
apps working. Share generic components and services when extracting a
dependency, and keep app-specific state in the app.
