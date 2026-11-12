# LUMEN Kindle emulator

Double-click `Emulator.command` in Finder or run it from the project root.
It builds the native host app, links the virtual device adapter and opens
the local address in your default browser. If the host MuPDF dependency is
missing, it invokes the existing host setup. The Terminal window keeps the
server running. Press Ctrl+C there to stop it and save the emulator profile.

The canvas and PNG export are 600 × 800 grayscale pixels. Use **1:1** for
native size, **Einpassen** for small windows and **Pixelraster 3×** to inspect
individual pixels. Mouse and touch produce the same tap, swipe, drag and hold
events used by the native app. The virtual power key supports short and
long presses.

The independent profile lives under `build/emulator`. Copy EPUBs or PDFs to
`build/emulator/books` before opening the launcher to read them in the emulator.
The first launch creates a sample EPUB.

## Display timing

The emulator uses the device’s refresh planner and models DU, GL16
and GC16, including a flash for a clearing update. Default durations are
260 ms for DU and 450 ms for GL16 and GC16. DU and GC16 are approximate
[FBInk reference timings](https://github.com/NiLuJe/FBInk/blob/master/fbink.h).
GL16 uses a 450 ms modeling value. Exact durations depend on the physical
panel and waveform.

To use measured timings, pass the values in milliseconds:

```sh
./Emulator.command --du-ms 260 --gl16-ms 450 --gc16-ms 450
```

`--instant` bypasses the simulated display delay. `--no-open` prints the URL
for automated checks. The web server listens only on loopback.

The emulator runs the native LUMEN UI with virtual display and input hardware.
The Kindle firmware, physical Wi-Fi, charging, sleep latency and panel
ghosting require device testing.
