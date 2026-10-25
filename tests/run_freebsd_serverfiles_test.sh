#!/bin/sh
# ==============================================================================
# Metin2 MT2009 - Rev 51: Real FreeBSD 15.1 x64 Serverfiles Installation,
# Exclusive Lua 5.5.1 Engine & 202 Active Quest Compilation Verification,
# Least-Privilege SQL & chmod 600 Config Security, Full 8-Service Boot,
# Cross-Core CMapLocation & Portal 10002 WarpSet (ch11 -> ch12 -> ch11),
# 3-Phase Ordered Graceful Shutdown, Log Rotation & Zero-syserr Verification
# ==============================================================================

set -e

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ROOT_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
BUILD_DIR="$ROOT_DIR/tests/build"
CXX=${CXX:-/usr/local/bin/g++14}

HOST="${MT2_TEST_HOST:-127.0.0.1}"
PORT="${MT2_TEST_PORT:-3307}"
USER="${MT2_RUNTIME_USER:-${MT2_TEST_USER:-mt2test}}"
PASS="${MT2_RUNTIME_PASSWORD:-${MT2_TEST_PASSWORD:-}}"
ENGINE="${MT2_TEST_ENGINE:-FreeBSD SQL}"
ENGINE_ID="${ENGINE_ID:-freebsd_sql}"

DB_BIN="${MT2_SMOKE_DB_BIN:-$ROOT_DIR/server/src/db/db_r41023}"
GAME55_BIN="${MT2_SMOKE_GAME55_BIN:-$ROOT_DIR/server/src/game/game_r41023}"
QC_BIN="${MT2_SMOKE_QC_BIN:-$ROOT_DIR/build/freebsd-gcc14/server/src/game/src/quest/qc}"
if [ ! -x "$QC_BIN" ] && [ -x "$ROOT_DIR/build/freebsd-gcc/server/src/game/src/quest/qc" ]; then
    QC_BIN="$ROOT_DIR/build/freebsd-gcc/server/src/game/src/quest/qc"
elif [ ! -x "$QC_BIN" ] && [ -x "$ROOT_DIR/server/src/game/src/quest/qc" ]; then
    QC_BIN="$ROOT_DIR/server/src/game/src/quest/qc"
fi
SF_ARCHIVE="$ROOT_DIR/server/serverfiles_share.tar.xz"
SFROOT="/usr/home/mt2"
ARTIFACT_DIR="$ROOT_DIR/artifacts/freebsd_serverfiles_logs_${ENGINE_ID}"

SMOKE_HWID_PLAIN="SMOKETEST_HWID1"
SMOKE_HWID_HEX="5BE8C5A4CAFB1014505B16C5D68D28A9"
SMOKE_TEST_LOGIN="bsd64hero"
SMOKE_TEST_PASS="Bsd64Hero!2026"
export SMOKE_TEST_LOGIN SMOKE_TEST_PASS

echo "=================================================================="
echo " METIN2 MT2009 - REV 51 REAL FREEBSD 15.1 SERVERFILES VERIFICATION"
echo " Engine:         $ENGINE ($HOST:$PORT)"
echo " Target Root:    $SFROOT"
echo " Artifact Dir:   $ARTIFACT_DIR"
echo "=================================================================="

if [ -z "$PASS" ]; then
    echo "[ERROR] MT2_RUNTIME_PASSWORD / MT2_TEST_PASSWORD is not set!" >&2
    exit 1
fi

for bin in "$DB_BIN" "$GAME55_BIN" "$QC_BIN"; do
    if [ ! -x "$bin" ]; then
        echo "[ERROR] Missing or non-executable binary: $bin" >&2
        exit 1
    fi
done

if [ ! -f "$SF_ARCHIVE" ]; then
    echo "[ERROR] Missing serverfiles archive: $SF_ARCHIVE" >&2
    exit 1
fi

mkdir -p "$BUILD_DIR"
RPATH=""
if [ -d "/usr/local/lib/gcc14" ]; then
    RPATH="-Wl,-rpath=/usr/local/lib/gcc14"
fi
$CXX -std=c++20 -Wall -Wextra -Werror -O2 $RPATH \
    "$ROOT_DIR/tests/test_server_smoke_client.cpp" \
    -o "$BUILD_DIR/test_server_smoke_client"

DB_CLI=$(which mariadb 2>/dev/null || which mysql 2>/dev/null)
if [ -z "$DB_CLI" ]; then
    echo "[ERROR] mariadb or mysql CLI client not found!" >&2
    exit 1
fi

collect_logs() {
    mkdir -p "$ARTIFACT_DIR"
    for svc_dir in "$SFROOT/db" "$SFROOT/auth" "$SFROOT/chs/ch11" "$SFROOT/chs/ch12" "$SFROOT/chs/ch13" "$SFROOT/chs/ch99" "$SFROOT/chs/ch00" "$SFROOT/chs/ch01"; do
        if [ -d "$svc_dir" ]; then
            svc_name=$(basename "$svc_dir")
            out_sub="$ARTIFACT_DIR/$svc_name"
            mkdir -p "$out_sub"
            for f in syserr syslog PTS stdout.log stderr.log; do
                if [ -f "$svc_dir/$f" ]; then
                    cp -f "$svc_dir/$f" "$out_sub/$f"
                fi
            done
        fi
    done
}

cleanup() {
    rc=$?
    set +e
    rm -f "$SFROOT/.test_client.cnf"
    if [ -x "$SFROOT/stop.sh" ]; then
        "$SFROOT/stop.sh" >/dev/null 2>&1 || true
    fi
    collect_logs
    exit "$rc"
}
trap cleanup EXIT INT TERM

echo "=== [1/7] Installing Real Serverfiles & 5 Schemas via server/scripts/freebsd/install.sh ==="
rm -rf "$SFROOT" "$ARTIFACT_DIR"
mkdir -p "$ARTIFACT_DIR"
chmod +x "$ROOT_DIR/server/scripts/freebsd/install.sh" \
         "$ROOT_DIR/server/scripts/freebsd/start.sh" \
         "$ROOT_DIR/server/scripts/freebsd/stop.sh" \
         "$ROOT_DIR/server/scripts/freebsd/status.sh" \
         "$ROOT_DIR/server/scripts/freebsd/mt2server.rc"

RUNTIME_ADMIN_PASS="${MT2_ADMINPAGE_PASSWORD:-$(openssl rand -hex 12)}"
RUNTIME_SERVER_KEY="${MT2_SERVER_KEY:-$(openssl rand -base64 48 | tr -d '\r\n')}"

MT2_DB_HOST="$HOST" \
MT2_DB_PORT="$PORT" \
MT2_DB_USER="$USER" \
MT2_DB_PASS="$PASS" \
MT2_LOG_DB_USER="${MT2_LOG_DB_USER:-$USER}" \
MT2_LOG_DB_PASS="${MT2_LOG_DB_PASS:-$PASS}" \
MT2_INIT_DB_USER="${MT2_TEST_USER:-$USER}" \
MT2_INIT_DB_PASS="${MT2_TEST_PASSWORD:-$PASS}" \
MT2_ADMINPAGE_PASSWORD="$RUNTIME_ADMIN_PASS" \
MT2_SERVER_KEY="$RUNTIME_SERVER_KEY" \
"$ROOT_DIR/server/scripts/freebsd/install.sh" --target-dir "$SFROOT" --init-sql

for cfg_file in "$SFROOT/db/conf.txt" "$SFROOT/auth/CONFIG" "$SFROOT/chs/ch11/CONFIG" "$SFROOT/chs/ch12/CONFIG" "$SFROOT/chs/ch13/CONFIG" "$SFROOT/chs/ch99/CONFIG" "$SFROOT/chs/ch00/CONFIG" "$SFROOT/chs/ch01/CONFIG"; do
    mode_oct=$(stat -f "%Lp" "$cfg_file")
    if [ "$mode_oct" != "600" ]; then
        echo "[ERROR] Expected chmod 600 on $cfg_file, got $mode_oct" >&2
        exit 1
    fi
