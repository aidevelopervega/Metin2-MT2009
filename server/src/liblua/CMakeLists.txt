# ==============================================================================
# liblua CMakeLists.txt
# ==============================================================================

set(LUA_SOURCES
    5.0/src/lapi.c
    5.0/src/lcode.c
    5.0/src/ldebug.c
    5.0/src/ldo.c
    5.0/src/ldump.c
    5.0/src/lfunc.c
    5.0/src/lgc.c
    5.0/src/llex.c
    5.0/src/lmem.c
    5.0/src/lobject.c
    5.0/src/lopcodes.c
    5.0/src/lparser.c
    5.0/src/lstate.c
    5.0/src/lstring.c
    5.0/src/ltable.c
    5.0/src/ltm.c
    5.0/src/lundump.c
    5.0/src/lvm.c
    5.0/src/lzio.c
    5.0/src/lib/lauxlib.c
    5.0/src/lib/lbaselib.c
    5.0/src/lib/ldblib.c
    5.0/src/lib/liolib.c
    5.0/src/lib/lmathlib.c
    5.0/src/lib/ltablib.c
    5.0/src/lib/lstrlib.c
    5.0/src/lib/loadlib.c
)

add_library(lua_lib STATIC ${LUA_SOURCES})

target_include_directories(lua_lib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/5.0/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/5.0/src"
)

# Lua 5.2.3 Modern Engine (Metin2 Custom Lexer/Syntax Extensions)
set(LUA52_SOURCES
    5.2/src/lapi.c
    5.2/src/lauxlib.c
    5.2/src/lbaselib.c
    5.2/src/lbitlib.c
    5.2/src/lcode.c
    5.2/src/lcorolib.c
    5.2/src/lctype.c
    5.2/src/ldblib.c
    5.2/src/ldebug.c
    5.2/src/ldo.c
    5.2/src/ldump.c
    5.2/src/lfunc.c
    5.2/src/lgc.c
    5.2/src/linit.c
    5.2/src/liolib.c
    5.2/src/llex.c
    5.2/src/lmathlib.c
    5.2/src/lmem.c
    5.2/src/loadlib.c
    5.2/src/lobject.c
    5.2/src/lopcodes.c
    5.2/src/loslib.c
    5.2/src/lparser.c
    5.2/src/lstate.c
    5.2/src/lstring.c
    5.2/src/lstrlib.c
    5.2/src/ltable.c
    5.2/src/ltablib.c
    5.2/src/ltm.c
    5.2/src/lundump.c
    5.2/src/lvm.c
    5.2/src/lzio.c
)

add_library(lua52_lib STATIC ${LUA52_SOURCES})

target_include_directories(lua52_lib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/5.2/src"
)

