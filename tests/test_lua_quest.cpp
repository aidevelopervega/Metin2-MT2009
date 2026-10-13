// ==============================================================================
// METIN2 MT2009 - LUA 5.0 QUEST ENGINE AUTOMATED REGRESSION SUITE
// Tests: Lua 5.0.3 C API, Coroutine Yield/Resume, Metin2 C functions,
//        __codecache 64-bit pointer safety with lightuserdata, and Scripting
// ==============================================================================

#include <iostream>
#include <cassert>
#include <cstring>
#include <cstdint>
#include <string>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

static int g_gold = 5000;
static const char* g_name = "WarriorAlpha";

// Metin2 style C API callbacks
static int quest_pc_get_name(lua_State* L)
{
    lua_pushstring(L, g_name);
    return 1;
}

static int quest_pc_get_gold(lua_State* L)
{
    lua_pushnumber(L, g_gold);
    return 1;
}

static int quest_pc_give_gold(lua_State* L)
{
    if (lua_isnumber(L, 1))
    {
        int amount = (int)lua_tonumber(L, 1);
        g_gold += amount;
    }
    return 0;
}

int main()
{
    std::cout << "==================================================" << std::endl;
    std::cout << " METIN2 MT2009 - LUA 5.0 QUEST REGRESSION SUITE   " << std::endl;
    std::cout << " Engine: Lua 5.0.3 (Metin2 Modified Lexer/Tokens) " << std::endl;
    std::cout << "==================================================" << std::endl;

    // 1. Engine Initialization
    std::cout << "[RUN] Test 1: Lua 5.0 engine initialization and libraries..." << std::endl;
    lua_State* L = lua_open();
    assert(L != nullptr && "Failed to create Lua 5.0 state");

    luaopen_base(L);
    luaopen_table(L);
    luaopen_string(L);
    luaopen_math(L);

    // Verify Lua Version
    lua_getglobal(L, "_VERSION");
    assert(!lua_isnil(L, -1));
    std::string ver = lua_tostring(L, -1);
    lua_pop(L, 1);
    std::cout << "  --> Lua version string: " << ver << std::endl;
    assert(ver.find("5.0") != std::string::npos);
    std::cout << "  [PASS] Test 1: Initialization verified." << std::endl;

    // 2. Metin2 Quest C API Registration (Simulating quest::RegisterPCFunctionTable)
    std::cout << "[RUN] Test 2: Metin2 Quest C API registration and execution..." << std::endl;
    lua_newtable(L);
    lua_pushstring(L, "get_name");
    lua_pushcfunction(L, quest_pc_get_name);
    lua_settable(L, -3);

    lua_pushstring(L, "get_gold");
    lua_pushcfunction(L, quest_pc_get_gold);
    lua_settable(L, -3);

    lua_pushstring(L, "give_gold");
    lua_pushcfunction(L, quest_pc_give_gold);
    lua_settable(L, -3);

    lua_setglobal(L, "pc");

    // Execute script calling Metin2 C functions
    const char* script = 
        "name = pc.get_name()\n"
        "initial_gold = pc.get_gold()\n"
        "pc.give_gold(1500)\n"
        "final_gold = pc.get_gold()\n";

    int err = lua_dobuffer(L, script, strlen(script), "test_pc_quest");
    assert(err == 0 && "Script execution failed");

    lua_getglobal(L, "name");
    assert(strcmp(lua_tostring(L, -1), "WarriorAlpha") == 0);
    lua_pop(L, 1);

    lua_getglobal(L, "initial_gold");
    assert(lua_tonumber(L, -1) == 5000);
    lua_pop(L, 1);

    lua_getglobal(L, "final_gold");
    assert(lua_tonumber(L, -1) == 6500);
    assert(g_gold == 6500);
    lua_pop(L, 1);
    std::cout << "  [PASS] Test 2: C API function table invocation passed." << std::endl;

    // 3. Coroutine Lifecycle & Yield/Resume (Quest dialog simulation)
    std::cout << "[RUN] Test 3: Coroutine state machine, yield, resume & registry ref..." << std::endl;
    const char* coroutine_script = 
        "function quest_dialog()\n"
        "    coroutine.yield('Choose weapon: 1=Sword, 2=Bow')\n"
        "    return 'You selected option ' .. tostring(choice)\n"
        "end\n";

    err = lua_dobuffer(L, coroutine_script, strlen(coroutine_script), "dialog_script");
    assert(err == 0);

    lua_State* co = lua_newthread(L);
    assert(co != nullptr);

    // Anchor coroutine thread in registry (lua_ref in Metin2)
    int ref_id = luaL_ref(L, LUA_REGISTRYINDEX);
    assert(ref_id > 0);

    // Start coroutine
    lua_getglobal(co, "quest_dialog");
    int resume_res = lua_resume(co, 0);
    // In Lua 5.0, lua_resume returns 0 on yield (LUA_YIELD was introduced in Lua 5.1+)
    assert(resume_res == 0);
    assert(lua_gettop(co) == 1);
    std::string yield_msg = lua_tostring(co, 1);
    assert(yield_msg == "Choose weapon: 1=Sword, 2=Bow");
    lua_pop(co, 1);

    // Player inputs answer 2
    lua_pushnumber(co, 2);
    lua_setglobal(co, "choice");

    // Resume coroutine
    resume_res = lua_resume(co, 0);
    assert(resume_res == 0); // finished
    assert(lua_gettop(co) == 1);
    std::string final_msg = lua_tostring(co, 1);
    assert(final_msg == "You selected option 2");
    lua_pop(co, 1);

    // Clean up registry reference
    luaL_unref(L, LUA_REGISTRYINDEX, ref_id);
    std::cout << "  [PASS] Test 3: Coroutine yield/resume dialog sequence passed." << std::endl;

    // 4. __codecache 64-Bit Pointer Safety: Lightuserdata vs Number Float Precision
    std::cout << "[RUN] Test 4: __codecache pointer hashing (Float precision loss vs lightuserdata)..." << std::endl;
    // Pointers above 2^53 where double-precision float loses the least significant bits:
    uintptr_t addrA = (1ULL << 54) + 1; // 18014398509481985
    uintptr_t addrB = (1ULL << 54) + 2; // 18014398509481986
    void* ptrA = (void*)addrA;
    void* ptrB = (void*)addrB;

    // Mathematical demonstration of float collision
    double dblA = (double)addrA;
    double dblB = (double)addrB;
    assert(dblA == dblB && "Addresses must be above 2^53 where IEEE-754 double loses precision");
    std::cout << "  --> Proven: (double)addrA == (double)addrB (FLOAT COLLISION CONFIRMED)" << std::endl;

    // Create global __codecache table in Lua
    lua_newtable(L);
    lua_setglobal(L, "__codecache");

    lua_getglobal(L, "__codecache");
    int cache_idx = lua_gettop(L);

    // Insert ptrA -> "compiled_quest_A" via lua_pushlightuserdata
    lua_pushlightuserdata(L, ptrA);
    lua_pushstring(L, "compiled_quest_A");
    lua_settable(L, cache_idx);

    // Insert ptrB -> "compiled_quest_B" via lua_pushlightuserdata
    lua_pushlightuserdata(L, ptrB);
    lua_pushstring(L, "compiled_quest_B");
    lua_settable(L, cache_idx);

    // Retrieve ptrA
    lua_pushlightuserdata(L, ptrA);
    lua_gettable(L, cache_idx);
    assert(!lua_isnil(L, -1));
    assert(strcmp(lua_tostring(L, -1), "compiled_quest_A") == 0);
    lua_pop(L, 1);

    // Retrieve ptrB
    lua_pushlightuserdata(L, ptrB);
    lua_gettable(L, cache_idx);
    assert(!lua_isnil(L, -1));
    assert(strcmp(lua_tostring(L, -1), "compiled_quest_B") == 0);
    lua_pop(L, 1);

    lua_pop(L, 1); // pop __codecache
    std::cout << "  --> Proven: lua_pushlightuserdata preserves exact 64-bit pointer separation!" << std::endl;
    std::cout << "  [PASS] Test 4: 64-bit pointer hashing verified with zero collision." << std::endl;

    // 5. Sandboxed Protected Call (lua_pcall) and Error Handling
    std::cout << "[RUN] Test 5: Error handling and protected mode..." << std::endl;
    const char* bad_script = "non_existent_function()";
    err = lua_dobuffer(L, bad_script, strlen(bad_script), "bad_script");
    assert(err != 0 && "Bad script must return non-zero error code");
    std::cout << "  [PASS] Test 5: Error handling safely caught runtime error." << std::endl;

    lua_close(L);
    std::cout << "==================================================" << std::endl;
    std::cout << " [SUCCESS] ALL 5 LUA 5.0 QUEST TESTS PASSED!     " << std::endl;
    std::cout << "==================================================" << std::endl;
    return 0;
}
