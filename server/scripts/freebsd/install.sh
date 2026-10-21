#!/bin/sh
set -e

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ -f "$SCRIPT_DIR/../../../CMakeLists.txt" ]; then
    REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../../.." && pwd)
elif [ -f "$SCRIPT_DIR/../../CMakeLists.txt" ]; then
    REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)
else
    REPO_ROOT=""
fi

TARGET_DIR="/usr/home/mt2"
ENV_FILE=""
INIT_SQL=0
SKIP_CONFIG=0

while [ "$#" -gt 0 ]; do
    case "$1" in
        --target-dir)
            TARGET_DIR="$2"
            shift 2
            ;;
        --env-file)
            ENV_FILE="$2"
            shift 2
            ;;
        --repo-root)
            REPO_ROOT="$2"
            shift 2
            ;;
        --init-sql)
            INIT_SQL=1
            shift 1
            ;;
        --skip-config)
            SKIP_CONFIG=1
            shift 1
            ;;
        *)
            echo "[ERROR] Unknown argument: $1" >&2
            exit 1
            ;;
    esac
done

if [ -z "$REPO_ROOT" ] || [ ! -f "$REPO_ROOT/CMakeLists.txt" ]; then
    echo "[ERROR] Cannot locate repository root. Pass --repo-root <path>." >&2
    exit 1
fi

DB_BIN="${MT2_SMOKE_DB_BIN:-$REPO_ROOT/server/src/db/db_r41023}"
GAME_BIN="${MT2_SMOKE_GAME55_BIN:-$REPO_ROOT/server/src/game/game_r41023}"
QC_BIN="${MT2_SMOKE_QC_BIN:-$REPO_ROOT/build/freebsd-gcc/server/src/game/src/quest/qc}"
if [ ! -x "$QC_BIN" ] && [ -x "$REPO_ROOT/server/src/game/src/quest/qc" ]; then
    QC_BIN="$REPO_ROOT/server/src/game/src/quest/qc"
fi
SF_ARCHIVE="$REPO_ROOT/server/serverfiles_share.tar.xz"

for bin in "$DB_BIN" "$GAME_BIN"; do
    if [ ! -x "$bin" ]; then
        echo "[ERROR] Missing or non-executable x64 binary: $bin" >&2
        exit 1
    fi
done

if [ ! -f "$SF_ARCHIVE" ]; then
    echo "[ERROR] Missing serverfiles share archive: $SF_ARCHIVE" >&2
    exit 1
fi

echo "=== [1/4] Preparing FreeBSD 15.1 x64 Serverfiles Directory: $TARGET_DIR ==="
mkdir -p "$TARGET_DIR/share/bin" "$TARGET_DIR/db/log/archive" "$TARGET_DIR/auth/log/archive"
for ch in ch11 ch12 ch13 ch99 ch00 ch01; do
    mkdir -p "$TARGET_DIR/chs/$ch/log/archive"
done

if [ ! -d "$TARGET_DIR/share/locale/poland/quest/object" ]; then
    echo "  -> Extracting $SF_ARCHIVE into $TARGET_DIR..."
    tar -xJf "$SF_ARCHIVE" --strip-components=3 -C "$TARGET_DIR"
fi

echo "=== [2/4] Installing x64 ELF Binaries, Symlinks, and Management Scripts ==="
cp -f "$DB_BIN" "$TARGET_DIR/share/bin/db"
cp -f "$GAME_BIN" "$TARGET_DIR/share/bin/game"
rm -f "$TARGET_DIR/share/bin/game_lua50"
chmod 750 "$TARGET_DIR/share/bin/db" "$TARGET_DIR/share/bin/game"
if [ -x "$QC_BIN" ]; then
    cp -f "$QC_BIN" "$TARGET_DIR/share/locale/poland/quest/qc"
    chmod 750 "$TARGET_DIR/share/locale/poland/quest/qc"
fi

for s in start.sh stop.sh status.sh install.sh mt2server.rc; do
    if [ -f "$REPO_ROOT/server/scripts/freebsd/$s" ]; then
        cp -f "$REPO_ROOT/server/scripts/freebsd/$s" "$TARGET_DIR/$s"
        chmod 750 "$TARGET_DIR/$s"
    fi
