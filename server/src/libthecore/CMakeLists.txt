# ==============================================================================
# libthecore CMakeLists.txt
# ==============================================================================

set(THECORE_SOURCES
    src/DES_table.c
    src/buffer.c
    src/des.c
    src/fdwatch.c
    src/gost.c
    src/hangul.c
    src/heart.c
    src/kstbl.c
    src/log.c
    src/main.c
    src/memcpy.c
    src/signal.c
    src/socket.c
    src/tea.c
    src/utils.c
    src/xdirent.c
    src/xgetopt.c
    src/xmd5.c
)

add_library(thecore STATIC ${THECORE_SOURCES})

set_source_files_properties(${THECORE_SOURCES} PROPERTIES LANGUAGE CXX)

target_include_directories(thecore PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../common"
    "${CMAKE_SOURCE_DIR}/extern/common/include"
    "${CMAKE_SOURCE_DIR}/extern/server/include"
)

target_compile_options(thecore PRIVATE
    -fno-exceptions
    -fno-rtti
)

if(WIN32)
    target_link_libraries(thecore PUBLIC ws2_32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_link_libraries(thecore PUBLIC pthread)
endif()
