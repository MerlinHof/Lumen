# LUMEN 4.2 · System und Apps

LUMEN ist ein natives ARMv7-Programm mit gemeinsamer App-Laufzeit. Apps sind
Quellmodule mit stabilen IDs und Routen; sie laufen im selben Prozess. Sie sind
keine voneinander isolierten Plugins. Manche ältere App-Ansichten sind noch in
gruppierten Headerfragmenten organisiert. Diese Grenze wird hier ausdrücklich
beschrieben, statt eine vollständige Prozess- oder Datenisolation zu behaupten.

## System

`lumen/system/app_runtime.{c,h}` verwaltet Aktivierung, App-Wechsel und
Lebenszyklus. `AppDefinition` beschreibt die stabile ID, den Namen und die Einstiegsroute. `lumen/apps/catalog.c` enthält die App-Registrierung.
IDs bleiben stabil, damit gespeicherte Launcherreihenfolgen nicht verrutschen.
Die zurückgezogenen Drawing-/NumScratch-IDs bleiben dafür reserviert.

`AppIntent` übergibt typisierte Nachrichten: Öffnen, Suche, Buch oder
Einstellungen. Text wird kopiert, Bücher werden durch stabile IDs referenziert,
Lesepositionen als Werte übergeben. Keine App erhält einen temporären Zeiger
auf den Suchtext einer anderen App. Der gemeinsame Reader öffnet Bücher,
Artikel, Rezepte, Antworten und Anleitungen mit derselben Darstellung.

`preferences_reader()` liefert einen Wert-Snapshot der Leseeinstellungen.
Die Reader-App bearbeitet ihre eigenen Leseeinstellungen im zentralen Modell; der Reader vergleicht
beim Anwenden den Snapshot mit seinem Layoutstand und bricht nur bei Änderungen
neu um. Der Reader liest keine Steuerelemente oder privaten Variablen der
Einstellungsansicht. Tastatursprache ist eine separate persistente Option.

`text_edit` implementiert UTF-8-sichere Einfüge-, Lösch- und Bereichsoperationen.
`text_input` ist der gemeinsame Eingabevertrag für alle Felder: Kapazität,
mehrzeilig/einzeilig, Geheimtext und Rand. Die Shell bindet ein Feld an den
Editor und liefert das Ergebnis bei Abschluss an dessen App zurück. Historische
numerische Feld-IDs sind zentral beschrieben und nicht über Apps verteilt.

Weitere Systemmodule: Dateiverwaltung und Metadaten (`library`), Textlayout und
Reader (`engine`), Display/Eingabe/Energie (`device`), Netzwerk und Dienste,
HTTP-Transfer sowie gemeinsame Schriften und Übersetzungen.

## Gemeinsame UI

`lumen/system/ui/ui.c` ist die Shell und integriert die Ansichten. Die anderen
Header in diesem Ordner liefern Widgets, Dropdowns, Checkboxen, verbundene
Tabselectoren, Icons, Statusleiste, Bilddarstellung und Spielfeldkacheln.
`text_flow.h` misst und zeichnet dieselben Zeilenbereiche für dynamische Listen.

`keyboard.h` besitzt genau ein Layout für Zeichnen und Trefferprüfung:
Zahlenzeile, versetzte DE/EN/FR-Reihen, einmalige Umschalttaste und Akzentpalette.
`text_editor.h` ist der gemeinsame mehrzeilige Eingabe-Viewport. Apps wie Notizen,
Journal, Aufgaben, Kalender und OpenAI teilen diese Implementierung.
`clipboard.h` begrenzt Auswahl und Copy/Paste auf bearbeitbare Flächen; Tasten,
Spielbretter und andere UI-Beschriftungen bieten das nicht an. Die Auswahl des
Readers für Buchmarkierungen bleibt ein eigener fachlicher Vorgang.

Während der Eingabe werden der App-Hintergrund und das unveränderte Tastenfeld
zwischengespeichert. Cursor und Text werden weiter neu gezeichnet, Trefferflächen
bleiben aktuell. Neue Eingaben werden vor wartender Darstellung verarbeitet.
Die adaptive Display-/Refresh-Engine und ihre Flächenerkennung bleiben erhalten.
Touch-down-Feedback läuft weiterhin unabhängig von der anschließenden App-Aktion.

## App-Regeln

Neue Ansichten kommen nach `lumen/apps`, wiederverwendbare UI nach `system/ui`.
Bestehende Widgets und Dienste verwenden, keine zweite Tastatur oder Kopie eines
Dropdowns einführen. Netzwerk und aufwendige Aufgaben gehören in die vorhandenen
begrenzten Hintergrundjobs. Die Statusleiste ist gemeinsames System-Chrome. Es gibt keine untere Navigationsleiste. Zustände beim Verlassen sichern; keine
synchronen Dateischreibvorgänge pro Tastendruck einführen.

## Repository and installation boundaries

`tools/device` contains target-only launcher/autostart code; `tools` host-side build and installation code. Dependencies live in ignored `.local`; generated objects are in `build/<target>/obj` and explicit source lists prevent stale objects from linking. `tools/config/runtime.json` is independent of all old releases. Shared `system/defaults` defines fresh boot, reset and option bounds. The installer validates every file first and prunes only recognized runtime-only backups after successful verification.

