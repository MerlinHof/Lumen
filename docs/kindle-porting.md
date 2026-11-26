# Kindle compatibility and version 8 validation

LUMEN presents a 600 × 800 logical portrait surface. The display backend
fits that surface to the usable framebuffer, paints any unused border
white, and converts the refresh region to panel coordinates. Touch input
is mapped back to the same surface. Packages never receive framebuffer
dimensions or DPI. The SDK normalizes package geometry at the protocol
boundary, while the renderer preserves the reference layout and proportions
across portrait panels.

The backend probes the framebuffer and the Linux input devices at launch.
It can present 8-bit grayscale and standard packed 16-, 24-, and 32-bit
true color in monochrome. The renderer still needs a compatible FBInk
build and a portrait framebuffer. Device-specific touch rotations,
non-framebuffer displays, firmware services, CPU architecture, and
launcher behavior require validation for each device family.

Settings → Hardware always shows brightness and Bluetooth radio.
Brightness requires a usable Linux backlight control. Bluetooth requires
an identifiable radio and writable kernel radio controls. A radio
toggle controls the block state. Pairing remains a firmware function.

| Target | Status |
|---|---|
| Kindle Touch 2, 2014, firmware 5.12.2.2 | Existing supported release target. Recheck the new backend on hardware. |
| Kindle Basic, 2026, model Z8PC57 | Port candidate. Firmware, processor ABI, launch path, touch calibration and E Ink refresh still need a hardware check. |
| Other Kindles | Evaluate display, input, ABI and firmware capabilities individually. |

Amazon documents the 2026 model and its 6-inch, 300 ppi panel and
Bluetooth. [Identify your Kindle](https://digprjsurvey.amazon.com/csad/help/node/GK33S847NN4V6Y83) ·
[2026 Kindle model details](https://digprjsurvey.amazon.com/csad/help/node/TFVPjL2afO98P7Zhlr)

Before promoting a device to supported status, launch the ARM build on
its prepared firmware, verify the whole screen and touch at every corner,
exercise partial and full refreshes, sleep and wake, and test available
hardware controls. Verify installation and rollback after the runtime
checks pass. Keep the existing validated installer target until then.

On a prepared test device, run
`sh /mnt/us/probe-compatibility.sh` after copying
`tools/device/probe-compatibility.sh` to its USB storage root. The report
contains architecture, firmware, display and input capabilities.
