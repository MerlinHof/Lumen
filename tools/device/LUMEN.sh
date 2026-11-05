#!/bin/sh
# Name: LUMEN
# Author: Merlin Hof
# DontUseFBInk
# Install the user-requested boot default on first launch, then detach the session.
if [ -d /tmp/lumen-session ]; then exit 0; fi
if [ ! -f /mnt/us/lumen/autostart.enabled ] && [ ! -f /mnt/us/lumen/autostart.disabled ] && [ ! -e /mnt/us/lumen/DISABLE_AUTOSTART ]; then
    if ! /bin/sh /mnt/us/lumen/install-autostart.sh; then
        /mnt/us/libkh/bin/fbink -c 'LUMEN: Autostart konnte nicht installiert werden.' 'Details: lumen/autostart-install.log'
        # Manual launch remains available if this firmware cannot install autostart.
    fi
fi
nohup setsid /bin/sh /mnt/us/lumen/launch.sh >/dev/null 2>&1 </dev/null &
exit 0