done

ln -sfn "$TARGET_DIR/share/bin/db" "$TARGET_DIR/db/db"

mkdir -p "$TARGET_DIR/chs/ch01/mark"
if [ ! -f "$TARGET_DIR/chs/ch01/mark/mark_index" ]; then
    : > "$TARGET_DIR/chs/ch01/mark/mark_index"
fi

ln -sfn "$TARGET_DIR/share/bin/game" "$TARGET_DIR/auth/auth"
ln -sfn "$TARGET_DIR/share/data" "$TARGET_DIR/auth/data"
ln -sfn "$TARGET_DIR/share/locale" "$TARGET_DIR/auth/locale"
ln -sfn "$TARGET_DIR/share/package" "$TARGET_DIR/auth/package"
ln -sfn "$TARGET_DIR/chs/ch01/CMD" "$TARGET_DIR/auth/CMD"
ln -sfn "$TARGET_DIR/chs/ch01/mark" "$TARGET_DIR/auth/mark"

for ch in ch11 ch12 ch13 ch99 ch00 ch01; do
    mkdir -p "$TARGET_DIR/chs/$ch/log/archive"
    ln -sfn "$TARGET_DIR/share/bin/game" "$TARGET_DIR/chs/$ch/$ch"
    ln -sfn "$TARGET_DIR/share/data" "$TARGET_DIR/chs/$ch/data"
    ln -sfn "$TARGET_DIR/share/locale" "$TARGET_DIR/chs/$ch/locale"
    ln -sfn "$TARGET_DIR/share/package" "$TARGET_DIR/chs/$ch/package"
    if [ "$ch" != "ch01" ]; then
        ln -sfn "$TARGET_DIR/chs/ch01/CMD" "$TARGET_DIR/chs/$ch/CMD"
        ln -sfn "$TARGET_DIR/chs/ch01/mark" "$TARGET_DIR/chs/$ch/mark"
    fi
done

if [ -z "$ENV_FILE" ]; then
    if [ -f "$TARGET_DIR/serverfiles.env" ]; then
        ENV_FILE="$TARGET_DIR/serverfiles.env"
    elif [ -f "$REPO_ROOT/server/config/serverfiles.env" ]; then
        ENV_FILE="$REPO_ROOT/server/config/serverfiles.env"
    elif [ -f "$REPO_ROOT/server/config/serverfiles.env.example" ]; then
        cp -f "$REPO_ROOT/server/config/serverfiles.env.example" "$TARGET_DIR/serverfiles.env"
        chmod 600 "$TARGET_DIR/serverfiles.env"
        ENV_FILE="$TARGET_DIR/serverfiles.env"
    fi
fi

if [ -n "$ENV_FILE" ] && [ -f "$ENV_FILE" ]; then
    chmod 600 "$ENV_FILE" 2>/dev/null || true
    while IFS='=' read -r k v || [ -n "$k" ]; do
        k_trimmed=$(printf '%s' "$k" | tr -d ' \t\r\n')
        case "$k_trimmed" in
            ""|\#*) continue ;;
        esac
        v_trimmed=$(printf '%s' "$v" | sed 's/^[[:space:]]*//;s/[[:space:]\r]*$//')
        eval "cur_val=\${$k_trimmed:-}"
        if [ -z "$cur_val" ]; then
            export "$k_trimmed=$v_trimmed"
        fi
    done < "$ENV_FILE"
fi

MT2_DB_HOST="${MT2_DB_HOST:-127.0.0.1}"
MT2_DB_PORT="${MT2_DB_PORT:-3306}"
MT2_DB_USER="${MT2_DB_USER:-mt2server}"
MT2_DB_PASS="${MT2_DB_PASS:-}"
MT2_BIND_IP="${MT2_BIND_IP:-127.0.0.1}"
MT2_DB_ADDR="${MT2_DB_ADDR:-127.0.0.1}"
MT2_DB_LISTEN_PORT="${MT2_DB_LISTEN_PORT:-18000}"
MT2_AUTH_PORT="${MT2_AUTH_PORT:-17105}"
MT2_AUTH_P2P_PORT="${MT2_AUTH_P2P_PORT:-19555}"
MT2_ADMINPAGE_IP="${MT2_ADMINPAGE_IP:-127.0.0.1}"
MT2_ADMINPAGE_PASSWORD="${MT2_ADMINPAGE_PASSWORD:-}"
MT2_SERVER_KEY="${MT2_SERVER_KEY:-}"
MT2_LOCALE_SERVICE="${MT2_LOCALE_SERVICE:-poland}"

