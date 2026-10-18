#!/bin/sh
# ==============================================================================
# Metin2 MT2009 - Rev 49: Real FreeBSD 15.1 x64 Serverfiles Installation,
# Full 8-Service Boot (db, auth, ch11, ch12, ch13, ch99, ch00, ch01),
# Zero-syserr Verification, End-to-End PHASE_GAME & Restart Persistence Test
# ==============================================================================

set -e

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ROOT_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
BUILD_DIR="$ROOT_DIR/tests/build"
CXX=${CXX:-/usr/local/bin/g++14}

HOST="${MT2_TEST_HOST:-127.0.0.1}"
PORT="${MT2_TEST_PORT:-3307}"
USER="${MT2_TEST_USER:-mt2test}"
PASS="${MT2_TEST_PASSWORD:-mt2pass}"
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
echo " METIN2 MT2009 - REV 49 REAL FREEBSD 15.1 SERVERFILES VERIFICATION"
echo " Engine:         $ENGINE ($HOST:$PORT)"
echo " Target Root:    $SFROOT"
echo " Artifact Dir:   $ARTIFACT_DIR"
echo "=================================================================="

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

if [ ! -x "$BUILD_DIR/test_server_smoke_client" ]; then
    mkdir -p "$BUILD_DIR"
    RPATH=""
    if [ -d "/usr/local/lib/gcc14" ]; then
        RPATH="-Wl,-rpath=/usr/local/lib/gcc14"
    fi
    $CXX -std=c++20 -Wall -Wextra -Werror -O2 $RPATH \
        "$ROOT_DIR/tests/test_server_smoke_client.cpp" \
        -o "$BUILD_DIR/test_server_smoke_client"
fi

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
    if [ -x "$SFROOT/stop.sh" ]; then
        "$SFROOT/stop.sh" >/dev/null 2>&1 || true
    fi
    collect_logs
}
trap cleanup EXIT INT TERM

echo "=== [1/6] Loading 5 Production Databases (account, common, log, player, world) ==="
STRICT_MODE="STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION"
for db_pair in "account:account.sql" "common:common.sql" "log:log.sql" "player:player.sql" "world:world.sql"; do
    db_name="${db_pair%%:*}"
    sql_file="${db_pair#*:}"
    echo "  -> Loading sql/$sql_file into database '$db_name'..."
    $DB_CLI -h "$HOST" -P "$PORT" -u "$USER" "-p$PASS" "$db_name" \
        --init-command="SET SESSION sql_mode='$STRICT_MODE';" < "$ROOT_DIR/sql/$sql_file"
done

echo "=== [2/6] Installing Real Serverfiles into $SFROOT ==="
rm -rf "$SFROOT" "$ARTIFACT_DIR"
mkdir -p "$SFROOT/share/bin" "$SFROOT/db/log" "$SFROOT/auth/log" "$ARTIFACT_DIR"
tar -xJf "$SF_ARCHIVE" -C /

cp -f "$DB_BIN" "$SFROOT/share/bin/db"
cp -f "$GAME55_BIN" "$SFROOT/share/bin/game"
cp -f "$GAME50_BIN" "$SFROOT/share/bin/game_lua50"
chmod +x "$SFROOT/share/bin/db" "$SFROOT/share/bin/game" "$SFROOT/share/bin/game_lua50"

cp -f "$ROOT_DIR/server/scripts/freebsd/start.sh" "$SFROOT/start.sh"
cp -f "$ROOT_DIR/server/scripts/freebsd/stop.sh" "$SFROOT/stop.sh"
cp -f "$ROOT_DIR/server/scripts/freebsd/status.sh" "$SFROOT/status.sh"
chmod +x "$SFROOT/start.sh" "$SFROOT/stop.sh" "$SFROOT/status.sh"

ln -sfn "$SFROOT/share/bin/db" "$SFROOT/db/db"

ln -sfn "$SFROOT/share/bin/game" "$SFROOT/auth/auth"
ln -sfn "$SFROOT/share/data" "$SFROOT/auth/data"
ln -sfn "$SFROOT/share/locale" "$SFROOT/auth/locale"
ln -sfn "$SFROOT/share/package" "$SFROOT/auth/package"
ln -sfn "$SFROOT/chs/ch01/CMD" "$SFROOT/auth/CMD"
ln -sfn "$SFROOT/chs/ch01/mark" "$SFROOT/auth/mark"

for ch in ch11 ch12 ch13 ch99 ch00 ch01; do
    mkdir -p "$SFROOT/chs/$ch/log"
    ln -sfn "$SFROOT/share/bin/game" "$SFROOT/chs/$ch/$ch"
    ln -sfn "$SFROOT/share/data" "$SFROOT/chs/$ch/data"
    ln -sfn "$SFROOT/share/locale" "$SFROOT/chs/$ch/locale"
    ln -sfn "$SFROOT/share/package" "$SFROOT/chs/$ch/package"
    if [ "$ch" != "ch01" ]; then
        ln -sfn "$SFROOT/chs/ch01/CMD" "$SFROOT/chs/$ch/CMD"
        ln -sfn "$SFROOT/chs/ch01/mark" "$SFROOT/chs/$ch/mark"
    fi
