#!/bin/sh
# Run from a shell on a prepared Kindle. This reports capabilities without
# recording serial numbers, account details, IP addresses, or file paths.
echo "kernel architecture: $(uname -m)"
if [ -r /etc/prettyversion.txt ]; then
    printf 'firmware: '
    head -n 1 /etc/prettyversion.txt
fi
for name in name virtual_size bits_per_pixel modes; do
    path="/sys/class/graphics/fb0/$name"
    if [ -r "$path" ]; then printf 'fb0 %s: ' "$name"; head -n 1 "$path"; fi
done
for event in /sys/class/input/event*; do
    if [ -r "$event/device/name" ]; then
        printf '%s: ' "${event##*/}"
        head -n 1 "$event/device/name"
    fi
done
[ -x /mnt/us/libkh/bin/fbink ] && echo 'FBInk launcher: available'
[ -w /dev/rfkill ] && echo 'Kernel radio control: available'
for light in /sys/class/backlight/*/brightness; do
    if [ -w "$light" ]; then echo 'Backlight control: available'; break; fi
done
