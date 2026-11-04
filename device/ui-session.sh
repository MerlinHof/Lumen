#!/bin/sh
# Reversible exclusive framebuffer ownership; never stop power/network/USB backends.
# The launcher owns the session directory. PID + start tick protects against reuse.
state=/tmp/lumen-session/ui-processes
identity() {
    [ -r "/proc/$1/stat" ] && awk '{print $22}' "/proc/$1/stat"
}
case "$1" in
 pause)
    : > "$state"
    for name in awesome cvm; do
        for pid in $(pidof "$name"); do
            start=$(identity "$pid")
            [ -n "$start" ] || continue
            grep -q '^State:.*T' "/proc/$pid/status" && continue
            if kill -STOP "$pid" 2>/dev/null; then printf '%s %s %s\n' "$pid" "$start" "$name" >> "$state"; fi
        done
    done
    ;;
 reassert|resume)
    [ -f "$state" ] || exit 0
    while read -r pid start name; do
        [ "$(identity "$pid")" = "$start" ] || continue
        [ "$(cat "/proc/$pid/comm" 2>/dev/null)" = "$name" ] || continue
        if [ "$1" = resume ]; then kill -CONT "$pid" 2>/dev/null
        else kill -STOP "$pid" 2>/dev/null; fi
    done < "$state"
    [ "$1" != resume ] || rm -f "$state"
    ;;
esac
exit 0
