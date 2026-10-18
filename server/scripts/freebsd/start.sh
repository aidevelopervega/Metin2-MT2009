#!/bin/sh
set -e

SFROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CHANNELS="1"
USE_LUA50="${USE_LUA50:-0}"
ROTATE_LOGS="${ROTATE_LOGS:-0}"

while [ "$#" -gt 0 ]; do
    case "$1" in
        --rotate-logs)
            ROTATE_LOGS=1
            shift 1
            ;;
        --lua50)
            USE_LUA50=1
            shift 1
            ;;
        *)
            CHANNELS="$1"
            shift 1
            ;;
    esac
done

LOG_MAX_BYTES="${MT2_LOG_MAX_BYTES:-10485760}"
LOG_MAX_ARCHIVES="${MT2_LOG_MAX_ARCHIVES:-10}"

port_pid() {
    port="$1"
    sockstat -4 -l 2>/dev/null | awk -v p=":${port}" '$6 ~ p"$" {print $3; exit}'
}

clear_stale_state() {
    svc_name="$1"
    svc_dir="$2"
    port="$3"
    pid_file="$svc_dir/pid"

    if [ -f "$pid_file" ]; then
        pid=$(tr -d ' \r\n\t' < "$pid_file")
        if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
            echo "[ERROR] Service '$svc_name' is already running (PID $pid). Run stop.sh first." >&2
            exit 1
        fi
        rm -f "$pid_file"
    fi
    rm -f "$svc_dir/stop_signal"

    existing_pid=$(port_pid "$port")
    if [ -n "$existing_pid" ]; then
        echo "[ERROR] Port $port required by '$svc_name' is already in use by PID $existing_pid." >&2
        exit 1
    fi
}

rotate_service_logs() {
    svc_dir="$1"
    force_rotate="$2"
    archive_dir="$svc_dir/log/archive"
    mkdir -p "$archive_dir"
    stamp=$(date +%Y%m%d_%H%M%S)

    for ln in syserr syslog PTS stdout.log stderr.log; do
        lp="$svc_dir/$ln"
        if [ -f "$lp" ]; then
            fsize=$(wc -c < "$lp" | tr -d ' \t\r\n')
            if [ -n "$fsize" ] && [ "$fsize" -gt 0 ]; then
                if [ "$force_rotate" = "1" ] || [ "$fsize" -ge "$LOG_MAX_BYTES" ]; then
                    mv -f "$lp" "$archive_dir/${ln}.${stamp}.log"
                fi
            fi
        fi
        count=0
        for arch in $(ls -1t "$archive_dir/${ln}."*.log 2>/dev/null); do
            count=$((count + 1))
            if [ "$count" -gt "$LOG_MAX_ARCHIVES" ]; then
                rm -f "$arch"
            fi
        done
    done
}

wait_for_port() {
    port="$1"
    name="$2"
    i=0
    while [ "$i" -lt 60 ]; do
        if [ -n "$(port_pid "$port")" ]; then
            return 0
        fi
        sleep 0.5
        i=$((i + 1))
    done
    echo "[ERROR] Timeout waiting for ${name} on port ${port}" >&2
    return 1
}

wait_for_boot() {
    log_file="$1"
    name="$2"
    i=0
    while [ "$i" -lt 60 ]; do
        if [ -f "$log_file" ] && awk '/Start of pid:/{found=0} /LoadLocaleFile: DragonSoulTable:/{found=1} END{exit !found}' "$log_file" 2>/dev/null; then
            return 0
        fi
        sleep 0.5
        i=$((i + 1))
    done
    echo "[ERROR] Timeout waiting for ${name} CInputDB::Boot completion in ${log_file}" >&2
    return 1
}

clear_stale_state "db"   "$SFROOT/db"       18000
clear_stale_state "auth" "$SFROOT/auth"     17105
clear_stale_state "ch11" "$SFROOT/chs/ch11" 28000
clear_stale_state "ch12" "$SFROOT/chs/ch12" 28001
clear_stale_state "ch13" "$SFROOT/chs/ch13" 28002
clear_stale_state "ch99" "$SFROOT/chs/ch99" 13099
clear_stale_state "ch00" "$SFROOT/chs/ch00" 13088
clear_stale_state "ch01" "$SFROOT/chs/ch01" 13098

for d in "$SFROOT/db" "$SFROOT/auth" "$SFROOT/chs/ch11" "$SFROOT/chs/ch12" "$SFROOT/chs/ch13" "$SFROOT/chs/ch99" "$SFROOT/chs/ch00" "$SFROOT/chs/ch01"; do
    [ -d "$d" ] && rotate_service_logs "$d" "$ROTATE_LOGS"
done

echo "=== [1/3] Starting DB Cache Server (127.0.0.1:18000) ==="
cd "$SFROOT/db"
./db > stdout.log 2> stderr.log &
wait_for_port 18000 "db"
echo "  -> DB Cache Server listening on port 18000"

echo "=== [2/3] Starting Game Channels ==="
ch_list=""
c=1
while [ "$c" -le "$CHANNELS" ]; do
    ch_list="$ch_list ch${c}1 ch${c}2 ch${c}3"
    c=$((c + 1))
done
ch_list="$ch_list ch99 ch00 ch01"

for ch in $ch_list; do
    ch_dir="$SFROOT/chs/$ch"
    if [ -f "$ch_dir/CONFIG" ]; then
        cd "$ch_dir"
        if [ "$USE_LUA50" = "1" ] && [ -x "$SFROOT/share/bin/game_lua50" ]; then
            "$SFROOT/share/bin/game_lua50" > stdout.log 2> stderr.log &
        else
            "./$ch" > stdout.log 2> stderr.log &
        fi
        echo "  -> $ch launched"
        sleep 0.3
    fi
done

echo "=== [3/3] Starting AUTH Server (127.0.0.1:17105) ==="
cd "$SFROOT/auth"
if [ "$USE_LUA50" = "1" ] && [ -x "$SFROOT/share/bin/game_lua50" ]; then
    "$SFROOT/share/bin/game_lua50" > stdout.log 2> stderr.log &
else
    ./auth > stdout.log 2> stderr.log &
fi
echo "  -> auth launched"

wait_for_port 17105 "auth"
wait_for_port 28000 "ch11"
wait_for_port 28001 "ch12"
wait_for_port 28002 "ch13"
wait_for_port 13099 "ch99"
wait_for_port 13088 "ch00"
wait_for_port 13098 "ch01"

for ch in $ch_list; do
    wait_for_boot "$SFROOT/chs/$ch/syslog" "$ch"
done
echo "=== All MT2009 FreeBSD services are ONLINE ==="
