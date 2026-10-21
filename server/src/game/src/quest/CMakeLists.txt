# ==============================================================================
# Metin2 Quest Compiler (qc) CMakeLists.txt — Exclusive Lua 5.5.1 Engine
# ==============================================================================

set(QC_SOURCES
    qc.cc
    crc32.cc
)

add_executable(qc ${QC_SOURCES})

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

if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_link_libraries(qc PRIVATE m)
endif()
