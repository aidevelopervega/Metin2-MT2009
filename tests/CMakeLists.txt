# ==============================================================================
# tests CMakeLists.txt - CTest Configuration
# ==============================================================================

# Ortak test include dizinleri
set(TEST_COMMON_INCLUDES
    ${DB_CLIENT_INCLUDE_DIRS}
    "${CMAKE_SOURCE_DIR}/server/src/common"
    "${CMAKE_SOURCE_DIR}/server/src/libthecore/include"
    "${CMAKE_SOURCE_DIR}/server/src/libthecore/src"
    "${CMAKE_SOURCE_DIR}/server/src/libpoly"
    "${CMAKE_SOURCE_DIR}/server/src/libgame/include"
    "${CMAKE_SOURCE_DIR}/server/src/libsql"
    "${CMAKE_SOURCE_DIR}/extern/common/include"
    "${CMAKE_SOURCE_DIR}/extern/server/include"
)

# 1. test_libpoly
add_executable(test_libpoly test_libpoly.cpp)
target_include_directories(test_libpoly PRIVATE ${TEST_COMMON_INCLUDES})
target_link_libraries(test_libpoly PRIVATE poly thecore)
add_test(NAME test_libpoly COMMAND test_libpoly)

# 2. test_libthecore
add_executable(test_libthecore test_libthecore.cpp)
target_include_directories(test_libthecore PRIVATE ${TEST_COMMON_INCLUDES})
target_link_libraries(test_libthecore PRIVATE thecore)
add_test(NAME test_libthecore COMMAND test_libthecore)

# 3. test_libgame
add_executable(test_libgame test_libgame.cpp)
target_include_directories(test_libgame PRIVATE ${TEST_COMMON_INCLUDES})
target_link_libraries(test_libgame PRIVATE game_lib thecore)
add_test(NAME test_libgame COMMAND test_libgame)

# 4. test_libsql
add_executable(test_libsql test_libsql.cpp)
target_include_directories(test_libsql PRIVATE ${TEST_COMMON_INCLUDES})
target_link_libraries(test_libsql PRIVATE sql_lib thecore Database::Client)
add_test(NAME test_libsql COMMAND test_libsql)

# 5. test_db_cache
add_executable(test_db_cache test_db_cache.cpp)
target_include_directories(test_db_cache PRIVATE ${TEST_COMMON_INCLUDES})
target_link_libraries(test_db_cache PRIVATE thecore sql_lib Database::Client)
add_test(NAME test_db_cache COMMAND test_db_cache)

# 6. test_db_cache_integration
add_executable(test_db_cache_integration test_db_cache_integration.cpp)
target_include_directories(test_db_cache_integration PRIVATE
    ${DB_CLIENT_INCLUDE_DIRS}
    "${CMAKE_SOURCE_DIR}/server/src/db/src"
    "${CMAKE_SOURCE_DIR}/server/src/libthecore/include"
    "${CMAKE_SOURCE_DIR}/server/src/libsql"
    "${CMAKE_SOURCE_DIR}/server/src/libpoly"
    "${CMAKE_SOURCE_DIR}/server/src/libgame/include"
    "${CMAKE_SOURCE_DIR}/server/src/libgame/src"
    "${CMAKE_SOURCE_DIR}/server/src/common"
    "${CMAKE_SOURCE_DIR}/extern/common/include"
    "${CMAKE_SOURCE_DIR}/extern/common/include/boost"
    "${CMAKE_SOURCE_DIR}/extern/server/include"
)
target_compile_options(test_db_cache_integration PRIVATE
    -fno-rtti
    -fno-exceptions
)
target_link_libraries(test_db_cache_integration PRIVATE
    db_core
    thecore
    sql_lib
    poly
    game_lib
    Database::Client
)
if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_link_libraries(test_db_cache_integration PRIVATE
        ssl
        crypto
        pthread
        m
        z
    )
endif()
add_test(NAME test_db_cache_integration COMMAND test_db_cache_integration)

# 7. test_packet_wire
add_executable(test_packet_wire test_packet_wire.cpp)
target_include_directories(test_packet_wire PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/server/src/game/src"
    "${CMAKE_SOURCE_DIR}/server/src/common"
    "${CMAKE_SOURCE_DIR}/server/src/libthecore/include"
    "${CMAKE_SOURCE_DIR}/extern/common/include"
    "${CMAKE_SOURCE_DIR}/extern/server/include"
)
add_test(NAME test_packet_wire COMMAND test_packet_wire)

# 8. test_lua55_quest (Lua 5.5.1 Exclusive Production Engine)
add_executable(test_lua55_quest test_lua55_quest.cpp)
target_include_directories(test_lua55_quest PRIVATE
    "${CMAKE_SOURCE_DIR}/server/src/liblua/5.5/src"
)
target_link_libraries(test_lua55_quest PRIVATE lua55_lib)
if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_link_libraries(test_lua55_quest PRIVATE m)
endif()
add_test(NAME test_lua55_quest COMMAND test_lua55_quest)

# 9. test_qc_compiler_lua55 (Real Metin2 quest compilation with Lua 5.5.1 qc)
add_test(NAME test_qc_compiler_lua55 COMMAND qc "${CMAKE_SOURCE_DIR}/tests/fixtures/sample_biologist.quest")

# 10. test_auth_security (Argon2id PHC, Legacy *SHA1 migration, Rate Limiter, CSPRNG LoginKey)
add_executable(test_auth_security test_auth_security.cpp)
target_include_directories(test_auth_security PRIVATE ${TEST_COMMON_INCLUDES})
target_link_libraries(test_auth_security PRIVATE thecore)
add_test(NAME test_auth_security COMMAND test_auth_security)

if(WIN32)
    # Windows uzerinde libmariadb.dll dosyasini test dizinine kopyala
    add_custom_target(copy_test_dlls ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/scratch/isolated_db/mariadb-11.4.13-winx64/lib/libmariadb.dll"
            "${CMAKE_CURRENT_BINARY_DIR}/libmariadb.dll"
        COMMENT "Copying runtime test DLLs..."
    )
    add_dependencies(test_db_cache_integration copy_test_dlls)
    add_dependencies(test_libsql copy_test_dlls)
endif()
