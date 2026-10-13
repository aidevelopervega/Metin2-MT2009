# ==============================================================================
# Metin2 MT2009 - Main CMakeLists.txt
# Supports Windows MinGW-w64 GCC 16.2 UCRT and FreeBSD 15.1 amd64 GCC 14
# ==============================================================================

cmake_minimum_required(VERSION 3.25)

project(Metin2_MT2009
    VERSION 1.0.0
    DESCRIPTION "Metin2 MT2009 Server and Core Libraries"
    LANGUAGES C CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# 1. Platform, Derleyici ve Uyarı Bayrakları
include(CompilerFlags)

# 2. Harici Bağımlılık Modülleri
include(FindDatabaseClient)
include(FindCryptoPP)
include(FindDevIL)

# 3. Sunucu Kütüphaneleri ve İkilileri
add_subdirectory(server)

# 4. Otomatik Testler ve CTest
option(BUILD_TESTING "Enable test targets" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