if [ "$SKIP_CONFIG" -eq 0 ]; then
    echo "=== [3/4] Generating Secure Runtime Configurations (chmod 600) ==="
    for secret_name in MT2_DB_PASS MT2_ADMINPAGE_PASSWORD MT2_SERVER_KEY; do
        eval "secret_val=\${$secret_name:-}"
        case "$secret_val" in
            ""|__CHANGE_ME_*)
                echo "[ERROR] $secret_name is unset or still uses a placeholder in $ENV_FILE." >&2
                exit 1
                ;;
        esac
    done

    umask 077
    cat > "$TARGET_DIR/db/conf.txt" <<EOF
WELCOME_MSG = "Mysql odpalone"
SQL_ACCOUNT = "$MT2_DB_HOST account $MT2_DB_USER $MT2_DB_PASS $MT2_DB_PORT"
SQL_HOTBACKUP = "$MT2_DB_HOST player $MT2_DB_USER $MT2_DB_PASS $MT2_DB_PORT"
SQL_PLAYER = "$MT2_DB_HOST player $MT2_DB_USER $MT2_DB_PASS $MT2_DB_PORT"
SQL_COMMON = "$MT2_DB_HOST common $MT2_DB_USER $MT2_DB_PASS $MT2_DB_PORT"
SQL_LOG = "$MT2_DB_HOST log $MT2_DB_USER $MT2_DB_PASS $MT2_DB_PORT"
SQL_WORLD = "$MT2_DB_HOST world $MT2_DB_USER $MT2_DB_PASS $MT2_DB_PORT"
TABLE_POSTFIX = ""
BIND_PORT = $MT2_DB_LISTEN_PORT
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
    chmod 600 "$TARGET_DIR/db/conf.txt"

    cat > "$TARGET_DIR/auth/CONFIG" <<EOF
