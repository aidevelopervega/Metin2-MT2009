#!/bin/sh
# ==============================================================================
# Metin2 MT2009 - Gerçek Canlı Veritabanı Test Çalıştırıcısı (FreeBSD / POSIX)
# ==============================================================================

set -e

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ROOT_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
BUILD_DIR="$ROOT_DIR/tests/build"
CXX=${CXX:-g++}
AR=${AR:-ar}
RANLIB=${RANLIB:-ranlib}

HOST="${MT2_TEST_HOST:-127.0.0.1}"
PORT="${MT2_TEST_PORT:-3306}"
USER="${MT2_TEST_USER:-mt2test}"
DB="${MT2_TEST_DB:-mt2_testdb}"
ENGINE="${MT2_TEST_ENGINE:-Auto-Detect}"

echo "=================================================="
echo " METIN2 MT2009 - CANLI VERITABANI TEST CALISTIRICISI (POSIX)"
echo " Derleyici:      $CXX"
echo " Hedef Uç Nokta: $HOST:$PORT / DB: $DB"
echo " Kullanıcı:      $USER / Parola: [KORUMALI / LOGLANMAZ]"
echo " Motor:          $ENGINE"
echo "=================================================="

mkdir -p "$BUILD_DIR"

FLAGS="-std=c++20 -Wall -Wextra -O2 -pipe -fno-exceptions -fno-rtti"

RPATH=""
if [ -d "/usr/local/lib/gcc14" ]; then
    RPATH="-Wl,-rpath=/usr/local/lib/gcc14"
fi

SQL_DIR="$ROOT_DIR/server/src/libsql"
CORE_DIR="$ROOT_DIR/server/src/libthecore"
COMMON_DIR="$ROOT_DIR/server/src/common"

# Bağlayıcı Başlık ve Kütüphane Tespiti
INC_EXTRA=""
if [ -f "/usr/local/include/mysql/mysql.h" ]; then
    INC_EXTRA="-I/usr/local/include/mysql -I/usr/local/include"
elif [ -f "/usr/local/include/mariadb/mysql.h" ]; then
    INC_EXTRA="-I/usr/local/include/mariadb -I/usr/local/include"
else
    INC_EXTRA="-I/usr/local/include"
fi

LIB_EXTRA=""
if echo "$ENGINE" | grep -qi "mysql"; then
    if [ -f "/usr/local/lib/mysql/libmysqlclient.so" ]; then
        LIB_EXTRA="-L/usr/local/lib/mysql -lmysqlclient"
    elif [ -f "/usr/local/lib/libmysqlclient.so" ]; then
        LIB_EXTRA="-L/usr/local/lib -lmysqlclient"
    fi
else
    if [ -f "/usr/local/lib/mysql/libmariadb.so" ]; then
        LIB_EXTRA="-L/usr/local/lib/mysql -lmariadb"
    elif [ -f "/usr/local/lib/libmariadb.so" ]; then
        LIB_EXTRA="-L/usr/local/lib -lmariadb"
    elif [ -f "/usr/local/lib/mysql/libmysqlclient.so" ]; then
        LIB_EXTRA="-L/usr/local/lib/mysql -lmysqlclient"
    elif [ -f "/usr/local/lib/libmysqlclient.so" ]; then
        LIB_EXTRA="-L/usr/local/lib -lmysqlclient"
    fi
fi

if [ -z "$LIB_EXTRA" ]; then
    echo "HATA: FreeBSD üzerinde geçerli 64-bit MariaDB veya MySQL istemci kütüphanesi bulunamadı!" >&2
    echo "Lütfen pkg install -y mariadb114-server (veya mariadb123-server veya mysql84-server) çalıştırın." >&2
    exit 1
fi

echo "Derleniyor: $BUILD_DIR/test_live_db..."
$CXX $FLAGS $RPATH \
    $INC_EXTRA \
    -I"$ROOT_DIR/tests" \
    -I"$SQL_DIR" \
    -I"$CORE_DIR/include" \
    -I"$COMMON_DIR" \
    "$SQL_DIR/AsyncSQL.cpp" \
    "$SQL_DIR/Semaphore.cpp" \
    "$ROOT_DIR/tests/test_live_db.cpp" \
    "$BUILD_DIR/libthecore.a" \
    $LIB_EXTRA -lpthread \
    -o "$BUILD_DIR/test_live_db"

echo "Çalıştırılıyor: $BUILD_DIR/test_live_db..."
export RUN_LIVE_DB_TESTS=1
"$BUILD_DIR/test_live_db" \
    --host "$HOST" \
    --port "$PORT" \
    --user "$USER" \
    --db "$DB" \
    --engine "$ENGINE"

echo "=== [BASARILI] CANLI VERITABANI TESTLERI TAMAMLANDI! ==="