done
echo "  -> Verified chmod 600 permissions on db/conf.txt, auth/CONFIG, and chs/*/CONFIG"

if [ ! -x "$SFROOT/share/locale/poland/quest/qc" ] || [ -e "$SFROOT/share/locale/poland/quest/qc.exe" ]; then
    echo "[ERROR] Expected ELF64 qc (and no qc.exe) in $SFROOT/share/locale/poland/quest!" >&2
    exit 1
fi
STATE_COUNT=$(find "$SFROOT/share/locale/poland/quest/object/state" -type f | wc -l | tr -d ' ')
OBJ_COUNT=$(find "$SFROOT/share/locale/poland/quest/object" -type f | wc -l | tr -d ' ')
echo "  -> Unpacked quest object counts: state_tables=$STATE_COUNT, total_object_files=$OBJ_COUNT"
if [ "$STATE_COUNT" -ne 202 ] || [ "$OBJ_COUNT" -ne 6900 ]; then
    echo "[ERROR] Expected 202 state tables and 6900 object files, got state=$STATE_COUNT object=$OBJ_COUNT" >&2
    exit 1
fi
echo "  -> Verifying all 202 active quests in $SFROOT/share/locale/poland/quest with 64-bit Lua 5.5.1 qc..."
if command -v python3 >/dev/null 2>&1; then
    echo "  -> FreeBSD Python Runtime & Default Encoding:"
    python3 -c 'import sys, locale; print("     Python:", sys.version.splitlines()[0], "| defaultencoding=" + sys.getdefaultencoding(), "| preferredencoding=" + locale.getpreferredencoding(False), "| filesystemencoding=" + sys.getfilesystemencoding())'
    python3 "$ROOT_DIR/tests/run_quest_matrix.py" \
        --quest-dir "$SFROOT/share/locale/poland/quest" \
        --qc "$SFROOT/share/locale/poland/quest/qc" \
        --force \
        --clean
else
    QC_WORK="/tmp/mt2_qc_verify_$$"
    rm -rf "$QC_WORK"
    mkdir -p "$QC_WORK/object"
    cp -f "$SFROOT/share/locale/poland/quest/quest_functions" "$QC_WORK/quest_functions"
    PASS_Q=0
    for qfile in $(find "$SFROOT/share/locale/poland/quest/quest" -type f \( -name "*.quest" -o -name "*.lua" \) | grep -v "/_unused/" | sort); do
        qbase=$(basename "$qfile")
        if [ -f "$SFROOT/share/locale/poland/quest/pre_qc/$qbase" ]; then
            qtarget="$SFROOT/share/locale/poland/quest/pre_qc/$qbase"
        else
            qtarget="$qfile"
        fi
        (cd "$QC_WORK" && "$SFROOT/share/locale/poland/quest/qc" "$qtarget" >/dev/null)
        PASS_Q=$((PASS_Q + 1))
    done
    REBUILT_STATES=$(find "$QC_WORK/object/state" -type f | wc -l | tr -d ' ')
    REBUILT_OBJS=$(find "$QC_WORK/object" -type f | wc -l | tr -d ' ')
    rm -rf "$QC_WORK"
    echo "  -> POSIX sh 64-bit qc matrix result: PASS=$PASS_Q/202, rebuilt_states=$REBUILT_STATES, rebuilt_objects=$REBUILT_OBJS"
    if [ "$PASS_Q" -ne 202 ] || [ "$REBUILT_STATES" -ne 202 ] || [ "$REBUILT_OBJS" -ne 6900 ]; then
        echo "[ERROR] FreeBSD qc verification mismatch!" >&2
        exit 1
    fi
fi
echo "  -> Running game_r41023 --check-lua against unpacked /usr/home/mt2/share/locale/poland..."
"$GAME55_BIN" --check-lua

assert_zero_syserr() {
    stage_label="$1"
    err_found=0
    for svc_dir in "$SFROOT/db" "$SFROOT/auth" "$SFROOT/chs/ch11" "$SFROOT/chs/ch12" "$SFROOT/chs/ch13" "$SFROOT/chs/ch99" "$SFROOT/chs/ch00" "$SFROOT/chs/ch01"; do
        svc_name=$(basename "$svc_dir")
        if [ -s "$svc_dir/syserr" ]; then
            echo "[ERROR] Non-empty syserr in $svc_name during $stage_label:" >&2
            cat "$svc_dir/syserr" >&2
            err_found=1
        fi
    done
    if [ "$err_found" -ne 0 ]; then
        exit 1
    fi
}

echo "=== [2/7] Starting All 8 FreeBSD Serverfiles Services (db, auth, ch11..ch01) ==="
"$SFROOT/start.sh"
sleep 2
"$SFROOT/status.sh" | tee "$ARTIFACT_DIR/status_initial_boot.txt"
if grep -q "STOPPED" "$ARTIFACT_DIR/status_initial_boot.txt"; then
    echo "[ERROR] One or more services failed to enter LISTENING state!" >&2
    exit 1
fi
assert_zero_syserr "initial_boot"

echo "=== [3/7] Creating Fresh Argon2id Account & Entering PHASE_GAME on ch11 ==="
CREATE_OUT=$(cd "$SFROOT/db" && printf '%s\n' "$SMOKE_TEST_PASS" | ./db --create-account "$SMOKE_TEST_LOGIN" "1234567" "$SMOKE_HWID_PLAIN")
echo "  -> $CREATE_OUT"
ACCOUNT_ID=$(printf '%s\n' "$CREATE_OUT" | sed -n 's/.*id=\([0-9][0-9]*\).*/\1/p')
if [ -z "$ACCOUNT_ID" ]; then
    echo "[ERROR] Failed to parse ACCOUNT_ID from --create-account output!" >&2
    exit 1
fi

KEY_FILE="$ARTIFACT_DIR/login_key_1.txt"
"$BUILD_DIR/test_server_smoke_client" --auth-login 17105 "@ENV" "@ENV" "$SMOKE_HWID_HEX" "$KEY_FILE"
LOGIN_KEY=$(tr -d ' \r\n\t' < "$KEY_FILE")
rm -f "$KEY_FILE"
if [ -z "$LOGIN_KEY" ]; then
    echo "[ERROR] Failed to obtain LOGIN_KEY from AUTH server!" >&2
    exit 1
fi

SMOKE_REAL_SERVERFILES=1 "$BUILD_DIR/test_server_smoke_client" --game-login-key 28000 "@ENV" "$LOGIN_KEY"
assert_zero_syserr "after_first_login_phase_game"

echo "=== [4/7] 3-Phase Ordered Graceful Stop & Log Rotation Verification ==="
"$SFROOT/stop.sh" --rotate-logs
"$SFROOT/status.sh" | tee "$ARTIFACT_DIR/status_after_stop.txt"
if grep -q "LISTENING" "$ARTIFACT_DIR/status_after_stop.txt"; then
    echo "[ERROR] One or more services remained LISTENING after stop.sh!" >&2
    exit 1
fi
assert_zero_syserr "after_graceful_stop"

ROTATED_COUNT=$(find "$SFROOT" -path "*/log/archive/*.log" -type f | wc -l | tr -d ' ')
echo "  -> Verified log rotation into log/archive ($ROTATED_COUNT archived log files)"
if [ "$ROTATED_COUNT" -lt 8 ]; then
    echo "[ERROR] Expected rotated log archives in */log/archive, found only $ROTATED_COUNT" >&2
    exit 1
fi

