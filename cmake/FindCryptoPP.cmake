# ==============================================================================
# Metin2 MT2009 - CryptoPP Discovery (Strict Pure GCC libstdc++, No libc++ Leak)
# ==============================================================================

if(TARGET CryptoPP::CryptoPP)
    return()
endif()

set(CRYPTOPP_INC_DIRS
    "${CMAKE_SOURCE_DIR}/extern/server/include"
)

# 1. Öncelik: GCC 14 ile derlenen yerel 64-bit CryptoPP statik kütüphanesi
if(WIN32)
    find_file(CRYPTOPP_STATIC_LIB
        NAMES libcryptopp.a
        PATHS
            "${CMAKE_SOURCE_DIR}/extern/platform/windows-x64/mingw-ucrt-gcc16/lib"
        NO_DEFAULT_PATH
    )
else()
    find_file(CRYPTOPP_STATIC_LIB
        NAMES libcryptopp.a
        PATHS
            "${CMAKE_SOURCE_DIR}/extern/server/cryptopp"
            "${CMAKE_SOURCE_DIR}/extern/server/lib"
        NO_DEFAULT_PATH
    )
endif()

if(NOT CRYPTOPP_STATIC_LIB AND CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" AND EXISTS "${CMAKE_SOURCE_DIR}/extern/server/cryptopp/GNUmakefile")
    message(STATUS "[CryptoPP] libcryptopp.a bulunamadı, GCC 14 ile derleniyor...")
    execute_process(
        COMMAND gmake -C "${CMAKE_SOURCE_DIR}/extern/server/cryptopp" -j4 static CXX=${CMAKE_CXX_COMPILER} CC=${CMAKE_C_COMPILER}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/extern/server/cryptopp"
    )
    unset(CRYPTOPP_STATIC_LIB CACHE)
    find_file(CRYPTOPP_STATIC_LIB
        NAMES libcryptopp.a
        PATHS
            "${CMAKE_SOURCE_DIR}/extern/server/cryptopp"
            "${CMAKE_SOURCE_DIR}/extern/server/lib"
        NO_DEFAULT_PATH
    )
endif()

if(CRYPTOPP_STATIC_LIB)
    add_library(CryptoPP::CryptoPP UNKNOWN IMPORTED)
    set_target_properties(CryptoPP::CryptoPP PROPERTIES
        IMPORTED_LOCATION "${CRYPTOPP_STATIC_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${CRYPTOPP_INC_DIRS}"
    )
    set(CryptoPP_FOUND TRUE)
    message(STATUS "[CryptoPP] GCC ile derlenmiş yerel CryptoPP bulundu: ${CRYPTOPP_STATIC_LIB} (Pure libstdc++)")
elseif(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/extern/server/cryptopp/cryptlib.cpp")
    file(GLOB CRYPTOPP_WIN_SRCS "${CMAKE_SOURCE_DIR}/extern/server/cryptopp/*.cpp")
    list(FILTER CRYPTOPP_WIN_SRCS EXCLUDE REGEX ".*/(adhoc|test|bench[0-9]*|datatest|dlltest|fipsalgt|validat[0-9]*|regtest[0-9]*|pch|arm_.*|ppc_.*|neon_.*|sse_.*|.*_sse|.*_avx|.*_ppc|.*_simd)\\.cpp$")
    add_library(cryptopp_bundled STATIC ${CRYPTOPP_WIN_SRCS})
    target_include_directories(cryptopp_bundled PUBLIC "${CRYPTOPP_INC_DIRS}" "${CMAKE_SOURCE_DIR}/extern/server/cryptopp")
    target_compile_definitions(cryptopp_bundled PUBLIC CRYPTOPP_DISABLE_ASM NDEBUG)
    target_compile_options(cryptopp_bundled PRIVATE -O2 -Wall -Wextra -fexceptions)
    add_library(CryptoPP::CryptoPP ALIAS cryptopp_bundled)
    set(CryptoPP_FOUND TRUE)
    message(STATUS "[CryptoPP] Windows x64 kaynak tabanlı statik CryptoPP hedefi yapılandırıldı (cryptopp_bundled).")
endif()

if(NOT CryptoPP_FOUND)
    message(STATUS "[CryptoPP] Önceden derlenmiş CryptoPP arşivi bulunamadı (kaynak üzerinden derlenebilir).")
endif()
