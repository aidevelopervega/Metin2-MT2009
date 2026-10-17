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
    src/auth_security.cpp
)

set(THECORE_ARGON2_C_SOURCES
    src/argon2/argon2.c
    src/argon2/core.c
    src/argon2/encoding.c
    src/argon2/opt.c
    src/argon2/thread.c
    src/argon2/blake2/blake2b.c
)

add_library(thecore STATIC ${THECORE_SOURCES} ${THECORE_ARGON2_C_SOURCES})

set_source_files_properties(${THECORE_SOURCES} PROPERTIES LANGUAGE CXX)
set_source_files_properties(${THECORE_ARGON2_C_SOURCES} PROPERTIES LANGUAGE C)

target_include_directories(thecore PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../common"
    "${CMAKE_SOURCE_DIR}/extern/common/include"
    "${CMAKE_SOURCE_DIR}/extern/server/include"
)

target_compile_options(thecore PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
)

if(WIN32)
    target_link_libraries(thecore PUBLIC ws2_32 bcrypt)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_link_libraries(thecore PUBLIC pthread)
endif()