echo "=== [5/7] Verifying SQL Quest Persistence & Placing Character on map_index=3 (ch12) ==="
TEST_CNF="$SFROOT/.test_client.cnf"
umask 077
cat > "$TEST_CNF" <<EOF
[client]
host=$HOST
port=$PORT
user=$USER
password="$PASS"
EOF
chmod 600 "$TEST_CNF"
QUEST_ROW_COUNT=$("$DB_CLI" "--defaults-extra-file=$TEST_CNF" -N -s player \
    -e "SELECT COUNT(*) FROM quest WHERE dwPID=(SELECT id FROM player WHERE name='SmokeHero' LIMIT 1);")
echo "  -> Verified persisted quest rows in player.quest for SmokeHero: $QUEST_ROW_COUNT"
if [ -z "$QUEST_ROW_COUNT" ] || [ "$QUEST_ROW_COUNT" -lt 1 ]; then
    echo "[ERROR] Expected persisted quest rows in player.quest for SmokeHero, got $QUEST_ROW_COUNT" >&2
    rm -f "$TEST_CNF"
    exit 1
fi
"$DB_CLI" "--defaults-extra-file=$TEST_CNF" player \
    -e "UPDATE player SET map_index=3, x=401600, y=899600, exit_map_index=3, exit_x=401600, exit_y=899600 WHERE name='SmokeHero';"
rm -f "$TEST_CNF"

"$SFROOT/start.sh"
sleep 2
"$SFROOT/status.sh" | tee "$ARTIFACT_DIR/status_after_restart.txt"
if grep -q "STOPPED" "$ARTIFACT_DIR/status_after_restart.txt"; then
    echo "[ERROR] One or more services failed to restart!" >&2
    exit 1
fi

echo "=== [6/7] Verifying Cross-Core CMapLocation (ch11->ch12), ch12 PHASE_GAME, & Portal 10002 WarpSet (ch12->ch11) ==="
KEY_FILE2="$ARTIFACT_DIR/login_key_2.txt"
"$BUILD_DIR/test_server_smoke_client" --auth-login 17105 "@ENV" "@ENV" "$SMOKE_HWID_HEX" "$KEY_FILE2"
LOGIN_KEY2=$(tr -d ' \r\n\t' < "$KEY_FILE2")
rm -f "$KEY_FILE2"
if [ -z "$LOGIN_KEY2" ]; then
    echo "[ERROR] Failed to obtain second LOGIN_KEY after restart!" >&2
    exit 1
fi

"$BUILD_DIR/test_server_smoke_client" --game-cross-core-warp 28000 28001 "@ENV" "$LOGIN_KEY2"
assert_zero_syserr "after_cross_core_warp"

echo "=== [7/7] Final Graceful Shutdown & Secret Leak Audit ==="
"$SFROOT/stop.sh"
"$SFROOT/status.sh" | tee "$ARTIFACT_DIR/status_final.txt"
assert_zero_syserr "final_shutdown"
collect_logs

if grep -rnF "$PASS" "$ARTIFACT_DIR" >/dev/null 2>&1 || grep -rnF "$RUNTIME_ADMIN_PASS" "$ARTIFACT_DIR" >/dev/null 2>&1; then
    echo "[ERROR] Plaintext SQL password or ADMINPAGE_PASSWORD leaked into collected logs!" >&2
    exit 1
fi
echo "  -> Verified 0 secret leaks in collected logs and artifacts"

if [ -n "${MT2_BACKUP_DB_USER:-}" ] && [ -n "${MT2_BACKUP_DB_PASS:-}" ]; then
    echo "=== [Bonus 4.A] Verifying FreeBSD MariaDB Socket Isolation (sockstat 127.0.0.1:$PORT, 0 external 3306) ==="
    if command -v sockstat >/dev/null 2>&1; then
        SOCK_OUT=$(sockstat -4 -6 -l | grep -E "mariadbd|mysqld|:$PORT|:3306" || true)
        echo "$SOCK_OUT"
        if printf '%s\n' "$SOCK_OUT" | grep -E "\*:$PORT|0\.0\.0\.0:$PORT|::$PORT|\*:3306|0\.0\.0\.0:3306" >/dev/null 2>&1; then
            echo "[ERROR] MariaDB or unexpected service is bound to wildcard/external interface!" >&2
            exit 1
        fi
        if ! printf '%s\n' "$SOCK_OUT" | grep -F "127.0.0.1:$PORT" >/dev/null 2>&1; then
            echo "[ERROR] Expected mariadbd listening on 127.0.0.1:$PORT in sockstat output!" >&2
            exit 1
        fi
        echo "  -> Verified via FreeBSD sockstat: mariadbd bound strictly to 127.0.0.1:$PORT (0 external 3306 listeners)"
    fi

    if [ -n "${MT2_AUDIT_LOG_FILE:-}" ] && [ -f "$MT2_AUDIT_LOG_FILE" ]; then
        echo "=== [Bonus 4.C] Verifying Real FreeBSD MariaDB server_audit Log (CONNECT, DDL, DCL, DML_NO_SELECT, Masking & Rotation) ==="
        AUDIT_LINES=$(wc -l < "$MT2_AUDIT_LOG_FILE" | tr -d ' \t\r\n')
        AUDIT_BYTES=$(wc -c < "$MT2_AUDIT_LOG_FILE" | tr -d ' \t\r\n')
        echo "  -> Live FreeBSD mariadb_audit.log stats: lines=$AUDIT_LINES, bytes=$AUDIT_BYTES"
        grep -q "ransom_attacker_unauth,.*FAILED_CONNECT,.*,1045" "$MT2_AUDIT_LOG_FILE"
        grep -q "mt2server,.*QUERY,mt2_testdb,'CREATE USER .* IDENTIFIED BY \*\*\*\*\*',1227" "$MT2_AUDIT_LOG_FILE"
        grep -q "mt2server,.*QUERY,mt2_testdb,'CREATE DATABASE z_ransom_note_test',1044" "$MT2_AUDIT_LOG_FILE"
        grep -q "mt2server,.*QUERY,mt2_testdb,'DROP DATABASE mt2_testdb',1044" "$MT2_AUDIT_LOG_FILE"
        grep -q "mt2server,.*QUERY,mt2_testdb,'DROP TABLE test_player_live',1142" "$MT2_AUDIT_LOG_FILE"
        grep -q "mt2server,.*QUERY,mt2_testdb,'UPDATE test_player_live SET gold = 56000 WHERE id = 99901',0" "$MT2_AUDIT_LOG_FILE"
        grep -q "mt2server,.*QUERY,mt2_testdb,'DELETE FROM test_player_live WHERE id = 99901',0" "$MT2_AUDIT_LOG_FILE"
        grep -Eq "mt2server,.*QUERY,mt2_testdb,'DELETE FROM log\.test_log_isolation WHERE id = 1',(1142|1044)" "$MT2_AUDIT_LOG_FILE"
        grep -q "mt2server,.*QUERY,mt2_testdb,'UPDATE world\.test_world_proto SET name = .*Hacked.* WHERE vnum = 10',1142" "$MT2_AUDIT_LOG_FILE"
        if grep -q "SELECT id, name, gold FROM test_player_live" "$MT2_AUDIT_LOG_FILE"; then
            echo "[ERROR] SELECT query was unexpectedly logged in mariadb_audit.log under QUERY_DML_NO_SELECT!" >&2
            exit 1
        fi
        if [ -n "${MT2_MYSQL_SOCKET:-}" ] && [ -S "${MT2_MYSQL_SOCKET:-}" ]; then
            "$DB_CLI" --socket="$MT2_MYSQL_SOCKET" -u root -e "SET GLOBAL server_audit_file_rotate_now = ON;"
        fi
        "$SFROOT/audit_guard.sh" --enforce-perms --audit-log "$MT2_AUDIT_LOG_FILE" --archive-dir "/tmp/mt2_live_audit_arch_$$" --alert-file "/tmp/mt2_live_audit_alert_$$.log"
        LIVE_AUDIT_DIR_MODE=$(stat -f "%Lp" "$(dirname "$MT2_AUDIT_LOG_FILE")")
        LIVE_AUDIT_FILE_MODE=$(stat -f "%Lp" "$MT2_AUDIT_LOG_FILE")
        if [ "$LIVE_AUDIT_DIR_MODE" != "750" ] || [ "$LIVE_AUDIT_FILE_MODE" != "640" ]; then
            echo "[ERROR] Expected dir mode 750 and file mode 640 on live audit log, got dir=$LIVE_AUDIT_DIR_MODE file=$LIVE_AUDIT_FILE_MODE" >&2
            exit 1
        fi
        chflags -R nosappnd "/tmp/mt2_live_audit_arch_$$" 2>/dev/null || true
        rm -rf "/tmp/mt2_live_audit_arch_$$" "/tmp/mt2_live_audit_alert_$$.log"
        echo "  -> Verified real FreeBSD mariadb_audit.log events, password masking (*****), 0 SELECT noise, rotation & 0750/0640 permissions"
    fi

    echo "=== [Bonus 4.D & 5] Verifying FreeBSD audit_guard.sh v2 (Redaction, Alarms, Rotation Drain, Holdback, TLS+ACK, Offset Safety & Cron) ==="
    FB_AG_DIR="/tmp/mt2_fb_audit_guard_$$"
    rm -rf "$FB_AG_DIR"
    mkdir -p "$FB_AG_DIR/data" "$FB_AG_DIR/archive" "$FB_AG_DIR/sf/chs/ch11"
    AG_LOG="$FB_AG_DIR/data/mariadb_audit.log"
    AG_ALERT="$FB_AG_DIR/alerts.log"

    cat > "$AG_LOG" <<'EOF'
