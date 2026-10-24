#!/bin/sh
# ==============================================================================
# Metin2 MT2009 — FreeBSD MariaDB Audit Guard, Syslog Forwarder & Spool Monitor
# Target Database: MariaDB 12.3 LTS
#
# Responsibilities:
#   1. Enforce 0750/0640 permissions on MariaDB server_audit logs and archive
#      rotated logs with FreeBSD `chflags sappnd` (system append-only protection).
#   2. Check audit log storage capacity (--check-capacity) and emit
#      [MT2_AUDIT_ALERT][CAPACITY_HIGH] when usage exceeds threshold.
#   3. Forward new audit log records to a remote TCP/UDP syslog collector
#      (--forward-once) with stateful byte-offset tracking, and emit
#      [MT2_AUDIT_ALERT][FORWARD_FAILURE] without advancing offset on failure.
#   4. Monitor game server syserr and durable SQL spool files (--check-spool-syserr)
#      for LogManager 64 MB spool overflow, disk sync failure, or poison quarantine.
# ==============================================================================

set -eu

AUDIT_LOG_PATH="${MT2_AUDIT_LOG_PATH:-/var/db/mysql/mariadb_audit.log}"
AUDIT_ARCHIVE_DIR="${MT2_AUDIT_ARCHIVE_DIR:-/var/log/mt2_audit_archive}"
AUDIT_ALERT_FILE="${MT2_AUDIT_ALERT_FILE:-/var/log/mt2_security_alerts.log}"
AUDIT_MAX_BYTES="${MT2_AUDIT_MAX_BYTES:-9437184000}" # 90 x 100 MB (~9 GB)
AUDIT_WARN_PCT="${MT2_AUDIT_WARN_PCT:-80}"
AUDIT_REMOTE_HOST="${MT2_AUDIT_REMOTE_HOST:-}"
AUDIT_REMOTE_PORT="${MT2_AUDIT_REMOTE_PORT:-514}"
SERVERFILES_DIR="${MT2_SERVERFILES_DIR:-/usr/home/mt2}"

ACTION=""

while [ "$#" -gt 0 ]; do
    case "$1" in
        --enforce-perms)
            ACTION="enforce_perms"
            shift
            ;;
        --check-capacity)
            ACTION="check_capacity"
            shift
            ;;
        --forward-once)
            ACTION="forward_once"
            shift
            ;;
        --check-spool-syserr)
            ACTION="check_spool_syserr"
            shift
            ;;
        --audit-log)
            AUDIT_LOG_PATH="$2"
            shift 2
            ;;
        --archive-dir)
            AUDIT_ARCHIVE_DIR="$2"
            shift 2
            ;;
        --alert-file)
            AUDIT_ALERT_FILE="$2"
            shift 2
            ;;
        --max-bytes)
            AUDIT_MAX_BYTES="$2"
            shift 2
            ;;
        --warn-pct)
            AUDIT_WARN_PCT="$2"
            shift 2
            ;;
        --remote-host)
            AUDIT_REMOTE_HOST="$2"
            shift 2
            ;;
        --remote-port)
            AUDIT_REMOTE_PORT="$2"
            shift 2
            ;;
        --serverfiles-dir)
            SERVERFILES_DIR="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

emit_alert() {
    alert_tag="$1"
    alert_msg="$2"
    ts=$(date -u +"%Y-%m-%dT%H:%M:%SZ" 2>/dev/null || date)
    line="$ts $alert_tag $alert_msg"
    echo "$line" >&2
    if [ -n "$AUDIT_ALERT_FILE" ]; then
        alert_dir=$(dirname "$AUDIT_ALERT_FILE")
        mkdir -p "$alert_dir" 2>/dev/null || true
        umask 077
        echo "$line" >> "$AUDIT_ALERT_FILE"
    fi
    if command -v logger >/dev/null 2>&1; then
        logger -p security.crit -t mt2_audit_guard "$alert_tag $alert_msg" 2>/dev/null || true
    fi
}

