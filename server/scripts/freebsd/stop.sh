#!/bin/sh

SFROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROTATE_LOGS="${ROTATE_LOGS:-0}"

while [ "$#" -gt 0 ]; do
    case "$1" in
        --rotate-logs)
            ROTATE_LOGS=1
            shift 1
            ;;
        *)
            shift 1
            ;;
    esac
done

LOG_MAX_BYTES="${MT2_LOG_MAX_BYTES:-10485760}"
LOG_MAX_ARCHIVES="${MT2_LOG_MAX_ARCHIVES:-10}"

stop_pid_file() {
    pid_file="$1"
    if [ -f "$pid_file" ]; then
        pid=$(tr -d ' \r\n\t' < "$pid_file")
        if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
            kill -15 "$pid" 2>/dev/null || true
        fi
    fi
}

wait_pid_file() {
    pid_file="$1"
    timeout_iter="${2:-40}"
    if [ -f "$pid_file" ]; then
        pid=$(tr -d ' \r\n\t' < "$pid_file")
        i=0
        while [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null && [ "$i" -lt "$timeout_iter" ]; do
            sleep 0.25
            i=$((i + 1))
        done
        if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
            kill -9 "$pid" 2>/dev/null || true
        fi
        rm -f "$pid_file"
    fi
}

rotate_service_logs() {
    svc_dir="$1"
    force_rotate="$2"
    [ -d "$svc_dir" ] || return 0
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

echo "=== [1/3] Stopping AUTH Server ==="
stop_pid_file "$SFROOT/auth/pid"
wait_pid_file "$SFROOT/auth/pid" 40
rm -f "$SFROOT/auth/stop_signal"
rotate_service_logs "$SFROOT/auth" "$ROTATE_LOGS"

echo "=== [2/3] Stopping Game Channels & Flushing Player/Item/Quest State ==="
for d in "$SFROOT"/chs/*; do
    [ -d "$d" ] && stop_pid_file "$d/pid"
done
for d in "$SFROOT"/chs/*; do
    if [ -d "$d" ]; then
        wait_pid_file "$d/pid" 60
        rm -f "$d/stop_signal"
        rotate_service_logs "$d" "$ROTATE_LOGS"
    fi
done

echo "=== [3/3] Stopping DB Cache Server & Draining SQL Write Queue ==="
stop_pid_file "$SFROOT/db/pid"
wait_pid_file "$SFROOT/db/pid" 60
rm -f "$SFROOT/db/stop_signal"
rotate_service_logs "$SFROOT/db" "$ROTATE_LOGS"

echo "All MT2009 FreeBSD server processes stopped cleanly."
