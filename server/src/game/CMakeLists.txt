# ==============================================================================
# game Server CMakeLists.txt
# ==============================================================================

file(GLOB GAME_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
list(APPEND GAME_SOURCES "src/minilzo.c")

add_executable(game ${GAME_SOURCES})

set(ENABLE_LUA_5_VERSION 0 CACHE STRING "Lua version for game server (0=5.0, 2=5.2, 5=5.5)")

if(ENABLE_LUA_5_VERSION EQUAL 5)
    set(GAME_LUA_INC "${CMAKE_CURRENT_SOURCE_DIR}/../liblua/5.5/src")
    set(GAME_LUA_LIB lua55_lib)
    set(GAME_LUA_DEF ENABLE_LUA_5_VERSION=5)
elseif(ENABLE_LUA_5_VERSION EQUAL 2)
    set(GAME_LUA_INC "${CMAKE_CURRENT_SOURCE_DIR}/../liblua/5.2/src")
    set(GAME_LUA_LIB lua52_lib)
    set(GAME_LUA_DEF ENABLE_LUA_5_VERSION=2)
else()
    set(GAME_LUA_INC "${CMAKE_CURRENT_SOURCE_DIR}/../liblua/5.0/include")
    set(GAME_LUA_LIB lua_lib)
    set(GAME_LUA_DEF ENABLE_LUA_5_VERSION=0)
endif()

target_include_directories(game PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../common"
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${CMAKE_CURRENT_SOURCE_DIR}/../libthecore/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libsql"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libpoly"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libgame/include"
    "${GAME_LUA_INC}"
    "${CMAKE_SOURCE_DIR}/extern/common/include"
    "${CMAKE_SOURCE_DIR}/extern/common/include/boost"
    "${CMAKE_SOURCE_DIR}/extern/server/include"
)

target_compile_definitions(game PRIVATE
    __USER__="metin2"
    __HOSTNAME__="mt2009"
    __PWD__=""
    __GAME_VERSION__="${MT2_SERVER_REVISION}"
    ${GAME_LUA_DEF}
)

# game sunucusunda RTTI ve Exception aciktir (CryptoPP ve CAsyncSQL2)
target_compile_options(game PRIVATE
    -fexceptions
)

target_link_libraries(game PRIVATE
    thecore
    sql_lib
    poly
    game_lib
    ${GAME_LUA_LIB}
    CryptoPP::CryptoPP
    Database::Client
)

set_target_properties(game PROPERTIES
    OUTPUT_NAME "game_r${MT2_SERVER_REVISION}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(WIN32)
    target_link_libraries(game PRIVATE ws2_32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_link_libraries(game PRIVATE
        DevIL::IL
        pthread
        m
        md
        $<$<TARGET_EXISTS:OpenSSL::SSL>:OpenSSL::SSL>
        $<$<NOT:$<TARGET_EXISTS:OpenSSL::SSL>>:ssl>
        $<$<TARGET_EXISTS:OpenSSL::Crypto>:OpenSSL::Crypto>
        $<$<NOT:$<TARGET_EXISTS:OpenSSL::Crypto>>:crypto>
        $<$<TARGET_EXISTS:ZLIB::ZLIB>:ZLIB::ZLIB>
        $<$<NOT:$<TARGET_EXISTS:ZLIB::ZLIB>>:z>
    )
endif()
