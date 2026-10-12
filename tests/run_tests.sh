#!/bin/sh
# ==============================================================================
# Metin2 MT2009 - Otomatik Modül Test Çalıştırıcısı (FreeBSD / POSIX)
# ==============================================================================

set -e

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ROOT_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
BUILD_DIR="$ROOT_DIR/tests/build"
SERVER_DIR="$ROOT_DIR/mt2009 - FULL SOURCE/Server Source"

CXX=${CXX:-g++}
AR=${AR:-ar}
RANLIB=${RANLIB:-ranlib}

echo "=== METIN2 MT2009 REGRESYON TESTLERI ==="
echo "Derleyici: $CXX"

mkdir -p "$BUILD_DIR"

FLAGS="-std=c++20 -Wall -Wextra -O2 -pipe -fno-exceptions -fno-rtti"

# FreeBSD GCC runtime rpath (GCC libstdc++.so konumunu bağla)
RPATH=""
if [ -d "/usr/local/lib/gcc14" ]; then
    RPATH="-Wl,-rpath=/usr/local/lib/gcc14"
fi

# 1. libpoly
echo "[1/2] libpoly derleniyor ve test ediliyor..."
if [ -d "$ROOT_DIR/server/src/libpoly" ]; then
    POLY_DIR="$ROOT_DIR/server/src/libpoly"
else
    POLY_DIR="$SERVER_DIR/Server/libpoly"
fi
EXTERN_DIR="$SERVER_DIR/Extern"

$CXX $FLAGS -I"$POLY_DIR" -I"$EXTERN_DIR/include" -c "$POLY_DIR/Base.cc" -o "$BUILD_DIR/Base.o"
$CXX $FLAGS -I"$POLY_DIR" -I"$EXTERN_DIR/include" -c "$POLY_DIR/Symbol.cc" -o "$BUILD_DIR/Symbol.o"
$CXX $FLAGS -I"$POLY_DIR" -I"$EXTERN_DIR/include" -c "$POLY_DIR/SymTable.cc" -o "$BUILD_DIR/SymTable.o"
$CXX $FLAGS -I"$POLY_DIR" -I"$EXTERN_DIR/include" -c "$POLY_DIR/Poly.cc" -o "$BUILD_DIR/Poly.o"
$AR cru "$BUILD_DIR/libpoly.a" "$BUILD_DIR/Base.o" "$BUILD_DIR/Symbol.o" "$BUILD_DIR/SymTable.o" "$BUILD_DIR/Poly.o"
$RANLIB "$BUILD_DIR/libpoly.a"
$CXX $FLAGS $RPATH -I"$POLY_DIR" -I"$EXTERN_DIR/include" "$ROOT_DIR/tests/test_libpoly.cpp" "$BUILD_DIR/libpoly.a" -o "$BUILD_DIR/test_libpoly"
"$BUILD_DIR/test_libpoly"

# 2. libthecore
echo "[2/3] libthecore derleniyor ve test ediliyor..."
if [ -d "$ROOT_DIR/server/src/libthecore" ]; then
    CORE_DIR="$ROOT_DIR/server/src/libthecore"
else
    CORE_DIR="$SERVER_DIR/Server/libthecore"
fi
COMMON_DIR="$SERVER_DIR/Server/common"

CORE_INCLUDES="-I\"$CORE_DIR/include\" -I\"$COMMON_DIR\" -I\"$EXTERN_DIR/include\""

for src in DES_table.c buffer.c des.c fdwatch.c gost.c hangul.c heart.c kstbl.c log.c main.c memcpy.c signal.c socket.c tea.c utils.c; do
    obj_name=$(basename "$src" .c)
    $CXX $FLAGS -I"$CORE_DIR/include" -I"$COMMON_DIR" -I"$EXTERN_DIR/include" -c "$CORE_DIR/src/$src" -o "$BUILD_DIR/core_${obj_name}.o"
done

$AR cru "$BUILD_DIR/libthecore.a" "$BUILD_DIR"/core_*.o
$RANLIB "$BUILD_DIR/libthecore.a"