## Shared quality and rendering contracts

`system/ui/list.h` measures visible-ink row groups, aligns leading icons with
titles and trailing actions with rows, and draws consistent metadata and rules.
`titlebar.h` fits headings into space reserved for action columns. Form labels
share visible vertical centers with their controls; last-line truncation respects
UTF-8. Apps keep their own content, selection and existing pagination semantics.

Text rendering caches exact non-white glyph bounds and visits only the useful
area. Filled rectangles use clipped row writes. Antialiased rings reject pixels
outside the stroke before expensive angle calculations and full circles need no
angle calculation. The adaptive E-Ink refresh engine is unchanged.

Cover display tiles are bounded to eight RAM entries, at most 80,000 bytes each.
The source file identity, timestamp (including nanoseconds), length, crop,
dimensions and ordered-dither phase form the key. Rounding is applied afterward
against the current screen. No new persistent image cache or backup is created.
Notes retain fourteen first-line titles until a scan/save invalidates them;
weather parses its bounded snapshot on activation or job completion, not redraw.
Periodic status queries are coalesced; detailed render logging is opt-in.

## Ownership after the 2.1 refactor

App code now lives under `lumen/apps/<app>/`: views, state, storage and adapters
are grouped by responsibility. Calculator evaluation, news providers/archives,
flashcard scheduling and game models are beside their app APIs. System headers
live beside their system implementation. Retired NumScratch and standalone
Drawing implementations are removed; their persisted IDs remain reserved.

`apps/host/compose.h` explicitly composes the unity-build fragments. The shared
host dispatch still connects older numerical routes and globals; this remains a
single-process architecture, not independently loadable apps. That remaining
coupling is documented rather than hidden behind directory names.

Semantic text/color roles live in `system/ui/theme.h`; typography, primary actions,
icons, continuous squircles and list geometry are shared. Notifications own their
model and view in `apps/notifications`; named app endpoints accept copied requests
without navigation. Overlay input owns the whole gesture before underlying apps.

The Library delegates supported Amazon conversion to `system/amazon.c` and its
app-owned Python adapter. The ARM converter stays compressed until first use;
preparation happens in the existing loading flow, outputs are cached in personal
data, and source files remain unchanged. UI processes are reversibly paused by
`tools/device/ui-session.sh`; essential firmware services retain ownership of hardware.

## App boundaries in 2.4

The app runtime registers views and input callbacks. The shell composes app-owned views for the status bar, lock screen,
keyboard and notifications. Text fields use the keyboard endpoint and
notifications use their own endpoint. App endpoints are synchronous and copy
the requested text while handling the call. The remaining legacy route adapters
still share shell state and build in the same translation unit.

The common contrast transfer lives in `lumen/system/image.h`. It runs during
image sampling and when producing reader-page and lock-screen rasters. UI icons
have their own deliberately drawn ink coverage.

## View ownership in 2.5

The runtime owns the active route. The legacy view host reads it through a
single accessor, and standalone app pages register draw, input and optional
checkpoint callbacks against their catalog IDs. Frog Solitaire and Sliding Puzzle
compile as separate modules and use the shared drawing interface. The
app-state service handles binary saves for standalone and legacy apps and
retains the existing file format and paths.

Reader, library, store, Wi-Fi, settings, Clock, editor, notes, tasks, status bar
and Wikipedia each declare their view state in their app directories. Their
legacy adapters still compile together and can access one another's state.
Reader document lifecycle and legacy route dispatch still run in the shell.


## Home and custom apps in 3.9

Home owns the illustration, date and paged app grid. Its first eight apps are
the first entries of the same launcher list. Settings owns visibility and order.
The engraved icon atlas has a stable base row count; new app icons may be embedded
in code for an executable-only custom install.

The paired File Transfer web page supplies a development guide with live connection
details. Custom apps are compiled into LUMEN. The updater stages one executable,
checks its approved SHA-256 and retains a trial rollback. It does not dynamically
load app binaries. Non-executable runtime changes require the USB installer.

Photos uses system/photo_image for lossless raster rotation and a shared display
canvas. Selected lock-screen photos reference their source file. Notes opens rich
Markdown through the same Reader document pipeline as other text apps.

## Release updates (4.1)

`system/release_update` is a background GitHub client. It checks latest-release metadata and only stages a binary after the release resource manifest matches the installed runtime and SHA-256/size/ARM checks pass. Activation uses the existing wireless trial/rollback path. This does not make source modules independently installable. Apps, including system apps, still link into one executable. A plugin loader and stable binary ABI are not implemented.

Calculator box baselines represent a mathematical axis derived from the plus-sign ink midpoint, not the Latin glyph baseline. Keep fractions and operators on that axis. Notes translates both text and ink from the same first visible row.

## Large display typography (4.2)

`ui_display_text` in `system/unicode` renders font outlines at final pixel size, centered by actual bounds. The system UI `type_display` component keeps a bounded two-tile cache, allowing stacked clock digits without repeated font rasterization on unchanged frames. Clock chooses a visual family and size; it does not implement its own font rasterizer or imitate digital segments. Stable face IDs are distinct from menu ordering.
