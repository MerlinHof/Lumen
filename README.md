<div align="center">

# LUMEN

### A calm personal computer for your Kindle.

An illustrated Home, a shared Reader, and useful tools for a paper screen.

[**Download 6.1**](https://github.com/MerlinHof/Lumen/releases/tag/v6.1) ·
[Release notes](docs/releases/6.1.md) ·
[Installation](docs/installation.md) ·
[Build](docs/building.md) ·
[Agent guide](AGENTS.md)

![LUMEN — native Home, library and calculator screens](docs/images/hero.png)

**Native C · 600 × 800 E-Ink · Local first · Deutsch / English / Français**

</div>

## A little room for your day

Home opens with the date, an engraved landscape and the tools you use most.
The first eight apps share the same grid as the pages that follow. Fifty
illustrated app icons are drawn as small physical objects, with deep blacks
and open white space. Classic icons remain available in Appearance.

<table>
<tr>
<td width="50%" align="center"><img src="docs/images/home.png" width="300" alt="LUMEN Home with an alpine lake, deer, date and eight engraved app icons"><br><strong>Home</strong><br>A quiet beginning, with everything within reach.</td>
<td width="50%" align="center"><img src="docs/images/apps.png" width="300" alt="A full page of the engraved app grid"><br><strong>Your tools</strong><br>The same grid continues across full app pages.</td>
</tr>
</table>

<details>
<summary><strong>See all 50 engraved icons</strong></summary>
<br>
<img src="docs/images/icons.png" alt="All 50 LUMEN engraved app icons, displayed at their native 76 by 76 pixels">
</details>

## Read, write, think

Your library has Continue Reading, covers, authors, pinned books and saved
progress. Reader preferences apply to books and the shared reading view for
articles, recipes, Markdown notes and model answers.

Notes combines folders, text and drawing. ToDo keeps flagged items at the top
and lets a checked task remain visible for the current session. The calculator
offers editable formulas, scientific functions, graphing and history.

<table>
<tr>
<td width="33%" align="center"><img src="docs/images/reader.png" width="270" alt="The shared Reader showing original demo prose"><br><strong>Reader</strong></td>
<td width="33%" align="center"><img src="docs/images/notes.png" width="270" alt="Notes with ruled text and a line-number gutter"><br><strong>Notes</strong></td>
<td width="33%" align="center"><img src="docs/images/calculator.png" width="270" alt="Calculator with a typeset formula and editable input"><br><strong>Calculator</strong></td>
</tr>
</table>

| Read & explore | Write & organize | Tools & play |
|---|---|---|
| Library, Reader and quotations | Notes, folders, Markdown and ink | Scientific calculator and graphs |
| Wikipedia and archived articles | ToDo, journal and local calendar | Clock, timer, alarms and photos |
| News and recipes | Flashcards and study tools | Weather and Home Assistant |
| Planetarium and periodic table | Basic language practice | Chess, Sudoku, Sokoban, Snake and more |

Online features depend on their providers. Home Assistant and OpenAI use your
own server or account configuration. The calendar is local. Timers provide a
visual interruption while LUMEN is awake and cannot wake the Kindle from deep sleep.

## What's new in 6.1

Notes owns folder changes and saves Study recall exercises through its own
package. Settings and more games use semantic controls. The SDK adds directory
operations, and Frog Solitaire and Sliding Puzzle have their Reader help back.

[App SDK and package guide](docs/app-packages.md) · [GitHub updates](docs/wireless-updates.md)

[Full 6.1 release notes](docs/releases/6.1.md) · [Version history](docs/changelog.md)

<table>
<tr>
<td width="33%" align="center"><img src="docs/images/todo.png" width="270" alt="ToDo with a compact segmented title bar and checkboxes"><br><strong>ToDo</strong></td>
<td width="33%" align="center"><img src="docs/images/clock.png" width="270" alt="Clock with a large analog face"><br><strong>Clock</strong></td>
<td width="33%" align="center"><img src="docs/images/sokoban.png" width="270" alt="Sokoban with a worker, crates and brick walls"><br><strong>Sokoban</strong></td>
</tr>
</table>

<details>
<summary><strong>Clock alarm, sliding puzzle and system updates</strong></summary>
<br>
<img src="docs/images/alarm.png" width="260" alt="Integrated alarm card with hour and minute controls"> <img src="docs/images/sliding-puzzle.png" width="260" alt="Direct two-dimensional fifteen-tile puzzle"> <img src="docs/images/system-update.png" width="260" alt="System Update with GitHub release check">
</details>

*These are current native software renders with original demo content.
Each individual screen is 600 × 800 pixels. They are software previews;
physical E-Ink appearance and response time depend on the Kindle.*

## Install LUMEN 6.1

Already running LUMEN? Use **Settings → System → Update** on the Kindle. The steps below cover first installation and USB recovery.

**Current hardware target — Kindle Touch 2 (KT2), firmware 5.12.2.2, ARMv7.**

A compatible jailbreak, the Kindle .sh launcher and FBInk must already be
installed. LUMEN's installer checks these prerequisites.

1. [Download LUMEN-6.1.zip](https://github.com/MerlinHof/Lumen/releases/download/v6.1/LUMEN-6.1.zip)
   and extract the complete folder.
2. Connect the prepared Kindle over USB.
3. Open **Install-LUMEN.command** on macOS, or run the Python installer with
   the mounted Kindle's path.
4. Wait for file verification, safely eject, disconnect USB and open LUMEN.

~~~sh
python3 Install-LUMEN.py --kindle /path/to/Kindle
~~~

Fresh installations use the illustrated Home, engraved icons, 24 px reading
text, automatic time sync and independent keyboard language. The status bar
shows time, Wi-Fi and a battery icon. Books and existing preferences survive updates.

The USB installer prunes recognized runtime backups and preserves books, app data and unknown content.

[Complete installation and recovery guide](docs/installation.md)

## Transfer from your phone. Build something of your own.

Open **File Transfer** on the Kindle, start the server, and open the displayed
address on a phone or computer on the same Wi-Fi. Pair with the four-character code.

Upload books and photos, export notes, or open **Install custom apps**.
That panel creates a prompt for an AI coding agent, including the live connection
details and instructions for the app runtime, shared controls, engraved icons,
translations and deployment.

Custom apps compile independently and install over Wi-Fi without rebuilding
LUMEN. Full wireless system updates can update runtime assets and the launcher.
The previous runtime is kept until the trial starts successfully.

[Custom app and wireless installation guide](docs/wireless-updates.md)

## Build and explore

~~~sh
git clone https://github.com/MerlinHof/Lumen.git
cd Lumen
python3 tools/setup.py host
python3 compile.py host
python3 tools/setup.py kindle
python3 compile.py
python3 tools/check.py
python3 tools/package.py
~~~

Dependencies are pinned and checksum-checked. Build products, device reports,
personal content and toolchains stay outside the tracked source tree.

~~~text
Lumen/
├── compile.py
├── lumen/
│   ├── apps/         app models, views, state and adapters
│   ├── system/       runtime, Reader engine, UI, input, storage and network
│   ├── assets/       artwork, fonts, dictionaries, certificates and web UI
│   └── vendor/       credited embedded dependencies
├── tools/            builds, packaging, installer, emulator and tests
└── docs/             guides, release notes and native screenshots
~~~

[Architecture](docs/architecture.md) · [Build guide](docs/building.md) ·
[Validation and limitations](docs/validation.md) · [Contributing principles](AGENTS.md)

All 52 bundled apps run as independent native processes. Some apps still use
host-side services for data or background jobs. The [6.1 release notes](docs/releases/6.1.md)
describe the remaining host dependencies and device validation limits.
The [6.2 development status](docs/releases/6.2.pending.md) tracks the UI fixes
and the remaining architecture work. Supported Amazon imports reuse locally prepared copies;
protection variants that the converter cannot handle retain the original-reader fallback.

## Credits & license

Developed in late 2026 by **GPT-6.1 Sol**, under the supervision, prompting and
license of **Merlin Hof**.

Distributed under **[AGPL-3.0](LICENSE)**. Third-party notices are in
[licenses](lumen/assets/licenses/), including MuPDF, fonts, libmobi, astronomy and scheduling
components. Artwork shown here is included in the repository. Home Assistant
and Wikipedia identities are credited in the bundled notices.
