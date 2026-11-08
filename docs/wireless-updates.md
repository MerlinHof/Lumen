# Custom app installation over Wi-Fi

Starting in 3.9, File Transfer's web page includes **Install custom apps**.
Open the server on the Kindle, pair from a phone or computer, and create a
prompt for an AI coding agent. The prompt includes the live address and
temporary pairing code, the installed version, the source repository, build
steps, UI conventions, icon guidance and installation endpoints.

Describe the app at the end of the prompt and give it to your agent. The agent
adds an app to the native LUMEN executable. Apps currently share a process.
This is not a sandboxed plugin system. Keep the pairing code private and
install code only from an agent or source you trust.

The agent builds a compatible executable-only package, verifies its local
SHA-256 against the upload receipt and requests installation. LUMEN saves its
state and restarts into the trial. The old executable is retained. After trying
the app, open File Transfer again and pair with its new code. Use the web panel
to keep the trial or restore the previous version. An unkept trial is restored
on the next launch, including after a reboot.

The package helper checks that every non-executable runtime file still matches
the installed bundle. If scripts, fonts or other assets change, use the USB
installer. New custom icon pixels can be embedded in the app's source.
The server only stages a bounded ARM executable and only a paired session may
request installation. Books and personal content are outside the update paths.

## For developers

Read the full generated guide in
[lumen/assets/web/custom-app-guide.txt](../lumen/assets/web/custom-app-guide.txt).

Run python3 tools/wireless-update.py with the installed and target bundle folders.

Authenticate requests with the current pairing code. PUT package bytes to
/api/custom-app, compare the returned SHA-256 with the local checksum, then POST
/api/custom-app/install?sha256=<checksum>. Status, keep and rollback use
/api/custom-app/status, /api/custom-app/keep and /api/custom-app/rollback.
The server closes during activation.

## GitHub releases (4.1)

Settings → System → Update checks the latest public release and records the last check. Install downloads the release's `.wireless.json`, compares its required resources with local files, verifies the ARM executable's SHA-256 and size, and uses the same trial restart. Keep the trial on this page or in Connect. Otherwise the next launch restores the previous binary.

Only executable-compatible updates use this path. If required scripts, fonts, images or other runtime resources differ, the UI explicitly requires USB. This is deliberate: a binary must never be installed against incompatible resources. The release publisher uploads the ZIP, ZIP checksum, `.lumen-update` and `.wireless.json` together. Personal files never enter either update asset.
