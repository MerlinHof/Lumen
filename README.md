<div align="center">

# LUMEN

### A quieter kind of Kindle.

**A native, paper-like reading environment — with a little more room to think.**

[Download](https://github.com/MerlinHof/Lumen/releases/latest) ·
[Install](docs/installation.md) ·
[Build](docs/building.md) ·
[Architecture](docs/architecture.md) ·
[Agent guide](AGENTS.md)

![LUMEN library, immersive reader and app launcher](docs/images/hero.png)

`Native C` &nbsp; `Monochrome E-Ink` &nbsp; `Local first` &nbsp; `DE · EN · FR`

</div>

---

## New in 2.4

Slide 3D replaces Signal with a sliding-tile puzzle rendered at the Kindle's
native viewport resolution. Frog Solitaire is a separate peg-jumping game.
Clock offers analog and digital faces, full screen, a timer and an alarm.
Offline reading lookups include German–French, English–French and French–English
FreeDict data and a small set of original definitions.

<img src="docs/images/slide-3d.png" width="260" alt="Slide 3D native-rendered puzzle">
<img src="docs/images/frog-solitaire.png" width="260" alt="Frog Solitaire lily pads">
<img src="docs/images/clock.png" width="260" alt="Clock with the classic analog face">

## UI refinement in 2.2

Smaller system action icons with generous touch targets, repaired navigation
arrows, icons in every dropdown, better Notes list alignment and a black Timer
start action. Photos receive a slightly stronger contrast lift. Launcher icon
sizes remain unchanged.

## Design foundation in 2.1

A quieter, more confident design: semantic typography, rounded shared icons,
continuous launcher squircles, restrained black primary actions and tactile
floating notifications. Photos gain a gentle display-only contrast lift.

App sources are organized by app; shared UI and services stay in the system.
Notification close gestures cannot reach buttons underneath. Supported
Amazon books are detected automatically and prepared locally on first opening,
including metadata and covers; unsupported protection retains the Kindle-reader
fallback. The converter adds about 20 MiB and is unpacked only when needed.
Target-side conversion and the revised firmware UI pause still need hardware
verification; see [validation](docs/validation.md).

<details><summary><strong>A consistent rhythm, from system settings to your notes</strong></summary>
<br>
<table><tr><td><img src="docs/images/wifi.png" width="280" alt="Wi-Fi list with aligned status and actions"></td><td><img src="docs/images/settings.png" width="280" alt="System settings with shared list geometry"></td><td><img src="docs/images/notes-list.png" width="280" alt="Notes folders with direct item counts"></td></tr></table>
</details>

## A small computer that knows when to be quiet

LUMEN gives a Kindle a clean library, an immersive reader and a collection of
thoughtful tools. Open a book. Write something down. Look up a word. Set a study
timer. Play a small puzzle. Then return to the page.

It is built around **clear typography, recognizable shapes and immediate touch
feedback**. No endless feed, no account required for local reading, and no need
to turn a monochrome device into a tiny imitation phone.

> **Current target:** Kindle Touch 2 · firmware **5.12.2.2** · ARMv7 · 600 × 800.
> LUMEN runs on prepared Kindle firmware. It does **not** jailbreak a stock device.
> Read the [installation requirements](docs/installation.md) before downloading.

## On the page, and beyond it

| Read & explore | Write & organize | A little more |
|---|---|---|
| Library with Continue Reading | Notes, folders, Markdown and ink | Scientific calculator and graphing |
| Shared configurable reader | Multiline ToDos and contexts | Weather and Home Assistant |
| Wikipedia and saved articles | Journaling and a local calendar | Timer, photos and planetarium |
| News, recipes and quotations | Flashcards and study tools | Chess, Sokoban, Sudoku and more |

Some online features depend on their providers and require Wi-Fi. Home Assistant
and OpenAI need your own configuration. The games and local tools do not require
an account. Protected Amazon formats are not universally readable; access to the
original Kindle reader remains available.

<table>
<tr>
<td align="center" width="33%"><img src="docs/images/calculator.png" alt="Scientific calculator with typeset formula" width="270"><br><b>Think clearly.</b><br>Typeset formulas, editable input and graphing.</td>
<td align="center" width="33%"><img src="docs/images/notes.png" alt="LUMEN notes editor" width="270"><br><b>Leave a little space.</b><br>Notes, Markdown and drawings together.</td>
<td align="center" width="33%"><img src="docs/images/sokoban.png" alt="Sokoban with a worker, wooden crates and brick walls" width="270"><br><b>Play something small.</b><br>Detailed monochrome shapes, made for E-Ink.</td>
</tr>
</table>

<details>
<summary><strong>A closer look at input and settings</strong></summary>
<br>
<table><tr>
<td align="center"><img src="docs/images/keyboard.png" alt="Staggered keyboard with number row" width="300"><br>Independent keyboard language, accents on long press, cursor editing and copy/paste.</td>
<td align="center"><img src="docs/images/settings.png" alt="LUMEN settings categories" width="300"><br>Shared controls, compact selectors and one place for system preferences.</td>
</tr></table>
</details>

*Screenshots are native software renders using original demo content. They are
not hardware photographs or claims about measured E-Ink response time.*

## Install in a few steps

1. Prepare the supported Kindle with its compatible jailbreak and `.sh`
   launcher/FBInk installation.
2. Download and extract the [latest release](https://github.com/MerlinHof/Lumen/releases/latest).
3. Connect USB and run **Install-LUMEN.command** on macOS, or:
   ```sh
   python3 Install-LUMEN.py --kindle /path/to/Kindle
   ```
4. Wait for copy verification, safely eject, disconnect and open **LUMEN** once.

The first launch checks native EPUB rendering and installs autostart on the
supported firmware. Reader size starts at **24 px**; the Apps tab and automatic
time sync are enabled. Keyboard language is independent of system language.
Existing preferences and personal content survive updates.

**[Read the complete installation guide →](docs/installation.md)**

### Small runtime. Personal data stays personal.

The packaged runtime is approximately **28 MiB**; actual size is printed when
packaging. A Kindle's `lumen` folder also includes personal data, caches and old
rollback copies. The installer now retains **two recognized runtime backups**
after a successful update, while leaving unknown folders and user data alone.
No image, font or book quality is reduced.

## Build it yourself

```sh
git clone https://github.com/MerlinHof/Lumen.git
cd Lumen
python3 tools/setup.py host
python3 tools/build.py host
python3 tools/setup.py kindle
python3 tools/build.py kindle
python3 tools/check.py
python3 tools/package.py
```

The setup step fetches pinned official dependencies and verifies their checksums.
Subsequent builds are local and incremental. See [Build and develop](docs/building.md)
for prerequisites, testing, preview generation and the optional local HTTP check.

```text
Lumen/
├── compile.py        Kindle and native preview build entry point
├── lumen/
│   ├── system/       app runtime, device, shared UI, storage and image rendering
│   ├── apps/         app-owned models, endpoints and views
│   ├── assets/       dictionaries, fonts, catalogues, certificates and web UI
│   └── vendor/       embedded third-party code
├── tools/            build, packaging, installer, device scripts and tests
└── docs/             guides and native screenshots
```

Dependencies live in ignored `.local/`, outputs in `build/`, packages in `dist/`.
No compilers, copied release trees, device logs or private library reports are
part of the public source tree.

## A few honest boundaries

LUMEN is a substantial personal software project, not an official Amazon product
or a universal replacement firmware. Apps share one native process; older views
are still being separated into clearer modules. Host checks and simulated fresh
installs do not replace physical validation of sleep/wake, touch latency, battery
life or individual account integrations. See [Validation](docs/validation.md).

The calendar is local; Apple/Google cloud sync is not included. The timer is
visual and cannot wake deep sleep. Article images and available books depend on
the source. The basic language-learning and tools are intentionally small.

## Contributing with care

Start with **[AGENTS.md](AGENTS.md)** — it describes both the engineering rules and
the feeling of the project. Prefer one coherent shared component to several
slightly different implementations. Keep the reader central, preserve personal
files, and make the next action obvious on a slow monochrome screen.

## Credits & license

Developed in late 2026 by **GPT-6.1 Sol**, under the supervision, prompting and
license of **Merlin Hof**.

LUMEN is distributed under **[AGPL-3.0](LICENSE)**. Third-party notices are in
[licenses](licenses/), including MuPDF, fonts, libmobi, astronomy and scheduling
components. Home Assistant and Wikipedia identities belong to their respective
owners; no affiliation is implied.
