# ==============================================================================
# Metin2 MT2009 - System Libraries Discovery (ZLIB & OpenSSL)
# Searches standard system libraries without copying system files into Git
# ==============================================================================

if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    # FreeBSD Base System provides ZLIB and OpenSSL out-of-the-box
    find_package(ZLIB QUIET)
    if(ZLIB_FOUND)
        message(STATUS "[ZLIB] FreeBSD sistem zlib bulundu: ${ZLIB_LIBRARIES} (Sürüm: ${ZLIB_VERSION_STRING})")
    else()
        # Fallback to direct library
        find_library(ZLIB_LIBRARY NAMES z PATHS /usr/lib)
        if(ZLIB_LIBRARY)
            add_library(ZLIB::ZLIB UNKNOWN IMPORTED)
            set_target_properties(ZLIB::ZLIB PROPERTIES
                IMPORTED_LOCATION "${ZLIB_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "/usr/include"
            )
            message(STATUS "[ZLIB] FreeBSD /usr/lib/libz.so bağlandı.")
        endif()
    endif()

    find_package(OpenSSL QUIET)
    if(OpenSSL_FOUND)
        message(STATUS "[OpenSSL] FreeBSD sistem OpenSSL bulundu: ${OPENSSL_VERSION}")
    else()
        find_library(OPENSSL_SSL_LIB NAMES ssl PATHS /usr/lib)
        find_library(OPENSSL_CRYPTO_LIB NAMES crypto PATHS /usr/lib)
        if(OPENSSL_SSL_LIB AND OPENSSL_CRYPTO_LIB)
            add_library(OpenSSL::Crypto UNKNOWN IMPORTED)
            set_target_properties(OpenSSL::Crypto PROPERTIES
                IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIB}"
                INTERFACE_INCLUDE_DIRECTORIES "/usr/include"
            )
            add_library(OpenSSL::SSL UNKNOWN IMPORTED)
            set_target_properties(OpenSSL::SSL PROPERTIES
                IMPORTED_LOCATION "${OPENSSL_SSL_LIB}"
                INTERFACE_INCLUDE_DIRECTORIES "/usr/include"
                INTERFACE_LINK_LIBRARIES OpenSSL::Crypto
            )
            message(STATUS "[OpenSSL] FreeBSD /usr/lib libssl ve libcrypto bağlandı.")
        endif()
    endif()

elseif(WIN32)
    # Windows MinGW / UCRT
    find_package(ZLIB QUIET)
    if(ZLIB_FOUND)
        message(STATUS "[ZLIB] Windows zlib bulundu: ${ZLIB_LIBRARIES}")
    endif()

    find_package(OpenSSL QUIET)
    if(OpenSSL_FOUND)
        message(STATUS "[OpenSSL] Windows OpenSSL bulundu: ${OPENSSL_VERSION}")
    else()
        set(_WIN_OSSL_INC "${CMAKE_SOURCE_DIR}/extern/client/include")
        set(_WIN_OSSL_SSL_LIB "${CMAKE_SOURCE_DIR}/extern/platform/windows-x64/mingw-ucrt-gcc16/lib/libssl.dll.a")
        set(_WIN_OSSL_CRYPTO_LIB "${CMAKE_SOURCE_DIR}/extern/platform/windows-x64/mingw-ucrt-gcc16/lib/libcrypto.dll.a")
        if(EXISTS "${_WIN_OSSL_INC}/openssl/ssl.h" AND EXISTS "${_WIN_OSSL_SSL_LIB}" AND EXISTS "${_WIN_OSSL_CRYPTO_LIB}")
            if(NOT TARGET OpenSSL::Crypto)
                add_library(OpenSSL::Crypto UNKNOWN IMPORTED)
                set_target_properties(OpenSSL::Crypto PROPERTIES
                    IMPORTED_LOCATION "${_WIN_OSSL_CRYPTO_LIB}"
                    INTERFACE_INCLUDE_DIRECTORIES "${_WIN_OSSL_INC}"
                    INTERFACE_LINK_LIBRARIES "ws2_32;crypt32;advapi32;user32"
                )
            endif()
            if(NOT TARGET OpenSSL::SSL)
                add_library(OpenSSL::SSL UNKNOWN IMPORTED)
                set_target_properties(OpenSSL::SSL PROPERTIES
                    IMPORTED_LOCATION "${_WIN_OSSL_SSL_LIB}"
                    INTERFACE_INCLUDE_DIRECTORIES "${_WIN_OSSL_INC}"
                    INTERFACE_LINK_LIBRARIES "OpenSSL::Crypto;ws2_32"
                )
            endif()
            set(OpenSSL_FOUND TRUE)
            set(OPENSSL_FOUND TRUE)
            set(OPENSSL_INCLUDE_DIR "${_WIN_OSSL_INC}")
            set(OPENSSL_LIBRARIES OpenSSL::SSL OpenSSL::Crypto)
            message(STATUS "[OpenSSL] Windows UCRT64 OpenSSL 3.6.5 bağlandı: ${_WIN_OSSL_SSL_LIB}")
        endif()
    endif()
endif()