case "$ACTION" in
    enforce_perms)
        audit_dir=$(dirname "$AUDIT_LOG_PATH")
        if [ -d "$audit_dir" ]; then
            chmod 0750 "$audit_dir" 2>/dev/null || true
        fi
        for f in "$AUDIT_LOG_PATH" "$AUDIT_LOG_PATH".[0-9]*; do
            if [ -f "$f" ]; then
                chmod 0640 "$f" 2>/dev/null || true
            fi
        done
        if [ -n "$AUDIT_ARCHIVE_DIR" ]; then
            mkdir -p "$AUDIT_ARCHIVE_DIR"
            chmod 0700 "$AUDIT_ARCHIVE_DIR" 2>/dev/null || true
            for rot in "$AUDIT_LOG_PATH".[0-9]*; do
                if [ -f "$rot" ]; then
                    base_rot=$(basename "$rot")
                    dest_rot="$AUDIT_ARCHIVE_DIR/$base_rot"
                    if [ ! -f "$dest_rot" ]; then
                        cp -p "$rot" "$dest_rot"
                        chmod 0400 "$dest_rot" 2>/dev/null || true
                        if [ "$(uname -s 2>/dev/null || true)" = "FreeBSD" ] && [ "$(id -u 2>/dev/null || echo 1)" = "0" ]; then
                            chflags sappnd "$dest_rot" 2>/dev/null || true
                        fi
                    fi
                fi
            done
        fi
        echo "[OK] Audit permissions and archive protections enforced."
        ;;

    check_capacity)
        total_bytes=0
        for f in "$AUDIT_LOG_PATH" "$AUDIT_LOG_PATH".[0-9]*; do
            if [ -f "$f" ]; then
                fsize=$(wc -c < "$f" | tr -d ' \t\r\n')
                total_bytes=$((total_bytes + fsize))
            fi
        done
        threshold_bytes=$((AUDIT_MAX_BYTES * AUDIT_WARN_PCT / 100))
        if [ "$total_bytes" -ge "$threshold_bytes" ]; then
            emit_alert "[MT2_AUDIT_ALERT][CAPACITY_HIGH]" "Audit log usage ${total_bytes}B reached/exceeded threshold ${threshold_bytes}B (${AUDIT_WARN_PCT}% of ${AUDIT_MAX_BYTES}B)"
            exit 2
        fi
        echo "[OK] Audit capacity within safe bounds (${total_bytes}B < ${threshold_bytes}B)."
        ;;

    forward_once)
        if [ ! -f "$AUDIT_LOG_PATH" ]; then
            emit_alert "[MT2_AUDIT_ALERT][FORWARD_FAILURE]" "Audit log file not found at $AUDIT_LOG_PATH"
            exit 3
        fi
        if [ -z "$AUDIT_REMOTE_HOST" ]; then
            emit_alert "[MT2_AUDIT_ALERT][FORWARD_FAILURE]" "MT2_AUDIT_REMOTE_HOST is not configured"
            exit 3
        fi
        offset_file="${AUDIT_LOG_PATH}.forward.offset"
        last_offset=0
        if [ -f "$offset_file" ]; then
            last_offset=$(cat "$offset_file" | tr -d ' \t\r\n')
        fi
        cur_size=$(wc -c < "$AUDIT_LOG_PATH" | tr -d ' \t\r\n')
        if [ "$cur_size" -lt "$last_offset" ]; then
            last_offset=0
        fi
        if [ "$cur_size" -eq "$last_offset" ]; then
            echo "[OK] No new audit bytes to forward (offset=$last_offset)."
            exit 0
        fi
        if python3 -c "import sys" >/dev/null 2>&1; then
            PY_CMD="python3"
        elif python -c "import sys" >/dev/null 2>&1; then
            PY_CMD="python"
        else
            emit_alert "[MT2_AUDIT_ALERT][FORWARD_FAILURE]" "Neither python3 nor python found for TCP syslog transport"
            exit 3
        fi
        if ! "$PY_CMD" - "$AUDIT_LOG_PATH" "$last_offset" "$AUDIT_REMOTE_HOST" "$AUDIT_REMOTE_PORT" "$offset_file" <<'PYEOF'
import socket, sys

log_path, offset_str, host, port_str, offset_path = sys.argv[1:6]
offset = int(offset_str)
port = int(port_str)

with open(log_path, "rb") as f:
    f.seek(offset)
    payload = f.read()
    new_offset = f.tell()

if payload:
    with socket.create_connection((host, port), timeout=3.0) as s:
        for raw_line in payload.splitlines():
            if raw_line.strip():
                msg = b"<38>1 mt2-mariadb-audit: " + raw_line + b"\n"
                s.sendall(msg)

with open(offset_path, "w", encoding="ascii") as out_f:
    out_f.write(str(new_offset))
PYEOF
        then
            emit_alert "[MT2_AUDIT_ALERT][FORWARD_FAILURE]" "Failed forwarding audit records from $AUDIT_LOG_PATH (offset=$last_offset) to ${AUDIT_REMOTE_HOST}:${AUDIT_REMOTE_PORT}"
            exit 3
        fi
        echo "[OK] Forwarded audit records to ${AUDIT_REMOTE_HOST}:${AUDIT_REMOTE_PORT} (new_offset=$cur_size)."
        ;;

    check_spool_syserr)
        alert_count=0
        for syserr_file in "$SERVERFILES_DIR"/auth/syserr "$SERVERFILES_DIR"/chs/*/syserr; do
            if [ -f "$syserr_file" ]; then
                if grep -q "CRITICAL: LogManager SQL durable spool overflow" "$syserr_file" 2>/dev/null; then
                    emit_alert "[MT2_SPOOL_ALERT][OVERFLOW_DETECTED]" "Spool overflow detected in $syserr_file"
                    alert_count=$((alert_count + 1))
                fi
                if grep -q "AsyncSQL::SyncSpoolToDiskLocked" "$syserr_file" 2>/dev/null; then
                    emit_alert "[MT2_SPOOL_ALERT][DISK_SYNC_FAILURE]" "Spool disk sync failure detected in $syserr_file"
                    alert_count=$((alert_count + 1))
                fi
            fi
        done
        for q_file in "$SERVERFILES_DIR"/auth/sql_log_spool*.quarantine "$SERVERFILES_DIR"/chs/*/sql_log_spool*.quarantine; do
            if [ -f "$q_file" ] && [ -s "$q_file" ]; then
                emit_alert "[MT2_SPOOL_ALERT][QUARANTINE_DETECTED]" "Quarantined SQL log queries present in $q_file"
                alert_count=$((alert_count + 1))
            fi
        done
        if [ "$alert_count" -gt 0 ]; then
            exit 4
        fi
        echo "[OK] No SQL log spool overflow or quarantine alerts detected."
        ;;

    *)
        echo "Usage: $0 [--enforce-perms | --check-capacity | --forward-once | --check-spool-syserr] [options]" >&2
        exit 1
        ;;
esac
