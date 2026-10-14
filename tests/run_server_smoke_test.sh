#!/bin/sh
# ==============================================================================
# Metin2 MT2009 - Isolated FreeBSD Server Startup & Wire Handshake Smoke Test
# Tests:
#   1. db_r41023 isolated startup against live MariaDB/MySQL under strict SQL mode
#   2. game_r41023 (Lua 5.0.3) isolated startup, QuestManager::InitializeLua(),
#      DB peer connection, and 32-bit client wire TPacketGCHandshake / PHASE_LOGIN
#   3. game_r41023_lua55 (Lua 5.5.1) isolated startup, QuestManager::InitializeLua(),
#      DB peer connection, and 32-bit client wire TPacketGCHandshake / PHASE_LOGIN
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
GAME50_BIN="${MT2_SMOKE_GAME50_BIN:-$ROOT_DIR/server/src/game/game_r41023}"
GAME55_BIN="${MT2_SMOKE_GAME55_BIN:-$ROOT_DIR/server/src/game/game_r41023_lua55}"

echo "=================================================="
echo " METIN2 MT2009 - ISOLATED SERVER STARTUP SMOKE TEST"
echo " Engine:         $ENGINE ($HOST:$PORT / $DB)"
echo " DB Binary:      $DB_BIN"
echo " Game 5.0 Bin:   $GAME50_BIN"
echo " Game 5.5 Bin:   $GAME55_BIN"
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
cleanup_all() {
    if [ -n "$ACTIVE_GAME_PID" ]; then
        kill "$ACTIVE_GAME_PID" 2>/dev/null || true
        wait "$ACTIVE_GAME_PID" 2>/dev/null || true
    fi
    if [ -n "$ACTIVE_MOCK_PID" ]; then
        kill "$ACTIVE_MOCK_PID" 2>/dev/null || true
        wait "$ACTIVE_MOCK_PID" 2>/dev/null || true
    fi
    if [ -n "$ACTIVE_DB_PID" ]; then
        kill "$ACTIVE_DB_PID" 2>/dev/null || true
        wait "$ACTIVE_DB_PID" 2>/dev/null || true
    fi
    rm -rf "$SMOKE_ROOT"
}
trap cleanup_all EXIT INT TERM

echo "[1/4] Strict SQL Mode altında izole duman testi şeması hazırlanıyor..."
"$DB_CLI" -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" <<'EOF'
SET SESSION sql_mode = 'STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION';

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
INSERT INTO exp_table (level, exp) VALUES (1, 300), (2, 800), (3, 1500);

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
  gold BIGINT NOT NULL DEFAULT 0,
  PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS item;