20261024 12:00:01,mt2,ransom_attacker_unauth,127.0.0.1,1,0,FAILED_CONNECT,,,1045
20261024 12:00:02,mt2,mt2server,127.0.0.1,2,1,QUERY,mt2_testdb,'DROP TABLE test_player_live',1142
20261024 12:00:03,mt2,mt2server,127.0.0.1,2,2,QUERY,account,'UPDATE account SET password=''$argon2id$v=19$m=65536,t=3,p=1$c2FsdA$aGFzaA'', social_id=''1234567'', email=''secret@example.com'', hwid=''HWID_SECRET_99'', old_pw=''*AABBCCDDEEFF00112233445566778899AABBCCDD'' WHERE id=1',0
20261024 12:00:04,mt2,mt2server,127.0.0.1,2,3,QUERY,mt2_testdb,'DELETE FROM test_player_live WHERE id = 99901',0
20261024 12:00:05,mt2,mt2server,127.0.0.1,2,4,QUERY,mt2_testdb,'DELETE FROM log.test_log_isolation WHERE id = 1',1142
EOF
    cat > "${AG_LOG}.001" <<'EOF'
20261024 11:59:00,mt2,mt2server,127.0.0.1,1,1,QUERY,account,'INSERT INTO account (login,password,social_id,email,hwid) VALUES (''u1'',''$argon2id$v=19$m=65536,t=3,p=1$c2FsdA$b2xk'',''7654321'',''old@example.com'',''OLD_HWID_77'')',0
20261024 11:59:01,mt2,mt2server,127.0.0.1,1,2,QUERY,account,'INSERT IGNORE INTO account (hwid,login,email,password,social_id) VALUES (''REORD_HWID_11'',''u2'',''reord@example.org'',''PlainPass_22'',''6543210''),(''MULTI_HWID_33'',''u3'',''row2@example.org'',''*11223344556677889900AABBCCDDEEFF00112233'',''5432109'')',0
20261024 11:59:02,mt2,mt2server,127.0.0.1,1,3,QUERY,account,'REPLACE INTO `account`.`account` (`token`,`secret`,`login`,`hwid`,`password`) VALUES (''TOK_SEC_44'','''' ,''u4'',''O''''Reilly_HWID_55'',''pa\''ss(word),66'')',0
20261024 11:59:03,mt2,mt2server,127.0.0.1,1,4,QUERY,account,'UPDATE account SET password=''şifre_ğüşıöç_UTF8'', hwid=''HWID_UTF8_ıİ_88'', token="DBL_QUOTE_TOK_99" WHERE login=''u5''',0
EOF

    "$SFROOT/audit_guard.sh" --enforce-perms --audit-log "$AG_LOG" --archive-dir "$FB_AG_DIR/archive" --alert-file "$AG_ALERT"
    ARCH_ROT=$(ls -1 "$FB_AG_DIR/archive"/mariadb_audit.log.001.* | head -n 1)
    ARCH_MODE=$(stat -f "%Lp" "$ARCH_ROT")
    if [ "$ARCH_MODE" != "400" ]; then
        echo "[ERROR] Expected mode 400 on archived rotated audit log, got $ARCH_MODE" >&2
        exit 1
    fi
    if grep -Eq "c2FsdA|7654321|old@example\.com|OLD_HWID_77|REORD_HWID_11|PlainPass_22|6543210|MULTI_HWID_33|11223344556677889900|TOK_SEC_44|Reilly_HWID_55|word\),66|şifre_ğüşıöç_UTF8|HWID_UTF8_ıİ_88|DBL_QUOTE_TOK_99" "$ARCH_ROT" || ! grep -q '\$argon2id\$\[REDACTED\]' "$ARCH_ROT" || ! grep -q '\*\[REDACTED_SHA1\]' "$ARCH_ROT"; then
        echo "[ERROR] Sensitive fields were not redacted in archived rotated audit log!" >&2
        exit 1
    fi

    # Capacity alarm (exit 2) & Free disk alarm (exit 2)
    set +e
    "$SFROOT/audit_guard.sh" --check-capacity --audit-log "$AG_LOG" --archive-dir "$FB_AG_DIR/archive" --alert-file "$AG_ALERT" --max-bytes 100 --warn-pct 50 >/dev/null 2>&1
    CAP_RC=$?
    "$SFROOT/audit_guard.sh" --check-capacity --audit-log "$AG_LOG" --archive-dir "$FB_AG_DIR/archive" --alert-file "$AG_ALERT" --max-bytes 999999999 --min-free-mb 999999999 >/dev/null 2>&1
    DISK_RC=$?
    set -e
    if [ "$CAP_RC" -ne 2 ] || [ "$DISK_RC" -ne 2 ] || ! grep -q "\[MT2_AUDIT_ALERT\]\[CAPACITY_HIGH\]" "$AG_ALERT" || ! grep -q "\[MT2_AUDIT_ALERT\]\[DISK_FREE_LOW\]" "$AG_ALERT"; then
        echo "[ERROR] audit_guard.sh capacity ($CAP_RC) or disk free ($DISK_RC) alarm check failed!" >&2
        exit 1
    fi

    # Spool overflow & quarantine alarms (exit 4)
    echo "SYSERR: Oct 24 12:10:00 :: OnLogManagerSpoolOverflow: CRITICAL: LogManager SQL durable spool overflow" > "$FB_AG_DIR/sf/chs/ch11/syserr"
    echo "-- quarantined record" > "$FB_AG_DIR/sf/chs/ch11/sql_log_spool.dat.quarantine"
    set +e
    "$SFROOT/audit_guard.sh" --check-spool-syserr --serverfiles-dir "$FB_AG_DIR/sf" --alert-file "$AG_ALERT" >/dev/null 2>&1
    SPOOL_RC=$?
    set -e
    if [ "$SPOOL_RC" -ne 4 ] || ! grep -q "\[MT2_SPOOL_ALERT\]\[OVERFLOW_DETECTED\]" "$AG_ALERT" || ! grep -q "\[MT2_SPOOL_ALERT\]\[QUARANTINE_DETECTED\]" "$AG_ALERT"; then
        echo "[ERROR] audit_guard.sh spool overflow/quarantine check ($SPOOL_RC) failed!" >&2
        exit 1
    fi

    # Test incomplete trailing line holdback, rotation tail drain (.1), TCP+ACK, TLS+ACK, and offset safety on send failure
    python3 - "$SFROOT/audit_guard.sh" "$AG_LOG" "$FB_AG_DIR/archive" "$AG_ALERT" "${MT2_SSL_DIR:-}" <<'PYEOF'
