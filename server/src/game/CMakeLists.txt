# ==============================================================================
# game Server CMakeLists.txt — Exclusive Lua 5.5.1 Engine
# ==============================================================================

file(GLOB GAME_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
if(NOT CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    list(FILTER GAME_SOURCES EXCLUDE REGEX ".*FileMonitor_FreeBSD\\.cpp$")
endif()
list(APPEND GAME_SOURCES "src/minilzo.c")

add_executable(game ${GAME_SOURCES})

target_include_directories(game PRIVATE
    ${DB_CLIENT_INCLUDE_DIRS}
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../common"
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${CMAKE_CURRENT_SOURCE_DIR}/../libthecore/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libsql"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libpoly"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libgame/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../liblua/5.5/src"
    "${CMAKE_SOURCE_DIR}/extern/common/include"
    "${CMAKE_SOURCE_DIR}/extern/server/include"
)

target_compile_definitions(game PRIVATE
    __USER__="metin2"
    __HOSTNAME__="mt2009"
    __PWD__=""
    __GAME_VERSION__="${MT2_SERVER_REVISION}"
    ENABLE_LUA_5_VERSION=5
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
    lua55_lib
    CryptoPP::CryptoPP
    Database::Client
)

set_target_properties(game PROPERTIES
    OUTPUT_NAME "game_r${MT2_SERVER_REVISION}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(WIN32)
    target_link_libraries(game PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
        ws2_32
        mswsock
        bcrypt
        crypt32
        advapi32
        user32
    )
    if(EXISTS "${CMAKE_SOURCE_DIR}/extern/client/dll/libssl-3-x64.dll" AND
       EXISTS "${CMAKE_SOURCE_DIR}/extern/client/dll/libcrypto-3-x64.dll")
        add_custom_command(TARGET game POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/extern/client/dll/libssl-3-x64.dll"
                "${CMAKE_CURRENT_SOURCE_DIR}/libssl-3-x64.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/extern/client/dll/libcrypto-3-x64.dll"
                "${CMAKE_CURRENT_SOURCE_DIR}/libcrypto-3-x64.dll"
            COMMENT "Copying OpenSSL 3.6.5 runtime DLLs to game server directory..."
        )
    endif()
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