done

cat > "$SFROOT/db/conf.txt" <<EOF
WELCOME_MSG = "Mysql odpalone"
SQL_ACCOUNT = "$HOST account $USER $PASS $PORT"
SQL_HOTBACKUP = "$HOST player $USER $PASS $PORT"
SQL_PLAYER = "$HOST player $USER $PASS $PORT"
SQL_COMMON = "$HOST common $USER $PASS $PORT"
SQL_LOG = "$HOST log $USER $PASS $PORT"
SQL_WORLD = "$HOST world $USER $PASS $PORT"
TABLE_POSTFIX = ""
BIND_PORT = 18000
DB_SLEEP_MSEC = 10
CLIENT_HEART_FPS = 10
HASH_PLAYER_LIFE_SEC = 600
BACKUP_LIMIT_SEC = 3600
PLAYER_ID_START = 100
PLAYER_DELETE_LEVEL_LIMIT = 75
PLAYER_DELETE_CHECK_SIMPLE = 1
ITEM_ID_RANGE = 2000000000 2100000000
PROTO_FROM_DB = 1
MIRROR2DB = 1
DISABLE_HOTBACKUP = 1
EOF

cat > "$SFROOT/auth/CONFIG" <<EOF
HOSTNAME: auth
CHANNEL: 1
PORT: 17105
P2P_PORT: 19555
DB_PORT: 18000
DB_ADDR: 127.0.0.1
PASSES_PER_SEC: 25
SAVE_EVENT_SECOND_CYCLE: 180
PING_EVENT_SECOND_CYCLE: 180
TABLE_POSTFIX: 
ITEM_ID_RANGE: 000000001 000000002
AUTH_SERVER: master
ACCOUNT_SQL: $HOST $USER $PASS account $PORT
COMMON_SQL: $HOST $USER $PASS common $PORT
LOG_SQL: $HOST $USER $PASS log $PORT
SQL_HOTBACKUP: $HOST $USER $PASS player $PORT
PLAYER_SQL: $HOST $USER $PASS player $PORT
SQL_WORLD: $HOST $USER $PASS world $PORT
LOCALE_SERVICE: poland
BIND_IP: 127.0.0.1
EOF

write_ch_config() {
    ch_name="$1"
    hostname_val="$2"
    channel_num="$3"
    port_num="$4"
    p2p_num="$5"
    map_allow="$6"

    cat > "$SFROOT/chs/$ch_name/CONFIG" <<EOF
HOSTNAME: $hostname_val
CHANNEL: $channel_num
PORT: $port_num
P2P_PORT: $p2p_num
DB_PORT: 18000
DB_ADDR: 127.0.0.1
MAP_ALLOW: $map_allow
TABLE_POSTFIX: 
PASSES_PER_SEC: 25
SAVE_EVENT_SECOND_CYCLE: 180
PING_EVENT_SECOND_CYCLE: 180
ACCOUNT_SQL: $HOST $USER $PASS account $PORT
COMMON_SQL: $HOST $USER $PASS common $PORT
LOG_SQL: $HOST $USER $PASS log $PORT
SQL_HOTBACKUP: $HOST $USER $PASS player $PORT
PLAYER_SQL: $HOST $USER $PASS player $PORT
SQL_WORLD: $HOST $USER $PASS world $PORT
SERVER_KEY: fIFjs5S47kdhsMUxsgjSrisTfGRB79PN4qi/D4U6yCX1LYUqBDQOxleqtAxp+cFteti2gp6fnnQ9Td2QFUWjpSuwLX/XDfCTgkCwoxYzb0P20YgIraXKDrN5FHbKYUvOl0Y69NcHUpGo+xU+Tbs2O6AEJYLrqW0BW3ggL6JeVUM*
MALL_URL: localhost
ADMINPAGE_IP: localhost
ADMINPAGE_IP1: localhost
ADMINPAGE_IP2: localhost
ADMINPAGE_IP3: localhost
ADMINPAGE_PASSWORD: Xn4HZ59P8lyBxAfZy6WP
VIEW_RANGE: 8000
CHECK_MULTIHACK: 0
MAX_LEVEL: 120
SPEEDHACK_LIMIT_COUNT: 300
SPEEDHACK_LIMIT_BONUS: 80
PK_PROTECT_LEVEL: 15
TEST_SERVER: 0
XTRAP_ENABLE: 0
GLOBAL_SHOUT: 1
SHOP_PRICE_3X_TAX: 0
BIND_IP: 127.0.0.1
EOF
}

