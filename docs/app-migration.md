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

## Completion checks

A migration counts when the app owns its behavior in an independently built
package and its old host view and dispatch are removed. Keep the same ID,
preserve existing data, exercise the normal entry points and verify the
package is present in both the USB and wireless installers. Keep Protocol 1
apps working. Share generic components and services when extracting a
dependency, and keep app-specific state in the app.
