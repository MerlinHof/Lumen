#!/bin/sh
export LC_ALL=en_US.UTF-8
umask 077
cd /mnt/us/lumen || exit 1
mkdir /tmp/lumen-session 2>/dev/null || exit 0
# Record ownership so unrelated processes are never signalled on cleanup.
native_pid=
paused_wm=
pillow_disabled=0

cleanup() {
    trap '' TERM
    if [ -n "$native_pid" ]; then kill -TERM "$native_pid" 2>/dev/null; wait "$native_pid" 2>/dev/null; fi
    for wm_pid in $paused_wm; do kill -CONT "$wm_pid" 2>/dev/null; done
    if [ "$pillow_disabled" = 1 ]; then lipc-set-prop com.lab126.pillow disableEnablePillow enable; fi
    lipc-set-prop -i com.lab126.powerd preventScreenSaver 0
    lipc-set-prop com.lab126.appmgrd start app://com.lab126.booklet.home >> /mnt/us/lumen/session.log 2>&1
    if [ -f /mnt/us/lumen/settings-request.txt ]; then
        rm -f /mnt/us/lumen/settings-request.txt
        tries=0
        while [ "$tries" -lt 20 ]; do
            lipc-set-prop com.lab126.appmgrd start 'app://com.lab126.booklet.settings' >> /mnt/us/lumen/session.log 2>&1 && break
            sleep 1
            tries=$((tries + 1))
        done
    fi
    if [ -s /mnt/us/lumen/reader-request.txt ]; then
        IFS= read -r reader_uri < /mnt/us/lumen/reader-request.txt || true
        rm -f /mnt/us/lumen/reader-request.txt
        case "$reader_uri" in
          app://com.lab126.booklet.reader/mnt/us/documents/*)
            tries=0
            while [ "$tries" -lt 20 ]; do
                lipc-set-prop com.lab126.appmgrd start "$reader_uri" >> /mnt/us/lumen/session.log 2>&1 && break
                sleep 1
                tries=$((tries + 1))
            done
            echo "kindle_reader_attempts=$tries" >> /mnt/us/lumen/session.log
            ;;
        esac
    fi
    rmdir /tmp/lumen-session 2>/dev/null
}
trap cleanup EXIT
trap 'exit 0' INT HUP TERM
# Bound diagnostic storage. Keep one previous session log.
if [ -f session.log ] && [ "$(wc -c < session.log)" -gt 262144 ]; then mv session.log session.previous.log; fi
exec >> /mnt/us/lumen/session.log 2>&1
echo '--- LUMEN session ---'
date
# Bounded, non-secret firmware diagnostics for the device paths used by this build.
{
    date
    command -v powerd_test wpa_cli lipc-hash-prop
    ls -ld /var/run/wpa* /var/run/wpa*/* 2>/dev/null
    lipc-get-prop com.lab126.powerd status
    lipc-get-prop com.lab126.wifid cmState
    lipc-get-prop com.lab126.cmd activeInterface
    ip -4 addr show 2>/dev/null
    iptables -S 2>/dev/null
    ps | grep -E "powerd|awesome|wifid|cvm"
} > /mnt/us/lumen/runtime-api-v5.txt 2>&1
# Copy the executable to RAM so exporting USB storage cannot truncate running code.
cp /mnt/us/lumen/bin/lumen /var/tmp/lumen-native || exit 1
chmod 700 /var/tmp/lumen-native
# Keep the firmware's power, network and application services alive.
# Pause only the drawing window manager, as used by native Kindle frontends.
lipc-set-prop com.lab126.pillow disableEnablePillow disable && pillow_disabled=1
for wm_pid in $(pidof awesome); do
    if ! grep -q '^State:.*T' "/proc/$wm_pid/status"; then
        kill -STOP "$wm_pid" && paused_wm="$paused_wm $wm_pid"
    fi
done
lipc-set-prop -i com.lab126.powerd preventScreenSaver 0
sleep 1
/var/tmp/lumen-native &
native_pid=$!
wait "$native_pid"
result=$?
native_pid=
echo "native_exit=$result"
rm -f /var/tmp/lumen-native
exit "$result"
