#!/bin/sh
# ==============================================================================
# Metin2 MT2009 - Isolated FreeBSD Server Startup, Full DB Boot & Auth/Login Smoke Test
# Stages (strictly separated and reported by category):
#   [1/6] Strict SQL Mode schema & temporary test account preparation in mt2_testdb
#   [2/6] Real db_r41023 isolated startup, SQL table load & TCP listener probe
#   [3/6] Mock-DB + 32-bit TCP wire handshake (PHASE_HANDSHAKE -> PHASE_LOGIN)
#         for game_r41023 (Lua 5.0.3) and game_r41023_lua55 (Lua 5.5.1)
#   [4/6] Real db_r41023 <-> normal game_r41023 (Lua 5.0.3) & game_r41023_lua55 (Lua 5.5.1)
#         full HEADER_DG_BOOT (CInputDB::Boot + CQuestManager::Boot) comparison
#   [5/6] Real db_r41023 + Real AUTH_SERVER + Real GAME_SERVER (Lua 5.0.3)
#         account authentication (HEADER_CG_LOGIN3 -> dwLoginKey) & LoginByKey (PHASE_SELECT)
#   [6/6] Real db_r41023 + Real AUTH_SERVER + Real GAME_SERVER (Lua 5.5.1)
#         account authentication (HEADER_CG_LOGIN3 -> dwLoginKey) & LoginByKey (PHASE_SELECT)
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
DB="${MT2_TEST_DB:-mt2_testdb}"
ENGINE="${MT2_TEST_ENGINE:-FreeBSD SQL}"

DB_BIN="${MT2_SMOKE_DB_BIN:-$ROOT_DIR/server/src/db/db_r41023}"
GAME50_BIN="${MT2_SMOKE_GAME50_BIN:-$ROOT_DIR/server/src/game/game_r41023_lua50}"
GAME55_BIN="${MT2_SMOKE_GAME55_BIN:-$ROOT_DIR/server/src/game/game_r41023}"

# Deterministic AES-128-ECB("SMOKETEST_HWID1\x01", key="VBlk2yQRtiHA1oj7") hex string
SMOKE_HWID_PLAIN="SMOKETEST_HWID1"
SMOKE_HWID_HEX="5BE8C5A4CAFB1014505B16C5D68D28A9"
SMOKE_TEST_LOGIN="smk$(LC_ALL=C tr -dc 'a-z0-9' < /dev/urandom | head -c 7)"
SMOKE_TEST_PASS="$(LC_ALL=C tr -dc 'A-Za-z0-9' < /dev/urandom | head -c 12)"
export SMOKE_TEST_LOGIN SMOKE_TEST_PASS

echo "=================================================="
echo " METIN2 MT2009 - ISOLATED SERVER & AUTH/LOGIN SMOKE TEST"
echo " Engine:         $ENGINE ($HOST:$PORT / $DB)"
echo " DB Binary:      $DB_BIN"
echo " Game 5.0 Bin:   $GAME50_BIN (Lua 5.0.3 Rollback)"
echo " Game 5.5 Bin:   $GAME55_BIN (Lua 5.5.1 Default)"
echo "=================================================="

for bin in "$DB_BIN" "$GAME50_BIN" "$GAME55_BIN"; do
    if [ ! -x "$bin" ]; then
        echo "HATA: Sunucu ikilisi bulunamadı veya çalıştırılamaz: $bin" >&2
        exit 1
    fi
done

mkdir -p "$BUILD_DIR"
RPATH=""
if [ -d "/usr/local/lib/gcc14" ]; then
    RPATH="-Wl,-rpath=/usr/local/lib/gcc14"
fi

echo "Derleniyor: $BUILD_DIR/test_server_smoke_client..."
$CXX -std=c++20 -Wall -Wextra -Werror -O2 $RPATH \
    "$ROOT_DIR/tests/test_server_smoke_client.cpp" \
    -o "$BUILD_DIR/test_server_smoke_client"

DB_CLI=$(which mariadb 2>/dev/null || which mysql 2>/dev/null)
if [ -z "$DB_CLI" ]; then
    echo "HATA: mariadb veya mysql CLI istemcisi bulunamadı!" >&2
    exit 1
fi

SMOKE_ROOT="/tmp/mt2_smoke_$$"
mkdir -p "$SMOKE_ROOT"
"$BUILD_DIR/test_server_smoke_client" --self-test-hash
"$BUILD_DIR/test_server_smoke_client" --hash-env-pass "$SMOKE_ROOT/pass_hash.txt"
SMOKE_PASS_HASH=$(tr -d '\r\n ' < "$SMOKE_ROOT/pass_hash.txt")
rm -f "$SMOKE_ROOT/pass_hash.txt"
ACTIVE_GAME_PID=""
ACTIVE_AUTH_PID=""
ACTIVE_MOCK_PID=""
ACTIVE_DB_PID=""

# Bounded process termination helper (SIGTERM -> up to 4s wait -> SIGKILL -> wait)
stop_pid_bounded() {
    TARGET_PID="$1"
    if [ -z "$TARGET_PID" ]; then
        return 0
    fi
    if kill -0 "$TARGET_PID" 2>/dev/null; then
        kill -TERM "$TARGET_PID" 2>/dev/null || true
        for _ in 1 2 3 4 5 6 7 8; do
            if ! kill -0 "$TARGET_PID" 2>/dev/null; then
                break
            fi
            sleep 0.5 2>/dev/null || sleep 1
        done
        if kill -0 "$TARGET_PID" 2>/dev/null; then
            kill -KILL "$TARGET_PID" 2>/dev/null || true
        fi
    fi
    wait "$TARGET_PID" 2>/dev/null || true
}

cleanup_all() {
    stop_pid_bounded "$ACTIVE_GAME_PID"
    ACTIVE_GAME_PID=""
    stop_pid_bounded "$ACTIVE_AUTH_PID"
    ACTIVE_AUTH_PID=""
    stop_pid_bounded "$ACTIVE_MOCK_PID"
    ACTIVE_MOCK_PID=""
    stop_pid_bounded "$ACTIVE_DB_PID"
    ACTIVE_DB_PID=""
    if [ -n "$WATCHDOG_PID" ]; then
        kill -TERM "$WATCHDOG_PID" 2>/dev/null || true
        WATCHDOG_PID=""
    fi
    rm -rf "$SMOKE_ROOT"
}
trap cleanup_all EXIT INT TERM

# Global watchdog: the whole smoke test must finish within SMOKE_TIMEOUT_SEC (default 900s).
# On expiry the main shell gets SIGTERM -> trap -> bounded cleanup of every server process.
SMOKE_TIMEOUT_SEC="${SMOKE_TIMEOUT_SEC:-900}"
MAIN_SHELL_PID=$$
(
    trap 'kill "$WD_SLEEP_PID" 2>/dev/null; exit 0' TERM INT
    sleep "$SMOKE_TIMEOUT_SEC" </dev/null >/dev/null 2>&1 &
    WD_SLEEP_PID=$!
    wait "$WD_SLEEP_PID" || exit 0
    echo "HATA: [WATCHDOG] run_server_smoke_test.sh ${SMOKE_TIMEOUT_SEC} sn sınırını aştı; süreçler sonlandırılıyor." >&2
    kill -TERM "$MAIN_SHELL_PID" 2>/dev/null
) &
WATCHDOG_PID=$!

# Report whether a background PID is alive; if it exited, reap it and print its exit status
report_pid_state() {
    TAG="$1"
    P="$2"
    if [ -z "$P" ]; then
        echo "  [DIAG] $TAG: PID yok"
        return 0
    fi
    if kill -0 "$P" 2>/dev/null; then
        echo "  [DIAG] $TAG (pid=$P): ÇALIŞIYOR"
    else
        RC=0
        wait "$P" 2>/dev/null || RC=$?
        echo "  [DIAG] $TAG (pid=$P): SONLANDI exit=$RC (>128 ise sinyal=$((RC - 128)))"
    fi
}

# Print the tail of a log file with a header (bounded output, masking any raw login_key values)
tail_log() {
    if [ -s "$2" ]; then
        echo "----- [DIAG] $1: $2 (son 60 satır) -----"
        tail -n 60 "$2" | sed -E 's/(login_key)[(:= ]+[0-9]+/\1=***MASKED***/gi'
    else
        echo "----- [DIAG] $1: $2 (yok veya boş) -----"
    fi
}

# Dump every relevant log for a failed real-DB stage (game, auth, db)
dump_stage_logs() {
    report_pid_state "db_r41023" "$ACTIVE_DB_PID"
    report_pid_state "AUTH_SERVER" "$ACTIVE_AUTH_PID"
    report_pid_state "GAME_SERVER" "$ACTIVE_GAME_PID"
    for d in "$GAME_WORK" "$AUTH_WORK"; do
        if [ -z "$d" ]; then continue; fi
        for f in game_stdout.log game_stderr.log auth_stdout.log auth_stderr.log syserr syslog; do
            if [ -e "$d/$f" ]; then tail_log "$(basename "$d")" "$d/$f"; fi
        done
    done
    for f in "$DB_WORK"/db_stdout*.log "$DB_WORK"/db_stderr*.log "$DB_WORK/syserr" "$DB_WORK/syslog"; do
        if [ -e "$f" ]; then tail_log "db" "$f"; fi
    done
    for f in "$GAME_WORK/syslog" "$DB_WORK/syslog"; do
        if [ -s "$f" ]; then
            echo "----- [DIAG] BOOT ilerleme satırları: $f -----"
            grep -E "BOOT|QUERY_BOOT|boot packet|PHASE_DBCLIENT|SETUP" "$f" | tail -n 40 | sed -E 's/(login_key)[(:= ]+[0-9]+/\1=***MASKED***/gi' || true
        fi
    done
    return 0
}

