#!/bin/sh
set -eu
base=/mnt/us/lumen
[ "$(id -u)" = 0 ] || exit 1
# Skip boots immediately, even if removing the job later fails.
touch "$base/DISABLE_AUTOSTART" "$base/autostart.disabled"
[ -f /etc/upstart/lumen.conf ] || exit 0
grep -q '^# LUMEN managed autostart v1' /etc/upstart/lumen.conf || exit 1
was_ro=0
mount | grep '^/dev/root on / type .*[(,]ro[,)]' >/dev/null && was_ro=1
restore_mount() { if [ "$was_ro" = 1 ]; then mntroot ro; fi; }
trap restore_mount EXIT
trap 'exit 1' INT TERM HUP
if [ "$was_ro" = 1 ]; then mntroot rw; fi
rm /etc/upstart/lumen.conf
sync
if [ "$was_ro" = 1 ]; then mntroot ro; fi
trap - EXIT
initctl reload-configuration
rm -f "$base/autostart.enabled"
