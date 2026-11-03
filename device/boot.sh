#!/bin/sh
[ ! -e /mnt/us/lumen/DISABLE_AUTOSTART ] || exit 0
[ -x /mnt/us/lumen/bin/lumen ] || exit 0
# launch.sh has its own process lock and always restores the original GUI.
exec /bin/sh /mnt/us/lumen/launch.sh
