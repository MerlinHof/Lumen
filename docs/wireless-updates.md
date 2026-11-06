# Wireless updates

LUMEN 3.7 adds a local update flow in File Transfer. Install 3.7 over USB once
to put the update-aware launcher on the Kindle. Wireless updates replace the
LUMEN executable only. Releases that change fonts, images, the launcher or
other bundled files still use the USB installer.

1. Get a compatible `.lumen-update` file and its SHA-256 checksum from a
   trusted source. Keep the checksum on the computer or phone for comparison.
2. Start the server in File Transfer on the Kindle. Pair in the browser,
   open **Update** and upload the file.
3. Open **Wireless update** in File Transfer on the Kindle. Compare both
   displayed lines with the trusted checksum. Approve only if all 64
   characters match. The browser connection is local HTTP, and the pairing
   code controls access only. It does not authenticate software publishers.
4. Open LUMEN again to try the update. When satisfied, choose **Keep this
   version** in the same panel. To restore the previous version, exit the
   trial and open LUMEN again. The launcher also restores it after a failed
   process or if a trial ends before it is kept.

The server can only stage a bounded ARM executable in a dedicated directory.
The launcher rechecks the approved SHA-256 before swapping files and retains
one previous executable until the trial is kept or rolled back. The updater
never writes to the book or personal data directories.

## Packaging a compatible update

Build and package the release normally. With the previous installed bundle
and the target bundle unpacked, run:

```sh
python3 tools/wireless-update.py dist/LUMEN-3.7 dist/LUMEN-3.8
```

The helper refuses changes to other runtime files or the hardware target.
It produces the browser upload and a separate checksum file. Distribute the
checksum through a source the user trusts independently of the transfer
server. There is no automatic download or publisher signing in this flow.
