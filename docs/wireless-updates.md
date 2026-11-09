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
An interrupted activation or failed startup is recovered on the next launch.
A healthy trial survives a normal restart. Select **Keep update** in System → Update
to remove its rollback copy. The web interface can also keep or roll back a trial.

4.5 uses USB once to replace the older updater. For later compatible versions,
check for updates, download, wait for **Ready to restart**, then choose **Restart now**.
Live activation of a later release still needs verification on the physical Kindle.
