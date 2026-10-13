# ==============================================================================
# game Server CMakeLists.txt
# ==============================================================================

file(GLOB GAME_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
list(APPEND GAME_SOURCES "src/minilzo.c")

add_executable(game ${GAME_SOURCES})

target_include_directories(game PRIVATE
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

target_compile_definitions(game PRIVATE
    __USER__="metin2"
    __HOSTNAME__="mt2009"
    __PWD__=""
    __GAME_VERSION__="${MT2_SERVER_REVISION}"
    ENABLE_LUA_5_VERSION=0
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
    lua_lib
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
        ssl
        crypto
        z
    )
endif()
