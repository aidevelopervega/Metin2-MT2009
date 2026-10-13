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
    endif()
endif()