import os, socket, ssl, subprocess, sys, threading

guard_sh, ag_log, arch_dir, alert_file, ssl_dir = sys.argv[1:6]

# Append incomplete 6th line (no trailing newline)
with open(ag_log, "ab") as f:
    f.write(b"20261024 12:00:06,mt2,mt2server,127.0.0.1,2,5,QUERY,player,'UPDATE player SET gold=123'")

received_batch1 = []
srv1 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv1.settimeout(5.0)
srv1.bind(("127.0.0.1", 0))
srv1.listen(1)
port1 = srv1.getsockname()[1]

def accept_plain():
    conn, _ = srv1.accept()
    with conn:
        data = b""
        while True:
            chunk = conn.recv(4096)
            if not chunk:
                break
            data += chunk
        received_batch1.extend([l for l in data.decode("utf-8").splitlines() if l.strip()])

t1 = threading.Thread(target=accept_plain, daemon=True)
t1.start()
rc1 = subprocess.run([guard_sh, "--forward-once", "--audit-log", ag_log, "--archive-dir", arch_dir, "--alert-file", alert_file, "--remote-host", "127.0.0.1", "--remote-port", str(port1)], check=False).returncode
t1.join(timeout=5)
srv1.close()
assert rc1 == 0 and len(received_batch1) == 5, f"Expected 5 complete lines in batch1, got rc={rc1}, len={len(received_batch1)}"
joined1 = "\n".join(received_batch1)
assert "$argon2id$[REDACTED]" in joined1 and "*[REDACTED_SHA1]" in joined1, "Missing redaction markers in batch1"
for secret in ("c2FsdA", "1234567", "secret@example.com", "HWID_SECRET_99", "AABBCCDDEEFF00112233445566778899AABBCCDD"):
    assert secret not in joined1, f"Unredacted secret {secret} leaked in batch1"

# Complete 6th line, rotate ag_log -> ag_log.1, write 7th line to new ag_log, and test --require-ack
with open(ag_log, "ab") as f:
    f.write(b",0\n")
os.replace(ag_log, ag_log + ".1")
with open(ag_log, "wb") as f:
    f.write(b"20261024 12:00:07,mt2,mt2server,127.0.0.1,2,6,QUERY,player,'UPDATE player SET gold=456',0\n")

received_batch2 = []
srv2 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv2.settimeout(5.0)
srv2.bind(("127.0.0.1", 0))
srv2.listen(1)
port2 = srv2.getsockname()[1]

def accept_ack():
    conn, _ = srv2.accept()
    with conn:
        sf = conn.makefile("rwb")
        for _ in range(2):
            line = sf.readline()
            if not line:
                break
            received_batch2.append(line.decode("utf-8").strip())
            sf.write(b"ACK\n")
            sf.flush()

t2 = threading.Thread(target=accept_ack, daemon=True)
t2.start()
rc2 = subprocess.run([guard_sh, "--forward-once", "--require-ack", "--audit-log", ag_log, "--archive-dir", arch_dir, "--alert-file", alert_file, "--remote-host", "127.0.0.1", "--remote-port", str(port2)], check=False).returncode
t2.join(timeout=5)
srv2.close()
assert rc2 == 0 and len(received_batch2) == 2, f"Expected 2 lines across rotation, got rc={rc2}, len={len(received_batch2)}"
assert "gold=123" in received_batch2[0] and "gold=456" in received_batch2[1], f"Wrong rotation ordering: {received_batch2}"

# Test strict RFC 5280 TLS verification (VERIFY_X509_STRICT) + ACK transport if ssl_dir is available
if ssl_dir and os.path.isfile(os.path.join(ssl_dir, "ca-cert.pem")):
    tls_tmp = os.path.join(os.path.dirname(ag_log), "tls_matrix")
    os.makedirs(tls_tmp, exist_ok=True)
    ca_cert = os.path.join(ssl_dir, "ca-cert.pem")
    ca_key = os.path.join(ssl_dir, "ca-key.pem")
    srv_cert = os.path.join(ssl_dir, "server-cert.pem")
    srv_key = os.path.join(ssl_dir, "server-key.pem")
    inv_ca_cert = os.path.join(ssl_dir, "invalid-ca-cert.pem")

    # Generate wrong-hostname cert (signed by valid CA, but SAN=DNS:wrong-host.mt2.internal)
    wrong_host_req = os.path.join(tls_tmp, "wrong-host-req.pem")
    wrong_host_cert = os.path.join(tls_tmp, "wrong-host-cert.pem")
    wrong_host_cnf = os.path.join(tls_tmp, "wrong-host-san.cnf")
    with open(wrong_host_cnf, "w", encoding="ascii") as cf:
        cf.write("basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectKeyIdentifier=hash\nauthorityKeyIdentifier=keyid,issuer\nsubjectAltName=DNS:wrong-host.mt2.internal\n")
    subprocess.run(["openssl", "req", "-new", "-key", srv_key, "-out", wrong_host_req, "-subj", "/CN=wrong-host.mt2.internal"], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["openssl", "x509", "-req", "-in", wrong_host_req, "-CA", ca_cert, "-CAkey", ca_key, "-CAcreateserial", "-extfile", wrong_host_cnf, "-out", wrong_host_cert, "-days", "365"], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    # Generate invalid certificate chain (leaf signed by a certificate with CA:FALSE, violating RFC 5280 / VERIFY_X509_STRICT)
    bad_ca_key = os.path.join(tls_tmp, "bad-ca-key.pem")
    bad_ca_cert = os.path.join(tls_tmp, "bad-ca-cert.pem")
    bad_leaf_cert = os.path.join(tls_tmp, "bad-leaf-cert.pem")
    bad_leaf_cnf = os.path.join(tls_tmp, "bad-leaf-san.cnf")
    with open(bad_leaf_cnf, "w", encoding="ascii") as cf:
        cf.write("basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectAltName=IP:127.0.0.1,DNS:localhost\n")
    subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-keyout", bad_ca_key, "-out", bad_ca_cert, "-days", "365", "-subj", "/CN=Invalid Non-CA Signer", "-addext", "basicConstraints=critical,CA:FALSE", "-addext", "keyUsage=critical,digitalSignature"], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["openssl", "x509", "-req", "-in", wrong_host_req, "-CA", bad_ca_cert, "-CAkey", bad_ca_key, "-CAcreateserial", "-extfile", bad_leaf_cnf, "-out", bad_leaf_cert, "-days", "365"], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def run_tls_case(cert_path, key_path, client_ca_path, require_ack=True, connect_host="127.0.0.1"):
        received_items = []
        srv_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        srv_sock.settimeout(5.0)
        srv_sock.bind(("127.0.0.1", 0))
        srv_sock.listen(1)
        lport = srv_sock.getsockname()[1]
        sctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        sctx.load_cert_chain(certfile=cert_path, keyfile=key_path)

        def _accept():
            try:
                raw_c, _ = srv_sock.accept()
                with sctx.wrap_socket(raw_c, server_side=True) as tls_c:
                    sf = tls_c.makefile("rwb")
                    line = sf.readline()
                    if line:
                        received_items.append(line.decode("utf-8").strip())
                        if require_ack:
                            sf.write(b"ACK\n")
                            sf.flush()
            except Exception:
                pass

        th = threading.Thread(target=_accept, daemon=True)
        th.start()
        cmd = [guard_sh, "--forward-once", "--tls", "--tls-ca-file", client_ca_path, "--audit-log", ag_log, "--archive-dir", arch_dir, "--alert-file", alert_file, "--remote-host", connect_host, "--remote-port", str(lport)]
        if require_ack:
            cmd.append("--require-ack")
        rc = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False).returncode
        th.join(timeout=5)
        srv_sock.close()
        return rc, received_items

    with open(ag_log, "ab") as f:
        f.write(b"20261024 12:00:08,mt2,mt2server,127.0.0.1,2,7,QUERY,player,'UPDATE player SET gold=701',0\n")
    offset_file = ag_log + ".forward.offset"
    pre_tls_offset = open(offset_file, "r", encoding="ascii").read().strip()

    # (1) Wrong CA -> REJECT (rc == 3, offset unchanged)
    rc_wrong_ca, rec_wrong_ca = run_tls_case(srv_cert, srv_key, inv_ca_cert, require_ack=True)
    assert rc_wrong_ca == 3 and len(rec_wrong_ca) == 0 and open(offset_file, "r", encoding="ascii").read().strip() == pre_tls_offset, f"Wrong CA was not rejected: rc={rc_wrong_ca}"

    # (2) Wrong Hostname -> REJECT (rc == 3, offset unchanged)
    rc_wrong_host, rec_wrong_host = run_tls_case(wrong_host_cert, srv_key, ca_cert, require_ack=True, connect_host="127.0.0.1")
    assert rc_wrong_host == 3 and len(rec_wrong_host) == 0 and open(offset_file, "r", encoding="ascii").read().strip() == pre_tls_offset, f"Wrong hostname was not rejected: rc={rc_wrong_host}"

    # (3) Invalid Certificate Chain (CA:FALSE signer under VERIFY_X509_STRICT & unanchored chain) -> REJECT (rc == 3, offset unchanged)
    rc_bad_chain1, rec_bad_chain1 = run_tls_case(bad_leaf_cert, srv_key, bad_ca_cert, require_ack=True)
    rc_bad_chain2, rec_bad_chain2 = run_tls_case(bad_leaf_cert, srv_key, ca_cert, require_ack=True)
    assert rc_bad_chain1 == 3 and rc_bad_chain2 == 3 and len(rec_bad_chain1) == 0 and len(rec_bad_chain2) == 0 and open(offset_file, "r", encoding="ascii").read().strip() == pre_tls_offset, f"Invalid cert chain was not rejected: rc1={rc_bad_chain1}, rc2={rc_bad_chain2}"

    # (4) Valid CA + Valid Hostname (TLS without ACK) -> PASS (rc == 0)
    rc_valid_tls, rec_valid_tls = run_tls_case(srv_cert, srv_key, ca_cert, require_ack=False, connect_host="127.0.0.1")
    assert rc_valid_tls == 0 and len(rec_valid_tls) == 1 and "gold=701" in rec_valid_tls[0], f"Valid CA + valid hostname TLS failed: rc={rc_valid_tls}, rec={rec_valid_tls}"

    # (5) TLS + ACK (Valid CA + Valid Hostname + Application ACK) -> PASS (rc == 0)
    with open(ag_log, "ab") as f:
        f.write(b"20261024 12:00:08,mt2,mt2server,127.0.0.1,2,7,QUERY,player,'UPDATE player SET gold=789',0\n")
    rc_tls_ack, rec_tls_ack = run_tls_case(srv_cert, srv_key, ca_cert, require_ack=True, connect_host="127.0.0.1")
    assert rc_tls_ack == 0 and len(rec_tls_ack) == 1 and "gold=789" in rec_tls_ack[0], f"TLS+ACK forward failed: rc={rc_tls_ack}, rec={rec_tls_ack}"