write_ch_config "ch11" "channel1"  "1"  "28000" "29000" "1 21 41"
write_ch_config "ch12" "channel1"  "1"  "28001" "29001" "3 23 43 4 24 44 63 65 67 68 64 61 62 70 104"
write_ch_config "ch13" "channel1"  "1"  "28002" "29002" "66 69 79 112 181 182 183 5 25 45 108 109"
write_ch_config "ch99" "channel99" "99" "13099" "14099" "110 111 81 113 200 90"
write_ch_config "ch00" "channel00" "99" "13088" "14088" "250"
write_ch_config "ch01" "channel01" "99" "13098" "14098" "71"

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

echo "=== [3/6] Starting All 8 FreeBSD Serverfiles Services (db, auth, ch11..ch01) ==="
"$SFROOT/start.sh"
sleep 2
"$SFROOT/status.sh" | tee "$ARTIFACT_DIR/status_initial_boot.txt"
if grep -q "STOPPED" "$ARTIFACT_DIR/status_initial_boot.txt"; then
    echo "[ERROR] One or more services failed to enter LISTENING state!" >&2
    exit 1
fi
assert_zero_syserr "initial_boot"

echo "=== [4/6] Creating Fresh Argon2id Account & Entering PHASE_GAME on ch11 ==="
CREATE_OUT=$(cd "$SFROOT/db" && printf '%s\n' "$SMOKE_TEST_PASS" | ./db --create-account "$SMOKE_TEST_LOGIN" "1234567" "$SMOKE_HWID_PLAIN")
echo "  -> $CREATE_OUT"
ACCOUNT_ID=$(printf '%s\n' "$CREATE_OUT" | sed -n 's/.*id=\([0-9][0-9]*\).*/\1/p')
if [ -z "$ACCOUNT_ID" ]; then
    echo "[ERROR] Failed to parse ACCOUNT_ID from --create-account output!" >&2
    exit 1
fi

AUTH_OUT=$("$BUILD_DIR/test_server_smoke_client" --auth-login 17105 "$SMOKE_TEST_LOGIN" "$SMOKE_HWID_HEX" 41 29 55 77)
echo "$AUTH_OUT"
LOGIN_KEY=$(printf '%s\n' "$AUTH_OUT" | sed -n 's/.*LOGIN_KEY=\([0-9][0-9]*\).*/\1/p')
if [ -z "$LOGIN_KEY" ]; then
    echo "[ERROR] Failed to obtain LOGIN_KEY from AUTH server!" >&2
    exit 1
fi

SMOKE_REAL_SERVERFILES=1 SMOKE_CREATE_CHAR_NAME="BsdHero" \
    "$BUILD_DIR/test_server_smoke_client" --game-login-key 28000 "$LOGIN_KEY" "$SMOKE_TEST_LOGIN" "$ACCOUNT_ID" 41 29 55 77
assert_zero_syserr "after_first_login_phase_game"

echo "=== [5/6] Graceful Stop & Zero-syserr Shutdown Verification ==="
"$SFROOT/stop.sh"
"$SFROOT/status.sh" | tee "$ARTIFACT_DIR/status_after_stop.txt"
if grep -q "LISTENING" "$ARTIFACT_DIR/status_after_stop.txt"; then
    echo "[ERROR] One or more services remained LISTENING after stop.sh!" >&2
    exit 1
fi
assert_zero_syserr "after_graceful_stop"

echo "=== [6/6] Full Restart & Persisted Character Re-Login into PHASE_GAME ==="
"$SFROOT/start.sh"
sleep 2
"$SFROOT/status.sh" | tee "$ARTIFACT_DIR/status_after_restart.txt"
if grep -q "STOPPED" "$ARTIFACT_DIR/status_after_restart.txt"; then
    echo "[ERROR] One or more services failed to restart!" >&2
    exit 1
fi

AUTH_OUT2=$("$BUILD_DIR/test_server_smoke_client" --auth-login 17105 "$SMOKE_TEST_LOGIN" "$SMOKE_HWID_HEX" 41 29 55 77)
echo "$AUTH_OUT2"
LOGIN_KEY2=$(printf '%s\n' "$AUTH_OUT2" | sed -n 's/.*LOGIN_KEY=\([0-9][0-9]*\).*/\1/p')
if [ -z "$LOGIN_KEY2" ]; then
    echo "[ERROR] Failed to obtain second LOGIN_KEY after restart!" >&2
    exit 1
fi

SMOKE_REAL_SERVERFILES=1 SMOKE_EXPECT_EXISTING_CHAR=1 SMOKE_CREATE_CHAR_NAME="BsdHero" \
    "$BUILD_DIR/test_server_smoke_client" --game-login-key 28000 "$LOGIN_KEY2" "$SMOKE_TEST_LOGIN" "$ACCOUNT_ID" 41 29 55 77

"$SFROOT/stop.sh"
"$SFROOT/status.sh" | tee "$ARTIFACT_DIR/status_final.txt"
assert_zero_syserr "final_shutdown"
collect_logs

echo "=================================================================="
echo " [PASS] REV 49 FREEBSD 15.1 SERVERFILES (8 SERVICES, 0 SYSERR,"
echo "        ARGON2ID -> EMPIRE -> BsdHero -> PHASE_GAME -> RESTART)"
echo "=================================================================="
