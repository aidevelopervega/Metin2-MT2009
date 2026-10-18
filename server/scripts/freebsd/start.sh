#!/bin/sh
set -e

SFROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CHANNELS="${1:-1}"
USE_LUA50="${USE_LUA50:-0}"

wait_for_port() {
    port="$1"
    name="$2"
    i=0
    while [ "$i" -lt 60 ]; do
        if sockstat -4 -l 2>/dev/null | grep -q ":${port} "; then
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