HOSTNAME: auth
CHANNEL: 1
PORT: $MT2_AUTH_PORT
P2P_PORT: $MT2_AUTH_P2P_PORT
DB_PORT: $MT2_DB_LISTEN_PORT
DB_ADDR: $MT2_DB_ADDR
PASSES_PER_SEC: 25
SAVE_EVENT_SECOND_CYCLE: 180
PING_EVENT_SECOND_CYCLE: 180
TABLE_POSTFIX: 
ITEM_ID_RANGE: 000000001 000000002
AUTH_SERVER: master
ACCOUNT_SQL: $MT2_DB_HOST $MT2_DB_USER $MT2_DB_PASS account $MT2_DB_PORT
COMMON_SQL: $MT2_DB_HOST $MT2_DB_USER $MT2_DB_PASS common $MT2_DB_PORT
LOG_SQL: $MT2_DB_HOST $MT2_DB_USER $MT2_DB_PASS log $MT2_DB_PORT
SQL_HOTBACKUP: $MT2_DB_HOST $MT2_DB_USER $MT2_DB_PASS player $MT2_DB_PORT
PLAYER_SQL: $MT2_DB_HOST $MT2_DB_USER $MT2_DB_PASS player $MT2_DB_PORT
SQL_WORLD: $MT2_DB_HOST $MT2_DB_USER $MT2_DB_PASS world $MT2_DB_PORT
LOCALE_SERVICE: $MT2_LOCALE_SERVICE
BIND_IP: $MT2_BIND_IP
EOF
    chmod 600 "$TARGET_DIR/auth/CONFIG"

    write_ch_config() {
        ch_name="$1"
        hostname_val="$2"
        channel_num="$3"
        port_num="$4"
        p2p_num="$5"
        map_allow="$6"

        cat > "$TARGET_DIR/chs/$ch_name/CONFIG" <<EOF
HOSTNAME: $hostname_val
CHANNEL: $channel_num
PORT: $port_num
P2P_PORT: $p2p_num
DB_PORT: $MT2_DB_LISTEN_PORT
DB_ADDR: $MT2_DB_ADDR
MAP_ALLOW: $map_allow
TABLE_POSTFIX: 
PASSES_PER_SEC: 25
SAVE_EVENT_SECOND_CYCLE: 180
PING_EVENT_SECOND_CYCLE: 180
ACCOUNT_SQL: $MT2_DB_HOST $MT2_DB_USER $MT2_DB_PASS account $MT2_DB_PORT
COMMON_SQL: $MT2_DB_HOST $MT2_DB_USER $MT2_DB_PASS common $MT2_DB_PORT
LOG_SQL: $MT2_DB_HOST $MT2_DB_USER $MT2_DB_PASS log $MT2_DB_PORT
SQL_HOTBACKUP: $MT2_DB_HOST $MT2_DB_USER $MT2_DB_PASS player $MT2_DB_PORT
PLAYER_SQL: $MT2_DB_HOST $MT2_DB_USER $MT2_DB_PASS player $MT2_DB_PORT
SQL_WORLD: $MT2_DB_HOST $MT2_DB_USER $MT2_DB_PASS world $MT2_DB_PORT
SERVER_KEY: $MT2_SERVER_KEY
MALL_URL: localhost
ADMINPAGE_IP: $MT2_ADMINPAGE_IP
ADMINPAGE_PASSWORD: $MT2_ADMINPAGE_PASSWORD
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
BIND_IP: $MT2_BIND_IP
EOF
        chmod 600 "$TARGET_DIR/chs/$ch_name/CONFIG"
    }

    write_ch_config "ch11" "channel1"  "1"  "28000" "29000" "1 21 41"
    write_ch_config "ch12" "channel1"  "1"  "28001" "29001" "3 23 43 4 24 44 63 65 67 68 64 61 62 70 104"
    write_ch_config "ch13" "channel1"  "1"  "28002" "29002" "66 69 79 112 181 182 183 5 25 45 108 109"
    write_ch_config "ch99" "channel99" "99" "13099" "14099" "110 111 81 113 200 90"
    write_ch_config "ch00" "channel00" "99" "13088" "14088" "250"
    write_ch_config "ch01" "channel01" "99" "13098" "14098" "71"
fi

if [ "$INIT_SQL" -eq 1 ]; then
    echo "=== [4/4] Non-Destructive 5-Database Schema Initialization ==="
    DB_CLI=$(which mariadb 2>/dev/null || which mysql 2>/dev/null)
    if [ -z "$DB_CLI" ]; then
        echo "[ERROR] Neither mariadb nor mysql CLI client found in PATH." >&2
        exit 1
    fi
    INIT_USER="${MT2_INIT_DB_USER:-$MT2_DB_USER}"
    INIT_PASS="${MT2_INIT_DB_PASS:-$MT2_DB_PASS}"
    TMP_CNF="$TARGET_DIR/.sql_init_client.cnf"
    umask 077
    cat > "$TMP_CNF" <<EOF
[client]
host=$MT2_DB_HOST
port=$MT2_DB_PORT
user=$INIT_USER
password="$INIT_PASS"
EOF
    chmod 600 "$TMP_CNF"
    STRICT_MODE="STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION"
    for db_name in account common log player world; do
        tbl_count=$("$DB_CLI" "--defaults-extra-file=$TMP_CNF" -N -s \
            -e "SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA='${db_name}';" | tr -d ' \t\r\n')
        if [ "$tbl_count" = "0" ]; then
            echo "  -> Loading sql/${db_name}.sql into empty database '${db_name}'..."
            "$DB_CLI" "--defaults-extra-file=$TMP_CNF" "$db_name" \
                --init-command="SET SESSION sql_mode='$STRICT_MODE';" < "$REPO_ROOT/sql/${db_name}.sql"
        else
            echo "  -> Database '${db_name}' already contains ${tbl_count} tables; skipping destructive reload."
        fi
    done
    rm -f "$TMP_CNF"
fi

echo "=== [OK] FreeBSD 15.1 x64 Serverfiles Installed at $TARGET_DIR ==="
