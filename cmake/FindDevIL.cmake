# ==============================================================================
# Metin2 MT2009 - DevIL (IL / ILU / ILUT) Discovery
# ==============================================================================

if(TARGET DevIL::IL)
    return()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    find_path(DEVIL_INCLUDE_DIR
        NAMES IL/il.h
        PATHS
            /usr/local/include
            "${CMAKE_SOURCE_DIR}/extern/server/include"
    )

    find_library(DEVIL_LIB
        NAMES IL
        PATHS
            "${CMAKE_SOURCE_DIR}/extern/server/lib"
            /usr/local/lib
    )

    if(DEVIL_LIB)
        add_library(DevIL::IL UNKNOWN IMPORTED)
        set_target_properties(DevIL::IL PROPERTIES
            IMPORTED_LOCATION "${DEVIL_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${DEVIL_INCLUDE_DIR}"
        )
        set(DevIL_FOUND TRUE)
        message(STATUS "[DevIL] FreeBSD IL kütüphanesi bulundu: ${DEVIL_LIB}")
    endif()
elseif(WIN32)
    set(DEVIL_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/extern/server/include")
    find_file(DEVIL_LIB
        NAMES libIL.a
        PATHS
            "${CMAKE_SOURCE_DIR}/extern/platform/windows-x64/mingw-ucrt-gcc16/lib"
        NO_DEFAULT_PATH
    )
    if(DEVIL_LIB)
        add_library(DevIL::IL UNKNOWN IMPORTED)
        set_target_properties(DevIL::IL PROPERTIES
            IMPORTED_LOCATION "${DEVIL_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${DEVIL_INCLUDE_DIR}"
            INTERFACE_COMPILE_DEFINITIONS "IL_STATIC_LIB"
        )
        set(DevIL_FOUND TRUE)
        message(STATUS "[DevIL] Windows x64 statik IL kütüphanesi bulundu: ${DEVIL_LIB}")
    else()
        message(WARNING "[DevIL] Windows x64 libIL.a bulunamadı!")
    endif()
endif()
