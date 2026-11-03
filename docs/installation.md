# Install LUMEN

## Supported device

The current device target is **Kindle Touch 2 (KT2), firmware 5.12.2.2,
ARMv7, 600 × 800**. Other models and firmware are not validated.

> LUMEN is an application shell on top of Kindle firmware. It does **not**
> jailbreak a stock Kindle. A compatible jailbreak, the Kindle `.sh` launcher
> and its `libkh/bin/fbink` installation must already be present.

The desktop installer checks this prerequisite before copying anything.
Do not use the package as a firmware update or copy it to the Kindle's update slot.

## First installation

1. Download **LUMEN-1.9.zip** from [Releases](https://github.com/MerlinHof/Lumen/releases/latest).
2. Extract the complete folder. Connect the prepared Kindle by USB and open its storage.
3. **macOS:** open `Install-LUMEN.command`. Alternatively, on macOS/Linux with Python 3.9+:
   ```sh
   python3 Install-LUMEN.py --kindle /path/to/mounted/Kindle
   ```
   On macOS the default is `/Volumes/Kindle`; on Windows use Python with the drive root.
4. Wait for **files copied and verified**, then safely eject the Kindle and disconnect USB.
5. Open the single **LUMEN** entry in the Kindle library. The first launch runs an EPUB
   rendering check and installs the managed autostart job on the supported firmware.
6. In **Settings**, connect Wi-Fi and choose system language, keyboard language and time zone.
   No account, server address or API key is required to read local books.

If the firmware cannot install autostart, manual launch remains available.
The diagnostic is stored in `lumen/autostart-install.log`. Do not override the
firmware guard. Autostart is enabled by default on the supported device and can
be disabled under **Settings → System → Management**.

### Defaults on a new installation

| Setting | Default |
|---|---|
| Reader text | 24 px, Charis, left aligned |
| Library | Grid; recently read first |
| System / keyboard | German / independent German QWERTZ |
| Apps tab | Enabled; icon labels hidden |
| Time | Automatic network sync; Berlin time zone |
| Status bar | Date, time, Wi-Fi and battery icon; percentage hidden |
| Covers | Cropped and rounded |
| Auto sleep | 10 minutes |
| Transfer server | Off; port 8080 when started |

Defaults come from `src/system/defaults.h`; first boot and Reset use the same values.
Existing settings are preserved during updates. The timer is visual and does not wake
the device from deep sleep.

## Updating, rollback and storage

Run the same installer. It verifies the entire payload before modifying the device,
backs up the existing runtime and verifies the copied files. Your books under
`documents` and app data under `lumen/data` are not replaced.

The two newest **recognized runtime-only backups** are retained. Older recognized
backups are removed after a successful copy. Unknown folders, backups containing
personal data, symlinks and ordinary app data are never pruned. Genuine macOS AppleDouble
metadata sidecars are recognized so they do not prevent retention on FAT storage. You can retain more:

```sh
python3 Install-LUMEN.py --keep-backups 5
python3 Install-LUMEN.py --kindle /Volumes/Kindle --storage
```

The storage command only reports runtime, personal data and backup bytes; it changes
nothing. Runtime size is printed by the packager. A reported 400 MB total may include
many old backups and cached/converted book files; it is not the application binary alone.
No image, font, photo or book quality is reduced to save space.

To roll back, exit LUMEN, mount USB storage, then restore the runtime files from the
chosen `lumen/backups/before-*` directory to the same relative paths. Keep your data folder.

## Returning to Kindle

Use LUMEN's exit command. The launcher resumes the original window manager and returns
to the Kindle home screen. To disable the boot default, use Settings or create the empty
file `lumen/DISABLE_AUTOSTART` over USB. Removing user files is not required.

## Troubleshooting

- **Not prepared:** install the compatible jailbreak and `.sh` launcher/FBInk first.
- **Web transfer unreachable:** phone and Kindle must be on the same Wi-Fi network;
  keep LUMEN awake, start Books Transfer, use the displayed address and four-character code.
- **Autostart check fails:** consult the log; the first native EPUB rendering check must pass.
- **Damaged package / insufficient space:** installer stops before replacing runtime files.
- **Protected Amazon books:** not every protected format is readable; the original Kindle
  reader remains available. The repository and release contain no personal books or keys.

The simulated fresh-install checks are documented in [Validation](validation.md).
A full physical reinstall on a freshly prepared Kindle has not been performed for 1.9.
