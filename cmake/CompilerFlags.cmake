# ==============================================================================
# Metin2 MT2009 - Compiler and Platform Configuration
# Strictly GCC / MinGW-w64 x64 / FreeBSD amd64 GCC 14. MSVC is forbidden.
# ==============================================================================

if(MSVC)
    message(FATAL_ERROR "Visual Studio / MSVC kesinlikle kullanilamaz! AGENTS.md Madde 1.2 geregi MinGW-w64 GCC veya FreeBSD GCC kullanilmalidir.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Metin2 MT2009 sunucusu 64-bit (amd64 / x86_64) mimarisini hedeflemektedir! 32-bit derleme engellendi.")
endif()

# C++20 ve C11 Standartlari
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Temel Derleme Bayraklari
add_compile_options(
    -Wall
    -Wextra
    -pipe
    -Wno-deprecated-declarations
    -Wno-array-bounds
    -Wno-address
    -Wno-format-truncation
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-enum-enum-conversion>
    -Wno-nonnull-compare
    -Wno-int-in-bool-context
    -Wno-stringop-truncation
    -Wno-sign-compare
)

# FreeBSD Platform Ayarlari
if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    add_compile_definitions(_THREAD_SAFE)
    # GCC 14 libstdc++.so icin rpath ekle
    if(EXISTS "/usr/local/lib/gcc14")
        add_link_options("-Wl,-rpath=/usr/local/lib/gcc14")
    endif()
endif()

# Windows MinGW Platform Ayarlari
if(WIN32)
    add_compile_definitions(
        WIN32
        _WIN32
        _WIN64
        __WIN32__
    )
endif()
