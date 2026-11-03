# LUMEN 1.9 · System und Apps

LUMEN ist ein natives ARMv7-Programm mit gemeinsamer App-Laufzeit. Apps sind
Quellmodule mit stabilen IDs und Routen; sie laufen im selben Prozess. Sie sind
keine voneinander isolierten Plugins. Manche ältere App-Ansichten sind noch in
gruppierten Headerfragmenten organisiert. Diese Grenze wird hier ausdrücklich
beschrieben, statt eine vollständige Prozess- oder Datenisolation zu behaupten.

## System

`src/system/app_runtime.{c,h}` verwaltet Aktivierung, App-Wechsel und
Lebenszyklus. `AppDefinition` beschreibt ID, Einstiegsroute, Vollbildstrategie
und zugehörigen Systemtab. `src/apps/registry.c` enthält die App-Registrierung.
IDs bleiben stabil, damit gespeicherte Launcherreihenfolgen nicht verrutschen.
Die zurückgezogenen Drawing-/NumScratch-IDs bleiben dafür reserviert.

`AppIntent` übergibt typisierte Nachrichten: Öffnen, Suche, Buch oder
Einstellungen. Text wird kopiert, Bücher werden durch stabile IDs referenziert,
Lesepositionen als Werte übergeben. Keine App erhält einen temporären Zeiger
auf den Suchtext einer anderen App. Der gemeinsame Reader öffnet Bücher,
Artikel, Rezepte, Antworten und Anleitungen mit derselben Darstellung.

`preferences_reader()` liefert einen Wert-Snapshot der Leseeinstellungen.
Die Einstellungs-App schreibt die zentralen Einstellungen; der Reader vergleicht
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

`src/system/ui/ui.c` ist die Shell und integriert die Ansichten. Die anderen
Header in diesem Ordner liefern Widgets, Dropdowns, Checkboxen, verbundene
Tabselectoren, Icons, Status-/Tabbar, Bilddarstellung und Spielfeldkacheln.
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

Neue Ansichten kommen nach `src/apps`, wiederverwendbare UI nach `system/ui`.
Bestehende Widgets und Dienste verwenden, keine zweite Tastatur oder Kopie eines
Dropdowns einführen. Netzwerk und aufwendige Aufgaben gehören in die vorhandenen
begrenzten Hintergrundjobs. Vollbild ist eine App-/Routenentscheidung, nicht eine
Eigenschaft der Status- oder Tabbar. Zustände beim Verlassen sichern; keine
synchronen Dateischreibvorgänge pro Tastendruck einführen.

## Repository and installation boundaries

`device` contains target-only launcher/autostart code; `tools` host-side build and installation code. Dependencies live in ignored `.local`; generated objects are in `build/<target>/obj` and explicit source lists prevent stale objects from linking. `packaging/runtime.json` is independent of all old releases. Shared `system/defaults` defines fresh boot, reset and option bounds. The installer validates every file first and prunes only recognized runtime-only backups after successful verification.
