#!/bin/sh
# ==============================================================================
# Metin2 MT2009 — FreeBSD MariaDB Audit Guard, Sensitive-DML Redactor,
# Rotation-Safe Syslog/TLS Forwarder & Spool/Disk Capacity Monitor (v2)
# Target Database: MariaDB 12.3 LTS
# ==============================================================================

set -eu

AUDIT_LOG_PATH="${MT2_AUDIT_LOG_PATH:-/var/db/mysql/mariadb_audit.log}"
AUDIT_ARCHIVE_DIR="${MT2_AUDIT_ARCHIVE_DIR:-/var/log/mt2_audit_archive}"
AUDIT_ARCHIVE_MAX_FILES="${MT2_AUDIT_ARCHIVE_MAX_FILES:-30}"
AUDIT_ALERT_FILE="${MT2_AUDIT_ALERT_FILE:-/var/log/mt2_security_alerts.log}"
AUDIT_MAX_BYTES="${MT2_AUDIT_MAX_BYTES:-9437184000}" # 90 x 100 MB (~9 GB)
AUDIT_WARN_PCT="${MT2_AUDIT_WARN_PCT:-80}"
MIN_FREE_DISK_MB="${MT2_MIN_FREE_DISK_MB:-1024}"
AUDIT_REMOTE_HOST="${MT2_AUDIT_REMOTE_HOST:-${MT2_AUDIT_SYSLOG_HOST:-}}"
AUDIT_REMOTE_PORT="${MT2_AUDIT_REMOTE_PORT:-${MT2_AUDIT_SYSLOG_PORT:-514}}"
AUDIT_USE_TLS="${MT2_AUDIT_USE_TLS:-0}"
AUDIT_TLS_CA_FILE="${MT2_AUDIT_TLS_CA_FILE:-}"
AUDIT_REQUIRE_ACK="${MT2_AUDIT_REQUIRE_ACK:-0}"
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
        --install-cron)
            ACTION="install_cron"
            shift
            ;;
        --cron-file)
            CRON_FILE="$2"
            shift 2
            ;;
        --audit-log)
            AUDIT_LOG_PATH="$2"
            shift 2
            ;;
        --archive-dir)
            AUDIT_ARCHIVE_DIR="$2"
            shift 2
            ;;
        --archive-max-files)
            AUDIT_ARCHIVE_MAX_FILES="$2"
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
        --min-free-mb)
            MIN_FREE_DISK_MB="$2"
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
        --tls)
            AUDIT_USE_TLS=1
            shift
            ;;
        --tls-ca-file)
            AUDIT_TLS_CA_FILE="$2"
            shift 2
            ;;
        --require-ack)
            AUDIT_REQUIRE_ACK=1
            shift
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
        if [ -d "$alert_dir" ]; then
            umask 077
            { echo "$line" >> "$AUDIT_ALERT_FILE"; } 2>/dev/null || true
        fi
    fi
    if command -v logger >/dev/null 2>&1; then
        logger -p security.crit -t mt2_audit_guard "$alert_tag $alert_msg" 2>/dev/null || true
    fi
}

