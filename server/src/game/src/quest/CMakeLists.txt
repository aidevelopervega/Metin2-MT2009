# ==============================================================================
# Metin2 Quest Compiler (qc) CMakeLists.txt
# Compiles qc for both Lua 5.0 (default) and Lua 5.2 (migration test)
# ==============================================================================

set(QC_SOURCES
    qc.cc
    crc32.cc
)

# 1. qc (Default: Lua 5.0.3)
add_executable(qc ${QC_SOURCES})

target_compile_definitions(qc PRIVATE
    LUA_V=503
    ENABLE_LUA_5_VERSION=0
)

target_include_directories(qc PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/server/src/liblua/5.0/include"
    "${CMAKE_SOURCE_DIR}/server/src/liblua/5.0/src"
    "${CMAKE_SOURCE_DIR}/extern/common/include"
    "${CMAKE_SOURCE_DIR}/extern/common/include/boost"
)

target_link_libraries(qc PRIVATE
    lua_lib
)

if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_link_libraries(qc PRIVATE m)
endif()

# 2. qc_lua52 (Staged Migration: Lua 5.2.3)
add_executable(qc_lua52 ${QC_SOURCES})

target_compile_definitions(qc_lua52 PRIVATE
    LUA_V=523
    ENABLE_LUA_5_VERSION=2
)

target_include_directories(qc_lua52 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/server/src/liblua/5.2/src"
    "${CMAKE_SOURCE_DIR}/extern/common/include"
    "${CMAKE_SOURCE_DIR}/extern/common/include/boost"
)

target_link_libraries(qc_lua52 PRIVATE
    lua52_lib
)

if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_link_libraries(qc_lua52 PRIVATE m)
endif()

# 3. qc_lua55 (Modern Upstream: Lua 5.5.1)
add_executable(qc_lua55 ${QC_SOURCES})

target_compile_definitions(qc_lua55 PRIVATE
    LUA_V=551
    ENABLE_LUA_5_VERSION=5
)

target_include_directories(qc_lua55 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/server/src/liblua/5.5/src"
    "${CMAKE_SOURCE_DIR}/extern/common/include"
    "${CMAKE_SOURCE_DIR}/extern/common/include/boost"
)

target_link_libraries(qc_lua55 PRIVATE
    lua55_lib
)

if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_link_libraries(qc_lua55 PRIVATE m)
endif()
