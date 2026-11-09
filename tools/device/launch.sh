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
# Preserve healthy full-system trials across normal restarts.
rollback_wireless() {
    [ ! -f .update/previous ] || { mv -f .update/previous bin/lumen && sync; }
}
# Keep the original supervisor and framebuffer ownership across an update.
# Installing launch.sh replaces its directory entry, leaving this open script intact.
lipc-set-prop com.lab126.pillow disableEnablePillow disable && pillow_disabled=1
sh /mnt/us/lumen/ui-session.sh pause
lipc-set-prop -i com.lab126.powerd preventScreenSaver 0

while :; do
    rm -f .update/relaunch
    recovered=0
    if [ -f .update/system-ready ]; then
        lipc-set-prop -i com.lab126.powerd preventScreenSaver 1
    fi
    if [ -x .update/system-installer ]; then
        if [ -f .update/system-starting ]; then
            .update/system-installer --system-rollback || exit 1
            rm -f .update/system-starting
            recovered=1
        elif ! .update/system-installer --system-maintenance; then
            echo 'Update failed, recovering previous runtime'
            .update/system-installer --system-rollback || exit 1
            rm -rf .update/system-stage
            rm -f .update/system-ready .update/system-installer
            recovered=1
        fi
    fi
    if [ -f .update/system-trial ]; then
        : > .update/system-starting
    else
        lipc-set-prop -i com.lab126.powerd preventScreenSaver 0
    fi
    rollback_wireless || exit 1
    if [ -f .update/approved ]; then
        if ! bin/lumen --apply-wireless; then
            echo 'wireless update rejected'
            rm -f .update/approved .update/incoming .update/received
            rollback_wireless || exit 1
        fi
    fi
    # Keep the running executable off the USB volume.
    cp /mnt/us/lumen/bin/lumen /var/tmp/lumen-native || exit 1
    chmod 700 /var/tmp/lumen-native
    sh /mnt/us/lumen/ui-session.sh reassert
    LUMEN_UPDATE_RECOVERED="$recovered" /var/tmp/lumen-native &
    native_pid=$!
    wait "$native_pid"
    result=$?
    native_pid=
    echo "native_exit=$result"
    rm -f /var/tmp/lumen-native
    if [ "$result" != 0 ]; then
        if [ -f .update/system-starting ] && [ -x .update/system-installer ]; then
            # Leave the marker for the next pass to restore the old runtime.
            continue
        fi
        rollback_wireless
        exit "$result"
    fi
    rm -f .update/system-starting
    [ -f .update/relaunch ] || exit 0
    echo 'Continuing LUMEN session with the staged update'
done
