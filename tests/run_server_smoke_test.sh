#!/bin/sh
# ==============================================================================
# Metin2 MT2009 - Isolated FreeBSD Server Startup, Full DB Boot & Auth/Login Smoke Test
# Stages (strictly separated and reported by category):
#   [1/4] Strict SQL Mode schema & temporary test account preparation in mt2_testdb
#   [2/4] Real db_r41023 isolated startup, SQL table load & TCP listener probe
#   [3/4] Mock-DB + 32-bit TCP wire handshake (PHASE_HANDSHAKE -> PHASE_LOGIN)
#         for game_r41023 (Exclusive Lua 5.5.1)
#   [4/4] Real db_r41023 + Real AUTH_SERVER + Real GAME_SERVER (Exclusive Lua 5.5.1)
#         full HEADER_DG_BOOT, Argon2id account authentication (HEADER_CG_LOGIN3 -> dwLoginKey),
#         LoginByKey (PHASE_SELECT), character creation, PHASE_GAME, restart & re-login
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
echo " Game Binary:    $GAME55_BIN (Lua 5.5.1 Exclusive)"
echo "=================================================="

for bin in "$DB_BIN" "$GAME55_BIN"; do
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

    if [ -f "$ROOT_DIR/server/files/share/locale/poland/quest/questlib.lua" ]; then
        cp -f "$ROOT_DIR/server/files/share/locale/poland/settings.lua" "$WORK_DIR/locale/poland/settings.lua"
        cp -f "$ROOT_DIR/server/files/share/locale/poland/BlueDragon.lua" "$WORK_DIR/locale/poland/BlueDragon.lua" 2>/dev/null || true
        cp -f "$ROOT_DIR/server/files/share/locale/poland/translate.lua" "$WORK_DIR/locale/poland/translate.lua"
        cp -R "$ROOT_DIR/server/files/share/locale/poland/quest/"* "$WORK_DIR/locale/poland/quest/" 2>/dev/null || true
    elif [ -f "$ROOT_DIR/server/serverfiles_share.tar.xz" ]; then
        FTP_EXTRACT="$SMOKE_ROOT/sf_extract"
        if [ ! -d "$FTP_EXTRACT" ]; then
            mkdir -p "$FTP_EXTRACT"
            tar -xJf "$ROOT_DIR/server/serverfiles_share.tar.xz" --strip-components=3 -C "$FTP_EXTRACT" \
                "usr/home/mt2/share/locale/poland/settings.lua" \
                "usr/home/mt2/share/locale/poland/BlueDragon.lua" \
                "usr/home/mt2/share/locale/poland/translate.lua" \
                "usr/home/mt2/share/locale/poland/quest" 2>/dev/null || true
        fi
        if [ -f "$FTP_EXTRACT/share/locale/poland/quest/questlib.lua" ]; then
            cp -f "$FTP_EXTRACT/share/locale/poland/settings.lua" "$WORK_DIR/locale/poland/settings.lua"
            cp -f "$FTP_EXTRACT/share/locale/poland/BlueDragon.lua" "$WORK_DIR/locale/poland/BlueDragon.lua" 2>/dev/null || true
            cp -f "$FTP_EXTRACT/share/locale/poland/translate.lua" "$WORK_DIR/locale/poland/translate.lua"
            cp -R "$FTP_EXTRACT/share/locale/poland/quest/"* "$WORK_DIR/locale/poland/quest/" 2>/dev/null || true
        else
            cp "$ROOT_DIR/tests/fixtures/settings.lua" "$WORK_DIR/locale/poland/settings.lua"
            cp "$ROOT_DIR/tests/fixtures/translate.lua" "$WORK_DIR/locale/poland/translate.lua"
            cp "$ROOT_DIR/tests/fixtures/questlib.lua" "$WORK_DIR/locale/poland/quest/questlib.lua"
            cp "$ROOT_DIR/tests/fixtures/locale.lua" "$WORK_DIR/locale/poland/quest/locale.lua"
        fi
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
if pc.count_item(19) == 0 then
    pc.give_item2(19, 1)