# Test send failure does NOT advance offset
offset_file = ag_log + ".forward.offset"
before_offset = open(offset_file, "r", encoding="ascii").read().strip()
with open(ag_log, "ab") as f:
    f.write(b"20261024 12:00:09,mt2,mt2server,127.0.0.1,2,8,QUERY,player,'UPDATE player SET gold=999',0\n")
rc_fail = subprocess.run([guard_sh, "--forward-once", "--audit-log", ag_log, "--archive-dir", arch_dir, "--alert-file", alert_file, "--remote-host", "127.0.0.1", "--remote-port", str(port2)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False).returncode
after_offset = open(offset_file, "r", encoding="ascii").read().strip()
assert rc_fail == 3 and before_offset == after_offset, f"Expected rc=3 and unchanged offset ({before_offset} vs {after_offset}), got rc={rc_fail}"
print("  -> [PASS] Python TCP/TLS/ACK receiver verified: holdback, redaction, .1 rotation ordering, strict RFC 5280 TLS (valid CA+host PASS, wrong CA REJECT, wrong host REJECT, invalid chain REJECT, TLS+ACK PASS) & offset safety")
PYEOF

    # Test --install-cron in isolated directory
    "$SFROOT/audit_guard.sh" --install-cron --cron-file "$FB_AG_DIR/cron_audit" --audit-log "$AG_LOG" --archive-dir "$FB_AG_DIR/archive" --alert-file "$AG_ALERT"
    "$SFROOT/backup_mariadb.sh" --install-cron --cron-file "$FB_AG_DIR/cron_backup" --backup-dir "$FB_AG_DIR/archive"
    if [ "$(stat -f "%Lp" "$FB_AG_DIR/cron_audit")" != "600" ] || [ "$(stat -f "%Lp" "$FB_AG_DIR/cron_backup")" != "600" ]; then
        echo "[ERROR] Expected mode 600 on generated cron files!" >&2
        exit 1
    fi
    chflags -R nosappnd "$FB_AG_DIR" 2>/dev/null || true
    rm -rf "$FB_AG_DIR"
    echo "  -> Verified FreeBSD audit_guard.sh v2 & isolated --install-cron (0 persistent runner cron files left)"

    echo "=== [Bonus 4.E] Verifying FreeBSD backup_mariadb.sh v2 (123 InnoDB, HMAC-SHA256(v2), Tamper/v1/Passphrase/Disk Rejection & 8-Table + Trigger Restore) ==="
    FB_BK_DIR="/tmp/mt2_fb_backup_test_$$"
    rm -rf "$FB_BK_DIR"
    mkdir -p "$FB_BK_DIR"
    FB_PASS_KEY="$(openssl rand -hex 24)"
    FB_HMAC_KEY="$(openssl rand -hex 32)"

    ADMIN_CNF="$FB_BK_DIR/.admin_client.cnf"
    umask 077
    cat > "$ADMIN_CNF" <<EOF
[client]
host=$HOST
port=$PORT
user=${MT2_TEST_USER:-$USER}
password="${MT2_TEST_PASSWORD:-$PASS}"
EOF
    chmod 600 "$ADMIN_CNF"

    # 1. Verify 123/123 InnoDB tables across 5 schemas
    TOTAL_INNODB=$("$DB_CLI" "--defaults-extra-file=$ADMIN_CNF" -N -s \
        -e "SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA IN ('account','common','log','player','world') AND TABLE_TYPE='BASE TABLE' AND ENGINE='InnoDB';" | tr -d ' \t\r\n')
    NON_INNODB=$("$DB_CLI" "--defaults-extra-file=$ADMIN_CNF" -N -s \
        -e "SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA IN ('account','common','log','player','world') AND TABLE_TYPE='BASE TABLE' AND ENGINE<>'InnoDB';" | tr -d ' \t\r\n')
    echo "  -> FreeBSD 5-Schema Storage Engine Check: InnoDB=$TOTAL_INNODB/123, Non-InnoDB=$NON_INNODB"
    if [ "$TOTAL_INNODB" != "123" ] || [ "$NON_INNODB" != "0" ]; then
        echo "[ERROR] Expected 123 InnoDB tables and 0 non-InnoDB tables, got InnoDB=$TOTAL_INNODB Non-InnoDB=$NON_INNODB" >&2
        exit 1
    fi

    # 2. Verify [MT2_BACKUP_WARN][NON_INNODB_ENGINE] warning when a temporary MEMORY table exists
    "$DB_CLI" "--defaults-extra-file=$ADMIN_CNF" player -e "CREATE TABLE tmp_non_innodb_check (id INT PRIMARY KEY) ENGINE=MEMORY;"
    WARN_OUT=$(MT2_DB_HOST="$HOST" MT2_DB_PORT="$PORT" MT2_BACKUP_DB_USER="$MT2_BACKUP_DB_USER" MT2_BACKUP_DB_PASS="$MT2_BACKUP_DB_PASS" MT2_BACKUP_PASSPHRASE="$FB_PASS_KEY" MT2_BACKUP_HMAC_SECRET="$FB_HMAC_KEY" "$SFROOT/backup_mariadb.sh" --backup --backup-dir "$FB_BK_DIR" --databases "player" 2>&1)
    "$DB_CLI" "--defaults-extra-file=$ADMIN_CNF" player -e "DROP TABLE tmp_non_innodb_check;"
    rm -f "$FB_BK_DIR"/mt2_backup_*
    if ! printf '%s\n' "$WARN_OUT" | grep -q "\[MT2_BACKUP_WARN\]\[NON_INNODB_ENGINE\]"; then
        echo "[ERROR] backup_mariadb.sh did not emit [MT2_BACKUP_WARN][NON_INNODB_ENGINE] for MEMORY table!" >&2
        exit 1
    fi
    echo "  -> Verified [MT2_BACKUP_WARN][NON_INNODB_ENGINE] warning detection"

    # 3. Populate 8-table relational dataset + trigger in player
    "$DB_CLI" "--defaults-extra-file=$ADMIN_CNF" --default-character-set=utf8mb4 player <<'EOSQL'
DELETE FROM guild_member WHERE guild_id = 301;
DELETE FROM guild WHERE id = 301;
DELETE FROM safebox WHERE account_id = 501;
DELETE FROM affect WHERE dwPID IN (1001, 1002);
DELETE FROM quest WHERE dwPID IN (1001, 1002);
DELETE FROM item WHERE id BETWEEN 2000101 AND 2000104;
DELETE FROM player WHERE id IN (1001, 1002);
DELETE FROM player_index WHERE id = 501;
DROP TRIGGER IF EXISTS trg_player_backup_audit;

INSERT INTO player_index (id, pid1, pid2, pid3, pid4, empire) VALUES (501, 1001, 1002, 0, 0, 1);
INSERT INTO player (id, account_id, name, job, level, exp, gold, x, y, map_index, exit_x, exit_y, exit_map_index, hp, mp, stamina, random_hp, random_sp, playtime, last_play, horse_hp_droptime) VALUES
(1001, 501, 'RelHeroA', 0, 99, 12345678, 85000000, 469300, 964200, 1, 469300, 964200, 1, 18500, 4200, 800, 4000, 2000, 1250, '2026-10-24 12:00:00', 0),
(1002, 501, 'RelHeroB', 2, 75, 4500000,  19500000, 401600, 899600, 3, 401600, 899600, 3, 14200, 6800, 750, 3000, 3500, 640,  '2026-10-24 12:05:00', 0);

INSERT INTO item (id, owner_id, `window`, pos, count, vnum, socket0, socket1, socket2, attrtype0, attrvalue0, attrtype1, attrvalue1) VALUES
(2000101, 1001, 'EQUIPMENT', 4,  1, 189,   1, 0, 0, 72, 45, 15, 10),
(2000102, 1001, 'EQUIPMENT', 19, 1, 74020, 604800, 1, 0, 54, 10, 0, 0),
(2000103, 1002, 'INVENTORY', 0,  200, 27003, 0, 0, 0, 0, 0, 0, 0),
(2000104, 501,  'SAFEBOX',   0,  1, 70038, 0, 0, 0, 0, 0, 0, 0);

INSERT INTO quest (dwPID, szName, szState, lValue) VALUES
(1001, 'main_quest_lv1', '__COMPLETE__', 1),
(1001, 'event_halloween_hair', 'start', 1),
(1002, 'main_quest_lv1', 'hunting', 5);

INSERT INTO affect (dwPID, bType, bApplyOn, lApplyValue, dwFlag, lDuration, lSPCost) VALUES
(1001, 514, 54, 10, 0, 604800, 0),
(1002, 200, 16, 20, 0, 3600, 0);

INSERT INTO safebox (account_id, size, password, gold) VALUES
(501, 3, '000000', 5000000);

INSERT INTO guild (id, name, sp, master, level, exp, skill_point, skill, win, draw, loss, ladder_point, gold) VALUES
(301, 'RelGuild_PL', 1000, 1001, 20, 999999, 19, X'0102030405060708090A0B0C', 15, 2, 1, 1800, 25000000);

INSERT INTO guild_member (pid, guild_id, grade, is_general, offer) VALUES
(1001, 301, 1, 1, 500000),
(1002, 301, 2, 0, 120000);

CREATE TRIGGER trg_player_backup_audit AFTER UPDATE ON safebox FOR EACH ROW BEGIN END;
EOSQL

    FP_SQL="SELECT CONCAT(COUNT(DISTINCT p.id),'|',SUM(p.gold),'|',(SELECT COUNT(*) FROM item WHERE id BETWEEN 2000101 AND 2000104),'|',(SELECT SUM(attrvalue0) FROM item WHERE id BETWEEN 2000101 AND 2000104),'|',(SELECT COUNT(*) FROM quest WHERE dwPID IN (1001,1002)),'|',(SELECT COUNT(*) FROM affect WHERE dwPID IN (1001,1002)),'|',(SELECT gold FROM safebox WHERE account_id=501),'|',(SELECT CONCAT(g.name,':',COUNT(gm.pid),':',SUM(gm.offer)) FROM guild g JOIN guild_member gm ON g.id=gm.guild_id WHERE g.id=301)) FROM player p JOIN player_index pi ON p.account_id=pi.id WHERE p.id IN (1001,1002);"
    PRE_FP=$("$DB_CLI" "--defaults-extra-file=$ADMIN_CNF" -N -s player -e "$FP_SQL" | tr -d '\r\n')
    echo "  -> Pre-backup 8-table relational fingerprint: $PRE_FP"

    # 4. Run authenticated encrypted backup via mt2backup
    MT2_DB_HOST="$HOST" \
    MT2_DB_PORT="$PORT" \
    MT2_BACKUP_DB_USER="$MT2_BACKUP_DB_USER" \
    MT2_BACKUP_DB_PASS="$MT2_BACKUP_DB_PASS" \
    MT2_INIT_DB_USER="${MT2_TEST_USER:-$USER}" \
    MT2_INIT_DB_PASS="${MT2_TEST_PASSWORD:-$PASS}" \
    MT2_BACKUP_PASSPHRASE="$FB_PASS_KEY" \
    MT2_BACKUP_HMAC_SECRET="$FB_HMAC_KEY" \
    "$SFROOT/backup_mariadb.sh" --backup --backup-dir "$FB_BK_DIR" --databases "player"

    FB_ENC_FILE=$(ls -1 "$FB_BK_DIR"/mt2_backup_*.sql.gz.enc | head -n 1)
    if ! grep -q "^HMAC-SHA256(v2)=" "${FB_ENC_FILE}.sha256"; then
        echo "[ERROR] FreeBSD backup manifest missing HMAC-SHA256(v2)!" >&2
        exit 1
    fi

    # 5. Test forged archive + recomputed SHA-256 rejection
    FB_FORGED="$FB_BK_DIR/forged.sql.gz.enc"
    cp "$FB_ENC_FILE" "$FB_FORGED"
    printf '\x00' >> "$FB_FORGED"
    FORGED_SHA=$(sha256 -q "$FB_FORGED" 2>/dev/null || sha256sum "$FB_FORGED" | awk '{print $1}')
    ORIG_HMAC=$(grep "^HMAC-SHA256(v2)=" "${FB_ENC_FILE}.sha256")
    printf '%s  forged.sql.gz.enc\n%s\n' "$FORGED_SHA" "$ORIG_HMAC" > "${FB_FORGED}.sha256"
    if MT2_DB_HOST="$HOST" MT2_DB_PORT="$PORT" MT2_INIT_DB_USER="${MT2_TEST_USER:-$USER}" MT2_INIT_DB_PASS="${MT2_TEST_PASSWORD:-$PASS}" MT2_BACKUP_PASSPHRASE="$FB_PASS_KEY" MT2_BACKUP_HMAC_SECRET="$FB_HMAC_KEY" "$SFROOT/backup_mariadb.sh" --restore "$FB_FORGED" --backup-dir "$FB_BK_DIR" --target-db "player_restore_verify" >/dev/null 2>&1; then
        echo "[ERROR] Forged archive + recomputed SHA-256 was NOT rejected by backup_mariadb.sh!" >&2
        exit 1
    fi
    rm -f "$FB_FORGED" "${FB_FORGED}.sha256"

    # 6. Test legacy v1 manifest default rejection
    FB_V1="$FB_BK_DIR/legacy_v1.sql.gz.enc"
    cp "$FB_ENC_FILE" "$FB_V1"
    V1_SHA=$(sha256 -q "$FB_V1" 2>/dev/null || sha256sum "$FB_V1" | awk '{print $1}')
    printf '%s  legacy_v1.sql.gz.enc\n' "$V1_SHA" > "${FB_V1}.sha256"
    if MT2_DB_HOST="$HOST" MT2_DB_PORT="$PORT" MT2_INIT_DB_USER="${MT2_TEST_USER:-$USER}" MT2_INIT_DB_PASS="${MT2_TEST_PASSWORD:-$PASS}" MT2_BACKUP_PASSPHRASE="$FB_PASS_KEY" MT2_BACKUP_HMAC_SECRET="$FB_HMAC_KEY" "$SFROOT/backup_mariadb.sh" --restore "$FB_V1" --backup-dir "$FB_BK_DIR" --target-db "player_restore_verify" >/dev/null 2>&1; then
        echo "[ERROR] Legacy v1 manifest without HMAC was NOT rejected by default!" >&2
        exit 1
    fi
    rm -f "$FB_V1" "${FB_V1}.sha256"

    # 7. Test wrong passphrase rejection & low disk space rejection
    if MT2_DB_HOST="$HOST" MT2_DB_PORT="$PORT" MT2_INIT_DB_USER="${MT2_TEST_USER:-$USER}" MT2_INIT_DB_PASS="${MT2_TEST_PASSWORD:-$PASS}" MT2_BACKUP_PASSPHRASE="WrongPassphraseValue999" MT2_BACKUP_HMAC_SECRET="$FB_HMAC_KEY" "$SFROOT/backup_mariadb.sh" --restore "$FB_ENC_FILE" --backup-dir "$FB_BK_DIR" --target-db "player_restore_verify" >/dev/null 2>&1; then
        echo "[ERROR] Restore with wrong passphrase was NOT rejected by backup_mariadb.sh!" >&2
        exit 1
    fi
    set +e
    MT2_DB_HOST="$HOST" MT2_DB_PORT="$PORT" MT2_BACKUP_DB_USER="$MT2_BACKUP_DB_USER" MT2_BACKUP_DB_PASS="$MT2_BACKUP_DB_PASS" MT2_BACKUP_PASSPHRASE="$FB_PASS_KEY" MT2_BACKUP_HMAC_SECRET="$FB_HMAC_KEY" MT2_MIN_FREE_DISK_MB=999999999 "$SFROOT/backup_mariadb.sh" --backup --backup-dir "$FB_BK_DIR" --databases "player" >/dev/null 2>&1
    LOW_DISK_RC=$?
    set -e
    if [ "$LOW_DISK_RC" -ne 2 ]; then
        echo "[ERROR] Expected exit code 2 on low disk space backup check, got $LOW_DISK_RC" >&2
        exit 1
    fi

    # 8. Simulate destructive DML in player, then restore into isolated player_restore_verify and verify 8-table + trigger integrity
    "$DB_CLI" "--defaults-extra-file=$ADMIN_CNF" player -e "DELETE FROM item WHERE id BETWEEN 2000101 AND 2000104; UPDATE player SET gold=0, level=1 WHERE id IN (1001,1002); UPDATE safebox SET gold=0 WHERE account_id=501;"
    MT2_DB_HOST="$HOST" \
    MT2_DB_PORT="$PORT" \
    MT2_INIT_DB_USER="${MT2_TEST_USER:-$USER}" \
    MT2_INIT_DB_PASS="${MT2_TEST_PASSWORD:-$PASS}" \
    MT2_BACKUP_PASSPHRASE="$FB_PASS_KEY" \
    MT2_BACKUP_HMAC_SECRET="$FB_HMAC_KEY" \
    "$SFROOT/backup_mariadb.sh" --restore "$FB_ENC_FILE" --backup-dir "$FB_BK_DIR" --target-db "player_restore_verify"

    POST_FP=$("$DB_CLI" "--defaults-extra-file=$ADMIN_CNF" -N -s player_restore_verify -e "$FP_SQL" | tr -d '\r\n')
    TRIG_CNT=$("$DB_CLI" "--defaults-extra-file=$ADMIN_CNF" -N -s -e "SELECT COUNT(*) FROM information_schema.TRIGGERS WHERE TRIGGER_SCHEMA='player_restore_verify' AND TRIGGER_NAME='trg_player_backup_audit';" | tr -d ' \t\r\n')
    echo "  -> Restored 8-table relational fingerprint:   $POST_FP | TriggerCount=$TRIG_CNT"
    if [ "$PRE_FP" != "$POST_FP" ] || [ "$TRIG_CNT" != "1" ]; then
        echo "[ERROR] Multi-table relational or trigger restore mismatch! Expected='$PRE_FP' (trig=1), Actual='$POST_FP' (trig=$TRIG_CNT)" >&2
        exit 1
    fi

    "$DB_CLI" "--defaults-extra-file=$ADMIN_CNF" player <<'EOSQL'
DROP TRIGGER IF EXISTS trg_player_backup_audit;
DELETE FROM guild_member WHERE guild_id = 301;
DELETE FROM guild WHERE id = 301;
DELETE FROM safebox WHERE account_id = 501;
DELETE FROM affect WHERE dwPID IN (1001, 1002);
DELETE FROM quest WHERE dwPID IN (1001, 1002);
DELETE FROM item WHERE id BETWEEN 2000101 AND 2000104;
DELETE FROM player WHERE id IN (1001, 1002);
DELETE FROM player_index WHERE id = 501;
DROP DATABASE IF EXISTS player_restore_verify;
EOSQL
    rm -rf "$FB_BK_DIR"
    echo "  -> Verified FreeBSD backup_mariadb.sh v2: 123 InnoDB, HMAC-SHA256(v2), forged/v1/passphrase/disk rejection & 8-table + trigger restore"
fi

echo "=================================================================="
echo " [PASS] REV 51 FREEBSD 15.1 SERVERFILES (LUA 5.5.1 EXCLUSIVE, 202 QUESTS,"
echo "        8 SERVICES, 0 SYSERR, LEAST-PRIVILEGE SQL, CH11->CH12->CH11 WARP)"
echo "=================================================================="