$CXX $FLAGS $RPATH -I"$CORE_DIR/include" -I"$COMMON_DIR" -I"$EXTERN_DIR/include" "$ROOT_DIR/tests/test_libthecore.cpp" "$BUILD_DIR/libthecore.a" -lpthread -o "$BUILD_DIR/test_libthecore"
"$BUILD_DIR/test_libthecore"

# 3. libgame (Gercek libthecore.a ile bagli)
echo "[3/3] libgame derleniyor ve test ediliyor..."
if [ -d "$ROOT_DIR/server/src/libgame" ]; then
    GAME_DIR="$ROOT_DIR/server/src/libgame"
else
    GAME_DIR="$SERVER_DIR/Server/libgame"
fi

$CXX $FLAGS -I"$GAME_DIR/include" -I"$CORE_DIR/include" -I"$COMMON_DIR" -I"$EXTERN_DIR/include" -c "$GAME_DIR/src/grid.cc" -o "$BUILD_DIR/game_grid.o"
$CXX $FLAGS -I"$GAME_DIR/include" -I"$CORE_DIR/include" -I"$COMMON_DIR" -I"$EXTERN_DIR/include" -c "$GAME_DIR/src/attribute.cc" -o "$BUILD_DIR/game_attribute.o"
$CXX $FLAGS -I"$GAME_DIR/include" -I"$CORE_DIR/include" -I"$COMMON_DIR" -I"$EXTERN_DIR/include" -c "$GAME_DIR/src/targa.cc" -o "$BUILD_DIR/game_targa.o"
$AR cru "$BUILD_DIR/libgame.a" "$BUILD_DIR/game_grid.o" "$BUILD_DIR/game_attribute.o" "$BUILD_DIR/game_targa.o"
$RANLIB "$BUILD_DIR/libgame.a"
$CXX $FLAGS $RPATH -I"$GAME_DIR/include" -I"$CORE_DIR/include" -I"$COMMON_DIR" -I"$EXTERN_DIR/include" "$ROOT_DIR/tests/test_libgame.cpp" "$BUILD_DIR/libgame.a" "$BUILD_DIR/libthecore.a" -lpthread -o "$BUILD_DIR/test_libgame"
"$BUILD_DIR/test_libgame"

# 4. libsql
echo "[4/4] libsql derleniyor ve test ediliyor..."
if [ -d "$ROOT_DIR/server/src/libsql" ]; then
    SQL_DIR="$ROOT_DIR/server/src/libsql"
else
    SQL_DIR="$SERVER_DIR/Server/libsql"
fi

for src in AsyncSQL.cpp Semaphore.cpp Statement.cpp Tellwait.cpp; do
    obj_name=$(basename "$src" .cpp)
    $CXX $FLAGS -I"$SQL_DIR" -I"$CORE_DIR/include" -I"$COMMON_DIR" -I"$EXTERN_DIR/include" -c "$SQL_DIR/$src" -o "$BUILD_DIR/sql_${obj_name}.o"
done

$AR cru "$BUILD_DIR/libsql.a" "$BUILD_DIR"/sql_*.o
$RANLIB "$BUILD_DIR/libsql.a"

$CXX $FLAGS $RPATH -I"$SQL_DIR" -I"$CORE_DIR/include" -I"$COMMON_DIR" -I"$EXTERN_DIR/include" "$ROOT_DIR/tests/test_libsql.cpp" "$BUILD_DIR/libsql.a" "$BUILD_DIR/libthecore.a" -lpthread -o "$BUILD_DIR/test_libsql"
"$BUILD_DIR/test_libsql"

# 5. db cache
echo "[5/5] db cache ve veri guvenligi testleri derleniyor ve test ediliyor..."
$CXX $FLAGS $RPATH -I"$CORE_DIR/include" -I"$COMMON_DIR" -I"$SQL_DIR" -I"$EXTERN_DIR/include" "$ROOT_DIR/tests/test_db_cache.cpp" "$BUILD_DIR/libthecore.a" -lpthread -o "$BUILD_DIR/test_db_cache"
"$BUILD_DIR/test_db_cache"

echo "=== [BASARILI] TUM MODUL VE REGRESYON TESTLERI GECTI! ==="


