#!/bin/sh
# ==============================================================================
# Metin2 MT2009 - Gerçek DB Cache / ACK Entegrasyon Test Çalıştırıcısı (FreeBSD / POSIX)
# ==============================================================================

set -e

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ROOT_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
BUILD_DIR="$ROOT_DIR/tests/build"
SERVER_DIR="$ROOT_DIR/mt2009 - FULL SOURCE/Server Source"
DB_SRC_DIR="$SERVER_DIR/Server/db/src"
DB_OBJ_DIR="$DB_SRC_DIR/.obj"

CXX=${CXX:-g++}

echo "=== METIN2 MT2009 - DB CACHE / ACK ENTEGRASYON TESTI ==="
echo "Derleyici: $CXX"

mkdir -p "$BUILD_DIR"

FLAGS="-std=c++20 -O2 -pipe -fno-rtti -fno-exceptions -pthread -D_THREAD_SAFE -w"

RPATH=""
if [ -d "/usr/local/lib/gcc14" ]; then
    RPATH="-Wl,-rpath=/usr/local/lib/gcc14"
fi

if [ -d "/usr/local/lib/mysql" ]; then
    RPATH="$RPATH -Wl,-rpath=/usr/local/lib/mysql"
fi
if [ -d "/usr/local/lib/mariadb" ]; then
    RPATH="$RPATH -Wl,-rpath=/usr/local/lib/mariadb"
fi

INCLUDES="-I\"$DB_SRC_DIR\" \
  -I\"$SERVER_DIR/Server/libthecore/include\" \
  -I\"$SERVER_DIR/Server/libsql\" \
  -I\"$SERVER_DIR/Server/libgame/include\" \
  -I\"$SERVER_DIR/Server/libpoly\" \
  -I\"$SERVER_DIR/Server/common\" \
  -I\"$SERVER_DIR/Extern/include\" \
  -I/usr/local/include/mysql \
  -I/usr/local/include/mariadb \
  -I/usr/local/include"

DB_CLIENT_LIB="-lmariadb"
if [ -f "/usr/local/lib/mysql/libmysqlclient.so" ] || [ -f "/usr/local/lib/libmysqlclient.so" ] || [ -f "/usr/local/lib/mysql/libmysqlclient.a" ]; then
    DB_CLIENT_LIB="-lmysqlclient"
fi

echo "Derlenmiş DB nesne dosyaları taranıyor..."
DB_OBJS=""
for f in "$DB_OBJ_DIR"/*.o; do
    case "$f" in
        *Main.o) ;;
        *) DB_OBJS="$DB_OBJS $f" ;;
    esac
done

if [ -z "$DB_OBJS" ]; then
    echo "HATA: DB nesne dosyaları ($DB_OBJ_DIR) bulunamadı! Önce db derlenmelidir." >&2
    exit 1
fi

echo "test_db_cache_integration derleniyor ve bağlanıyor..."
eval $CXX $FLAGS $RPATH $INCLUDES \
    "$ROOT_DIR/tests/test_db_cache_integration.cpp" \
    $DB_OBJS \
    "$BUILD_DIR/libthecore.a" \
    "$BUILD_DIR/libsql.a" \
    "$BUILD_DIR/libpoly.a" \
    "$BUILD_DIR/libgame.a" \
    -L/usr/local/lib/mysql -L/usr/local/lib/mariadb -L/usr/local/lib \
    $DB_CLIENT_LIB -lz -lssl -lcrypto -lpthread \
    -o "$BUILD_DIR/test_db_cache_integration"

echo "--> test_db_cache_integration çalıştırılıyor..."
"$BUILD_DIR/test_db_cache_integration"
echo "=== [BAŞARILI] DB CACHE ENTEGRASYON TESTİ GEÇTİ! ==="
