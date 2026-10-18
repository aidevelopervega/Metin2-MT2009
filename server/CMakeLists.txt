# ==============================================================================
# server CMakeLists.txt
# ==============================================================================

# Server Revizyon Numarasi Okuma
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/__REVISION__")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/__REVISION__" MT2_SERVER_REVISION)
    string(STRIP "${MT2_SERVER_REVISION}" MT2_SERVER_REVISION)
else()
    set(MT2_SERVER_REVISION "1")
endif()
message(STATUS "[Server] Revizyon: ${MT2_SERVER_REVISION}")

add_subdirectory(src/libthecore)
add_subdirectory(src/libpoly)
add_subdirectory(src/libgame)
add_subdirectory(src/libsql)
add_subdirectory(src/liblua)
add_subdirectory(src/db)
add_subdirectory(src/game/src/quest)
add_subdirectory(src/game)