end
local lcnt = pc.getqf("login_count") + 1
pc.setqf("login_count", lcnt)
local has_yes = (type(locale) == "table" and type(locale.yes) == "string" and CONFIRM_YES == 1) and 1 or 0
pc.setqf("login_verified", 1)
syschat(string.format("[SMOKE_QUEST_LOGIN_OK] login_count=%d lv=%d item19=%d qlib=%d", lcnt, pc.get_level(), pc.count_item(19), has_yes))
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

    # Prepare map index 1 (metin2_map_a1: 4x5, BasePosition 409600x896000) so newly created characters at Empire 1 start (459800x953900 -> 502x579) and NPC 20016 spawn cleanly
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
    printf "502 579\n" > "$WORK_DIR/locale/poland/map/metin2_map_a1/Town.txt"
    printf "m\t502\t579\t0\t0\t0\t0\t1m\t100\t1\t20016\nm\t598\t682\t0\t0\t0\t0\t1m\t100\t1\t20016\n" > "$WORK_DIR/locale/poland/map/metin2_map_a1/npc.txt"
    "$BUILD_DIR/test_server_smoke_client" --gen-server-attr "$WORK_DIR/locale/poland/map/metin2_map_a1/server_attr" 16 20
    : > "$WORK_DIR/CMD"
}

# ------------------------------------------------------------------------------
# [1/6] Zero-Data Fresh Server Bootstrap from sql/*.sql & Secure Argon2id Account CLI
# ------------------------------------------------------------------------------
echo "[1/6] Strict SQL Mode altında depo sql/*.sql (account, common, log, player) şemaları sıfırdan yükleniyor..."
for sql_file in "$ROOT_DIR/sql/account.sql" "$ROOT_DIR/sql/common.sql" "$ROOT_DIR/sql/log.sql" "$ROOT_DIR/sql/player.sql"; do
    if [ ! -f "$sql_file" ]; then
        echo "HATA: [1/6] SQL şema dosyası bulunamadı: $sql_file" >&2
        exit 1
    fi
    {
        printf "SET SESSION sql_mode = 'STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION';\n"
        cat "$sql_file"
    } | "$DB_CLI" -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB"
done

"$DB_CLI" -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" <<'EOF'
SET SESSION sql_mode = 'STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION';

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
  account_id BIGINT UNSIGNED NOT NULL DEFAULT 0,
  name VARCHAR(24) NOT NULL DEFAULT '',
  PRIMARY KEY (account_id, name)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS land;
