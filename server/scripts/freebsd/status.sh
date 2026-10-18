#!/bin/sh

SFROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

check_service() {
    name="$1"
    port="$2"
    dir="$3"

    line=$(sockstat -4 -l 2>/dev/null | awk -v p=":${port}" '$6 ~ p"$" {print $3; exit}')
    if [ -n "$line" ]; then
        status="LISTENING (PID $line)"
    else
        status="STOPPED"
    fi

    syserr_lines=0
    if [ -f "$dir/syserr" ]; then
        syserr_lines=$(wc -l < "$dir/syserr" | tr -d ' ')
    elif [ -f "$dir/syserr.txt" ]; then
        syserr_lines=$(wc -l < "$dir/syserr.txt" | tr -d ' ')
    fi

    printf "%-8s Port %-6s : %-26s | syserr lines: %s\n" "$name" "$port" "$status" "$syserr_lines"
}

check_service "db"   "18000" "$SFROOT/db"
check_service "auth" "17105" "$SFROOT/auth"
check_service "ch11" "28000" "$SFROOT/chs/ch11"
check_service "ch12" "28001" "$SFROOT/chs/ch12"
check_service "ch13" "28002" "$SFROOT/chs/ch13"
check_service "ch99" "13099" "$SFROOT/chs/ch99"
check_service "ch00" "13088" "$SFROOT/chs/ch00"
check_service "ch01" "13098" "$SFROOT/chs/ch01"