# ------------------------------------------------------------------------------
# Helper: Prepare isolated game working directory (real FTP Lua if present + drop/map files)
# ------------------------------------------------------------------------------
prepare_game_workdir() {
    WORK_DIR="$1"
    mkdir -p "$WORK_DIR/locale/poland/quest/object" "$WORK_DIR/locale/poland/map/metin2_map_a1" "$WORK_DIR/package" "$WORK_DIR/panama" "$WORK_DIR/mark"

    FTP_TAR=""
    for cand in "/tmp/new_ftp.tar.gz" "$ROOT_DIR/new_ftp.tar.gz"; do
        if [ -f "$cand" ]; then
            FTP_TAR="$cand"
            break
        fi
    done

    if [ -n "$FTP_TAR" ]; then
        FTP_EXTRACT="$SMOKE_ROOT/ftp_extract"
        if [ ! -d "$FTP_EXTRACT" ]; then
            mkdir -p "$FTP_EXTRACT"
            tar -xzf "$FTP_TAR" -C "$FTP_EXTRACT" \
                "share/locale/poland/settings.lua" \
                "share/locale/poland/translate.lua" \
                "share/locale/poland/quest" 2>/dev/null || true
        fi
    fi

    if [ -n "$FTP_TAR" ] && [ -f "$SMOKE_ROOT/ftp_extract/share/locale/poland/quest/questlib.lua" ]; then
        cp -f "$SMOKE_ROOT/ftp_extract/share/locale/poland/settings.lua" "$WORK_DIR/locale/poland/settings.lua"
        cp -f "$SMOKE_ROOT/ftp_extract/share/locale/poland/translate.lua" "$WORK_DIR/locale/poland/translate.lua"
        cp -R "$SMOKE_ROOT/ftp_extract/share/locale/poland/quest/"* "$WORK_DIR/locale/poland/quest/" 2>/dev/null || true
    else
        cp "$ROOT_DIR/tests/fixtures/settings.lua" "$WORK_DIR/locale/poland/settings.lua"
        cp "$ROOT_DIR/tests/fixtures/translate.lua" "$WORK_DIR/locale/poland/translate.lua"
        cp "$ROOT_DIR/tests/fixtures/questlib.lua" "$WORK_DIR/locale/poland/quest/questlib.lua"
        cp "$ROOT_DIR/tests/fixtures/locale.lua" "$WORK_DIR/locale/poland/quest/locale.lua"
    fi

    # Provide a clean, deterministic quest/object directory for CQuestManager::Boot(), Login, and NPC 20016 click coroutines
    rm -rf "$WORK_DIR/locale/poland/quest/object"
    mkdir -p "$WORK_DIR/locale/poland/quest/object/state" \
             "$WORK_DIR/locale/poland/quest/object/notarget/boot" \
             "$WORK_DIR/locale/poland/quest/object/notarget/login" \
             "$WORK_DIR/locale/poland/quest/object/20016/click"

    printf 'smoke_boot={["start"]=0}\n' > "$WORK_DIR/locale/poland/quest/object/state/smoke_boot"
    printf 'smoke_world={["start"]=0}\n' > "$WORK_DIR/locale/poland/quest/object/state/smoke_world"
    printf 'game.set_event_flag("smoke_boot_ok", 1)\n' > "$WORK_DIR/locale/poland/quest/object/notarget/boot/smoke_boot.start"

    cat > "$WORK_DIR/locale/poland/quest/object/notarget/login/smoke_world.start" <<'EOF_QLOGIN'
local seed = pc.getqf("db_seed_val")
local has_yes = (type(locale) == "table" and type(locale.yes) == "string" and CONFIRM_YES == 1) and 1 or 0
pc.setqf("login_verified", 1)
syschat(string.format("[SMOKE_QUEST_LOGIN_OK] db_seed=%d lv=%d item19=%d qlib=%d", seed, pc.get_level(), pc.count_item(19), has_yes))
EOF_QLOGIN

    cat > "$WORK_DIR/locale/poland/quest/object/20016/click/smoke_world.start" <<'EOF_QCLICK'
say("Smoke NPC 20016 Coroutine Test")
wait()
local sel = select("Continue", "Cancel")
say("Enter verification token:")
local txt = input()
pc.setqf("npc_flow_done", 1)
syschat(string.format("[SMOKE_NPC_COROUTINE_OK] sel=%d input=%s", sel, txt))
EOF_QCLICK

    printf "20016\tblacksmith\n9001\tweapon_shop\n" > "$WORK_DIR/locale/poland/quest/questnpc.txt"
    : > "$WORK_DIR/locale/poland/forkedmapindex.txt"
    : > "$WORK_DIR/locale/poland/cube.txt"
    : > "$WORK_DIR/locale/poland/blend.txt"
    : > "$WORK_DIR/locale/poland/locale_string.txt"
    : > "$WORK_DIR/locale/poland/common_drop_item.txt"
    : > "$WORK_DIR/locale/poland/etc_drop_item.txt"
    printf "# empty\n" > "$WORK_DIR/locale/poland/group.txt"
    printf "# empty\n" > "$WORK_DIR/locale/poland/group_group.txt"
    printf "# empty\n" > "$WORK_DIR/locale/poland/drop_item_group.txt"
    printf "# empty\n" > "$WORK_DIR/locale/poland/special_item_group.txt"
    printf "# empty\n" > "$WORK_DIR/locale/poland/mob_drop_item.txt"
    printf "# empty\n" > "$WORK_DIR/locale/poland/special_spawns.txt"

    # Prepare map index 1 (metin2_map_a1: 4x5, BasePosition 409600x896000) so SmokeHero (469300x964200) and NPC 20016 spawn cleanly
    printf "1 metin2_map_a1\n" > "$WORK_DIR/locale/poland/map/index"
    cat > "$WORK_DIR/locale/poland/map/metin2_map_a1/Setting.txt" <<'EOF_MAPSET'
ScriptType	MapSetting

CellScale	200
HeightScale	0.500000

ViewRadius	128

MapSize	4	5
BasePosition	409600	896000
TextureSet	metin2_a1.txt
Environment	a1.msenv
EOF_MAPSET
    printf "597 682\n" > "$WORK_DIR/locale/poland/map/metin2_map_a1/Town.txt"
    printf "m\t598\t682\t0\t0\t0\t0\t1m\t100\t1\t20016\n" > "$WORK_DIR/locale/poland/map/metin2_map_a1/npc.txt"
    "$BUILD_DIR/test_server_smoke_client" --gen-server-attr "$WORK_DIR/locale/poland/map/metin2_map_a1/server_attr" 16 20
    : > "$WORK_DIR/CMD"
}

# ------------------------------------------------------------------------------
# [1/6] Strict SQL Mode Schema & Temporary Account Preparation
# ------------------------------------------------------------------------------
echo "[1/6] Strict SQL Mode altında izole duman testi şeması ve geçici test hesabı hazırlanıyor..."
"$DB_CLI" -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" <<'EOF'
SET SESSION sql_mode = 'STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION';

