# ==============================================================================
# Metin2 Quest Compiler (qc) CMakeLists.txt
# Compiles qc with Lua 5.5.1 by default (rollback via ENABLE_LUA_5_VERSION=0 / qc_lua50)
# ==============================================================================

set(QC_SOURCES
    qc.cc
    crc32.cc
)

if(NOT DEFINED ENABLE_LUA_5_VERSION)
    set(ENABLE_LUA_5_VERSION 5)
endif()

# 1. qc (Default: Lua 5.5.1, or rollback via -DENABLE_LUA_5_VERSION=0)
add_executable(qc ${QC_SOURCES})

if(ENABLE_LUA_5_VERSION EQUAL 5)
    target_compile_definitions(qc PRIVATE
        LUA_V=551
        ENABLE_LUA_5_VERSION=5
    )
    target_include_directories(qc PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/server/src/liblua/5.5/src"
        "${CMAKE_SOURCE_DIR}/extern/common/include"
        "${CMAKE_SOURCE_DIR}/extern/common/include/boost"
    )
    target_link_libraries(qc PRIVATE lua55_lib)
elseif(ENABLE_LUA_5_VERSION EQUAL 2)
    target_compile_definitions(qc PRIVATE
        LUA_V=523
        ENABLE_LUA_5_VERSION=2
    )
    target_include_directories(qc PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/server/src/liblua/5.2/src"
        "${CMAKE_SOURCE_DIR}/extern/common/include"
        "${CMAKE_SOURCE_DIR}/extern/common/include/boost"
    )
    target_link_libraries(qc PRIVATE lua52_lib)
else()
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
    target_link_libraries(qc PRIVATE lua_lib)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_link_libraries(qc PRIVATE m)
endif()

# 2. qc_lua50 (Explicit Rollback: Lua 5.0.3)
add_executable(qc_lua50 ${QC_SOURCES})

target_compile_definitions(qc_lua50 PRIVATE
    LUA_V=503
    ENABLE_LUA_5_VERSION=0
)

target_include_directories(qc_lua50 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/server/src/liblua/5.0/include"
    "${CMAKE_SOURCE_DIR}/server/src/liblua/5.0/src"
    "${CMAKE_SOURCE_DIR}/extern/common/include"
    "${CMAKE_SOURCE_DIR}/extern/common/include/boost"
)

target_link_libraries(qc_lua50 PRIVATE
    lua_lib
)

if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_link_libraries(qc_lua50 PRIVATE m)
endif()

# 3. qc_lua52 (Staged Migration: Lua 5.2.3)
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

# 4. qc_lua55 (Explicit Modern Upstream: Lua 5.5.1)
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