CREATE TABLE land (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  map_index INT NOT NULL DEFAULT 0,
  x INT NOT NULL DEFAULT 0,
  y INT NOT NULL DEFAULT 0,
  width INT NOT NULL DEFAULT 0,
  height INT NOT NULL DEFAULT 0,
  guild_id BIGINT UNSIGNED NOT NULL DEFAULT 0,
  guild_level_limit TINYINT NOT NULL DEFAULT 0,
  price BIGINT UNSIGNED NOT NULL DEFAULT 0,
  enable ENUM('YES','NO') NOT NULL DEFAULT 'YES',
  PRIMARY KEY (id)
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
  gold BIGINT NOT NULL DEFAULT 0,
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
  price BIGINT NOT NULL DEFAULT 0,
  items VARCHAR(255) NOT NULL DEFAULT '',
  random_item_count INT UNSIGNED NOT NULL DEFAULT 0,
  random_item_group INT UNSIGNED NOT NULL DEFAULT 0,
  limittype0 TINYINT UNSIGNED NOT NULL DEFAULT 0,
  limitvalue0 INT NOT NULL DEFAULT 0,
  limittype1 TINYINT UNSIGNED NOT NULL DEFAULT 0,
  limitvalue1 INT NOT NULL DEFAULT 0,
  PRIMARY KEY (vnum)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
EOF

PASS_COL_LEN=$("$DB_CLI" -N -s -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" -e "SELECT CHARACTER_MAXIMUM_LENGTH FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA='$DB' AND TABLE_NAME='account' AND COLUMN_NAME='password';")
EMPTY_COUNTS=$("$DB_CLI" -N -s -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" -e "SELECT CONCAT((SELECT COUNT(*) FROM account), ':', (SELECT COUNT(*) FROM player), ':', (SELECT COUNT(*) FROM player_index), ':', (SELECT COUNT(*) FROM guild), ':', (SELECT COUNT(*) FROM item), ':', (SELECT COUNT(*) FROM quest));")
if [ "$PASS_COL_LEN" != "255" ] || [ "$EMPTY_COUNTS" != "0:0:0:0:0:0" ]; then
    echo "HATA: [1/6] Temiz kurulum SQL şema doğrulaması başarısız (password_len=$PASS_COL_LEN, empty_counts=$EMPTY_COUNTS)!" >&2
    exit 1
fi
echo "  [PASS] [1/6] Depo sql/*.sql şemaları Strict SQL Mode altında yüklendi (account.password=VARCHAR(255), başlangıç satır sayıları account:player:player_index:guild:item:quest = $EMPTY_COUNTS)."

DB_WORK="$SMOKE_ROOT/db"
mkdir -p "$DB_WORK"

cat > "$DB_WORK/conf.txt" <<EOF
CLIENT_HEART_FPS = 25
HASH_PLAYER_LIFE_SEC = 600
BACKUP_LIMIT_SEC = 3600
PLAYER_CACHE_FLUSH_SECONDS = 1
ITEM_CACHE_FLUSH_SECONDS = 1
PLAYER_ID_START = 5000000001
PLAYER_DELETE_LEVEL_LIMIT = 70
ITEM_ID_RANGE = 5000000001 5100000000
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

# Verify that --create-account rejects 1-char weak passwords and >16-char passwords (32-bit client PASSWD_MAX_LEN=16)
if (cd "$DB_WORK" && printf '%s\n' "a" | "$DB_BIN" --create-account "bad_pwd_user" "1234567" >/dev/null 2>&1); then
    echo "HATA: [1/6] db_r41023 --create-account 1 karakterli zayıf şifreyi reddetmedi!" >&2
    exit 1
fi
if (cd "$DB_WORK" && printf '%s\n' "Password17Chars!!" | "$DB_BIN" --create-account "bad_pwd_user" "1234567" >/dev/null 2>&1); then
    echo "HATA: [1/6] db_r41023 --create-account 17 karakterli geçersiz şifreyi reddetmedi!" >&2
    exit 1
fi

# Create brand-new player account directly with Argon2id via stdin (default social_id=1234567, never SHA-1, never password in argv)
CREATE_OUT=$(cd "$DB_WORK" && printf '%s\n' "$SMOKE_TEST_PASS" | "$DB_BIN" --create-account "$SMOKE_TEST_LOGIN" "1234567" "$SMOKE_HWID_PLAIN")
case "$CREATE_OUT" in
    *"ACCOUNT_CREATED id="*" login=$SMOKE_TEST_LOGIN format=argon2id"*) ;;
    *)
        echo "HATA: [1/6] db_r41023 --create-account çıktısı beklenen formatta değil: $CREATE_OUT" >&2
        exit 1
        ;;
esac

NEW_ACCT_ROW=$("$DB_CLI" -N -s -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" -e "SELECT CONCAT(LEFT(password, 31), '|', social_id) FROM account WHERE login='$SMOKE_TEST_LOGIN';")
if [ "$NEW_ACCT_ROW" != '$argon2id$v=19$m=19456,t=2,p=1$|1234567' ]; then
    echo "HATA: [1/6] Yeni oluşturulan hesap Argon2id PHC veya social_id=1234567 formatında değil (row=$NEW_ACCT_ROW)!" >&2
    exit 1
fi
echo "  [PASS] [1/6] Güvenli CLI (db_r41023 --create-account) ile yeni hesap doğrudan Argon2id PHC formatında ve social_id=1234567 varsayılanıyla oluşturuldu ($CREATE_OUT)."

# ------------------------------------------------------------------------------
# [2/6] Real db_r41023 Isolated Startup, SQL Table Load & TCP Listener Probe
# ------------------------------------------------------------------------------
echo "[2/6] Gerçek db_r41023 izole başlatma, SQL tablo yükleme ve TCP dinleme duman testi..."

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

echo "[3/4] Mock-DB + 32-Bit TCP Wire Handshake (Lua 5.5.1 Exclusive) testi..."
run_mock_db_game_smoke "[3/4] game_r41023 (Lua 5.5.1 Exclusive)" "$GAME55_BIN" "mock_lua55" 13012 14012 15012

# ------------------------------------------------------------------------------
# Helper: Start db_r41023 + AUTH_SERVER + GAME_SERVER and wait for HEADER_DG_BOOT
# ------------------------------------------------------------------------------
start_db_auth_and_game_cluster() {
    STAGE_TAG="$1"
    LABEL="$2"
    GAME_BIN="$3"
    RUN_TAG="$4"
    AUTH_PORT="$5"
    AUTH_P2P_PORT="$6"
    GAME_PORT="$7"
    GAME_P2P_PORT="$8"

    (
        cd "$DB_WORK"
        exec "$DB_BIN" >"$DB_WORK/db_stdout_${RUN_TAG}.log" 2>"$DB_WORK/db_stderr_${RUN_TAG}.log"
    ) &
    ACTIVE_DB_PID=$!
    if ! "$BUILD_DIR/test_server_smoke_client" --probe-db 15010; then
        echo "HATA: $STAGE_TAG db_r41023 ($RUN_TAG) başlatılamadı!" >&2
        cat "$DB_WORK/db_stderr_${RUN_TAG}.log" 2>/dev/null || true
        exit 1
    fi

    AUTH_WORK="$SMOKE_ROOT/auth_${RUN_TAG}"
    prepare_game_workdir "$AUTH_WORK"

    cat > "$AUTH_WORK/CONFIG" <<EOF
HOSTNAME: auth_${RUN_TAG}
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

    GAME_WORK="$SMOKE_ROOT/game_${RUN_TAG}"
    prepare_game_workdir "$GAME_WORK"

    cat > "$GAME_WORK/CONFIG" <<EOF
HOSTNAME: $RUN_TAG
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
            echo "HATA: $STAGE_TAG $LABEL ($RUN_TAG) GAME_SERVER HEADER_DG_BOOT tamamlanmadan sonlandı!" >&2
        else
            echo "HATA: $STAGE_TAG $LABEL ($RUN_TAG) gerçek db_r41023 üzerinden HEADER_DG_BOOT (CInputDB::Boot) akışını 30 sn içinde tamamlamadı!" >&2
        fi
        dump_stage_logs
        exit 1
    fi
}

# ------------------------------------------------------------------------------
# [4/6] Real db_r41023 <-> Normal Game Server Full Zero-Data Bootstrap:
#       Argon2id Auth -> PHASE_SELECT (0 chars) -> Character Create -> PHASE_GAME
#       -> Server Restart -> Re-Login & Persisted Character World Entry
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

    echo "$STAGE_TAG $LABEL: Sıfırdan boş oyuncu veritabanı -> Argon2id AUTH -> Karakter Oluşturma -> PHASE_GAME -> Sunucu Yeniden Başlatma -> Aynı Hesap ve Karakterle Tekrar Giriş testi..."

    # Ensure player/item/quest tables start completely empty (0 rows) before starting this engine's test
    stop_pid_bounded "$ACTIVE_GAME_PID"
    ACTIVE_GAME_PID=""
    stop_pid_bounded "$ACTIVE_AUTH_PID"
    ACTIVE_AUTH_PID=""
    stop_pid_bounded "$ACTIVE_DB_PID"
    ACTIVE_DB_PID=""

    "$DB_CLI" -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" <<'EOF_RESET'
SET SESSION sql_mode = 'STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION';
DELETE FROM quest;
DELETE FROM item;
DELETE FROM affect;
DELETE FROM player_special_flag;
DELETE FROM player_index;
DELETE FROM `player`;
ALTER TABLE `player` AUTO_INCREMENT = 5000000001;
ALTER TABLE `item` AUTO_INCREMENT = 5000000001;
EOF_RESET
    "$DB_CLI" -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" -e "UPDATE account SET id = 5000000001 WHERE login = '$SMOKE_TEST_LOGIN';"

    start_db_auth_and_game_cluster "$STAGE_TAG" "$LABEL" "$GAME_BIN" "$HOSTNAME_TAG" "$AUTH_PORT" "$AUTH_P2P_PORT" "$GAME_PORT" "$GAME_P2P_PORT"
    echo "  [PASS] $STAGE_TAG $LABEL gerçek db_r41023 üzerinden HEADER_DG_BOOT (CInputDB::Boot + CQuestManager::Boot) paketini eksiksiz yükledi (boot packet end check ok [ffff]==0xffff)."

    if [ -f "$GAME_WORK/syserr" ] && grep -qiE "LUA_ERROR|InitializeLua|questlib|locale\.lua|settings\.lua|translate\.lua" "$GAME_WORK/syserr"; then
        echo "HATA: $STAGE_TAG $LABEL syserr içinde Lua çalışma zamanı hatası tespit edildi!" >&2
        cat "$GAME_WORK/syserr" >&2
        exit 1
    fi
    echo "  [PASS] $STAGE_TAG $LABEL CQuestManager::InitializeLua() + CQuestManager::Boot() 0 Lua hatası ile tamamlandı."

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

    # 1. Anti-enumeration & wrong password checks (must return uniform WRONGPWD, never NOID)
    if ! "$BUILD_DIR/test_server_smoke_client" --auth-expect-fail "$AUTH_PORT" "smk_nonexistent" "WrongPass999" "$SMOKE_HWID_HEX" "WRONGPWD"; then
        echo "HATA: $STAGE_TAG $LABEL AUTH_SERVER var olmayan hesabı WRONGPWD ile reddetmedi!" >&2
        dump_stage_logs
        exit 1
    fi
    sleep 0.4 2>/dev/null || sleep 1

    if ! "$BUILD_DIR/test_server_smoke_client" --auth-expect-fail "$AUTH_PORT" "@ENV" "WrongPass999" "$SMOKE_HWID_HEX" "WRONGPWD"; then
        echo "HATA: $STAGE_TAG $LABEL AUTH_SERVER yanlış şifreyi WRONGPWD ile reddetmedi!" >&2
        dump_stage_logs
        exit 1
    fi
    sleep 0.4 2>/dev/null || sleep 1

    # 1b. Protocol Framing Audit (G4.5: 10/10 checks on legacy 52B/130B, wrong version, TCP fragmentation, coalesced packets & partial timeout)
    if ! "$BUILD_DIR/test_server_smoke_client" --protocol-framing-audit "$AUTH_PORT" "$GAME_PORT" "@ENV" "@ENV" "$SMOKE_HWID_HEX"; then
        echo "HATA: $STAGE_TAG $LABEL --protocol-framing-audit (10/10) başarısız oldu!" >&2
        dump_stage_logs
        exit 1
    fi
    sleep 0.4 2>/dev/null || sleep 1

    # 2. First Login: Authenticate Argon2id account via AUTH_SERVER -> LoginByKey -> PHASE_SELECT (0 chars) -> Empire 1 + CharacterCreate('SmokeHero') -> PHASE_GAME + Quest/NPC Coroutine
    KEY_FILE="$AUTH_WORK/login_key.txt"
    rm -f "$KEY_FILE"
    if ! "$BUILD_DIR/test_server_smoke_client" --auth-login "$AUTH_PORT" "@ENV" "@ENV" "$SMOKE_HWID_HEX" "$KEY_FILE"; then
        echo "HATA: $STAGE_TAG $LABEL gerçek AUTH_SERVER Argon2id hesap girişi (HEADER_CG_LOGIN3) başarısız oldu!" >&2
        dump_stage_logs
        exit 1
    fi

    LOGIN_KEY=$(cat "$KEY_FILE" 2>/dev/null | tr -d '\r\n ')
    rm -f "$KEY_FILE"
    if [ -z "$LOGIN_KEY" ] || [ "$LOGIN_KEY" = "0" ]; then
        echo "HATA: $STAGE_TAG $LABEL AUTH_SERVER geçerli bir dwLoginKey üretmedi!" >&2
        exit 1
    fi

    if ! "$BUILD_DIR/test_server_smoke_client" --game-login-key "$GAME_PORT" "@ENV" "$LOGIN_KEY"; then
        echo "HATA: $STAGE_TAG $LABEL ilk giriş (PHASE_SELECT -> Karakter Oluşturma -> PHASE_GAME + Quest/NPC Coroutine) başarısız oldu!" >&2
        dump_stage_logs
        exit 1
    fi

    # 2b. Replay the consumed dwLoginKey -> must be rejected (single-use LoginByKey enforcement)
    if ! "$BUILD_DIR/test_server_smoke_client" --game-login-key-expect-fail "$GAME_PORT" "@ENV" "$LOGIN_KEY"; then
        echo "HATA: $STAGE_TAG $LABEL tüketilmiş dwLoginKey tekrar kullanıldığında (replay) GAME_SERVER tarafından reddedilmedi!" >&2
        dump_stage_logs
        exit 1
    fi

    if [ -f "$GAME_WORK/syserr" ] && grep -qiE "GetServerLocation|cannot find sectree|PlayerLoad|LUA_ERROR|WriteRunningStateToSyserr" "$GAME_WORK/syserr"; then
        echo "HATA: $STAGE_TAG $LABEL ilk giriş veya karakter oluşturma sırasında syserr hatası oluştu!" >&2
        cat "$GAME_WORK/syserr" >&2
        exit 1
    fi

    # 3. Verify that the newly created character (SmokeHero, pid=5000000001 > 2^32), player_index, starter item (vnum=19), and quest state flags were persisted to SQL
    CREATED_PID="0"
    for _ in $(seq 1 10); do
        CREATED_PID=$("$DB_CLI" -N -s -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" -e "SELECT id FROM \`player\` WHERE name='SmokeHero' LIMIT 1;")
        QUEST_SAVED_COUNT=$("$DB_CLI" -N -s -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" -e "SELECT COUNT(*) FROM quest WHERE szName='smoke_world' AND ((szState='login_verified' AND lValue=1) OR (szState='npc_flow_done' AND lValue=1) OR (szState='login_count' AND lValue=1));")
        ITEM_SAVED_COUNT=$("$DB_CLI" -N -s -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" -e "SELECT COUNT(*) FROM item WHERE vnum=19 AND id > 0 AND owner_id = 5000000001;")
        if [ "$CREATED_PID" = "5000000001" ] && [ "$QUEST_SAVED_COUNT" = "3" ] && [ "$ITEM_SAVED_COUNT" = "1" ]; then
            break
        fi
        sleep 1
    done

    INDEX_MATCH=$("$DB_CLI" -N -s -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" -e "SELECT COUNT(*) FROM player_index WHERE id=5000000001 AND pid1=${CREATED_PID:-0} AND empire=1;")
    if [ "$CREATED_PID" != "5000000001" ] || [ "$INDEX_MATCH" != "1" ] || [ "$QUEST_SAVED_COUNT" != "3" ] || [ "$ITEM_SAVED_COUNT" != "1" ]; then
        echo "HATA: $STAGE_TAG $LABEL yeni oluşturulan >2^32 karakter/envanter/quest veritabanına tam kaydedilmedi (pid=$CREATED_PID, index_match=$INDEX_MATCH, item19=$ITEM_SAVED_COUNT, quest_flags=$QUEST_SAVED_COUNT)!" >&2
        dump_stage_logs
        exit 1
    fi
    echo "  [PASS] $STAGE_TAG $LABEL yeni >2^32 karakter (SmokeHero, aid=5000000001, pid=$CREATED_PID, empire=1), >2^32 owner_id envanter (vnum=19) ve quest bayrakları (login_count=1, login_verified=1, npc_flow_done=1) canlı SQL veritabanına kaydedildi."

    # 4. Restart db_r41023 + AUTH_SERVER + GAME_SERVER and log in a SECOND time with the SAME Argon2id account and persisted character
    echo "  [INFO] $STAGE_TAG $LABEL sunucu kümesi (db_r41023 + AUTH_SERVER + GAME_SERVER) yeniden başlatılıyor..."
    stop_pid_bounded "$ACTIVE_GAME_PID"
    ACTIVE_GAME_PID=""
    stop_pid_bounded "$ACTIVE_AUTH_PID"
    ACTIVE_AUTH_PID=""
    stop_pid_bounded "$ACTIVE_DB_PID"
    ACTIVE_DB_PID=""

    RESTART_TAG="${HOSTNAME_TAG}_r2"
    start_db_auth_and_game_cluster "$STAGE_TAG" "$LABEL" "$GAME_BIN" "$RESTART_TAG" "$AUTH_PORT" "$AUTH_P2P_PORT" "$GAME_PORT" "$GAME_P2P_PORT"

    KEY_FILE2="$AUTH_WORK/login_key2.txt"
    rm -f "$KEY_FILE2"
    if ! "$BUILD_DIR/test_server_smoke_client" --auth-login "$AUTH_PORT" "@ENV" "@ENV" "$SMOKE_HWID_HEX" "$KEY_FILE2"; then
        echo "HATA: $STAGE_TAG $LABEL sunucu yeniden başlatıldıktan sonra AUTH_SERVER girişi başarısız oldu!" >&2
        dump_stage_logs
        exit 1
    fi

    LOGIN_KEY2=$(cat "$KEY_FILE2" 2>/dev/null | tr -d '\r\n ')
    rm -f "$KEY_FILE2"
    if [ -z "$LOGIN_KEY2" ] || [ "$LOGIN_KEY2" = "0" ]; then
        echo "HATA: $STAGE_TAG $LABEL yeniden başlatma sonrası AUTH_SERVER geçerli bir dwLoginKey üretmedi!" >&2
        exit 1
    fi

    if ! "$BUILD_DIR/test_server_smoke_client" --game-login-key "$GAME_PORT" "@ENV" "$LOGIN_KEY2"; then
        echo "HATA: $STAGE_TAG $LABEL sunucu yeniden başlatıldıktan sonra kalıcı karakterle (SmokeHero) oyuna ikinci giriş başarısız oldu!" >&2
        dump_stage_logs
        exit 1
    fi

    if [ -f "$GAME_WORK/syserr" ] && grep -qiE "GetServerLocation|cannot find sectree|PlayerLoad|LUA_ERROR|WriteRunningStateToSyserr" "$GAME_WORK/syserr"; then
        echo "HATA: $STAGE_TAG $LABEL yeniden başlatma sonrası ikinci giriş sırasında syserr hatası oluştu!" >&2
        cat "$GAME_WORK/syserr" >&2
        exit 1
    fi

    LOGIN_COUNT_AFTER="0"
    for _ in $(seq 1 10); do
        LOGIN_COUNT_AFTER=$("$DB_CLI" -N -s -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" -e "SELECT lValue FROM quest WHERE dwPID=$CREATED_PID AND szName='smoke_world' AND szState='login_count' LIMIT 1;")
        if [ "$LOGIN_COUNT_AFTER" = "2" ]; then
            break
        fi
        sleep 1
    done
    if [ "$LOGIN_COUNT_AFTER" != "2" ]; then
        echo "HATA: $STAGE_TAG $LABEL yeniden başlatma sonrası ikinci girişte quest login_count=2 olarak güncellenmedi (bulunan: $LOGIN_COUNT_AFTER)!" >&2
        dump_stage_logs
        exit 1
    fi
    echo "  [PASS] $STAGE_TAG $LABEL sunucu yeniden başlatıldıktan sonra aynı Argon2id hesap ve kalıcı karakter (SmokeHero, pid=$CREATED_PID) ile tekrar giriş ve PHASE_GAME doğrulandı (quest login_count=2)."

    # 5. Brute-force rate-limit & temporary exponential backoff check on AUTH_SERVER (5 failures -> 6th throttled with FAILURE -> recovers after 2s backoff)
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
    if ! "$BUILD_DIR/test_server_smoke_client" --auth-login "$AUTH_PORT" "@ENV" "@ENV" "$SMOKE_HWID_HEX" "$KEY_FILE2"; then
        echo "HATA: $STAGE_TAG $LABEL geçici backoff süresi dolduktan sonra normal hesap girişi iyileşmedi (kalıcı kilitlenme / DoS)!" >&2
        dump_stage_logs
        exit 1
    fi
    rm -f "$KEY_FILE2"
    echo "  [PASS] $STAGE_TAG $LABEL AuthRateLimiter brute-force koruması (5 hata -> geçici 2sn backoff FAILURE) ve otomatik iyileşme (kalıcı kilitlenme yok) doğrulandı."

    stop_pid_bounded "$ACTIVE_GAME_PID"
    ACTIVE_GAME_PID=""
    stop_pid_bounded "$ACTIVE_AUTH_PID"
    ACTIVE_AUTH_PID=""
    stop_pid_bounded "$ACTIVE_DB_PID"
    ACTIVE_DB_PID=""
}

echo "[4/4] Gerçek db_r41023 + AUTH_SERVER + GAME_SERVER (Lua 5.5.1 Exclusive) sıfırdan kurulum, karakter oluşturma, PHASE_GAME ve yeniden başlatma testi..."
run_real_db_auth_and_game_smoke "[4/4]" "game_r41023 (Lua 5.5.1 Exclusive)" "$GAME55_BIN" "real_lua55" 15014 15015 15022 15023

# Clean up all tables in mt2_testdb
DROP_QUERY=$("$DB_CLI" -N -s -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" -e "SELECT CONCAT('DROP TABLE IF EXISTS \`', table_name, '\`;') FROM information_schema.tables WHERE table_schema = '$DB';")
if [ -n "$DROP_QUERY" ]; then
    printf "SET FOREIGN_KEY_CHECKS=0;\n%s\nSET FOREIGN_KEY_CHECKS=1;\n" "$DROP_QUERY" | "$DB_CLI" -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB"
fi

echo "=================================================="
echo " [BASARILI] SIFIRDAN SQL KURULUMU + ARGON2ID HESAP CLI + KARAKTER OLUSTURMA + PHASE_GAME + YENIDEN BASLATMA SONRASI GIRIS (LUA 5.5.1 EXCLUSIVE) TESTLERI TAMAMLANDI!"
echo "=================================================="