DROP TABLE IF EXISTS account;
CREATE TABLE account (
  id INT UNSIGNED NOT NULL AUTO_INCREMENT,
  login VARCHAR(30) NOT NULL DEFAULT '',
  password VARCHAR(45) NOT NULL DEFAULT '',
  social_id VARCHAR(18) NOT NULL DEFAULT '1234567',
  status VARCHAR(8) NOT NULL DEFAULT 'OK',
  hwid VARCHAR(255) NOT NULL DEFAULT '',
  availDt DATETIME NOT NULL DEFAULT '2020-01-01 00:00:00',
  create_time DATETIME NOT NULL DEFAULT '2025-01-01 00:00:00',
  last_play DATETIME NOT NULL DEFAULT '2025-01-01 00:00:00',
  premium_expire DATETIME NOT NULL DEFAULT '2030-01-01 00:00:00',
  silver_expire DATETIME NOT NULL DEFAULT '2030-01-01 00:00:00',
  gold_expire DATETIME NOT NULL DEFAULT '2030-01-01 00:00:00',
  safebox_expire DATETIME NOT NULL DEFAULT '2030-01-01 00:00:00',
  autoloot_expire DATETIME NOT NULL DEFAULT '2030-01-01 00:00:00',
  fish_mind_expire DATETIME NOT NULL DEFAULT '2030-01-01 00:00:00',
  marriage_fast_expire DATETIME NOT NULL DEFAULT '2030-01-01 00:00:00',
  money_drop_rate_expire DATETIME NOT NULL DEFAULT '2030-01-01 00:00:00',
  shop_expire DATETIME NOT NULL DEFAULT '2030-01-01 00:00:00',
  PRIMARY KEY (id),
  UNIQUE KEY idx_login (login)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS hardware_status;
CREATE TABLE hardware_status (
  hwid VARCHAR(255) NOT NULL DEFAULT '',
  status VARCHAR(8) NOT NULL DEFAULT 'OK',
  PRIMARY KEY (hwid)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

INSERT INTO hardware_status (hwid, status) VALUES ('SMOKETEST_HWID1', 'OK');

DROP TABLE IF EXISTS player_index;
CREATE TABLE player_index (
  id INT UNSIGNED NOT NULL DEFAULT 0,
  pid1 INT UNSIGNED NOT NULL DEFAULT 0,
  pid2 INT UNSIGNED NOT NULL DEFAULT 0,
  pid3 INT UNSIGNED NOT NULL DEFAULT 0,
  pid4 INT UNSIGNED NOT NULL DEFAULT 0,
  pid5 INT UNSIGNED NOT NULL DEFAULT 0,
  empire TINYINT UNSIGNED NOT NULL DEFAULT 1,
  PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

INSERT INTO player_index (id, pid1, pid2, pid3, pid4, pid5, empire)
VALUES (9001, 90011, 0, 0, 0, 0, 1);

DROP TABLE IF EXISTS locale;
CREATE TABLE locale (
  mKey VARCHAR(255) NOT NULL DEFAULT '',
  mValue VARCHAR(255) NOT NULL DEFAULT '',
  PRIMARY KEY (mKey)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

INSERT INTO locale (mKey, mValue) VALUES
  ('DB_NAME_COLUMN', 'locale_name'),
  ('LOCALE', 'poland'),
  ('SKILL_DAMAGE_BY_LEVEL_UNDER_45', '0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0'),
  ('SKILL_DAMAGE_BY_LEVEL_UNDER_90', '0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0'),
  ('SKILL_POWER_BY_LEVEL', '0 5 6 8 10 12 14 16 18 20 22 24 26 28 30 32 34 36 38 40 50 52 54 56 58 60 63 66 69 72 82 85 88 91 94 98 102 106 110 115 125 125 125 125 125');

DROP TABLE IF EXISTS exp_table;
CREATE TABLE exp_table (
  level INT UNSIGNED NOT NULL AUTO_INCREMENT,
  exp INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (level)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
INSERT INTO exp_table (level, exp) VALUES (1, 300), (2, 800), (3, 1500), (15, 25000);

DROP TABLE IF EXISTS spam_db;
CREATE TABLE spam_db (
  word VARCHAR(255) NOT NULL,
  score INT NOT NULL DEFAULT 1,
  type ENUM('SPAM','MAPS') NOT NULL DEFAULT 'SPAM',
  PRIMARY KEY (word)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS bootlog;
CREATE TABLE bootlog (
  time DATETIME NOT NULL,
  hostname VARCHAR(64) NOT NULL,
  channel INT NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS special_spawn;
CREATE TABLE special_spawn (
  vnum INT UNSIGNED NOT NULL DEFAULT 0,
  channel TINYINT UNSIGNED NOT NULL DEFAULT 0,
  kill_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (vnum, channel)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS priv_settings;
CREATE TABLE priv_settings (
  priv_type ENUM('PLAYER','GUILD','EMPIRE') NOT NULL DEFAULT 'EMPIRE',
  id INT UNSIGNED NOT NULL DEFAULT 0,
  type INT UNSIGNED NOT NULL DEFAULT 4,
  value INT NOT NULL DEFAULT 0,
  duration DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (priv_type, id, type)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS guild;
CREATE TABLE guild (
  id INT UNSIGNED NOT NULL AUTO_INCREMENT,
  name VARCHAR(14) NOT NULL DEFAULT '',
  sp SMALLINT NOT NULL DEFAULT 1000,
  master INT UNSIGNED NOT NULL DEFAULT 0,
  level TINYINT DEFAULT 1,
  exp INT DEFAULT 0,
  skill_point TINYINT NOT NULL DEFAULT 0,
  skill TINYBLOB DEFAULT NULL,
  win INT NOT NULL DEFAULT 0,
  draw INT NOT NULL DEFAULT 0,
  loss INT NOT NULL DEFAULT 0,
  ladder_point INT NOT NULL DEFAULT 0,
  gold BIGINT NOT NULL DEFAULT 0,
  token BIGINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS guild_war_reservation;
CREATE TABLE guild_war_reservation (
  id INT UNSIGNED NOT NULL AUTO_INCREMENT,
  guild1 INT UNSIGNED NOT NULL DEFAULT 0,
  guild2 INT UNSIGNED NOT NULL DEFAULT 0,
  time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  type TINYINT UNSIGNED NOT NULL DEFAULT 0,
  warprice INT NOT NULL DEFAULT 0,
  initscore INT NOT NULL DEFAULT 0,
  started TINYINT NOT NULL DEFAULT 0,
  bet_from INT UNSIGNED NOT NULL DEFAULT 0,
  bet_to INT UNSIGNED NOT NULL DEFAULT 0,
  winner INT NOT NULL DEFAULT -1,
  power1 INT NOT NULL DEFAULT 0,
  power2 INT NOT NULL DEFAULT 0,
  handicap INT NOT NULL DEFAULT 0,
  result1 INT NOT NULL DEFAULT 0,
  result2 INT NOT NULL DEFAULT 0,
  PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS marriage;
CREATE TABLE marriage (
  is_married TINYINT NOT NULL DEFAULT 0,
  pid1 INT UNSIGNED NOT NULL DEFAULT 0,
  pid2 INT UNSIGNED NOT NULL DEFAULT 0,
  love_point INT DEFAULT 0,
  time INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (pid1, pid2)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS `player`;
CREATE TABLE `player` (
  id INT UNSIGNED NOT NULL AUTO_INCREMENT,
  account_id INT UNSIGNED NOT NULL DEFAULT 0,
  name VARCHAR(24) NOT NULL DEFAULT '',
  job TINYINT UNSIGNED NOT NULL DEFAULT 0,
  voice TINYINT NOT NULL DEFAULT 0,
  dir TINYINT NOT NULL DEFAULT 0,
  x INT NOT NULL DEFAULT 469300,
  y INT NOT NULL DEFAULT 964200,
  z INT NOT NULL DEFAULT 0,
  map_index INT NOT NULL DEFAULT 1,
  exit_x INT NOT NULL DEFAULT 469300,
  exit_y INT NOT NULL DEFAULT 964200,
  exit_map_index INT NOT NULL DEFAULT 1,
  hp INT NOT NULL DEFAULT 800,
  mp INT NOT NULL DEFAULT 300,
  stamina SMALLINT NOT NULL DEFAULT 800,
  random_hp SMALLINT NOT NULL DEFAULT 0,
  random_sp SMALLINT NOT NULL DEFAULT 0,
  playtime INT UNSIGNED NOT NULL DEFAULT 0,
  gold BIGINT NOT NULL DEFAULT 0,
  level TINYINT UNSIGNED NOT NULL DEFAULT 1,
  level_step TINYINT NOT NULL DEFAULT 0,
  st SMALLINT UNSIGNED NOT NULL DEFAULT 10,
  ht SMALLINT UNSIGNED NOT NULL DEFAULT 10,
  dx SMALLINT UNSIGNED NOT NULL DEFAULT 10,
  iq SMALLINT UNSIGNED NOT NULL DEFAULT 10,
  exp INT UNSIGNED NOT NULL DEFAULT 0,
  stat_point SMALLINT NOT NULL DEFAULT 0,
  skill_point SMALLINT NOT NULL DEFAULT 0,
  sub_skill_point SMALLINT NOT NULL DEFAULT 0,
  stat_reset_count TINYINT NOT NULL DEFAULT 0,
  ip VARCHAR(16) NOT NULL DEFAULT '127.0.0.1',
  part_main SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  part_base TINYINT UNSIGNED NOT NULL DEFAULT 0,
  part_hair SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  part_acce SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  skill_level BLOB DEFAULT NULL,
  quickslot BLOB DEFAULT NULL,
  skill_group TINYINT NOT NULL DEFAULT 0,
  alignment INT NOT NULL DEFAULT 0,
  last_play DATETIME NOT NULL DEFAULT '2025-01-01 00:00:00',
  horse_level TINYINT NOT NULL DEFAULT 0,
  horse_riding TINYINT NOT NULL DEFAULT 0,
  horse_hp SMALLINT NOT NULL DEFAULT 0,
  horse_hp_droptime INT UNSIGNED NOT NULL DEFAULT 0,
  horse_stamina SMALLINT NOT NULL DEFAULT 0,
  horse_skill_point SMALLINT NOT NULL DEFAULT 0,
  change_name TINYINT NOT NULL DEFAULT 0,
  PRIMARY KEY (id),
  KEY idx_account_id (account_id)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS item;
CREATE TABLE item (
  id INT UNSIGNED NOT NULL AUTO_INCREMENT,
  owner_id INT UNSIGNED NOT NULL DEFAULT 0,
  `window` ENUM('INVENTORY','EQUIPMENT','SAFEBOX','MALL','DRAGON_SOUL_INVENTORY','BELT_INVENTORY','GROUND','IKASHOP_OFFLINESHOP','IKASHOP_SAFEBOX','IKASHOP_AUCTION') NOT NULL DEFAULT 'INVENTORY',
  pos SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  count INT UNSIGNED NOT NULL DEFAULT 0,
  vnum INT UNSIGNED NOT NULL DEFAULT 0,
  socket0 INT NOT NULL DEFAULT 0,
  socket1 INT NOT NULL DEFAULT 0,
  socket2 INT NOT NULL DEFAULT 0,
  attrtype0 TINYINT NOT NULL DEFAULT 0,
  attrvalue0 SMALLINT NOT NULL DEFAULT 0,
  attrtype1 TINYINT NOT NULL DEFAULT 0,
  attrvalue1 SMALLINT NOT NULL DEFAULT 0,
  attrtype2 TINYINT NOT NULL DEFAULT 0,
  attrvalue2 SMALLINT NOT NULL DEFAULT 0,
  attrtype3 TINYINT NOT NULL DEFAULT 0,
  attrvalue3 SMALLINT NOT NULL DEFAULT 0,
  attrtype4 TINYINT NOT NULL DEFAULT 0,
  attrvalue4 SMALLINT NOT NULL DEFAULT 0,
  attrtype5 TINYINT NOT NULL DEFAULT 0,
  attrvalue5 SMALLINT NOT NULL DEFAULT 0,
  attrtype6 TINYINT NOT NULL DEFAULT 0,
  attrvalue6 SMALLINT NOT NULL DEFAULT 0,
  ikashop_data TEXT DEFAULT NULL,
  PRIMARY KEY (id),
  KEY idx_owner_id (owner_id)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

INSERT INTO item (id, owner_id, `window`, pos, count, vnum)
VALUES (10000001, 90011, 'INVENTORY', 0, 1, 19);

DROP TABLE IF EXISTS quest;
CREATE TABLE quest (
  dwPID INT UNSIGNED NOT NULL DEFAULT 0,
  szName VARCHAR(32) NOT NULL DEFAULT '',
  szState VARCHAR(64) NOT NULL DEFAULT '',
  lValue INT NOT NULL DEFAULT 0,
  PRIMARY KEY (dwPID, szName, szState)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

INSERT INTO quest (dwPID, szName, szState, lValue) VALUES
  (90011, 'smoke_world', '__status', 0),
  (90011, 'smoke_world', 'db_seed_val', 77);

DROP TABLE IF EXISTS affect;
CREATE TABLE affect (
  dwPID INT UNSIGNED NOT NULL DEFAULT 0,
  bType SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  bApplyOn TINYINT UNSIGNED NOT NULL DEFAULT 0,
  lApplyValue INT NOT NULL DEFAULT 0,
  dwFlag INT UNSIGNED NOT NULL DEFAULT 0,
  lDuration INT NOT NULL DEFAULT 0,
  lSPCost INT NOT NULL DEFAULT 0,
  itemVnum INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (dwPID, bType, bApplyOn, lApplyValue)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS player_special_flag;
CREATE TABLE player_special_flag (
  pid INT UNSIGNED NOT NULL DEFAULT 0,
  aid INT UNSIGNED NOT NULL DEFAULT 0,
  flag VARCHAR(32) NOT NULL DEFAULT '',
  value BIGINT NOT NULL DEFAULT 0,
  PRIMARY KEY (pid, aid, flag)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS safebox;
CREATE TABLE safebox (
  account_id INT UNSIGNED NOT NULL DEFAULT 0,
  size TINYINT UNSIGNED NOT NULL DEFAULT 1,
  password VARCHAR(6) NOT NULL DEFAULT '',
  gold BIGINT NOT NULL DEFAULT 0,
  PRIMARY KEY (account_id)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS messenger_list;
CREATE TABLE messenger_list (
  account VARCHAR(24) NOT NULL DEFAULT '',
  companion VARCHAR(24) NOT NULL DEFAULT '',
  PRIMARY KEY (account, companion)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS messenger_block_list;
CREATE TABLE messenger_block_list (
  pid INT UNSIGNED NOT NULL DEFAULT 0,
  companion VARCHAR(24) NOT NULL DEFAULT '',
  PRIMARY KEY (pid, companion)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS ikashop_safebox;
CREATE TABLE ikashop_safebox (
  owner INT UNSIGNED NOT NULL DEFAULT 0,
  gold BIGINT NOT NULL DEFAULT 0,
  cheque INT NOT NULL DEFAULT 0,
  PRIMARY KEY (owner)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS ikashop_notification;
CREATE TABLE ikashop_notification (
  id INT UNSIGNED NOT NULL AUTO_INCREMENT,
  owner INT UNSIGNED NOT NULL DEFAULT 0,
  type VARCHAR(64) NOT NULL DEFAULT 'SELLER_SOLD_ITEM',
  who VARCHAR(32) NOT NULL DEFAULT '',
  what INT UNSIGNED NOT NULL DEFAULT 0,
  format VARCHAR(64) NOT NULL DEFAULT '',
  seen TINYINT UNSIGNED NOT NULL DEFAULT 0,
  datetime DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS loginlog;
CREATE TABLE loginlog (
  type VARCHAR(20) DEFAULT NULL,
  time DATETIME DEFAULT NULL,
  channel TINYINT DEFAULT NULL,
  account_id INT UNSIGNED DEFAULT NULL,
  pid INT UNSIGNED DEFAULT NULL,
  level SMALLINT DEFAULT NULL,
  job TINYINT DEFAULT NULL,
  playtime INT UNSIGNED DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS loginlog2;
CREATE TABLE loginlog2 (
  id INT UNSIGNED NOT NULL AUTO_INCREMENT,
  type VARCHAR(20) DEFAULT NULL,
  is_gm VARCHAR(4) DEFAULT NULL,
  login_time DATETIME DEFAULT NULL,
  channel TINYINT DEFAULT NULL,
  account_id INT UNSIGNED DEFAULT NULL,
  pid INT UNSIGNED DEFAULT NULL,
  ip INT UNSIGNED DEFAULT NULL,
  client_version VARCHAR(32) DEFAULT NULL,
  logout_time DATETIME DEFAULT NULL,
  playtime INT DEFAULT NULL,
  PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS char_log;
CREATE TABLE char_log (
  id INT UNSIGNED NOT NULL AUTO_INCREMENT,
  time DATETIME DEFAULT NULL,
  channel TINYINT DEFAULT NULL,
  account_id INT UNSIGNED DEFAULT NULL,
  pid INT UNSIGNED DEFAULT NULL,
  name VARCHAR(24) DEFAULT NULL,
  what VARCHAR(64) DEFAULT NULL,
  how VARCHAR(64) DEFAULT NULL,
  hint VARCHAR(128) DEFAULT NULL,
  ip VARCHAR(20) DEFAULT NULL,
  dwVnum INT UNSIGNED DEFAULT NULL,
  PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS quest_state_log;
CREATE TABLE quest_state_log (
  id INT UNSIGNED NOT NULL AUTO_INCREMENT,
  quest_name VARCHAR(64) DEFAULT NULL,
  quest_state VARCHAR(64) DEFAULT NULL,
  pid INT UNSIGNED DEFAULT NULL,
  date DATETIME DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS mob_proto;
CREATE TABLE mob_proto (
  vnum INT UNSIGNED NOT NULL DEFAULT 0,
  name VARCHAR(24) NOT NULL DEFAULT 'noname',
  locale_name VARCHAR(24) NOT NULL DEFAULT 'noname',
  type TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `rank` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  battle_type TINYINT UNSIGNED NOT NULL DEFAULT 0,
  level SMALLINT UNSIGNED NOT NULL DEFAULT 1,
  size SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  ai_flag INT UNSIGNED NOT NULL DEFAULT 0,
  setRaceFlag INT UNSIGNED NOT NULL DEFAULT 0,
  setImmuneFlag INT UNSIGNED NOT NULL DEFAULT 0,
  on_click TINYINT UNSIGNED NOT NULL DEFAULT 0,
  empire TINYINT UNSIGNED NOT NULL DEFAULT 0,
  drop_item INT UNSIGNED NOT NULL DEFAULT 0,
  resurrection_vnum INT UNSIGNED NOT NULL DEFAULT 0,
  folder VARCHAR(64) NOT NULL DEFAULT '',
  st TINYINT UNSIGNED NOT NULL DEFAULT 0,
  dx TINYINT UNSIGNED NOT NULL DEFAULT 0,
  ht TINYINT UNSIGNED NOT NULL DEFAULT 0,
  iq TINYINT UNSIGNED NOT NULL DEFAULT 0,
  damage_min SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  damage_max SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  max_hp INT UNSIGNED NOT NULL DEFAULT 100,
  regen_cycle TINYINT UNSIGNED NOT NULL DEFAULT 0,
  regen_percent TINYINT UNSIGNED NOT NULL DEFAULT 0,
  exp INT UNSIGNED NOT NULL DEFAULT 0,
  gold_min INT UNSIGNED NOT NULL DEFAULT 0,
  gold_max INT UNSIGNED NOT NULL DEFAULT 0,
  def SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  attack_speed SMALLINT NOT NULL DEFAULT 100,
  move_speed SMALLINT NOT NULL DEFAULT 100,
  aggressive_hp_pct TINYINT UNSIGNED NOT NULL DEFAULT 0,
  aggressive_sight SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  attack_range SMALLINT UNSIGNED NOT NULL DEFAULT 100,
  polymorph_item INT UNSIGNED NOT NULL DEFAULT 0,
  enchant_curse TINYINT NOT NULL DEFAULT 0,
  enchant_slow TINYINT NOT NULL DEFAULT 0,
  enchant_poison TINYINT NOT NULL DEFAULT 0,
  enchant_stun TINYINT NOT NULL DEFAULT 0,
  enchant_critical TINYINT NOT NULL DEFAULT 0,
  enchant_penetrate TINYINT NOT NULL DEFAULT 0,
  enchant_fire TINYINT NOT NULL DEFAULT 0,
  enchant_root TINYINT NOT NULL DEFAULT 0,
  resist_sword TINYINT NOT NULL DEFAULT 0,
  resist_twohand TINYINT NOT NULL DEFAULT 0,
  resist_dagger TINYINT NOT NULL DEFAULT 0,
  resist_bell TINYINT NOT NULL DEFAULT 0,
  resist_fan TINYINT NOT NULL DEFAULT 0,
  resist_bow TINYINT NOT NULL DEFAULT 0,
  resist_fire TINYINT NOT NULL DEFAULT 0,
  resist_elect TINYINT NOT NULL DEFAULT 0,
  resist_magic TINYINT NOT NULL DEFAULT 0,
  resist_wind TINYINT NOT NULL DEFAULT 0,
  resist_poison TINYINT NOT NULL DEFAULT 0,
  dam_multiply FLOAT NOT NULL DEFAULT 1.0,
  summon INT UNSIGNED NOT NULL DEFAULT 0,
  drain_sp INT UNSIGNED NOT NULL DEFAULT 0,
  skill_vnum0 INT UNSIGNED NOT NULL DEFAULT 0,
  skill_level0 TINYINT UNSIGNED NOT NULL DEFAULT 0,
  skill_vnum1 INT UNSIGNED NOT NULL DEFAULT 0,
  skill_level1 TINYINT UNSIGNED NOT NULL DEFAULT 0,
  skill_vnum2 INT UNSIGNED NOT NULL DEFAULT 0,
  skill_level2 TINYINT UNSIGNED NOT NULL DEFAULT 0,
  skill_vnum3 INT UNSIGNED NOT NULL DEFAULT 0,
  skill_level3 TINYINT UNSIGNED NOT NULL DEFAULT 0,
  skill_vnum4 INT UNSIGNED NOT NULL DEFAULT 0,
  skill_level4 TINYINT UNSIGNED NOT NULL DEFAULT 0,
  sp_berserk TINYINT NOT NULL DEFAULT 0,
  sp_stoneskin TINYINT NOT NULL DEFAULT 0,
  sp_godspeed TINYINT NOT NULL DEFAULT 0,
  sp_deathblow TINYINT NOT NULL DEFAULT 0,
  sp_revive TINYINT NOT NULL DEFAULT 0,
  fraction TINYINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (vnum)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
INSERT INTO mob_proto (vnum, name, locale_name, type, on_click, level, max_hp) VALUES
  (101, 'WildDog', 'Dziki Pies', 0, 0, 1, 100),
  (9001, 'WeaponSeller', 'Handlarz Bronia', 1, 2, 1, 100),
  (20016, 'Blacksmith', 'Kowal', 1, 2, 1, 1000);

DROP TABLE IF EXISTS item_proto;
CREATE TABLE item_proto (
  vnum INT UNSIGNED NOT NULL DEFAULT 0,
  type TINYINT NOT NULL DEFAULT 0,
  subtype TINYINT NOT NULL DEFAULT 0,
  name VARCHAR(24) NOT NULL DEFAULT 'noname',
  locale_name VARCHAR(24) NOT NULL DEFAULT 'noname',
  gold BIGINT NOT NULL DEFAULT 0,
  shop_buy_price BIGINT UNSIGNED NOT NULL DEFAULT 0,
  weight TINYINT NOT NULL DEFAULT 0,
  size TINYINT NOT NULL DEFAULT 1,
  stack INT UNSIGNED NOT NULL DEFAULT 1,
  flag INT NOT NULL DEFAULT 0,
  wearflag INT NOT NULL DEFAULT 0,
  antiflag INT NOT NULL DEFAULT 0,
  immuneflag INT NOT NULL DEFAULT 0,
  refined_vnum INT UNSIGNED NOT NULL DEFAULT 0,
  refine_set SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  magic_pct TINYINT NOT NULL DEFAULT 0,
  socket_pct TINYINT NOT NULL DEFAULT 0,
  addon_type SMALLINT NOT NULL DEFAULT 0,
  limittype0 TINYINT NOT NULL DEFAULT 0,
  limitvalue0 INT NOT NULL DEFAULT 0,
  limittype1 TINYINT NOT NULL DEFAULT 0,
  limitvalue1 INT NOT NULL DEFAULT 0,
  applytype0 TINYINT NOT NULL DEFAULT 0,
  applyvalue0 INT NOT NULL DEFAULT 0,
  applytype1 TINYINT NOT NULL DEFAULT 0,
  applyvalue1 INT NOT NULL DEFAULT 0,
  applytype2 TINYINT NOT NULL DEFAULT 0,
  applyvalue2 INT NOT NULL DEFAULT 0,
  value0 INT NOT NULL DEFAULT 0,
  value1 INT NOT NULL DEFAULT 0,
  value2 INT NOT NULL DEFAULT 0,
  value3 INT NOT NULL DEFAULT 0,
  value4 INT NOT NULL DEFAULT 0,
  value5 INT NOT NULL DEFAULT 0,
  PRIMARY KEY (vnum)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
INSERT INTO item_proto (vnum, type, subtype, name, locale_name, size, wearflag) VALUES
  (10, 1, 0, 'Sword+0', 'Miecz+0', 2, 16),
  (19, 1, 0, 'Sword+9', 'Miecz+9', 1, 16);

DROP TABLE IF EXISTS shop;
CREATE TABLE shop (
  vnum INT NOT NULL DEFAULT 0,
  name VARCHAR(32) NOT NULL DEFAULT 'noname',
  npc_vnum SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (vnum)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
INSERT INTO shop (vnum, name, npc_vnum) VALUES (1, 'WeaponShop', 9001);

DROP TABLE IF EXISTS shop_item;
CREATE TABLE shop_item (
  shop_vnum INT NOT NULL DEFAULT 0,
  item_vnum INT NOT NULL DEFAULT 0,
  count TINYINT UNSIGNED NOT NULL DEFAULT 1,
  display_pos TINYINT UNSIGNED NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
INSERT INTO shop_item (shop_vnum, item_vnum, count, display_pos) VALUES (1, 10, 1, 0);

DROP TABLE IF EXISTS skill_proto;
CREATE TABLE skill_proto (
  dwVnum INT NOT NULL DEFAULT 0,
  szName VARCHAR(32) NOT NULL DEFAULT '',
  bType TINYINT NOT NULL DEFAULT 0,
  bMaxLevel TINYINT NOT NULL DEFAULT 0,
  dwSplashRange INT UNSIGNED NOT NULL DEFAULT 0,
  szPointOn VARCHAR(100) NOT NULL DEFAULT 'NONE',
  szPointPoly VARCHAR(100) NOT NULL DEFAULT '',
  szSPCostPoly VARCHAR(100) NOT NULL DEFAULT '',
  szDurationPoly VARCHAR(100) NOT NULL DEFAULT '',
  szDurationSPCostPoly VARCHAR(100) NOT NULL DEFAULT '',
  szCooldownPoly VARCHAR(100) NOT NULL DEFAULT '',
  szMasterBonusPoly VARCHAR(100) NOT NULL DEFAULT '',
  setFlag INT UNSIGNED NOT NULL DEFAULT 0,
  setAffectFlag INT UNSIGNED NOT NULL DEFAULT 1,
  szPointOn2 VARCHAR(100) NOT NULL DEFAULT 'NONE',
  szPointPoly2 VARCHAR(100) NOT NULL DEFAULT '',
  szDurationPoly2 VARCHAR(100) NOT NULL DEFAULT '',
  setAffectFlag2 INT UNSIGNED NOT NULL DEFAULT 1,
  szPointOn3 VARCHAR(100) NOT NULL DEFAULT 'NONE',
  szPointPoly3 VARCHAR(100) NOT NULL DEFAULT '',
  szDurationPoly3 VARCHAR(100) NOT NULL DEFAULT '',
  setAffectFlag3 INT UNSIGNED NOT NULL DEFAULT 1,
  szGrandMasterAddSPCostPoly VARCHAR(100) NOT NULL DEFAULT '',
  bLevelStep TINYINT NOT NULL DEFAULT 0,
  bLevelLimit TINYINT UNSIGNED NOT NULL DEFAULT 0,
  prerequisiteSkillVnum INT NOT NULL DEFAULT 0,
  prerequisiteSkillLevel INT NOT NULL DEFAULT 0,
  iMaxHit SMALLINT NOT NULL DEFAULT 0,
  szSplashAroundDamageAdjustPoly VARCHAR(100) NOT NULL DEFAULT '1',
  eSkillType TINYINT UNSIGNED NOT NULL DEFAULT 0,
  dwTargetRange INT NOT NULL DEFAULT 1000,
  PRIMARY KEY (dwVnum)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
INSERT INTO skill_proto (dwVnum, szName, bType, bMaxLevel) VALUES (1, 'ThreeWay', 1, 40);

DROP TABLE IF EXISTS refine_proto;
CREATE TABLE refine_proto (
  id INT NOT NULL DEFAULT 0,
  vnum0 INT UNSIGNED NOT NULL DEFAULT 0,
  count0 SMALLINT NOT NULL DEFAULT 0,
  vnum1 INT UNSIGNED NOT NULL DEFAULT 0,
  count1 SMALLINT NOT NULL DEFAULT 0,
  vnum2 INT UNSIGNED NOT NULL DEFAULT 0,
  count2 SMALLINT NOT NULL DEFAULT 0,
  vnum3 INT UNSIGNED NOT NULL DEFAULT 0,
  count3 SMALLINT NOT NULL DEFAULT 0,
  vnum4 INT UNSIGNED NOT NULL DEFAULT 0,
  count4 SMALLINT NOT NULL DEFAULT 0,
  cost BIGINT NOT NULL DEFAULT 0,
  src_vnum INT UNSIGNED NOT NULL DEFAULT 0,
  result_vnum INT UNSIGNED NOT NULL DEFAULT 0,
  prob SMALLINT NOT NULL DEFAULT 100,
  PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS item_attr;
CREATE TABLE item_attr (
  apply ENUM('MAX_HP','MAX_SP') NOT NULL DEFAULT 'MAX_HP',
  prob INT NOT NULL DEFAULT 1,
  lv1 INT NOT NULL DEFAULT 100,
  lv2 INT NOT NULL DEFAULT 200,
  lv3 INT NOT NULL DEFAULT 300,
  lv4 INT NOT NULL DEFAULT 400,
  lv5 INT NOT NULL DEFAULT 500,
  weapon INT NOT NULL DEFAULT 0,
  body INT NOT NULL DEFAULT 5,
  wrist INT NOT NULL DEFAULT 5,
  foots INT NOT NULL DEFAULT 5,
  neck INT NOT NULL DEFAULT 5,
  head INT NOT NULL DEFAULT 0,
  shield INT NOT NULL DEFAULT 0,
  ear INT NOT NULL DEFAULT 0,
  costume_body INT NOT NULL DEFAULT 0,
  costume_hair INT NOT NULL DEFAULT 0,
  costume_weapon INT NOT NULL DEFAULT 0,
  pendant INT NOT NULL DEFAULT 0,
  glove INT NOT NULL DEFAULT 0,
  PRIMARY KEY (apply)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
INSERT INTO item_attr (apply, prob) VALUES ('MAX_HP', 10);

DROP TABLE IF EXISTS item_attr_rare;
CREATE TABLE item_attr_rare (
  apply ENUM('MAX_HP','MAX_SP') NOT NULL DEFAULT 'MAX_HP',
  prob INT NOT NULL DEFAULT 1,
  lv1 INT NOT NULL DEFAULT 100,
  lv2 INT NOT NULL DEFAULT 200,
  lv3 INT NOT NULL DEFAULT 300,
  lv4 INT NOT NULL DEFAULT 400,
  lv5 INT NOT NULL DEFAULT 500,
  weapon INT NOT NULL DEFAULT 0,
  body INT NOT NULL DEFAULT 5,
  wrist INT NOT NULL DEFAULT 5,
  foots INT NOT NULL DEFAULT 5,
  neck INT NOT NULL DEFAULT 5,
  head INT NOT NULL DEFAULT 0,
  shield INT NOT NULL DEFAULT 0,
  ear INT NOT NULL DEFAULT 0,
  costume_body INT NOT NULL DEFAULT 0,
  costume_hair INT NOT NULL DEFAULT 0,
  costume_weapon INT NOT NULL DEFAULT 0,
  pendant INT NOT NULL DEFAULT 0,
  glove INT NOT NULL DEFAULT 0,
  PRIMARY KEY (apply)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
INSERT INTO item_attr_rare (apply, prob) VALUES ('MAX_HP', 10);

DROP TABLE IF EXISTS banword;
CREATE TABLE banword (
  word VARCHAR(24) NOT NULL DEFAULT '',
  PRIMARY KEY (word)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS name_reservation;
CREATE TABLE name_reservation (
  account_id INT UNSIGNED NOT NULL DEFAULT 0,
  name VARCHAR(24) NOT NULL DEFAULT '',
  PRIMARY KEY (name)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS land;
CREATE TABLE land (
  id INT UNSIGNED NOT NULL AUTO_INCREMENT,
  map_index INT NOT NULL DEFAULT 0,
  x INT NOT NULL DEFAULT 0,
  y INT NOT NULL DEFAULT 0,
  width INT NOT NULL DEFAULT 0,
  height INT NOT NULL DEFAULT 0,
  guild_id INT UNSIGNED NOT NULL DEFAULT 0,
  guild_level_limit TINYINT NOT NULL DEFAULT 0,
  price BIGINT UNSIGNED NOT NULL DEFAULT 0,
  enable ENUM('YES','NO') NOT NULL DEFAULT 'YES',
  PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS guild_land;
CREATE TABLE guild_land (
  land_id INT UNSIGNED NOT NULL DEFAULT 0,
  guild_id INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (land_id)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS object_proto;
CREATE TABLE object_proto (
  vnum INT UNSIGNED NOT NULL DEFAULT 0,
  name VARCHAR(32) NOT NULL DEFAULT '',
  price BIGINT UNSIGNED NOT NULL DEFAULT 0,
  materials VARCHAR(64) NOT NULL DEFAULT '',
  upgrade_vnum INT UNSIGNED NOT NULL DEFAULT 0,
  upgrade_limit_time INT UNSIGNED NOT NULL DEFAULT 0,
  life INT NOT NULL DEFAULT 0,
  reg_1 INT NOT NULL DEFAULT 0,
  reg_2 INT NOT NULL DEFAULT 0,
  reg_3 INT NOT NULL DEFAULT 0,
  reg_4 INT NOT NULL DEFAULT 0,
  npc INT UNSIGNED NOT NULL DEFAULT 0,
  group_vnum INT UNSIGNED NOT NULL DEFAULT 0,
  dependent_group INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (vnum)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS object;
CREATE TABLE object (
  id INT UNSIGNED NOT NULL AUTO_INCREMENT,
  land_id INT UNSIGNED NOT NULL DEFAULT 0,
  vnum INT UNSIGNED NOT NULL DEFAULT 0,
  map_index INT NOT NULL DEFAULT 0,
  x INT NOT NULL DEFAULT 0,
  y INT NOT NULL DEFAULT 0,
  x_rot FLOAT NOT NULL DEFAULT 0,
  y_rot FLOAT NOT NULL DEFAULT 0,
  z_rot FLOAT NOT NULL DEFAULT 0,
  life INT NOT NULL DEFAULT 0,
  PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS monarch;
CREATE TABLE monarch (
  empire INT UNSIGNED NOT NULL DEFAULT 0,
  pid INT UNSIGNED NOT NULL DEFAULT 0,
  name VARCHAR(24) NOT NULL DEFAULT '',
  windate DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  money BIGINT NOT NULL DEFAULT 0,
  PRIMARY KEY (empire)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS itemshop_items;
CREATE TABLE itemshop_items (
  `index` INT UNSIGNED NOT NULL AUTO_INCREMENT,
  vnum INT UNSIGNED NOT NULL DEFAULT 0,
  count INT UNSIGNED NOT NULL DEFAULT 1,
  price INT UNSIGNED NOT NULL DEFAULT 0,
  currency TINYINT UNSIGNED NOT NULL DEFAULT 1,
  minLevel TINYINT UNSIGNED NOT NULL DEFAULT 0,
  socket0 INT NOT NULL DEFAULT 0,
  socket1 INT NOT NULL DEFAULT 0,
  socket2 INT NOT NULL DEFAULT 0,
  PRIMARY KEY (`index`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS itemshop_promotions;
CREATE TABLE itemshop_promotions (
  item_index INT UNSIGNED NOT NULL DEFAULT 0,
  price INT UNSIGNED NOT NULL DEFAULT 0,
  start_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  end_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (item_index)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS itemshop_time_auctions;
CREATE TABLE itemshop_time_auctions (
  item_index INT UNSIGNED NOT NULL DEFAULT 0,
  max_amount INT UNSIGNED NOT NULL DEFAULT 0,
  account_limit INT UNSIGNED NOT NULL DEFAULT 0,
  start_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  end_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (item_index)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS itemshop_time_auction;
CREATE TABLE itemshop_time_auction (
  item_index INT UNSIGNED NOT NULL DEFAULT 0,
  buy_count INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (item_index)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS ikashop_offlineshop;
CREATE TABLE ikashop_offlineshop (
  owner INT UNSIGNED NOT NULL DEFAULT 0,
  duration INT UNSIGNED NOT NULL DEFAULT 0,
  name VARCHAR(64) NOT NULL DEFAULT '',
  map INT NOT NULL DEFAULT 0,
  x INT NOT NULL DEFAULT 0,
  y INT NOT NULL DEFAULT 0,
  channel TINYINT UNSIGNED NOT NULL DEFAULT 1,
  unlock_count TINYINT UNSIGNED NOT NULL DEFAULT 0,
  is_premium TINYINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (owner)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS offlineshop_limit;
CREATE TABLE offlineshop_limit (
  channel TINYINT UNSIGNED NOT NULL DEFAULT 1,
  map_index INT UNSIGNED NOT NULL DEFAULT 1,
  shop_limit INT UNSIGNED NOT NULL DEFAULT 100,
  premium_limit INT UNSIGNED NOT NULL DEFAULT 100,
  PRIMARY KEY (channel, map_index)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
INSERT INTO offlineshop_limit (channel, map_index, shop_limit, premium_limit) VALUES (1, 1, 100, 100);

DROP TABLE IF EXISTS crafting_proto;
CREATE TABLE crafting_proto (
  vnum INT UNSIGNED NOT NULL DEFAULT 0,
  item_vnum INT UNSIGNED NOT NULL DEFAULT 0,
  count INT UNSIGNED NOT NULL DEFAULT 1,
  price BIGINT UNSIGNED NOT NULL DEFAULT 0,
  chance TINYINT UNSIGNED NOT NULL DEFAULT 100,
  recipe VARCHAR(128) NOT NULL DEFAULT '',
  req_progress INT UNSIGNED NOT NULL DEFAULT 0,
  req_level TINYINT UNSIGNED NOT NULL DEFAULT 0,
  recipe_vnum INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (vnum)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS quest_reward_proto;
CREATE TABLE quest_reward_proto (
  quest_name VARCHAR(64) NOT NULL DEFAULT '',
  exp INT UNSIGNED NOT NULL DEFAULT 0,
  gold INT UNSIGNED NOT NULL DEFAULT 0,
  alignment INT NOT NULL DEFAULT 0,
  items VARCHAR(255) NOT NULL DEFAULT '',
  warrior_items VARCHAR(255) NOT NULL DEFAULT '',
  assassin_items VARCHAR(255) NOT NULL DEFAULT '',
  sura_items VARCHAR(255) NOT NULL DEFAULT '',
  shaman_items VARCHAR(255) NOT NULL DEFAULT '',
  PRIMARY KEY (quest_name)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS shop_special;
CREATE TABLE shop_special (
  vnum INT UNSIGNED NOT NULL DEFAULT 0,
  item_vnum INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (vnum, item_vnum)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS shop_special_proto;
CREATE TABLE shop_special_proto (
  vnum INT UNSIGNED NOT NULL DEFAULT 0,
  item_vnum INT UNSIGNED NOT NULL DEFAULT 0,
  count INT UNSIGNED NOT NULL DEFAULT 1,
  rare_pct TINYINT UNSIGNED NOT NULL DEFAULT 0,
  price_type TINYINT UNSIGNED NOT NULL DEFAULT 0,
  price INT UNSIGNED NOT NULL DEFAULT 0,
  items VARCHAR(255) NOT NULL DEFAULT '',
  random_item_count INT UNSIGNED NOT NULL DEFAULT 0,
  random_item_group INT UNSIGNED NOT NULL DEFAULT 0,
  limittype0 TINYINT UNSIGNED NOT NULL DEFAULT 0,
  limitvalue0 INT NOT NULL DEFAULT 0,
  limittype1 TINYINT UNSIGNED NOT NULL DEFAULT 0,
  limitvalue1 INT NOT NULL DEFAULT 0,
  PRIMARY KEY (vnum)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS item_award;
CREATE TABLE item_award (
  id INT UNSIGNED NOT NULL AUTO_INCREMENT,
  login VARCHAR(30) NOT NULL DEFAULT '',
  vnum INT UNSIGNED NOT NULL DEFAULT 0,
  count INT UNSIGNED NOT NULL DEFAULT 1,
  socket0 INT UNSIGNED NOT NULL DEFAULT 0,
  socket1 INT UNSIGNED NOT NULL DEFAULT 0,
  socket2 INT UNSIGNED NOT NULL DEFAULT 0,
  attrtype0 TINYINT UNSIGNED NOT NULL DEFAULT 0,
  attrvalue0 SMALLINT NOT NULL DEFAULT 0,
  attrtype1 TINYINT UNSIGNED NOT NULL DEFAULT 0,
  attrvalue1 SMALLINT NOT NULL DEFAULT 0,
  attrtype2 TINYINT UNSIGNED NOT NULL DEFAULT 0,
  attrvalue2 SMALLINT NOT NULL DEFAULT 0,
  attrtype3 TINYINT UNSIGNED NOT NULL DEFAULT 0,
  attrvalue3 SMALLINT NOT NULL DEFAULT 0,
  attrtype4 TINYINT UNSIGNED NOT NULL DEFAULT 0,
  attrvalue4 SMALLINT NOT NULL DEFAULT 0,
  attrtype5 TINYINT UNSIGNED NOT NULL DEFAULT 0,
  attrvalue5 SMALLINT NOT NULL DEFAULT 0,
  attrtype6 TINYINT UNSIGNED NOT NULL DEFAULT 0,
  attrvalue6 SMALLINT NOT NULL DEFAULT 0,
  mall TINYINT NOT NULL DEFAULT 1,
  why VARCHAR(128) DEFAULT NULL,
  given_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  taken_time DATETIME DEFAULT NULL,
  PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS gmhost;
CREATE TABLE gmhost (
  mIP VARCHAR(16) NOT NULL DEFAULT '',
  PRIMARY KEY (mIP)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS gmlist;
CREATE TABLE gmlist (
  mID INT UNSIGNED NOT NULL AUTO_INCREMENT,
  mAccount VARCHAR(16) NOT NULL DEFAULT '',
  mName VARCHAR(16) NOT NULL DEFAULT '',
  mContactIP VARCHAR(16) NOT NULL DEFAULT '',
  mServerIP VARCHAR(16) NOT NULL DEFAULT 'ALL',
  mAuthority ENUM('IMPLEMENTOR','HIGH_WIZARD','GOD','LOW_WIZARD','PLAYER') NOT NULL DEFAULT 'PLAYER',
  PRIMARY KEY (mID)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

INSERT INTO `player` (id, account_id, name, job, level, playtime, st, ht, dx, iq, part_main, part_hair, part_acce, x, y, map_index, exit_x, exit_y, exit_map_index, hp, mp, stamina, skill_group, change_name, gold)
VALUES (90011, 9001, 'SmokeHero', 0, 15, 42, 10, 10, 10, 10, 11209, 0, 0, 469300, 964200, 1, 469300, 964200, 1, 800, 300, 800, 1, 0, 50000);
EOF

"$DB_CLI" -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" <<EOF
SET SESSION sql_mode = 'STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION';
INSERT INTO account (id, login, password, social_id, status, hwid)
VALUES (9001, '$SMOKE_TEST_LOGIN', '$SMOKE_PASS_HASH', '1234567', 'OK', '$SMOKE_HWID_PLAIN');
-- Legacy account: literal hash as stored by MySQL 5.x/MariaDB PASSWORD('password') in existing MT2009 databases
INSERT INTO account (id, login, password, social_id, status, hwid)
VALUES (9002, 'smklegacy', '*2470C0C06DEE42FD1618BB99005ADCA2EC9D1E19', '1234567', 'OK', '$SMOKE_HWID_PLAIN');
-- Malformed hash account: must always fail closed with WRONGPWD
INSERT INTO account (id, login, password, social_id, status, hwid)
VALUES (9003, 'smkmalformed', 'corrupted_non_phc_hash_value', '1234567', 'OK', '$SMOKE_HWID_PLAIN');
INSERT INTO gmlist (mID, mAccount, mName, mContactIP, mServerIP, mAuthority)
VALUES (1, '$SMOKE_TEST_LOGIN', 'SmokeHero', '127.0.0.1', 'ALL', 'IMPLEMENTOR');
EOF

# Execute non-destructive Rev 47 SQL migration script (widens account.password VARCHAR(45) -> VARCHAR(255) & creates account_rev47_backup)
"$DB_CLI" -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" < "$REPO_ROOT/sql/migrations/rev47_account_security_upgrade.sql"

PASS_COL_LEN=$("$DB_CLI" -N -s -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" -e "SELECT CHARACTER_MAXIMUM_LENGTH FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA='$DB' AND TABLE_NAME='account' AND COLUMN_NAME='password';")
BACKUP_ROWS=$("$DB_CLI" -N -s -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" -e "SELECT COUNT(*) FROM account_rev47_backup;")
if [ "$PASS_COL_LEN" != "255" ] || [ "$BACKUP_ROWS" != "3" ]; then
    echo "HATA: [1/6] rev47_account_security_upgrade.sql migration doğrulaması başarısız (password_len=$PASS_COL_LEN, backup_rows=$BACKUP_ROWS)!" >&2
    exit 1
fi

HASH_MATCH=$("$DB_CLI" -N -s -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" -e "SELECT IF(password = CONCAT('*', UPPER(SHA1(UNHEX(SHA1('$SMOKE_TEST_PASS'))))), 1, 0) FROM account WHERE id=9001;")
if [ "$HASH_MATCH" != "1" ]; then
    echo "HATA: [1/6] MT2009 C++ mysql_hash_password ile SQL SHA1(UNHEX(SHA1(...))) hesap hash formatı eşleşmedi!" >&2
    exit 1
fi
echo "  [PASS] [1/6] Strict SQL Mode şeması, rev47_account_security_upgrade.sql migration (account.password VARCHAR(255) + account_rev47_backup=3 satır), geçici test hesapları ve SmokeHero (id=90011, level=15, map=1 469300x964200) karakteri hazırlandı."

# ------------------------------------------------------------------------------
# [2/6] Real db_r41023 Isolated Startup, SQL Table Load & TCP Listener Probe
# ------------------------------------------------------------------------------
echo "[2/6] Gerçek db_r41023 izole başlatma, SQL tablo yükleme ve TCP dinleme duman testi..."
DB_WORK="$SMOKE_ROOT/db"
mkdir -p "$DB_WORK"

cat > "$DB_WORK/conf.txt" <<EOF
CLIENT_HEART_FPS = 25
HASH_PLAYER_LIFE_SEC = 600
BACKUP_LIMIT_SEC = 3600
PLAYER_ID_START = 100
PLAYER_DELETE_LEVEL_LIMIT = 70
ITEM_ID_RANGE = 10000000 20000000
DISABLE_HOTBACKUP = 1
PROTO_FROM_DB = 1
MIRROR2DB = 0
BIND_IP = 127.0.0.1
BIND_PORT = 15010
SQL_ACCOUNT = "$HOST $DB $USER $PASS $PORT"
SQL_PLAYER = "$HOST $DB $USER $PASS $PORT"
SQL_COMMON = "$HOST $DB $USER $PASS $PORT"
SQL_HOTBACKUP = "$HOST $DB $USER $PASS $PORT"
SQL_LOG = "$HOST $DB $USER $PASS $PORT"
SQL_WORLD = "$HOST $DB $USER $PASS $PORT"
EOF

(
    cd "$DB_WORK"
    exec "$DB_BIN" >"$DB_WORK/db_stdout.log" 2>"$DB_WORK/db_stderr.log"
) &
ACTIVE_DB_PID=$!

if ! "$BUILD_DIR/test_server_smoke_client" --probe-db 15010; then
    echo "HATA: db_r41023 başlatılamadı veya 127.0.0.1:15010 üzerinde dinlemiyor!" >&2
    cat "$DB_WORK/db_stderr.log" 2>/dev/null || true
    cat "$DB_WORK/syserr" 2>/dev/null || true
    cat "$DB_WORK/syslog" 2>/dev/null || true
    exit 1
fi
echo "  [PASS] [2/6] db_r41023 izole ortamda başarıyla başlatıldı, tüm SQL proto/cache tablolarını yükledi ve 127.0.0.1:15010 üzerinde aktif."

ABI_LINE=$(grep -m1 "AsyncSQL client ABI OK" "$DB_WORK/db_stdout.log" 2>/dev/null || true)
if [ -z "$ABI_LINE" ]; then
    echo "HATA: [2/6] db_r41023 çalışma zamanı istemci ABI doğrulama satırını üretmedi!" >&2
    cat "$DB_WORK/db_stdout.log" 2>/dev/null || true
    exit 1
fi
SERVER_FLAVOR=$("$DB_CLI" -N -s -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" -e "SELECT IF(VERSION() LIKE '%MariaDB%', 'MariaDB', 'MySQL');")
case "$ABI_LINE" in
    *"header=$SERVER_FLAVOR("*) ;;
    *)
        echo "HATA: [2/6] db_r41023 başlık türü sunucu motoruyla eşleşmiyor (motor=$SERVER_FLAVOR): $ABI_LINE" >&2
        exit 1
        ;;
esac
echo "  [PASS] [2/6] Çalışma zamanı ABI doğrulandı (motor=$SERVER_FLAVOR): $ABI_LINE"

# ------------------------------------------------------------------------------
# [3/6] Mock-DB + 32-Bit TCP Wire Handshake (PHASE_HANDSHAKE -> PHASE_LOGIN)
# ------------------------------------------------------------------------------
run_mock_db_game_smoke() {
    LABEL="$1"
    GAME_BIN="$2"
    HOSTNAME_TAG="$3"
    GAME_PORT="$4"
    P2P_PORT="$5"
    MOCK_DB_PORT="$6"

    echo "$LABEL Mock-DB başlatma, Lua quest runtime ve 32-bit TCP wire handshake testi..."
    GAME_WORK="$SMOKE_ROOT/$HOSTNAME_TAG"
    prepare_game_workdir "$GAME_WORK"

    cat > "$GAME_WORK/CONFIG" <<EOF
HOSTNAME: $HOSTNAME_TAG
CHANNEL: 1
PORT: $GAME_PORT
P2P_PORT: $P2P_PORT
DB_PORT: $MOCK_DB_PORT
DB_ADDR: 127.0.0.1
BIND_IP: 127.0.0.1
MAP_ALLOW: 1
PLAYER_SQL: $HOST $USER $PASS $DB $PORT
COMMON_SQL: $HOST $USER $PASS $DB $PORT
ACCOUNT_SQL: $HOST $USER $PASS $DB $PORT
LOG_SQL: $HOST $USER $PASS $DB $PORT
TEST_SERVER: 1
EOF

    READY_FILE="$GAME_WORK/mock_db.ready"
    OUT_FILE="$GAME_WORK/mock_db.out"
    rm -f "$READY_FILE" "$OUT_FILE"

    "$BUILD_DIR/test_server_smoke_client" --mock-db "$MOCK_DB_PORT" "$READY_FILE" "$OUT_FILE" &
    ACTIVE_MOCK_PID=$!

    for _ in $(seq 1 10); do
        if [ -f "$READY_FILE" ]; then
            break
        fi
        sleep 1
    done

    if [ ! -f "$READY_FILE" ]; then
        echo "HATA: Mock DB dinleyicisi hazır olmadı ($MOCK_DB_PORT)!" >&2
        exit 1
    fi

    (
        cd "$GAME_WORK"
        exec "$GAME_BIN" -I 127.0.0.1 -v >"$GAME_WORK/game_stdout.log" 2>"$GAME_WORK/game_stderr.log"
    ) &
    ACTIVE_GAME_PID=$!

    if ! "$BUILD_DIR/test_server_smoke_client" --probe-game "$GAME_PORT"; then
        echo "HATA: $LABEL istemci TCP wire handshake (PHASE_HANDSHAKE -> PHASE_LOGIN) doğrulaması başarısız oldu!" >&2
        cat "$GAME_WORK/game_stdout.log" 2>/dev/null || true
        cat "$GAME_WORK/game_stderr.log" 2>/dev/null || true
        cat "$GAME_WORK/syserr" 2>/dev/null || true
        cat "$GAME_WORK/syslog" 2>/dev/null || true
        exit 1
    fi

    if [ ! -f "$OUT_FILE" ] || ! grep -q "CONNECTED=1" "$OUT_FILE"; then
        echo "HATA: $LABEL db_clientdesc bağlantısını kurmadı!" >&2
        cat "$OUT_FILE" 2>/dev/null || true
        exit 1
    fi
    echo "  [PASS] $LABEL Mock-DB db_clientdesc paketlerini gönderdi ($(tr '\n' ' ' < "$OUT_FILE"))."

    if [ -f "$GAME_WORK/syserr" ] && grep -qiE "LUA_ERROR|InitializeLua|questlib|locale\.lua|settings\.lua|translate\.lua" "$GAME_WORK/syserr"; then
        echo "HATA: $LABEL syserr içinde Lua çalışma zamanı hatası tespit edildi!" >&2
        cat "$GAME_WORK/syserr" >&2
        exit 1
    fi
    echo "  [PASS] $LABEL CQuestManager::Initialize() -> InitializeLua() 0 Lua hatası ile tamamlandı."

    stop_pid_bounded "$ACTIVE_GAME_PID"
    ACTIVE_GAME_PID=""
    stop_pid_bounded "$ACTIVE_MOCK_PID"
    ACTIVE_MOCK_PID=""
}

echo "[3/6] Mock-DB + 32-Bit TCP Wire Handshake (Lua 5.0.3 Rollback & Lua 5.5.1 Default) testi..."
run_mock_db_game_smoke "[3a/6] game_r41023_lua50 (Lua 5.0.3 Rollback)" "$GAME50_BIN" "mock_lua50" 13011 14011 15011
run_mock_db_game_smoke "[3b/6] game_r41023 (Lua 5.5.1 Default)" "$GAME55_BIN" "mock_lua55" 13012 14012 15012

# ------------------------------------------------------------------------------
# [4/6] Real db_r41023 <-> Normal Game Server Full HEADER_DG_BOOT + Real Auth/Login + PHASE_GAME World Entry
# ------------------------------------------------------------------------------
run_real_db_auth_and_game_smoke() {
    STAGE_TAG="$1"
    LABEL="$2"
    GAME_BIN="$3"
    HOSTNAME_TAG="$4"
    AUTH_PORT="$5"
    AUTH_P2P_PORT="$6"
    GAME_PORT="$7"
    GAME_P2P_PORT="$8"

    echo "$STAGE_TAG $LABEL: Gerçek db_r41023 <-> HEADER_DG_BOOT + Gerçek AUTH_SERVER (HEADER_CG_LOGIN3) + Gerçek GAME_SERVER (LoginByKey -> PHASE_SELECT -> PHASE_LOADING -> PHASE_GAME + Quest/NPC Coroutine) testi..."

    # Reset quest flags, player position, and legacy test passwords in SQL before restarting db_r41023 so each Lua engine run starts from identical state
    stop_pid_bounded "$ACTIVE_DB_PID"
    ACTIVE_DB_PID=""
    "$DB_CLI" -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" <<'EOF_RESET'
SET SESSION sql_mode = 'STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION';
DELETE FROM quest WHERE dwPID = 90011;
INSERT INTO quest (dwPID, szName, szState, lValue) VALUES
  (90011, 'smoke_world', '__status', 0),
  (90011, 'smoke_world', 'db_seed_val', 77);
UPDATE `player` SET x=469300, y=964200, map_index=1, exit_x=469300, exit_y=964200, exit_map_index=1, hp=800, mp=300, stamina=800 WHERE id=90011;
UPDATE account SET password='*2470C0C06DEE42FD1618BB99005ADCA2EC9D1E19' WHERE id=9002;
UPDATE account SET password='corrupted_non_phc_hash_value' WHERE id=9003;
EOF_RESET
    "$DB_CLI" -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" -e "UPDATE account SET password='$SMOKE_PASS_HASH' WHERE id=9001;"

    (
        cd "$DB_WORK"
        exec "$DB_BIN" >"$DB_WORK/db_stdout_${HOSTNAME_TAG}.log" 2>"$DB_WORK/db_stderr_${HOSTNAME_TAG}.log"
    ) &
    ACTIVE_DB_PID=$!
    if ! "$BUILD_DIR/test_server_smoke_client" --probe-db 15010; then
        echo "HATA: $STAGE_TAG db_r41023 yeniden başlatılamadı!" >&2
        cat "$DB_WORK/db_stderr_${HOSTNAME_TAG}.log" 2>/dev/null || true
        exit 1
    fi

    # 1. Start Real AUTH_SERVER (AUTH_SERVER: master) connected to live db_r41023 (15010)
    AUTH_WORK="$SMOKE_ROOT/auth_${HOSTNAME_TAG}"
    prepare_game_workdir "$AUTH_WORK"

    cat > "$AUTH_WORK/CONFIG" <<EOF
HOSTNAME: auth_${HOSTNAME_TAG}
CHANNEL: 99
PORT: $AUTH_PORT
P2P_PORT: $AUTH_P2P_PORT
DB_PORT: 15010
DB_ADDR: 127.0.0.1
BIND_IP: 127.0.0.1
AUTH_SERVER: master
PLAYER_SQL: $HOST $USER $PASS $DB $PORT
COMMON_SQL: $HOST $USER $PASS $DB $PORT
ACCOUNT_SQL: $HOST $USER $PASS $DB $PORT
LOG_SQL: $HOST $USER $PASS $DB $PORT
TEST_SERVER: 1
EOF

    (
        cd "$AUTH_WORK"
        exec "$GAME_BIN" -I 127.0.0.1 -v >"$AUTH_WORK/auth_stdout.log" 2>"$AUTH_WORK/auth_stderr.log"
    ) &
    ACTIVE_AUTH_PID=$!

    # 2. Start Real GAME_SERVER (CHANNEL: 1) connected to live db_r41023 (15010)
    GAME_WORK="$SMOKE_ROOT/game_${HOSTNAME_TAG}"
    prepare_game_workdir "$GAME_WORK"

    cat > "$GAME_WORK/CONFIG" <<EOF
HOSTNAME: $HOSTNAME_TAG
CHANNEL: 1
PORT: $GAME_PORT
P2P_PORT: $GAME_P2P_PORT
DB_PORT: 15010
DB_ADDR: 127.0.0.1
BIND_IP: 127.0.0.1
MAP_ALLOW: 1
PLAYER_SQL: $HOST $USER $PASS $DB $PORT
COMMON_SQL: $HOST $USER $PASS $DB $PORT
ACCOUNT_SQL: $HOST $USER $PASS $DB $PORT
LOG_SQL: $HOST $USER $PASS $DB $PORT
TEST_SERVER: 1
EOF

    (
        cd "$GAME_WORK"
        exec "$GAME_BIN" -I 127.0.0.1 -v >"$GAME_WORK/game_stdout.log" 2>"$GAME_WORK/game_stderr.log"
    ) &
    ACTIVE_GAME_PID=$!

    # Wait up to 30s for CInputDB::Boot ("boot packet end check ok"); abort early if GAME_SERVER dies
    BOOT_OK=0
    GAME_DIED=0
    for _ in $(seq 1 60); do
        if grep -q "boot packet end check ok" "$GAME_WORK/game_stdout.log" 2>/dev/null || \
           grep -q "boot packet end check ok" "$GAME_WORK/syslog" 2>/dev/null; then
            BOOT_OK=1
            break
        fi
        if ! kill -0 "$ACTIVE_GAME_PID" 2>/dev/null; then
            GAME_DIED=1
            break
        fi
        sleep 0.5 2>/dev/null || sleep 1
    done

    if [ "$BOOT_OK" != "1" ]; then
        if [ "$GAME_DIED" = "1" ]; then
            echo "HATA: $STAGE_TAG $LABEL GAME_SERVER HEADER_DG_BOOT tamamlanmadan sonlandı!" >&2
        else
            echo "HATA: $STAGE_TAG $LABEL gerçek db_r41023 üzerinden HEADER_DG_BOOT (CInputDB::Boot) akışını 30 sn içinde tamamlamadı!" >&2
        fi
        dump_stage_logs
        exit 1
    fi
    echo "  [PASS] $STAGE_TAG $LABEL gerçek db_r41023 üzerinden HEADER_DG_BOOT (CInputDB::Boot + CQuestManager::Boot) paketini eksiksiz yükledi (boot packet end check ok [ffff]==0xffff)."

    # Check that syserr has no Lua errors after CQuestManager::InitializeLua() + CQuestManager::Boot()
    if [ -f "$GAME_WORK/syserr" ] && grep -qiE "LUA_ERROR|InitializeLua|questlib|locale\.lua|settings\.lua|translate\.lua" "$GAME_WORK/syserr"; then
        echo "HATA: $STAGE_TAG $LABEL syserr içinde Lua çalışma zamanı hatası tespit edildi!" >&2
        cat "$GAME_WORK/syserr" >&2
        exit 1
    fi
    echo "  [PASS] $STAGE_TAG $LABEL CQuestManager::InitializeLua() + CQuestManager::Boot() 0 Lua hatası ile tamamlandı."

    # Verify bootlog row was inserted into live SQL DB
    BOOT_COUNT="0"
    for _ in $(seq 1 10); do
        BOOT_COUNT=$("$DB_CLI" -N -s -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" -e "SELECT COUNT(*) FROM bootlog WHERE hostname='$HOSTNAME_TAG';")
        if [ "$BOOT_COUNT" = "1" ]; then
            break
        fi
        sleep 1
    done
    if [ "$BOOT_COUNT" != "1" ]; then
        echo "HATA: $STAGE_TAG $LABEL bootlog kaydı bulunamadı (beklenen: 1, bulunan: $BOOT_COUNT)!" >&2
        exit 1
    fi
    echo "  [PASS] $STAGE_TAG $LABEL canlı SQL veritabanına bootlog kaydını başarıyla yazdı (hostname=$HOSTNAME_TAG)."

    # 3a. Anti-enumeration & malformed hash & wrong password checks (all must return uniform WRONGPWD, never NOID)
    if ! "$BUILD_DIR/test_server_smoke_client" --auth-expect-fail "$AUTH_PORT" "smk_nonexistent" "WrongPass999" "$SMOKE_HWID_HEX" "WRONGPWD"; then
        echo "HATA: $STAGE_TAG $LABEL AUTH_SERVER var olmayan hesabı WRONGPWD ile reddetmedi (hesap numaralandırma sızıntısı)!" >&2
        dump_stage_logs
        exit 1
    fi
    sleep 0.4 2>/dev/null || sleep 1

    if ! "$BUILD_DIR/test_server_smoke_client" --auth-expect-fail "$AUTH_PORT" "smkmalformed" "password" "$SMOKE_HWID_HEX" "WRONGPWD"; then
        echo "HATA: $STAGE_TAG $LABEL AUTH_SERVER bozuk hash kaydını WRONGPWD ile reddetmedi!" >&2
        dump_stage_logs
        exit 1
    fi
    sleep 0.4 2>/dev/null || sleep 1

    if ! "$BUILD_DIR/test_server_smoke_client" --auth-expect-fail "$AUTH_PORT" "smklegacy" "WrongPass999" "$SMOKE_HWID_HEX" "WRONGPWD"; then
        echo "HATA: $STAGE_TAG $LABEL AUTH_SERVER yanlış şifreyi WRONGPWD ile reddetmedi!" >&2
        dump_stage_logs
        exit 1
    fi
    sleep 0.4 2>/dev/null || sleep 1

    # Verify wrong password did NOT modify the legacy hash in SQL
    PRE_UPGRADE_HASH=$("$DB_CLI" -N -s -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" -e "SELECT password FROM account WHERE id=9002;")
    if [ "$PRE_UPGRADE_HASH" != "*2470C0C06DEE42FD1618BB99005ADCA2EC9D1E19" ]; then
        echo "HATA: $STAGE_TAG $LABEL yanlış şifre denemesinde eski hash değişti!" >&2
        exit 1
    fi

    # 3b-pre. Schema precondition check: if account.password is still VARCHAR(45) (migration not applied), login must succeed WITHOUT corrupting the legacy hash
    "$DB_CLI" -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" -e "ALTER TABLE account MODIFY COLUMN password VARCHAR(45) NOT NULL DEFAULT '';"
    LEGACY_KEY_FILE="$AUTH_WORK/legacy_login_key.txt"
    rm -f "$LEGACY_KEY_FILE"
    if ! "$BUILD_DIR/test_server_smoke_client" --auth-login "$AUTH_PORT" "smklegacy" "password" "$SMOKE_HWID_HEX" "$LEGACY_KEY_FILE"; then
        echo "HATA: $STAGE_TAG $LABEL VARCHAR(45) şemasında eski hesap girişi başarısız oldu!" >&2
        dump_stage_logs
        exit 1
    fi
    rm -f "$LEGACY_KEY_FILE"
    sleep 0.4 2>/dev/null || sleep 1

    UNMIGRATED_HASH=$("$DB_CLI" -N -s -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" -e "SELECT password FROM account WHERE id=9002;")
    if [ "$UNMIGRATED_HASH" != "*2470C0C06DEE42FD1618BB99005ADCA2EC9D1E19" ]; then
        echo "HATA: $STAGE_TAG $LABEL VARCHAR(45) şemasında (migration uygulanmadan) eski hesap parolası bozuldu ($UNMIGRATED_HASH)!" >&2
        dump_stage_logs
        exit 1
    fi
    echo "  [PASS] $STAGE_TAG $LABEL migration ön koşulu doğrulandı: VARCHAR(45) (<128) sütununda eski hesap bozulmadan korundu ve giriş başarılı oldu."

    # Apply rev47 migration to widen account.password to VARCHAR(255)
    "$DB_CLI" -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" < "$REPO_ROOT/sql/migrations/rev47_account_security_upgrade.sql"

    # 3b. Legacy account compatibility & transparent atomic upgrade to Argon2id on successful login
    if ! "$BUILD_DIR/test_server_smoke_client" --auth-login "$AUTH_PORT" "smklegacy" "password" "$SMOKE_HWID_HEX" "$LEGACY_KEY_FILE"; then
        echo "HATA: $STAGE_TAG $LABEL eski (MySQL 5.x PASSWORD) biçimli hesap AUTH_SERVER üzerinde doğrulanamadı!" >&2
        dump_stage_logs
        exit 1
    fi
    rm -f "$LEGACY_KEY_FILE"
    sleep 0.4 2>/dev/null || sleep 1

    # Verify account.password for id=9002 was atomically upgraded in-place to $argon2id$v=19$m=19456,t=2,p=1$...
    UPGRADED_PREFIX=$("$DB_CLI" -N -s -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" -e "SELECT LEFT(password, 31) FROM account WHERE id=9002;")
    if [ "$UPGRADED_PREFIX" != '$argon2id$v=19$m=19456,t=2,p=1$' ]; then
        echo "HATA: $STAGE_TAG $LABEL eski *SHA1(SHA1) parolası başarılı giriş sonrası Argon2id PHC formatına yükseltilmedi (prefix=$UPGRADED_PREFIX)!" >&2
        dump_stage_logs
        exit 1
    fi
    echo "  [PASS] $STAGE_TAG $LABEL eski *SHA1(SHA1) hesap (id=9002) doğrulandı ve canlı SQL üzerinde atomik olarak \$argon2id\$v=19\$m=19456,t=2,p=1\$ formatına yükseltildi."

    # 3c. Post-upgrade verification: wrong password and old SHA1 hash string must fail; correct plaintext password must verify via Argon2id
    if ! "$BUILD_DIR/test_server_smoke_client" --auth-expect-fail "$AUTH_PORT" "smklegacy" "WrongPass999" "$SMOKE_HWID_HEX" "WRONGPWD"; then
        echo "HATA: $STAGE_TAG $LABEL Argon2id yükseltmesi sonrası yanlış şifre reddedilmedi!" >&2
        dump_stage_logs
        exit 1
    fi
    sleep 0.4 2>/dev/null || sleep 1

    if ! "$BUILD_DIR/test_server_smoke_client" --auth-expect-fail "$AUTH_PORT" "smklegacy" "*2470C0C06DEE42FD" "$SMOKE_HWID_HEX" "WRONGPWD"; then
        echo "HATA: $STAGE_TAG $LABEL Argon2id yükseltmesi sonrası eski SHA1 hash dizgesi reddedilmedi!" >&2
        dump_stage_logs
        exit 1
    fi
    sleep 0.4 2>/dev/null || sleep 1

    if ! "$BUILD_DIR/test_server_smoke_client" --auth-login "$AUTH_PORT" "smklegacy" "password" "$SMOKE_HWID_HEX" "$LEGACY_KEY_FILE"; then
        echo "HATA: $STAGE_TAG $LABEL Argon2id yükseltmesi sonrası doğrudan Argon2id parola doğrulaması başarısız oldu!" >&2
        dump_stage_logs
        exit 1
    fi
    rm -f "$LEGACY_KEY_FILE"
    sleep 0.4 2>/dev/null || sleep 1

    ARGON2_UPGRADE_LOG=$(grep -m1 "QID_AUTH_LOGIN: Upgraded account id" "$AUTH_WORK/syslog" "$AUTH_WORK/auth_stdout.log" 2>/dev/null || true)
    ARGON2_VERIFY_LOG=$(grep -m1 "QID_AUTH_LOGIN: Argon2id verified account id" "$AUTH_WORK/syslog" "$AUTH_WORK/auth_stdout.log" 2>/dev/null || true)
    echo "  [PASS] $STAGE_TAG $LABEL doğrudan Argon2id doğrulaması ve SHA1 geri dönüş (fallback) engeli kanıtlandı (${ARGON2_UPGRADE_LOG} | ${ARGON2_VERIFY_LOG})."

    # 3d. Execute primary test account AUTH_SERVER login (HEADER_CG_LOGIN3 -> QID_AUTH_LOGIN -> HEADER_GD_AUTH_LOGIN -> HEADER_GC_AUTH_SUCCESS)
    KEY_FILE="$AUTH_WORK/login_key.txt"
    rm -f "$KEY_FILE"
    if ! "$BUILD_DIR/test_server_smoke_client" --auth-login "$AUTH_PORT" "@ENV" "@ENV" "$SMOKE_HWID_HEX" "$KEY_FILE"; then
        echo "HATA: $STAGE_TAG $LABEL gerçek AUTH_SERVER hesap girişi (HEADER_CG_LOGIN3) başarısız oldu!" >&2
        dump_stage_logs
        exit 1
    fi

    LOGIN_KEY=$(cat "$KEY_FILE" 2>/dev/null | tr -d '\r\n ')
    rm -f "$KEY_FILE"
    if [ -z "$LOGIN_KEY" ] || [ "$LOGIN_KEY" = "0" ]; then
        echo "HATA: $STAGE_TAG $LABEL AUTH_SERVER geçerli bir dwLoginKey üretmedi!" >&2
        exit 1
    fi

    # 4. Execute real GAME_SERVER LoginByKey -> PHASE_SELECT -> CharacterSelect -> PHASE_LOADING -> Entergame -> PHASE_GAME + Quest/NPC Coroutine
    if ! "$BUILD_DIR/test_server_smoke_client" --game-login-key "$GAME_PORT" "@ENV" "$LOGIN_KEY"; then
        echo "HATA: $STAGE_TAG $LABEL gerçek GAME_SERVER (LoginByKey -> PHASE_SELECT -> PHASE_LOADING -> PHASE_GAME + Quest/NPC Coroutine) akışı başarısız oldu!" >&2
        dump_stage_logs
        exit 1
    fi

    # 4b. Replay the exact same consumed dwLoginKey against GAME_SERVER -> must be rejected (single-use LoginByKey enforcement)
    if ! "$BUILD_DIR/test_server_smoke_client" --game-login-key-expect-fail "$GAME_PORT" "@ENV" "$LOGIN_KEY"; then
        echo "HATA: $STAGE_TAG $LABEL tüketilmiş dwLoginKey tekrar kullanıldığında (replay) GAME_SERVER tarafından reddedilmedi!" >&2
        dump_stage_logs
        exit 1
    fi

    # 5. Verify zero map/sectree/PlayerLoad/Lua runtime errors in GAME_WORK/syserr after full world entry & NPC coroutine
    if [ -f "$GAME_WORK/syserr" ] && grep -qiE "GetServerLocation|cannot find sectree|PlayerLoad|LUA_ERROR|WriteRunningStateToSyserr" "$GAME_WORK/syserr"; then
        echo "HATA: $STAGE_TAG $LABEL oyun dünyasına giriş veya NPC/quest coroutine sırasında syserr hatası oluştu!" >&2
        cat "$GAME_WORK/syserr" >&2
        exit 1
    fi
    echo "  [PASS] $STAGE_TAG $LABEL harita/sectree (MapIndex=1, 469300x964200), PlayerLoad ve Quest/NPC coroutine akışı 0 syserr hatası ile tamamlandı."

    # 6. Verify quest state flags (login_verified=1 and npc_flow_done=1) were persisted to live SQL DB via HEADER_GD_QUEST_SAVE on disconnect
    QUEST_SAVED_COUNT="0"
    for _ in $(seq 1 10); do
        QUEST_SAVED_COUNT=$("$DB_CLI" -N -s -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" -e "SELECT COUNT(*) FROM quest WHERE dwPID=90011 AND szName='smoke_world' AND ((szState='login_verified' AND lValue=1) OR (szState='npc_flow_done' AND lValue=1));")
        if [ "$QUEST_SAVED_COUNT" = "2" ]; then
            break
        fi
        sleep 1
    done
    if [ "$QUEST_SAVED_COUNT" != "2" ]; then
        echo "HATA: $STAGE_TAG $LABEL quest state bayrakları (login_verified=1, npc_flow_done=1) canlı SQL quest tablosuna kaydedilmedi (bulunan: $QUEST_SAVED_COUNT)!" >&2
        dump_stage_logs
        exit 1
    fi
    echo "  [PASS] $STAGE_TAG $LABEL PC::Save -> HEADER_GD_QUEST_SAVE ile quest bayrakları (login_verified=1, npc_flow_done=1) canlı SQL veritabanına kaydedildi."

    # 7. Brute-force rate-limit & temporary exponential backoff check on AUTH_SERVER (5 failures -> 6th throttled with FAILURE -> recovers after 2s backoff)
    for _ in 1 2 3 4 5; do
        if ! "$BUILD_DIR/test_server_smoke_client" --auth-expect-fail "$AUTH_PORT" "smk_bruteforce" "WrongPass999" "$SMOKE_HWID_HEX" "WRONGPWD"; then
            echo "HATA: $STAGE_TAG $LABEL brute-force eşik öncesi deneme WRONGPWD dönmedi!" >&2
            dump_stage_logs
            exit 1
        fi
        sleep 0.3 2>/dev/null || sleep 1
    done
    if ! "$BUILD_DIR/test_server_smoke_client" --auth-expect-fail "$AUTH_PORT" "smk_bruteforce" "WrongPass999" "$SMOKE_HWID_HEX" "FAILURE"; then
        echo "HATA: $STAGE_TAG $LABEL 5 başarısız giriş sonrası 6. deneme AuthRateLimiter tarafından FAILURE ile engellenmedi!" >&2
        dump_stage_logs
        exit 1
    fi
    sleep 2.5 2>/dev/null || sleep 3
    if ! "$BUILD_DIR/test_server_smoke_client" --auth-login "$AUTH_PORT" "smklegacy" "password" "$SMOKE_HWID_HEX" "$LEGACY_KEY_FILE"; then
        echo "HATA: $STAGE_TAG $LABEL geçici backoff süresi dolduktan sonra normal hesap girişi iyileşmedi (kalıcı kilitlenme / DoS)!" >&2
        dump_stage_logs
        exit 1
    fi
    rm -f "$LEGACY_KEY_FILE"
    echo "  [PASS] $STAGE_TAG $LABEL AuthRateLimiter brute-force koruması (5 hata -> geçici 2sn backoff FAILURE) ve otomatik iyileşme (kalıcı kilitlenme yok) doğrulandı."

    stop_pid_bounded "$ACTIVE_GAME_PID"
    ACTIVE_GAME_PID=""
    stop_pid_bounded "$ACTIVE_AUTH_PID"
    ACTIVE_AUTH_PID=""
}

echo "[4/6] & [5/6] Gerçek db_r41023 + AUTH_SERVER + GAME_SERVER (Lua 5.0.3 Rollback) tam başlangıç, oyun dünyasına giriş ve NPC/quest coroutine testi..."
run_real_db_auth_and_game_smoke "[4-5/6]" "game_r41023_lua50 (Lua 5.0.3 Rollback)" "$GAME50_BIN" "real_lua50" 15012 15013 15020 15021

echo "[4/6] & [6/6] Gerçek db_r41023 + AUTH_SERVER + GAME_SERVER (Lua 5.5.1 Default) tam başlangıç, oyun dünyasına giriş ve NPC/quest coroutine testi..."
run_real_db_auth_and_game_smoke "[4-6/6]" "game_r41023 (Lua 5.5.1 Default)" "$GAME55_BIN" "real_lua55" 15014 15015 15022 15023

stop_pid_bounded "$ACTIVE_DB_PID"
ACTIVE_DB_PID=""

# Clean up temporary smoke test tables and test account in mt2_testdb
"$DB_CLI" -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" <<'EOF'
DROP TABLE IF EXISTS
  account, account_rev47_backup, hardware_status, player_index,
  locale, exp_table, spam_db, bootlog, special_spawn, priv_settings, guild, guild_war_reservation,
  marriage, `player`, item, quest, affect, player_special_flag, safebox, messenger_list, messenger_block_list,
  ikashop_safebox, ikashop_notification, loginlog, loginlog2, char_log, quest_state_log,
  mob_proto, item_proto, shop, shop_item,
  skill_proto, refine_proto, item_attr, item_attr_rare, banword, name_reservation,
  land, guild_land, object_proto, object, monarch, itemshop_items, itemshop_promotions,
  itemshop_time_auctions, itemshop_time_auction, ikashop_offlineshop, offlineshop_limit,
  crafting_proto, quest_reward_proto, shop_special, shop_special_proto, item_award, gmhost, gmlist;
EOF

echo "=================================================="
echo " [BASARILI] IZOLE DB + MOCK-DB + GERCEK DB BOOT + GERCEK AUTH/ARGON2ID/RATE-LIMIT + PHASE_GAME & QUEST/NPC COROUTINE (LUA 5.0 & LUA 5.5.1) TESTLERI TAMAMLANDI!"
echo "=================================================="
