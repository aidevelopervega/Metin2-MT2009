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

# 1. libpoly
echo "[1/2] libpoly derleniyor ve test ediliyor..."
POLY_DIR="$SERVER_DIR/Server/libpoly"
EXTERN_DIR="$SERVER_DIR/Extern"
POLY_INCLUDES="-I$POLY_DIR -I$EXTERN_DIR/include"

$CXX $FLAGS $POLY_INCLUDES -c "$POLY_DIR/Base.cc" -o "$BUILD_DIR/Base.o"
$CXX $FLAGS $POLY_INCLUDES -c "$POLY_DIR/Symbol.cc" -o "$BUILD_DIR/Symbol.o"
$CXX $FLAGS $POLY_INCLUDES -c "$POLY_DIR/SymTable.cc" -o "$BUILD_DIR/SymTable.o"
$CXX $FLAGS $POLY_INCLUDES -c "$POLY_DIR/Poly.cc" -o "$BUILD_DIR/Poly.o"
$AR cru "$BUILD_DIR/libpoly.a" "$BUILD_DIR/Base.o" "$BUILD_DIR/Symbol.o" "$BUILD_DIR/SymTable.o" "$BUILD_DIR/Poly.o"
$RANLIB "$BUILD_DIR/libpoly.a"
$CXX $FLAGS $POLY_INCLUDES "$ROOT_DIR/tests/test_libpoly.cpp" "$BUILD_DIR/libpoly.a" -o "$BUILD_DIR/test_libpoly"
"$BUILD_DIR/test_libpoly"

# 2. libgame
echo "[2/2] libgame derleniyor ve test ediliyor..."
GAME_DIR="$SERVER_DIR/Server/libgame"
CORE_DIR="$SERVER_DIR/Server/libthecore"
COMMON_DIR="$SERVER_DIR/Server/common"
EXTERN_DIR="$SERVER_DIR/Extern"
GAME_INCLUDES="-I$GAME_DIR/include -I$CORE_DIR/include -I$COMMON_DIR -I$EXTERN_DIR/include"

$CXX $FLAGS $GAME_INCLUDES -c "$GAME_DIR/src/grid.cc" -o "$BUILD_DIR/game_grid.o"
$CXX $FLAGS $GAME_INCLUDES -c "$GAME_DIR/src/attribute.cc" -o "$BUILD_DIR/game_attribute.o"
$CXX $FLAGS $GAME_INCLUDES -c "$GAME_DIR/src/targa.cc" -o "$BUILD_DIR/game_targa.o"
$AR cru "$BUILD_DIR/libgame.a" "$BUILD_DIR/game_grid.o" "$BUILD_DIR/game_attribute.o" "$BUILD_DIR/game_targa.o"
$RANLIB "$BUILD_DIR/libgame.a"
$CXX $FLAGS $GAME_INCLUDES "$ROOT_DIR/tests/test_libgame.cpp" "$BUILD_DIR/libgame.a" -o "$BUILD_DIR/test_libgame"
"$BUILD_DIR/test_libgame"

echo "=== [BASARILI] TUM MODUL VE REGRESYON TESTLERI GECTI! ==="
