#!/bin/sh
# ==============================================================================
# Metin2 MT2009 - Rev 50: Real FreeBSD 15.1 x64 Serverfiles Installation,
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
GAME50_BIN="${MT2_SMOKE_GAME50_BIN:-$ROOT_DIR/server/src/game/game_r41023_lua50}"
GAME55_BIN="${MT2_SMOKE_GAME55_BIN:-$ROOT_DIR/server/src/game/game_r41023}"
SF_ARCHIVE="$ROOT_DIR/server/serverfiles_share.tar.xz"
SFROOT="/usr/home/mt2"
ARTIFACT_DIR="$ROOT_DIR/artifacts/freebsd_serverfiles_logs_${ENGINE_ID}"

SMOKE_HWID_PLAIN="SMOKETEST_HWID1"
SMOKE_HWID_HEX="5BE8C5A4CAFB1014505B16C5D68D28A9"
SMOKE_TEST_LOGIN="bsd64hero"
SMOKE_TEST_PASS="Bsd64Hero!2026"
export SMOKE_TEST_LOGIN SMOKE_TEST_PASS

echo "=================================================================="
echo " METIN2 MT2009 - REV 50 REAL FREEBSD 15.1 SERVERFILES VERIFICATION"
echo " Engine:         $ENGINE ($HOST:$PORT)"
echo " Target Root:    $SFROOT"
echo " Artifact Dir:   $ARTIFACT_DIR"
echo "=================================================================="

if [ -z "$PASS" ]; then
    echo "[ERROR] MT2_RUNTIME_PASSWORD / MT2_TEST_PASSWORD is not set!" >&2
    exit 1
fi

for bin in "$DB_BIN" "$GAME50_BIN" "$GAME55_BIN"; do
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
    rm -f "$SFROOT/.test_client.cnf"
    if [ -x "$SFROOT/stop.sh" ]; then
        "$SFROOT/stop.sh" >/dev/null 2>&1 || true
    fi
    collect_logs
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

echo "=== [5/7] Placing Character on map_index=3 (ch12) & Restarting All 8 Services ==="
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

echo "=================================================================="
echo " [PASS] REV 50 FREEBSD 15.1 SERVERFILES (8 SERVICES, 0 SYSERR,"
echo "        LEAST-PRIVILEGE SQL, CHMOD 600, CH11->CH12->CH11 WARP)"
echo "=================================================================="