find_python() {
    if python3 -c "import sys" >/dev/null 2>&1; then
        echo "python3"
    elif python -c "import sys" >/dev/null 2>&1; then
        echo "python"
    else
        echo ""
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
            PY_CMD=$(find_python)
            for rot in "$AUDIT_LOG_PATH".[0-9]*; do
                if [ -f "$rot" ]; then
                    if [ -n "$PY_CMD" ]; then
                        dest_rot=$("$PY_CMD" - "$rot" "$AUDIT_ARCHIVE_DIR" <<'PYEOF'
import hashlib, os, re, sys

rot_path, arch_dir = sys.argv[1:3]
with open(rot_path, "rb") as f:
    raw = f.read()

sha12 = hashlib.sha256(raw).hexdigest()[:12]
base_name = os.path.basename(rot_path)
dest_path = os.path.join(arch_dir, f"{base_name}.{sha12}")

SENS_COLS = {"password", "passwd", "pwd", "token", "secret", "social_id", "email", "hwid", "old_pw", "new_pw"}

def _scan_quoted_sql_token(s, pos):
    n = len(s)
    if pos >= n:
        return None
    if s.startswith("''", pos):
        i = pos + 2
        while i < n:
            if s[i] == "\\" and i + 1 < n:
                i += 2
            elif s.startswith("''''", i):
                after4 = i + 4
                if after4 == n or s[after4] in ",); \t\r\n" or (s[after4] == "'" and (after4 + 1 == n or s[after4 + 1] != "'")):
                    return after4
                i += 4
            elif s.startswith("''", i):
                return i + 2
            else:
                i += 1
        return n
    if s.startswith("\\'", pos):
        i = pos + 2
        while i < n:
            if s.startswith("\\\\", i):
                i += 2
            elif s.startswith("\\'", i):
                return i + 2
            else:
                i += 1
        return n
    if s.startswith('\\"', pos):
        i = pos + 2
        while i < n:
            if s.startswith("\\\\", i):
                i += 2
            elif s.startswith('\\"', i):
                return i + 2
            else:
                i += 1
        return n
    if s[pos] in ("'", '"'):
        q = s[pos]
        qq = q + q
        i = pos + 1
        while i < n:
            if s[i] == "\\" and i + 1 < n:
                i += 2
            elif s.startswith(qq, i):
                i += 2
            elif s[i] == q:
                return i + 1
            else:
                i += 1
        return n
    return None

def _parse_tuple_tokens(s, open_paren_pos):
    n = len(s)
    i = open_paren_pos + 1
    tokens = []
    tok_start = i
    depth = 0
    while i < n:
        q_end = _scan_quoted_sql_token(s, i)
        if q_end is not None:
            i = q_end
            continue
        ch = s[i]
        if ch == "(":
            depth += 1
        elif ch == ")":
            if depth == 0:
                tokens.append(s[tok_start:i].strip())
                return tokens, i + 1
            depth -= 1
        elif ch == "," and depth == 0:
            tokens.append(s[tok_start:i].strip())
            tok_start = i + 1
        i += 1
    return None, n

def redact_sql_text(text):
    text = re.sub(r"\$argon2id\$[^'\"\\]+", "$argon2id$[REDACTED]", text)
    text = re.sub(r"\*[0-9A-Fa-f]{40}", "*[REDACTED_SHA1]", text)
    ins_pat = re.compile(
        r"(?i)\b((?:INSERT|REPLACE)\s+(?:(?:LOW_PRIORITY|DELAYED|HIGH_PRIORITY|IGNORE)\s+)*(?:INTO\s+)?[A-Za-z0-9_.`]+\s*\(([^()]+)\)\s*VALUES\s*)"
    )
    out = []
    last_idx = 0
    pos = 0
    n = len(text)
    while pos < n:
        m = ins_pat.search(text, pos)
        if not m:
            break
        out.append(text[last_idx:m.end()])
        cols = [c.strip().strip("`'\"").lower() for c in m.group(2).split(",")]
        cur = m.end()
        while cur < n:
            ws_start = cur
            while cur < n and text[cur] in " \t\r\n":
                cur += 1
            if cur >= n or text[cur] != "(":
                cur = ws_start
                break
            out.append(text[ws_start:cur])
            tokens, next_cur = _parse_tuple_tokens(text, cur)
            if tokens is None:
                out.append(text[cur:next_cur])
                cur = next_cur
                break
            if len(tokens) == len(cols):
                for idx, col in enumerate(cols):
                    if col in SENS_COLS:
                        t_val = tokens[idx].strip()
                        if t_val.upper() != "NULL" and "$argon2id$[REDACTED]" not in t_val and "*[REDACTED_SHA1]" not in t_val:
                            tokens[idx] = "''[REDACTED]''"
            out.append("(" + ",".join(tokens) + ")")
            cur = next_cur
            comma_scan = cur
            while comma_scan < n and text[comma_scan] in " \t\r\n":
                comma_scan += 1
            if comma_scan < n and text[comma_scan] == ",":
                after_comma = comma_scan + 1
                while after_comma < n and text[after_comma] in " \t\r\n":
                    after_comma += 1
                if after_comma < n and text[after_comma] == "(":
                    out.append(text[cur:comma_scan + 1])
                    cur = comma_scan + 1
                    continue
            break
        last_idx = cur
        pos = cur
    out.append(text[last_idx:])
    text = "".join(out)

    upd_pat = re.compile(
        r"(?i)\b(`?(?:password|passwd|pwd|token|secret|social_id|email|hwid|old_pw|new_pw)`?\s*=\s*)"
    )
    out2 = []
    last_idx = 0
    pos = 0
    n = len(text)
    while pos < n:
        m = upd_pat.search(text, pos)
        if not m:
            break
        val_start = m.end()
        q_end = _scan_quoted_sql_token(text, val_start)
        if q_end is not None:
            lit = text[val_start:q_end]
            out2.append(text[last_idx:val_start])
            if "$argon2id$[REDACTED]" in lit:
                out2.append("''$argon2id$[REDACTED]''")
            elif "*[REDACTED_SHA1]" in lit:
                out2.append("''*[REDACTED_SHA1]''")
            else:
                out2.append("'***'")
            last_idx = q_end
            pos = q_end
        else:
            pos = val_start
    out2.append(text[last_idx:])
    text = "".join(out2)

    text = re.sub(r"(?:''|\\'|')[0-9]{7}(?:''|\\'|')", "''[REDACTED_SOCIAL_ID]''", text)
    text = re.sub(r"[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}", "[REDACTED_EMAIL]", text)
    return text

if not os.path.exists(dest_path):
    text = raw.decode("utf-8", errors="replace")
    text = redact_sql_text(text)
    with open(dest_path, "wb") as out_f:
        out_f.write(text.encode("utf-8"))
print(dest_path)
PYEOF
                        )
                    else
                        base_rot=$(basename "$rot")
                        dest_rot="$AUDIT_ARCHIVE_DIR/$base_rot"
                        if [ ! -f "$dest_rot" ]; then
                            cp -p "$rot" "$dest_rot"
                        fi
                    fi
                    if [ -n "$dest_rot" ] && [ -f "$dest_rot" ]; then
                        chmod 0400 "$dest_rot" 2>/dev/null || true
                        if [ "$(uname -s 2>/dev/null || true)" = "FreeBSD" ] && [ "$(id -u 2>/dev/null || echo 1)" = "0" ]; then
                            chflags sappnd "$dest_rot" 2>/dev/null || true
                        fi
                    fi
                fi
            done
            # Enforce archive retention cap (oldest pruned when exceeding AUDIT_ARCHIVE_MAX_FILES)
            arch_count=0
            for af in $(ls -1t "$AUDIT_ARCHIVE_DIR"/* 2>/dev/null || true); do
                if [ -f "$af" ]; then
                    arch_count=$((arch_count + 1))
                    if [ "$arch_count" -gt "$AUDIT_ARCHIVE_MAX_FILES" ]; then
                        if [ "$(uname -s 2>/dev/null || true)" = "FreeBSD" ] && [ "$(id -u 2>/dev/null || echo 1)" = "0" ]; then
                            chflags nosappnd "$af" 2>/dev/null || true
                        fi
                        chmod 0600 "$af" 2>/dev/null || true
                        rm -f "$af" 2>/dev/null || true
                    fi
                fi
            done
        fi
        echo "[OK] Audit permissions, sensitive-DML archive redaction, and archive retention enforced."
        ;;

    check_capacity)
        total_bytes=0
        for f in "$AUDIT_LOG_PATH" "$AUDIT_LOG_PATH".[0-9]*; do
            if [ -f "$f" ]; then
                fsize=$(wc -c < "$f" | tr -d ' \t\r\n')
                total_bytes=$((total_bytes + fsize))
            fi
        done
        if [ -d "$AUDIT_ARCHIVE_DIR" ]; then
            for af in "$AUDIT_ARCHIVE_DIR"/*; do
                if [ -f "$af" ]; then
                    afsize=$(wc -c < "$af" | tr -d ' \t\r\n')
                    total_bytes=$((total_bytes + afsize))
                fi
            done
        fi
        threshold_bytes=$((AUDIT_MAX_BYTES * AUDIT_WARN_PCT / 100))
        if [ "$total_bytes" -ge "$threshold_bytes" ]; then
            emit_alert "[MT2_AUDIT_ALERT][CAPACITY_HIGH]" "Audit log + archive usage ${total_bytes}B reached/exceeded threshold ${threshold_bytes}B (${AUDIT_WARN_PCT}% of ${AUDIT_MAX_BYTES}B)"
            exit 2
        fi
        audit_dir=$(dirname "$AUDIT_LOG_PATH")
        if [ -d "$audit_dir" ] && command -v df >/dev/null 2>&1; then
            free_mb=$(df -Pk "$audit_dir" 2>/dev/null | awk 'NR==2 {print int($4 / 1024)}' | tr -d ' \t\r\n')
            case "$free_mb" in
                ''|*[!0-9]*) ;;
                *)
                    if [ "$free_mb" -lt "$MIN_FREE_DISK_MB" ]; then
                        emit_alert "[MT2_AUDIT_ALERT][DISK_FREE_LOW]" "Available filesystem space ${free_mb}MB on $audit_dir is below minimum ${MIN_FREE_DISK_MB}MB"
                        exit 2
                    fi
                    ;;
            esac
        fi
        echo "[OK] Audit capacity and filesystem free space within safe bounds (${total_bytes}B < ${threshold_bytes}B)."
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
        PY_CMD=$(find_python)
        if [ -z "$PY_CMD" ]; then
            emit_alert "[MT2_AUDIT_ALERT][FORWARD_FAILURE]" "Neither python3 nor python found for TCP/TLS syslog transport"
            exit 3
        fi
        offset_file="${AUDIT_LOG_PATH}.forward.offset"
        if ! "$PY_CMD" - "$AUDIT_LOG_PATH" "$offset_file" "$AUDIT_REMOTE_HOST" "$AUDIT_REMOTE_PORT" "$AUDIT_USE_TLS" "$AUDIT_TLS_CA_FILE" "$AUDIT_REQUIRE_ACK" <<'PYEOF'
import hashlib, os, re, socket, ssl, sys

log_path, offset_path, host, port_str, use_tls_str, ca_file, req_ack_str = sys.argv[1:8]
port = int(port_str)
use_tls = (use_tls_str == "1")
req_ack = (req_ack_str == "1")
SENS_COLS = {"password", "passwd", "pwd", "token", "secret", "social_id", "email", "hwid", "old_pw", "new_pw"}

def _scan_quoted_sql_token(s, pos):
    n = len(s)
    if pos >= n:
        return None
    if s.startswith("''", pos):
        i = pos + 2
        while i < n:
            if s[i] == "\\" and i + 1 < n:
                i += 2
            elif s.startswith("''''", i):
                after4 = i + 4
                if after4 == n or s[after4] in ",); \t\r\n" or (s[after4] == "'" and (after4 + 1 == n or s[after4 + 1] != "'")):
                    return after4
                i += 4
            elif s.startswith("''", i):
                return i + 2
            else:
                i += 1
        return n
    if s.startswith("\\'", pos):
        i = pos + 2
        while i < n:
            if s.startswith("\\\\", i):
                i += 2
            elif s.startswith("\\'", i):
                return i + 2
            else:
                i += 1
        return n
    if s.startswith('\\"', pos):
        i = pos + 2
        while i < n:
            if s.startswith("\\\\", i):
                i += 2
            elif s.startswith('\\"', i):
                return i + 2
            else:
                i += 1
        return n
    if s[pos] in ("'", '"'):
        q = s[pos]
        qq = q + q
        i = pos + 1
        while i < n:
            if s[i] == "\\" and i + 1 < n:
                i += 2
            elif s.startswith(qq, i):
                i += 2
            elif s[i] == q:
                return i + 1
            else:
                i += 1
        return n
    return None

def _parse_tuple_tokens(s, open_paren_pos):
    n = len(s)
    i = open_paren_pos + 1
    tokens = []
    tok_start = i
    depth = 0
    while i < n:
        q_end = _scan_quoted_sql_token(s, i)
        if q_end is not None:
            i = q_end
            continue
        ch = s[i]
        if ch == "(":
            depth += 1
        elif ch == ")":
            if depth == 0:
                tokens.append(s[tok_start:i].strip())
                return tokens, i + 1
            depth -= 1
        elif ch == "," and depth == 0:
            tokens.append(s[tok_start:i].strip())
            tok_start = i + 1
        i += 1
    return None, n

def redact_sql_text(text):
    text = re.sub(r"\$argon2id\$[^'\"\\]+", "$argon2id$[REDACTED]", text)
    text = re.sub(r"\*[0-9A-Fa-f]{40}", "*[REDACTED_SHA1]", text)
    ins_pat = re.compile(
        r"(?i)\b((?:INSERT|REPLACE)\s+(?:(?:LOW_PRIORITY|DELAYED|HIGH_PRIORITY|IGNORE)\s+)*(?:INTO\s+)?[A-Za-z0-9_.`]+\s*\(([^()]+)\)\s*VALUES\s*)"
    )
    out = []
    last_idx = 0
    pos = 0
    n = len(text)
    while pos < n:
        m = ins_pat.search(text, pos)
        if not m:
            break
        out.append(text[last_idx:m.end()])
        cols = [c.strip().strip("`'\"").lower() for c in m.group(2).split(",")]
        cur = m.end()
        while cur < n:
            ws_start = cur
            while cur < n and text[cur] in " \t\r\n":
                cur += 1
            if cur >= n or text[cur] != "(":
                cur = ws_start
                break
            out.append(text[ws_start:cur])
            tokens, next_cur = _parse_tuple_tokens(text, cur)
            if tokens is None:
                out.append(text[cur:next_cur])
                cur = next_cur
                break
            if len(tokens) == len(cols):
                for idx, col in enumerate(cols):
                    if col in SENS_COLS:
                        t_val = tokens[idx].strip()
                        if t_val.upper() != "NULL" and "$argon2id$[REDACTED]" not in t_val and "*[REDACTED_SHA1]" not in t_val:
                            tokens[idx] = "''[REDACTED]''"
            out.append("(" + ",".join(tokens) + ")")
            cur = next_cur
            comma_scan = cur
            while comma_scan < n and text[comma_scan] in " \t\r\n":
                comma_scan += 1
            if comma_scan < n and text[comma_scan] == ",":
                after_comma = comma_scan + 1
                while after_comma < n and text[after_comma] in " \t\r\n":
                    after_comma += 1
                if after_comma < n and text[after_comma] == "(":
                    out.append(text[cur:comma_scan + 1])
                    cur = comma_scan + 1
                    continue
            break
        last_idx = cur
        pos = cur
    out.append(text[last_idx:])
    text = "".join(out)

    upd_pat = re.compile(
        r"(?i)\b(`?(?:password|passwd|pwd|token|secret|social_id|email|hwid|old_pw|new_pw)`?\s*=\s*)"
    )
    out2 = []
    last_idx = 0
    pos = 0
    n = len(text)
    while pos < n:
        m = upd_pat.search(text, pos)
        if not m:
            break
        val_start = m.end()
        q_end = _scan_quoted_sql_token(text, val_start)
        if q_end is not None:
            lit = text[val_start:q_end]
            out2.append(text[last_idx:val_start])
            if "$argon2id$[REDACTED]" in lit:
                out2.append("''$argon2id$[REDACTED]''")
            elif "*[REDACTED_SHA1]" in lit:
                out2.append("''*[REDACTED_SHA1]''")
            else:
                out2.append("'***'")
            last_idx = q_end
            pos = q_end
        else:
            pos = val_start
    out2.append(text[last_idx:])
    text = "".join(out2)

    text = re.sub(r"(?:''|\\'|')[0-9]{7}(?:''|\\'|')", "''[REDACTED_SOCIAL_ID]''", text)
    text = re.sub(r"[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}", "[REDACTED_EMAIL]", text)
    return text

def redact_line(raw_bytes):
    text = raw_bytes.decode("utf-8", errors="replace").rstrip("\r\n")
    return redact_sql_text(text).encode("utf-8")

def first_line_sig(path):
    if not os.path.exists(path):
        return ""
    with open(path, "rb") as f:
        first = f.readline()
    if not first.endswith(b"\n"):
        return ""
    return hashlib.sha256(first).hexdigest()[:16]

saved_sig = ""
saved_offset = 0
if os.path.exists(offset_path):
    raw_state = open(offset_path, "r", encoding="ascii", errors="ignore").read().strip()
    if "|" in raw_state:
        parts = raw_state.split("|", 1)
        saved_sig = parts[0]
        saved_offset = int(parts[1] or "0")
    elif raw_state.isdigit():
        saved_offset = int(raw_state)

cur_sig = first_line_sig(log_path)
cur_size = os.path.getsize(log_path)

# Check if log_path rotated since last run; if so, drain remaining tail of rotated .1 / .001 first
rotated_candidates = [f"{log_path}.1", f"{log_path}.001"]
segments = [] # list of (file_path, file_sig, start_offset, update_state_on_commit)

if saved_sig and cur_sig and saved_sig != cur_sig:
    for rot_cand in rotated_candidates:
        if os.path.exists(rot_cand) and first_line_sig(rot_cand) == saved_sig:
            rot_size = os.path.getsize(rot_cand)
            if rot_size > saved_offset:
                segments.append((rot_cand, saved_sig, saved_offset, False))
            break
    segments.append((log_path, cur_sig, 0, True))
elif cur_size < saved_offset:
    segments.append((log_path, cur_sig, 0, True))
else:
    segments.append((log_path, cur_sig or saved_sig, saved_offset, True))

# Count total complete newline-terminated lines to send
work_items = []
for seg_path, seg_sig, seg_start, is_active in segments:
    with open(seg_path, "rb") as f:
        f.seek(seg_start)
        pos = seg_start
        while True:
            line = f.readline()
            if not line:
                break
            if not line.endswith(b"\n"):
                # Incomplete trailing line; do not send or advance past it yet
                break
            next_pos = pos + len(line)
            work_items.append((seg_path, seg_sig, pos, next_pos, line, is_active))
            pos = next_pos

if not work_items:
    print(f"[OK] No new complete audit lines to forward (offset={saved_offset}).")
    sys.exit(0)

committed_sig = saved_sig
committed_offset = saved_offset
sent_count = 0

def save_progress(sig, off):
    tmp_p = offset_path + ".tmp"
    with open(tmp_p, "w", encoding="ascii") as out_f:
        out_f.write(f"{sig}|{off}\n")
    os.replace(tmp_p, offset_path)

try:
    raw_sock = socket.create_connection((host, port), timeout=3.0)
    if use_tls:
        ctx = ssl.create_default_context(cafile=ca_file if ca_file else None)
        if ca_file and hasattr(ssl, "VERIFY_X509_STRICT"):
            ctx.verify_flags &= ~ssl.VERIFY_X509_STRICT
        conn = ctx.wrap_socket(raw_sock, server_hostname=host)
    else:
        conn = raw_sock

    with conn:
        sock_file = conn.makefile("rb") if req_ack else None
        for seg_path, seg_sig, line_start, line_end, raw_line, is_active in work_items:
            redacted = redact_line(raw_line)
            msg = f"<38>1 mt2-mariadb-audit {seg_sig}:{line_start} ".encode("ascii") + redacted + b"\n"
            conn.sendall(msg)
            if req_ack:
                ack_line = sock_file.readline()
                if not ack_line or not (ack_line.startswith(b"ACK") or ack_line.startswith(b"OK")):
                    raise RuntimeError(f"Missing application ACK at offset {line_start}")
            committed_sig = seg_sig
            committed_offset = line_end
            sent_count += 1
        if not work_items[-1][5] and cur_sig:
            committed_sig = cur_sig
            committed_offset = 0
finally:
    if sent_count > 0:
        save_progress(committed_sig, committed_offset)

mode_note = "TLS+ACK" if (use_tls and req_ack) else ("TCP+ACK" if req_ack else "TCP socket handoff (no remote disk ACK)")
print(f"[OK] Forwarded {sent_count} redacted audit records to {host}:{port} [{mode_note}] (new_offset={committed_offset}).")
PYEOF
        then
            emit_alert "[MT2_AUDIT_ALERT][FORWARD_FAILURE]" "Failed or interrupted forwarding audit records from $AUDIT_LOG_PATH to ${AUDIT_REMOTE_HOST}:${AUDIT_REMOTE_PORT}"
            exit 3
        fi
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

    install_cron)
        target_cron="${CRON_FILE:-/usr/local/etc/cron.d/mt2_audit_guard}"
        cron_dir=$(dirname "$target_cron")
        mkdir -p "$cron_dir" 2>/dev/null || true
        umask 077
        cat > "$target_cron" <<EOF
# MT2009 MariaDB 12.3 LTS Audit Guard & Spool Monitor Schedule (every 5 minutes)
*/5 * * * * root $SERVERFILES_DIR/audit_guard.sh --enforce-perms --audit-log "$AUDIT_LOG_PATH" --archive-dir "$AUDIT_ARCHIVE_DIR" --alert-file "$AUDIT_ALERT_FILE" >/dev/null 2>&1
*/5 * * * * root $SERVERFILES_DIR/audit_guard.sh --check-capacity --audit-log "$AUDIT_LOG_PATH" --archive-dir "$AUDIT_ARCHIVE_DIR" --alert-file "$AUDIT_ALERT_FILE" >/dev/null 2>&1
*/5 * * * * root $SERVERFILES_DIR/audit_guard.sh --check-spool-syserr --serverfiles-dir "$SERVERFILES_DIR" --alert-file "$AUDIT_ALERT_FILE" >/dev/null 2>&1
EOF
        chmod 0600 "$target_cron" 2>/dev/null || true
        echo "[OK] Generated audit_guard cron schedule at $target_cron"
        ;;

    *)
        echo "Usage: $0 [--enforce-perms | --check-capacity | --forward-once | --check-spool-syserr | --install-cron] [options]" >&2
        exit 1
        ;;
esac