CREATE TABLE item (
  id INT UNSIGNED NOT NULL AUTO_INCREMENT,
  owner_id INT UNSIGNED NOT NULL DEFAULT 0,
  `window` VARCHAR(32) NOT NULL DEFAULT 'INVENTORY',
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
  PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS quest;
CREATE TABLE quest (
  dwPID INT UNSIGNED NOT NULL DEFAULT 0,
  szName VARCHAR(32) NOT NULL DEFAULT '',
  szState VARCHAR(64) NOT NULL DEFAULT '',
  lValue INT NOT NULL DEFAULT 0,
  PRIMARY KEY (dwPID, szName, szState)
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
INSERT INTO mob_proto (vnum, name, locale_name, level, max_hp) VALUES (101, 'WildDog', 'Dziki Pies', 1, 100);

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
INSERT INTO item_proto (vnum, type, subtype, name, locale_name, size) VALUES (10, 1, 0, 'Sword+0', 'Miecz+0', 2);

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
  count TINYINT UNSIGNED NOT NULL DEFAULT 1
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
INSERT INTO shop_item (shop_vnum, item_vnum, count) VALUES (1, 10, 1);

DROP TABLE IF EXISTS skill_proto;
CREATE TABLE skill_proto (
  dwVnum INT NOT NULL DEFAULT 0,
  szName VARCHAR(32) NOT NULL DEFAULT '',
  bType TINYINT NOT NULL DEFAULT 0,
  bMaxLevel TINYINT NOT NULL DEFAULT 20,
  dwSplashRange INT NOT NULL DEFAULT 0,
  szPointOn VARCHAR(64) NOT NULL DEFAULT 'NONE',
  szPointPoly VARCHAR(100) NOT NULL DEFAULT '0',
  szSPCostPoly VARCHAR(100) NOT NULL DEFAULT '0',
  szDurationPoly VARCHAR(100) NOT NULL DEFAULT '0',
  szDurationSPCostPoly VARCHAR(100) NOT NULL DEFAULT '0',
  szCooldownPoly VARCHAR(100) NOT NULL DEFAULT '0',
  szMasterBonusPoly VARCHAR(100) NOT NULL DEFAULT '0',
  setFlag INT UNSIGNED NOT NULL DEFAULT 0,
  setAffectFlag INT UNSIGNED NOT NULL DEFAULT 1,
  szPointOn2 VARCHAR(64) NOT NULL DEFAULT 'NONE',
  szPointPoly2 VARCHAR(100) NOT NULL DEFAULT '0',
  szDurationPoly2 VARCHAR(100) NOT NULL DEFAULT '0',
  setAffectFlag2 INT UNSIGNED NOT NULL DEFAULT 1,
  szPointOn3 VARCHAR(64) NOT NULL DEFAULT 'NONE',
  szPointPoly3 VARCHAR(100) NOT NULL DEFAULT '0',
  szDurationPoly3 VARCHAR(100) NOT NULL DEFAULT '0',
  setAffectFlag3 INT UNSIGNED NOT NULL DEFAULT 1,
  szGrandMasterAddSPCostPoly VARCHAR(100) NOT NULL DEFAULT '0',
  bLevelStep TINYINT NOT NULL DEFAULT 0,
  bLevelLimit TINYINT NOT NULL DEFAULT 0,
  prerequisiteSkillVnum INT NOT NULL DEFAULT 0,
  prerequisiteSkillLevel INT NOT NULL DEFAULT 0,
  iMaxHit SMALLINT NOT NULL DEFAULT 0,
  szSplashAroundDamageAdjustPoly VARCHAR(100) NOT NULL DEFAULT '1',
  eSkillType TINYINT NOT NULL DEFAULT 0,
  dwTargetRange INT NOT NULL DEFAULT 0,
  PRIMARY KEY (dwVnum)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
INSERT INTO skill_proto (dwVnum, szName, bType) VALUES (1, 'ThreeWay', 1);

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
  cost INT NOT NULL DEFAULT 0,
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
  weapon INT NOT NULL DEFAULT 5,
  body INT NOT NULL DEFAULT 5,
  wrist INT NOT NULL DEFAULT 5,
  foots INT NOT NULL DEFAULT 5,
  neck INT NOT NULL DEFAULT 5,
  head INT NOT NULL DEFAULT 5,
  shield INT NOT NULL DEFAULT 5,
  ear INT NOT NULL DEFAULT 5,
  pendant INT NOT NULL DEFAULT 5,
  PRIMARY KEY (apply)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
INSERT INTO item_attr (apply, prob) VALUES ('MAX_HP', 10);

DROP TABLE IF EXISTS item_attr_rare;
CREATE TABLE item_attr_rare LIKE item_attr;
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
  id INT NOT NULL AUTO_INCREMENT,
  map_index INT NOT NULL DEFAULT 0,
  x INT NOT NULL DEFAULT 0,
  y INT NOT NULL DEFAULT 0,
  width INT NOT NULL DEFAULT 0,
  height INT NOT NULL DEFAULT 0,
  guild_id INT UNSIGNED NOT NULL DEFAULT 0,
  guild_level_limit TINYINT NOT NULL DEFAULT 0,
  price BIGINT UNSIGNED NOT NULL DEFAULT 0,
  enable ENUM('YES','NO') NOT NULL DEFAULT 'NO',
  PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS guild_land;
CREATE TABLE guild_land (
  land_id INT NOT NULL DEFAULT 0,
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
  id INT NOT NULL AUTO_INCREMENT,
  land_id INT NOT NULL DEFAULT 0,
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
  windate DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  money BIGINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (empire)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS itemshop_items;
CREATE TABLE itemshop_items (
  `index` INT NOT NULL,
  vnum INT NOT NULL,
  count SMALLINT NOT NULL,
  price INT NOT NULL,
  currency ENUM('DRAGON_COIN','DRAGON_MARK') NOT NULL DEFAULT 'DRAGON_COIN',
  minLevel INT NOT NULL DEFAULT 0,
  socket0 INT NOT NULL DEFAULT 0,
  socket1 INT NOT NULL DEFAULT 0,
  socket2 INT NOT NULL DEFAULT 0,
  PRIMARY KEY (`index`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS itemshop_promotions;
CREATE TABLE itemshop_promotions (
  item_index INT NOT NULL,
  price INT NOT NULL,
  start_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  end_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (item_index)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS itemshop_time_auctions;
CREATE TABLE itemshop_time_auctions (
  item_index INT NOT NULL,
  max_amount INT NOT NULL DEFAULT 0,
  account_limit INT UNSIGNED NOT NULL DEFAULT 0,
  start_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  end_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (item_index)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS itemshop_time_auction;
CREATE TABLE itemshop_time_auction (
  item_index INT NOT NULL,
  buy_count INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (item_index)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS ikashop_offlineshop;
CREATE TABLE ikashop_offlineshop (
  owner INT UNSIGNED NOT NULL,
  duration INT UNSIGNED NOT NULL DEFAULT 0,
  name VARCHAR(64) NOT NULL DEFAULT '',
  map INT NOT NULL DEFAULT 1,
  x INT NOT NULL DEFAULT 0,
  y INT NOT NULL DEFAULT 0,
  channel TINYINT NOT NULL DEFAULT 1,
  unlock_count TINYINT UNSIGNED NOT NULL DEFAULT 0,
  is_premium TINYINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (owner)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS offlineshop_limit;
CREATE TABLE offlineshop_limit (
  channel TINYINT NOT NULL DEFAULT 1,
  map_index INT NOT NULL DEFAULT 1,
  shop_limit INT UNSIGNED NOT NULL DEFAULT 100,
  premium_limit INT UNSIGNED NOT NULL DEFAULT 100
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
INSERT INTO offlineshop_limit (channel, map_index, shop_limit, premium_limit) VALUES (1, 1, 100, 100);

DROP TABLE IF EXISTS crafting_proto;
CREATE TABLE crafting_proto (
  vnum INT UNSIGNED NOT NULL DEFAULT 0,
  item_vnum INT UNSIGNED NOT NULL DEFAULT 0,
  count SMALLINT UNSIGNED NOT NULL DEFAULT 1,
  price BIGINT UNSIGNED NOT NULL DEFAULT 0,
  chance SMALLINT UNSIGNED NOT NULL DEFAULT 100,
  recipe VARCHAR(128) NOT NULL DEFAULT '',
  req_progress SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  req_level SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  recipe_vnum INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (vnum)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS quest_reward_proto;
CREATE TABLE quest_reward_proto (
  quest_name VARCHAR(64) NOT NULL DEFAULT '',
  exp INT UNSIGNED NOT NULL DEFAULT 0,
  gold INT UNSIGNED NOT NULL DEFAULT 0,
  alignment INT NOT NULL DEFAULT 0,
  items VARCHAR(128) NOT NULL DEFAULT '',
  warrior_items VARCHAR(128) NOT NULL DEFAULT '',
  assassin_items VARCHAR(128) NOT NULL DEFAULT '',
  sura_items VARCHAR(128) NOT NULL DEFAULT '',
  shaman_items VARCHAR(128) NOT NULL DEFAULT '',
  PRIMARY KEY (quest_name)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS shop_special;
CREATE TABLE shop_special (
  vnum INT UNSIGNED NOT NULL DEFAULT 0,
  item_vnum INT UNSIGNED NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS shop_special_proto;
CREATE TABLE shop_special_proto (
  vnum INT UNSIGNED NOT NULL DEFAULT 0,
  item_vnum INT UNSIGNED NOT NULL DEFAULT 0,
  count SMALLINT UNSIGNED NOT NULL DEFAULT 1,
  rare_pct SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  price_type TINYINT UNSIGNED NOT NULL DEFAULT 1,
  price BIGINT UNSIGNED NOT NULL DEFAULT 0,
  items VARCHAR(128) NOT NULL DEFAULT '',
  random_item_count SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  random_item_group INT UNSIGNED NOT NULL DEFAULT 0,
  limittype0 TINYINT UNSIGNED NOT NULL DEFAULT 1,
  limitvalue0 INT NOT NULL DEFAULT 0,
  limittype1 TINYINT UNSIGNED NOT NULL DEFAULT 1,
  limitvalue1 INT NOT NULL DEFAULT 0,
  PRIMARY KEY (vnum)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS gmhost;
CREATE TABLE gmhost (
  mIP VARCHAR(16) NOT NULL DEFAULT '',
  PRIMARY KEY (mIP)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
INSERT INTO gmhost (mIP) VALUES ('127.0.0.1');

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
EOF

# ------------------------------------------------------------------------------
# 2. db_r41023 Isolated Startup & TCP Peer Acceptance Smoke Test
# ------------------------------------------------------------------------------
echo "[2/4] db_r41023 izole başlatma ve TCP dinleme duman testi..."
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
    "$DB_BIN" >"$DB_WORK/db_stdout.log" 2>"$DB_WORK/db_stderr.log"
) &
ACTIVE_DB_PID=$!

if ! "$BUILD_DIR/test_server_smoke_client" --probe-db 15010; then
    echo "HATA: db_r41023 başlatılamadı veya 127.0.0.1:15010 üzerinde dinlemiyor!" >&2
    cat "$DB_WORK/db_stderr.log" 2>/dev/null || true
    cat "$DB_WORK/sysserr" 2>/dev/null || true
    cat "$DB_WORK/syslog" 2>/dev/null || true
    exit 1
fi

kill "$ACTIVE_DB_PID" 2>/dev/null || true
wait "$ACTIVE_DB_PID" 2>/dev/null || true
ACTIVE_DB_PID=""
echo "  [PASS] db_r41023 izole ortamda başarıyla başlatıldı, tüm SQL tablolarını yükledi ve temiz kapatıldı."

# ------------------------------------------------------------------------------
# Helper function to smoke-test a Game Server binary (Lua 5.0.3 or Lua 5.5.1)
# ------------------------------------------------------------------------------
run_game_smoke() {
    LABEL="$1"
    GAME_BIN="$2"
    HOSTNAME_TAG="$3"
    GAME_PORT="$4"
    P2P_PORT="$5"
    MOCK_DB_PORT="$6"

    echo "$LABEL izole başlatma, Lua quest runtime ve 32-bit istemci el sıkışma (handshake) testi..."
    GAME_WORK="$SMOKE_ROOT/$HOSTNAME_TAG"
    mkdir -p "$GAME_WORK/locale/poland/quest/object" "$GAME_WORK/package" "$GAME_WORK/mark"

    cp "$ROOT_DIR/tests/fixtures/settings.lua" "$GAME_WORK/locale/poland/settings.lua"
    cp "$ROOT_DIR/tests/fixtures/translate.lua" "$GAME_WORK/locale/poland/translate.lua"
    cp "$ROOT_DIR/tests/fixtures/questlib.lua" "$GAME_WORK/locale/poland/quest/questlib.lua"
    cp "$ROOT_DIR/tests/fixtures/locale.lua" "$GAME_WORK/locale/poland/quest/locale.lua"
    printf "20016\tblacksmith\n9001\tweapon_shop\n" > "$GAME_WORK/locale/poland/quest/questnpc.txt"
    : > "$GAME_WORK/locale/poland/forkedmapindex.txt"
    : > "$GAME_WORK/locale/poland/cube.txt"
    : > "$GAME_WORK/locale/poland/blend.txt"
    : > "$GAME_WORK/locale/poland/locale_string.txt"
    : > "$GAME_WORK/CMD"

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
        "$GAME_BIN" -I 127.0.0.1 -v >"$GAME_WORK/game_stdout.log" 2>"$GAME_WORK/game_stderr.log"
    ) &
    ACTIVE_GAME_PID=$!

    if ! "$BUILD_DIR/test_server_smoke_client" --probe-game "$GAME_PORT"; then
        echo "HATA: $LABEL istemci TCP/handshake doğrulaması başarısız oldu!" >&2
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
    echo "  [PASS] $LABEL db_clientdesc paketlerini gönderdi ($(tr '\n' ' ' < "$OUT_FILE"))."

    # Check that syserr has no Lua errors
    if [ -f "$GAME_WORK/syserr" ] && grep -qiE "LUA_ERROR|InitializeLua|questlib|locale\.lua|settings\.lua|translate\.lua" "$GAME_WORK/syserr"; then
        echo "HATA: $LABEL syserr içinde Lua çalışma zamanı hatası tespit edildi!" >&2
        cat "$GAME_WORK/syserr" >&2
        exit 1
    fi
    echo "  [PASS] $LABEL CQuestManager::Initialize() -> InitializeLua() 0 Lua hatası ile tamamlandı."

    # Verify bootlog row was inserted into live SQL DB (wait up to 10s for CAsyncSQL worker flush)
    BOOT_COUNT="0"
    for _ in $(seq 1 10); do
        BOOT_COUNT=$("$DB_CLI" -N -s -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" -e "SELECT COUNT(*) FROM bootlog WHERE hostname='$HOSTNAME_TAG';")
        if [ "$BOOT_COUNT" = "1" ]; then
            break
        fi
        sleep 1
    done
    if [ "$BOOT_COUNT" != "1" ]; then
        echo "HATA: $LABEL bootlog kaydı bulunamadı (beklenen: 1, bulunan: $BOOT_COUNT)!" >&2
        exit 1
    fi
    echo "  [PASS] $LABEL canlı SQL veritabanına bootlog kaydını başarıyla yazdı (hostname=$HOSTNAME_TAG)."

    kill "$ACTIVE_GAME_PID" 2>/dev/null || true
    wait "$ACTIVE_GAME_PID" 2>/dev/null || true
    ACTIVE_GAME_PID=""

    kill "$ACTIVE_MOCK_PID" 2>/dev/null || true
    wait "$ACTIVE_MOCK_PID" 2>/dev/null || true
    ACTIVE_MOCK_PID=""
}

# ------------------------------------------------------------------------------
# 3. game_r41023 (Lua 5.0.3) Isolated Startup & Handshake Smoke Test
# ------------------------------------------------------------------------------
run_game_smoke "[3/4] game_r41023 (Lua 5.0.3)" "$GAME50_BIN" "smoke_lua50" 13011 14011 15011

# ------------------------------------------------------------------------------
# 4. game_r41023_lua55 (Lua 5.5.1) Isolated Startup & Handshake Smoke Test
# ------------------------------------------------------------------------------
run_game_smoke "[4/4] game_r41023_lua55 (Lua 5.5.1)" "$GAME55_BIN" "smoke_lua55" 13012 14012 15012

# Clean up temporary smoke test tables in mt2_testdb
"$DB_CLI" -h "$HOST" -P "$PORT" -u "$USER" -p"$PASS" "$DB" <<'EOF'
DROP TABLE IF EXISTS
  locale, exp_table, spam_db, bootlog, priv_settings, guild, guild_war_reservation,
  marriage, `player`, item, quest, mob_proto, item_proto, shop, shop_item,
  skill_proto, refine_proto, item_attr, item_attr_rare, banword, name_reservation,
  land, guild_land, object_proto, object, monarch, itemshop_items, itemshop_promotions,
  itemshop_time_auctions, itemshop_time_auction, ikashop_offlineshop, offlineshop_limit,
  crafting_proto, quest_reward_proto, shop_special, shop_special_proto, gmhost, gmlist;
EOF

echo "=================================================="
echo " [BASARILI] IZOLE DB + GAME (LUA 5.0 & LUA 5.5.1) SUNUCU DUMAN TESTI TAMAMLANDI!"
echo "=================================================="
