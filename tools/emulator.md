# LUMEN Kindle emulator

Open `Emulator.command` from Finder or run it from the project root.
The web interface opens in your browser and listens on localhost.
Press Ctrl+C in Terminal to stop it.

Use the **Display** dropdown to switch panels while LUMEN is running.
The presets cover 6-inch, 7-inch and 10.2-inch displays. Choose
**Benutzerdefiniert** to enter a portrait resolution and PPI.
**Einpassen** fits the panel to the window, **1:1** uses one browser pixel per
panel pixel, and **PPI-Maßstab** shows the approximate physical size based on
the browser’s 96-PPI reference. **PNG** exports the selected resolution.
The virtual power key supports short and long presses.

## Boot the Kindle ARM build

Install QEMU and supply an ARMv7 Linux kernel with QEMU `virt` board,
PL011 serial, initramfs and devtmpfs support. For example, the ARMv7
`vmlinuz-lts` image from [Alpine’s netboot directory](https://dl-cdn.alpinelinux.org/alpine/v3.23/releases/armv7/netboot/)
works with the following command:

```sh
./Emulator.command --backend arm --kernel /path/to/vmlinuz-lts
```

Place the kernel at `build/emulator/vmlinuz-armv7` to select ARM mode
automatically when QEMU is available. The launcher checks your PATH for
`qemu-system-arm` and also accepts `--qemu /path/to/qemu-system-arm`.
QEMU runs on the Mac or Linux host. The browser displays and controls the guest.
QEMU boots the existing `build/kindle/lumen` and bundled ARM app binaries.
It builds the standard Kindle target if those files are missing. Use
`--rebuild` after changing the source.

QEMU emulates an ARM CPU and boots generic Linux with a virtual display and
touchscreen. The ARM guest uses temporary storage and stays offline. Wi-Fi controls
show a simulated network. Frontlight changes affect the web preview.
The Bluetooth tab can discover and connect a simulated speaker.

On macOS, pass an already-paired speaker address to connect or disconnect
that speaker through the host:

```sh
./Emulator.command --bluetooth-speaker AA:BB:CC:DD:EE:FF
```

This option requires [blueutil](https://github.com/toy/blueutil) on PATH
and macOS Bluetooth access. Use `blueutil --paired` to find the address.
This tests the connection through the Mac radio. The guest has no audio
output path. The
`build/emulator/books` directory is read at guest startup. Exact Kindle
firmware, radio, power, and E Ink driver behavior require a physical device.
Use `--backend native` for the host build and a persistent preview profile.

## Display timing

Frames publish immediately by default. To preview E Ink refresh delays,
pass explicit durations in milliseconds. [FBInk reference timings](https://github.com/NiLuJe/FBInk/blob/master/fbink.h)

```sh
./Emulator.command --du-ms 260 --gl16-ms 450 --gc16-ms 450
```

The `--instant` flag overrides the timing flags. The `--no-open` flag prints
the URL without opening a browser. Use `--profile 2026` or
`--panel 1072x1448 --ppi 300` to set the initial display.

Preview modes use filtered scaling to avoid jagged noninteger enlargement.
The pixel grid keeps nearest-neighbor display. The guest still renders the
600 × 800 reference canvas. A PPI change cannot create new source detail.
