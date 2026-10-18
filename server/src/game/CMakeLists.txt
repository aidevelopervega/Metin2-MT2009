# ==============================================================================
# game Server CMakeLists.txt
# ==============================================================================

file(GLOB GAME_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
if(NOT CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    list(FILTER GAME_SOURCES EXCLUDE REGEX ".*FileMonitor_FreeBSD\\.cpp$")
endif()
list(APPEND GAME_SOURCES "src/minilzo.c")

add_executable(game ${GAME_SOURCES})

set(ENABLE_LUA_5_VERSION 5 CACHE STRING "Lua version for game server (5=5.5.1 default, 0=5.0.3 rollback, 2=5.2.3)")

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
    ${DB_CLIENT_INCLUDE_DIRS}
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
    DevIL::IL
)

set_target_properties(game PROPERTIES
    OUTPUT_NAME "game_r${MT2_SERVER_REVISION}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(WIN32)
    target_link_libraries(game PRIVATE ws2_32 bcrypt)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_link_libraries(game PRIVATE
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

if(ENABLE_LUA_5_VERSION EQUAL 5)
    add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:game>"
            "${CMAKE_CURRENT_SOURCE_DIR}/game_r${MT2_SERVER_REVISION}_lua55${CMAKE_EXECUTABLE_SUFFIX}"
        COMMENT "Syncing default Lua 5.5.1 game_r${MT2_SERVER_REVISION} to game_r${MT2_SERVER_REVISION}_lua55..."
    )
endif()

# ==============================================================================
# game_lua50: Explicit Rollback Target with Legacy Lua 5.0.3 Engine
# ==============================================================================
add_executable(game_lua50 ${GAME_SOURCES})

target_include_directories(game_lua50 PRIVATE
    ${DB_CLIENT_INCLUDE_DIRS}
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../common"
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${CMAKE_CURRENT_SOURCE_DIR}/../libthecore/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libsql"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libpoly"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libgame/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../liblua/5.0/include"
    "${CMAKE_SOURCE_DIR}/extern/common/include"
    "${CMAKE_SOURCE_DIR}/extern/common/include/boost"
    "${CMAKE_SOURCE_DIR}/extern/server/include"
)

target_compile_definitions(game_lua50 PRIVATE
    __USER__="metin2"
    __HOSTNAME__="mt2009"
    __PWD__=""
    __GAME_VERSION__="${MT2_SERVER_REVISION}"
    ENABLE_LUA_5_VERSION=0
)

target_compile_options(game_lua50 PRIVATE
    -fexceptions
)

target_link_libraries(game_lua50 PRIVATE
    thecore
    sql_lib
    poly
    game_lib
    lua_lib
    CryptoPP::CryptoPP
    Database::Client
    DevIL::IL
)

set_target_properties(game_lua50 PROPERTIES
    OUTPUT_NAME "game_r${MT2_SERVER_REVISION}_lua50"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(WIN32)
    target_link_libraries(game_lua50 PRIVATE ws2_32 bcrypt)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_link_libraries(game_lua50 PRIVATE
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
