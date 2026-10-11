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

DB_CLIENT_LIB="-lmariadb"
if [ -f "/usr/local/lib/mysql/libmysqlclient.so" ] || [ -f "/usr/local/lib/libmysqlclient.so" ] || [ -f "/usr/local/lib/mysql/libmysqlclient.a" ]; then
    DB_CLIENT_LIB="-lmysqlclient"
fi

# Negatif Kontrol 1: Temiz derleme ortamı hazırlığı (eski kalıntıları temizle)
rm -f "$BUILD_DIR/libdb_objs.a"
rm -f "$BUILD_DIR/test_db_cache_integration"

# Negatif Kontrol 2: DB nesne dizini denetimi
echo "Derlenmiş DB nesne dosyaları taranıyor..."
if [ ! -d "$DB_OBJ_DIR" ]; then
    echo "HATA: DB nesne dizini ($DB_OBJ_DIR) bulunamadı! Önce db derlenmelidir." >&2
    exit 1
fi

# Negatif Kontrol 3: Zorunlu DB nesne dosyaları manifestosu
# Sadece sayı kontrolü yapılmaz; her zorunlu nesnenin varlığı tek tek doğrulanır.
REQUIRED_DB_OBJS="Cache.o ClientManager.o ClientManagerBoot.o ClientManagerGuild.o ClientManagerIkarusShop.o ClientManagerItem.o ClientManagerLogin.o ClientManagerParty.o ClientManagerPlayer.o Config.o DBManager.o IkarusShopCache.o ItemIDRangeManager.o Peer.o"
for obj in $REQUIRED_DB_OBJS; do
    if [ ! -f "$DB_OBJ_DIR/$obj" ]; then
        echo "HATA: Zorunlu DB nesne dosyası eksik: $DB_OBJ_DIR/$obj! db tam derlenmemiş." >&2
        exit 1
    fi
done

# Negatif Kontrol 4: Nesne dosyalarının mimari denetimi (64-Bit ELF / amd64)
SAMPLE_OBJ="$DB_OBJ_DIR/DBManager.o"
OBJ_ARCH=$(file "$SAMPLE_OBJ" 2>/dev/null || echo "unknown")
case "$OBJ_ARCH" in
    *64-bit*|*x86-64*|*amd64*)
        echo "--> DB nesne mimarisi doğrulandı ($SAMPLE_OBJ: 64-bit)."
        ;;
    *)
        echo "HATA: DB nesne dosyası 64-bit mimaride değil ($OBJ_ARCH)!" >&2
        exit 1
        ;;
esac

# Negatif Kontrol 5: Temiz arşiv oluşturma ve Main.o çakışmasını engelleme
# Arşiv sıfırdan oluşturulur ('ar cr'), eski üyeler barındırmaz.
ar cr "$BUILD_DIR/libdb_objs.a" "$DB_OBJ_DIR"/*.o
# Main.o sembol çakışmasını önlemek için kesinlikle arşivden çıkarılır:
ar d "$BUILD_DIR/libdb_objs.a" Main.o 2>/dev/null || true

# Main.o'nun arşivden tamamen çıkarıldığını doğrula:
if ar t "$BUILD_DIR/libdb_objs.a" | grep -q '^Main\.o$'; then
    echo "HATA: Main.o nesnesi libdb_objs.a arşivinden çıkarılamadı!" >&2
    exit 1
fi

ranlib "$BUILD_DIR/libdb_objs.a"

# Negatif Kontrol 6: Bağımlılık kütüphanelerinin varlık denetimi
for lib in "$BUILD_DIR/libthecore.a" "$BUILD_DIR/libsql.a" "$BUILD_DIR/libpoly.a" "$BUILD_DIR/libgame.a" "$BUILD_DIR/libdb_objs.a"; do
    if [ ! -f "$lib" ]; then
        echo "HATA: Zorunlu kütüphane eksik: $lib" >&2
        exit 1
    fi
done

echo "test_db_cache_integration derleniyor ve bağlanıyor..."
# eval KULLANILMAZ: Her yol doğrudan çift tırnakla korunur
$CXX $FLAGS \
    -I"$DB_SRC_DIR" \
    -I"$SERVER_DIR/Server/libthecore/include" \
    -I"$SERVER_DIR/Server/libsql" \
    -I"$SERVER_DIR/Server/libgame/include" \
    -I"$SERVER_DIR/Server/libpoly" \
    -I"$SERVER_DIR/Server/common" \
    -I"$SERVER_DIR/Extern/include" \
    -I/usr/local/include/mysql \
    -I/usr/local/include/mariadb \
    -I/usr/local/include \
    "$ROOT_DIR/tests/test_db_cache_integration.cpp" \
    "$BUILD_DIR/libdb_objs.a" \
    "$BUILD_DIR/libthecore.a" \
    "$BUILD_DIR/libsql.a" \
    "$BUILD_DIR/libpoly.a" \
    "$BUILD_DIR/libgame.a" \
    $RPATH \
    -L/usr/local/lib/mysql -L/usr/local/lib/mariadb -L/usr/local/lib \
    $DB_CLIENT_LIB -lz -lssl -lcrypto -lpthread \
    -o "$BUILD_DIR/test_db_cache_integration"

# Negatif Kontrol 3: Test ikili dosyasının varlığı ve çalıştırılabilirlik kontrolü
if [ ! -f "$BUILD_DIR/test_db_cache_integration" ] || [ ! -x "$BUILD_DIR/test_db_cache_integration" ]; then
    echo "HATA: test_db_cache_integration ikili dosyası üretilemedi veya çalıştırılamaz!" >&2
    exit 1
fi

# Negatif Kontrol 4: 64-bit mimari doğrulaması
TEST_ARCH=$(file "$BUILD_DIR/test_db_cache_integration" 2>/dev/null || echo "unknown")
echo "Üretilen test ikili mimarisi: $TEST_ARCH"
case "$TEST_ARCH" in
    *64-bit*|*x86-64*|*amd64*)
        echo "--> 64-Bit ikili mimari doğrulandı."
        ;;
    *)
        echo "HATA: Üretilen test ikili dosyası 64-bit değil ($TEST_ARCH)!" >&2
        exit 1
        ;;
esac

# Yürütme ve çıkış kodu yakalama
echo "--> test_db_cache_integration çalıştırılıyor..."
"$BUILD_DIR/test_db_cache_integration"
TEST_EXIT_CODE=$?

if [ $TEST_EXIT_CODE -ne 0 ]; then
    echo "HATA: test_db_cache_integration başarısız oldu (Çıkış kodu: $TEST_EXIT_CODE)!" >&2
    exit $TEST_EXIT_CODE
fi

echo "=== [BAŞARILI] DB CACHE ENTEGRASYON TESTİ GEÇTİ! ==="
