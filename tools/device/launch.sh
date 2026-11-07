#!/bin/sh
export LC_ALL=en_US.UTF-8
umask 077
cd /mnt/us/lumen || exit 1
mkdir /tmp/lumen-session 2>/dev/null || exit 0
# Record ownership so unrelated processes are never signalled on cleanup.
native_pid=
pillow_disabled=0

cleanup() {
    trap '' TERM
    if [ -n "$native_pid" ]; then kill -TERM "$native_pid" 2>/dev/null; wait "$native_pid" 2>/dev/null; fi
    sh /mnt/us/lumen/ui-session.sh resume
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
    if [ -f /mnt/us/lumen/.update/relaunch ]; then
        rm -f /mnt/us/lumen/.update/relaunch
        nohup setsid /bin/sh /mnt/us/lumen/launch.sh >/dev/null 2>&1 </dev/null &
    fi
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
# An unkept trial is restored before the next launch, even after a power loss.
rollback_wireless() {
    [ ! -f .update/previous ] || { mv -f .update/previous bin/lumen && sync; }
}
rollback_wireless || exit 1
if [ -f .update/approved ]; then
    if ! bin/lumen --apply-wireless; then
        echo 'wireless update rejected'
        rm -f .update/approved .update/incoming .update/received
        rollback_wireless || exit 1
    fi
fi
# Copy the executable to RAM so exporting USB storage cannot truncate running code.
cp /mnt/us/lumen/bin/lumen /var/tmp/lumen-native || exit 1
chmod 700 /var/tmp/lumen-native
# Pause only UI owners, not powerd, wifid, appmgrd, D-Bus or USB services.
lipc-set-prop com.lab126.pillow disableEnablePillow disable && pillow_disabled=1
sh /mnt/us/lumen/ui-session.sh pause
lipc-set-prop -i com.lab126.powerd preventScreenSaver 0
sleep 1
/var/tmp/lumen-native &
native_pid=$!
wait "$native_pid"
result=$?
native_pid=
echo "native_exit=$result"
[ "$result" = 0 ] || rollback_wireless
rm -f /var/tmp/lumen-native
exit "$result"
