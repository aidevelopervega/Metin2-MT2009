# ==============================================================================
# liblua CMakeLists.txt — Exclusive Lua 5.5.1 Upstream Stable Engine
# ==============================================================================

set(LUA55_SOURCES
    5.5/src/lapi.c
    5.5/src/lauxlib.c
    5.5/src/lbaselib.c
    5.5/src/lcode.c
    5.5/src/lcorolib.c
    5.5/src/lctype.c
    5.5/src/ldblib.c
    5.5/src/ldebug.c
    5.5/src/ldo.c
    5.5/src/ldump.c
    5.5/src/lfunc.c
    5.5/src/lgc.c
    5.5/src/linit.c
    5.5/src/liolib.c
    5.5/src/llex.c
    5.5/src/lmathlib.c
    5.5/src/lmem.c
    5.5/src/loadlib.c
    5.5/src/lobject.c
    5.5/src/lopcodes.c
    5.5/src/loslib.c
    5.5/src/lparser.c
    5.5/src/lstate.c
    5.5/src/lstring.c
    5.5/src/lstrlib.c
    5.5/src/ltable.c
    5.5/src/ltablib.c
    5.5/src/ltm.c
    5.5/src/lundump.c
    5.5/src/lutf8lib.c
    5.5/src/lvm.c
    5.5/src/lzio.c
)

add_library(lua55_lib STATIC ${LUA55_SOURCES})

target_include_directories(lua55_lib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/5.5/src"
)
