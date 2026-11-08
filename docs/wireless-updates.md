# Apps and full system updates

## Install apps

File Transfer → Apps provides an AI development prompt, the user's live LAN
address/code and a package upload. The agent builds only the app using the SDK,
then uploads `.lumen-app`. LUMEN does not restart or rebuild. Shipped and separately
installed implementations share IDs, the catalog and installation contract.
See [App packages](app-packages.md).

## Update LUMEN from GitHub

Settings → System → Update checks the latest public GitHub release, shows the
version and last check, and installs after an explicit tap. This now downloads a
**complete LUMEN runtime**, including the main program, fonts, images, dictionaries,
launcher scripts and shipped app packages. It does not update Amazon firmware.

The client verifies the downloaded SHA-256, manifest, individual file hashes,
sizes and paths before activation. Extraction cannot write to personal data,
books, separately installed app folders or rollback control files. Symlinks and
traversal paths are rejected. Personal app state stays outside the runtime.

The installer runs from a durable helper, not the file being replaced. Before
changing anything it copies changed originals and commits a write-ahead journal.
An interrupted activation is recovered on the next launch. A successful update
is a trial: keep it in System → Update or the web interface. Otherwise the next
launch restores the previous runtime, including resources and scripts. Keeping
the update removes its rollback copy; updates do not accumulate old full bundles.

This release needs one USB install to establish the full-runtime updater. Later
compatible LUMEN releases use this GitHub path even when their assets change.
Live activation on the physical Kindle still needs observation outside USB mode.

## Publish an update

Bump VERSION and system/version.h, write docs/releases/<version>.md, commit and
push to main. The existing Native checks workflow must succeed. The new
Publish Kindle update workflow then builds and publishes the missing version's
release automatically. It uploads:

- USB ZIP and its SHA-256 sidecar;
- `.lumen-system` runtime ZIP and `.wireless.json` metadata;
- standalone SDK ZIP and the independent shipped `.lumen-app` packages.

Only a tested main-branch commit from this repository can trigger publication.
An already published version is not overwritten. You can also dispatch the
workflow manually. A source push without a new VERSION does not create a release.
