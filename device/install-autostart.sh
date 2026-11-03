#!/bin/sh
set -eu
base=/mnt/us/lumen
log="$base/autostart-install.log"
exec >> "$log" 2>&1
date
[ "$(id -u)" = 0 ] || { echo 'ERROR: root required'; exit 1; }
[ "$(uname -m)" = armv7l ] || { echo 'ERROR: unexpected architecture'; exit 1; }
grep -q '^Kindle 5.12.2.2 ' /etc/prettyversion.txt || { echo 'ERROR: firmware mismatch'; exit 1; }
[ -f /etc/upstart/lab126_gui.conf ] || { echo 'ERROR: lab126_gui job missing'; exit 1; }
[ -s "$base/system/lumen.conf" ] && [ -x "$base/bin/lumen" ] || exit 1
# Require a real EPUB render on this device before enabling its boot default.
"$base/bin/lumen" --inspect "/mnt/us/lumen/system/selftest.epub" --render "$base/native-selftest.pgm" > "$base/native-selftest.txt" 2>&1 || { echo 'ERROR: native EPUB self-test failed'; exit 1; }
# Collect firmware-specific event/API diagnostics once, without changing them.
{ command -v lipc-wait-event; lipc-wait-event -h; lipc-probe -a com.lab126.powerd; lipc-probe -a com.lab126.hal; } > "$base/device-api.txt" 2>&1 || true
if [ -f /etc/upstart/lumen.conf ]; then
  grep -q '^# LUMEN managed autostart v1' /etc/upstart/lumen.conf || { echo 'ERROR: unrelated existing lumen.conf'; exit 1; }
  cp /etc/upstart/lumen.conf "$base/system/lumen.conf.previous"
fi
# Preserve whether rootfs was writable when invoked.
was_ro=0
mount | grep '^/dev/root on / type .*[(,]ro[,)]' >/dev/null && was_ro=1
restore_mount() { if [ "$was_ro" = 1 ]; then mntroot ro; fi; }
trap restore_mount EXIT
trap 'exit 1' INT TERM HUP
if [ "$was_ro" = 1 ]; then mntroot rw; fi
cp "$base/system/lumen.conf" /etc/upstart/lumen.conf.tmp
chmod 644 /etc/upstart/lumen.conf.tmp
mv /etc/upstart/lumen.conf.tmp /etc/upstart/lumen.conf
sync
if [ "$was_ro" = 1 ]; then mntroot ro; fi
trap - EXIT
initctl reload-configuration
# The marker prevents the GUI restart on exiting this first session from relaunching LUMEN.
touch /tmp/lumen-boot-started
printf 'enabled\n' > "$base/autostart.enabled"
echo 'SUCCESS: autostart installed; effective at next boot.'
