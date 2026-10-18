#!/bin/sh

SFROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

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
    if [ -f "$pid_file" ]; then
        pid=$(tr -d ' \r\n\t' < "$pid_file")
        i=0
        while [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null && [ "$i" -lt 30 ]; do
            sleep 0.3
            i=$((i + 1))
        done
        if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
            kill -9 "$pid" 2>/dev/null || true
        fi
        rm -f "$pid_file"
    fi
}

echo "Sending graceful SIGTERM to AUTH and Game Channels..."
stop_pid_file "$SFROOT/auth/pid"
for d in "$SFROOT"/chs/*; do
    [ -d "$d" ] && stop_pid_file "$d/pid"
done

wait_pid_file "$SFROOT/auth/pid"
for d in "$SFROOT"/chs/*; do
    [ -d "$d" ] && wait_pid_file "$d/pid"
done

echo "Sending graceful SIGTERM to DB Cache Server..."
stop_pid_file "$SFROOT/db/pid"
wait_pid_file "$SFROOT/db/pid"

echo "All MT2009 FreeBSD server processes stopped."
