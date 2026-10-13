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
