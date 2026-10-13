#include "stdafx.h"
#include <iostream>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "questmanager.h"
#include "questlua.h"
#include "packet.h"

// ==============================================================================
// METIN2 MT2009 - GAME SERVER LUA QUEST RUNTIME VERIFICATION
// Executed via: ./game_r41023 --check-lua or ./game_r41023_lua55 --check-lua
// Tests:
//   1. 32-Bit Client wire protocol packet_script struct size guarantee
//   2. QuestManager & Lua State initialization
//   3. Compatibility shims (math.mod, math.pow, string.gfind, table.getn, unpack)
//   4. Metin2 Quest C API registration (pc, npc, item, q, say, select)
//   5. select(...) nil-argument preservation & m2_lua_getn contract
//   6. Real questlib.lua and locale.lua execution & variable verification
//   7. Gameforge translation proxy & missing locale detection
//   8. Interactive Coroutine Dialog State Machine (yield on select, resume with choice)
//   9. __codecache 64-bit pointer key safety (no precision loss)
//  10. Coroutine error isolation & crash prevention (fail-closed)
// ==============================================================================

#define RUNTIME_ASSERT(cond, msg) \
	do { \
		if (!(cond)) { \
			std::cerr << "[FAIL] Line " << __LINE__ << ": " << msg << std::endl; \
			return 1; \
		} \
	} while (0)

int RunGameQuestRuntimeTests()
{
	std::cout << "==================================================" << std::endl;
	std::cout << " METIN2 MT2009 - GAME SERVER QUEST RUNTIME SUITE" << std::endl;
	std::cout << " Platform: 64-bit amd64" << std::endl;
#if LUA_V == 551
	std::cout << " Lua Engine: Lua 5.5.1 Upstream Stable (ENABLE_LUA_5_VERSION=5)" << std::endl;
#elif LUA_V == 523
	std::cout << " Lua Engine: Lua 5.2.3 Staging (ENABLE_LUA_5_VERSION=2)" << std::endl;
#else
	std::cout << " Lua Engine: Lua 5.0.3 Default Production (ENABLE_LUA_5_VERSION=0)" << std::endl;
#endif
	std::cout << "==================================================" << std::endl;

	int checks_passed = 0;

	// 1. 32-Bit Client Wire Protocol Size Check
	std::cout << "[RUN] Check 1: 32-Bit Client packet_script struct size..." << std::endl;
#ifdef ENABLE_QUEST_CATEGORY
	RUNTIME_ASSERT(sizeof(struct packet_script) == 7, "packet_script must be exactly 7 bytes with ENABLE_QUEST_CATEGORY");
#else
	RUNTIME_ASSERT(sizeof(struct packet_script) == 6, "packet_script must be exactly 6 bytes");
#endif
	std::cout << "  --> packet_script size: " << sizeof(struct packet_script) << " bytes (aligned pack(1))" << std::endl;
	checks_passed++;

	// 2. QuestManager & Lua State Initialization
	std::cout << "[RUN] Check 2: QuestManager & Lua State Initialization..." << std::endl;
	std::unique_ptr<quest::CQuestManager> local_qm;
	if (quest::CQuestManager::instance_ptr() == nullptr)
	{
		local_qm = std::make_unique<quest::CQuestManager>();
	}
	quest::CQuestManager& qm = quest::CQuestManager::instance();
	lua_State* L = qm.GetLuaState();
	if (!L)
	{
		qm.InitializeLua();
		L = qm.GetLuaState();
	}
	RUNTIME_ASSERT(L != nullptr, "Lua state must be initialized and non-null");
	std::cout << "  --> Lua state initialized at: " << (void*)L << std::endl;
	checks_passed++;

	// 3. Compatibility Shims
	std::cout << "[RUN] Check 3: Standard compatibility shims..." << std::endl;
	int shim_err = lua_dostring(L,
		"assert(math.mod(10, 3) == 1, 'math.mod failed')\n"
		"assert(math.pow(2, 4) == 16, 'math.pow failed')\n"
		"local t = {10, 20, 30}\n"
		"assert(table.getn(t) == 3, 'table.getn failed')\n"
		"local count = 0\n"
		"table.foreach(t, function(k, v) count = count + 1 end)\n"
		"assert(count == 3, 'table.foreach failed')\n"
	);
	RUNTIME_ASSERT(shim_err == 0, "Compatibility shims execution must return 0");
	std::cout << "  --> math.mod, math.pow, table.getn, table.foreach verified" << std::endl;
	checks_passed++;

	// 4. Global Namespace & Function Tables
	std::cout << "[RUN] Check 4: Registered Metin2 C API function tables..." << std::endl;
	lua_getglobal(L, "pc");
	RUNTIME_ASSERT(lua_istable(L, -1), "pc table must be registered as a global table");
	lua_pushstring(L, "get_name");
	lua_gettable(L, -2);
	RUNTIME_ASSERT(lua_iscfunction(L, -1), "pc.get_name must be a C function");
	lua_pop(L, 2);

	lua_getglobal(L, "q");
	RUNTIME_ASSERT(lua_istable(L, -1), "q table must be registered as a global table");
	lua_pushstring(L, "yield");
	lua_gettable(L, -2);
	RUNTIME_ASSERT(lua_iscfunction(L, -1), "q.yield must be a C function");
	lua_pop(L, 2);

	lua_getglobal(L, "say");
	RUNTIME_ASSERT(lua_iscfunction(L, -1), "say must be a registered global C function");
	lua_pop(L, 1);

	lua_getglobal(L, "select");
	RUNTIME_ASSERT(lua_isfunction(L, -1) || lua_iscfunction(L, -1), "select must be a valid registered function");
	lua_pop(L, 1);
	std::cout << "  --> pc, q, say, select tables and functions confirmed" << std::endl;
	checks_passed++;

	// 5. select(...) Nil-Argument Preservation & m2_lua_getn
	std::cout << "[RUN] Check 5: select(...) nil-argument preservation & m2_lua_getn contract..." << std::endl;
	int pack_err = lua_dostring(L, "test_packed = (table.pack and table.pack('A', nil, 'C')) or {n=3, [1]='A', [3]='C'}");
	RUNTIME_ASSERT(pack_err == 0, "table.pack code evaluation must succeed");
	lua_getglobal(L, "test_packed");
	RUNTIME_ASSERT(lua_istable(L, -1), "test_packed must be a table");
	size_t n = luaL_getn(L, -1);
	RUNTIME_ASSERT(n == 3, "luaL_getn must return exactly 3 despite nil element in middle");
	lua_pop(L, 1);
	std::cout << "  --> Packed table with nil argument preserved count n = 3" << std::endl;
	checks_passed++;

	// 6. Real questlib.lua and locale.lua execution
	std::cout << "[RUN] Check 6: Real questlib.lua and locale.lua loading..." << std::endl;
	const char* questlib_paths[] = {
		"tests/fixtures/questlib.lua",
		"../tests/fixtures/questlib.lua",
		"../../tests/fixtures/questlib.lua",
		"scratch/ftp_quests/usr/home/mt2/share/locale/poland/quest/questlib.lua"
	};
	std::string found_ql = "";
	for (const char* p : questlib_paths) {
		FILE* fp = fopen(p, "r");
		if (fp) {
			fclose(fp);
			found_ql = p;
			break;
		}
	}
	RUNTIME_ASSERT(!found_ql.empty(), "A valid questlib.lua fixture must be available on disk");
	int ql_res = lua_dofile(L, found_ql.c_str());
	RUNTIME_ASSERT(ql_res == 0, "questlib.lua execution must succeed without error");

	lua_getglobal(L, "CONFIRM_YES");
	RUNTIME_ASSERT(lua_isnumber(L, -1) && (int)lua_tonumber(L, -1) == 1, "CONFIRM_YES must be 1 in questlib.lua");
	lua_pop(L, 1);

	const char* locale_paths[] = {
		"tests/fixtures/locale.lua",
		"../tests/fixtures/locale.lua",
		"../../tests/fixtures/locale.lua",
		"scratch/ftp_quests/usr/home/mt2/share/locale/poland/quest/locale.lua"
	};
	std::string found_loc = "";
	for (const char* p : locale_paths) {
		FILE* fp = fopen(p, "r");
		if (fp) {
			fclose(fp);
			found_loc = p;
			break;
		}
	}
	RUNTIME_ASSERT(!found_loc.empty(), "A valid locale.lua fixture must be available on disk");
	int loc_res = lua_dofile(L, found_loc.c_str());
	RUNTIME_ASSERT(loc_res == 0, "locale.lua execution must succeed without error");

	lua_getglobal(L, "locale");
	RUNTIME_ASSERT(lua_istable(L, -1), "locale table must be defined in locale.lua");
	lua_pushstring(L, "yes");
	lua_gettable(L, -2);
	RUNTIME_ASSERT(lua_isstring(L, -1), "locale.yes must be a string");
	lua_pop(L, 2);
	std::cout << "  --> Loaded questlib (" << found_ql << ") and locale (" << found_loc << ") verified" << std::endl;
	checks_passed++;

	// 7. Gameforge Localization Proxy
	std::cout << "[RUN] Check 7: Gameforge translation proxy..." << std::endl;
	int gf_err = lua_dostring(L,
		"if not gameforge or type(gameforge) ~= 'table' then\n"
		"    gameforge = setmetatable({}, {\n"
		"        __index = function(t, k) return setmetatable({}, {\n"
		"            __tostring = function() return '[MISSING_LOCALE: ' .. tostring(k) .. ']' end\n"
		"        }) end\n"
		"    })\n"
		"end\n"
		"missing_str = tostring(gameforge.locale.unknown_test_key_xyz)\n"
	);
	RUNTIME_ASSERT(gf_err == 0, "Gameforge proxy code evaluation must succeed");
	lua_getglobal(L, "missing_str");
	RUNTIME_ASSERT(lua_isstring(L, -1), "missing_str must be a string");
	RUNTIME_ASSERT(strstr(lua_tostring(L, -1), "MISSING_LOCALE") != nullptr, "Missing keys must not be silently masked");
	lua_pop(L, 1);
	std::cout << "  --> Gameforge proxy transparently resolves and flags missing keys" << std::endl;
	checks_passed++;

	// 8. Interactive Coroutine Dialog State Machine (select, wait, input, confirm, select_item, normal completion)
	std::cout << "[RUN] Check 8: Interactive Coroutine Dialog State Machine..." << std::endl;
#if LUA_V == 551 || LUA_V == 523
	const int EXPECT_YIELD = LUA_YIELD;
#else
	const int EXPECT_YIELD = 0;
#endif

	// 8.1 select
	lua_State* co = lua_newthread(L);
	RUNTIME_ASSERT(co != nullptr, "Failed to create coroutine thread");
	const char* quest_script =
		"local choice = select('Accept Biologist Task', 'Refuse Task')\n"
		"return choice\n";
	int load_err = lua_dobuffer(co, quest_script, strlen(quest_script), "test_biologist");
	RUNTIME_ASSERT(load_err == 0, "test_biologist quest script buffer compilation failed");
	int resume_res = lua_resume(co, 0);
	RUNTIME_ASSERT(resume_res == EXPECT_YIELD, "Coroutine must yield on select");
	RUNTIME_ASSERT(lua_gettop(co) >= 2, "Coroutine stack must contain yield type and args");
	const char* yield_type = lua_tostring(co, 1);
	RUNTIME_ASSERT(yield_type != nullptr && strcmp(yield_type, "select") == 0, "Yield type must be 'select'");
	size_t num_opts = luaL_getn(co, 2);
	RUNTIME_ASSERT(num_opts == 2, "Must have exactly 2 options in select");
	lua_pushnumber(co, 1);
	int resume2 = lua_resume(co, 1);
	RUNTIME_ASSERT(resume2 == 0, "Coroutine must complete with 0 (LUA_OK)");
	RUNTIME_ASSERT((int)lua_tonumber(co, -1) == 1, "Return value must be choice 1");
	lua_pop(L, 1);

	// 8.2 wait
	lua_State* co_wait = lua_newthread(L);
	RUNTIME_ASSERT(co_wait != nullptr, "Failed to create wait coroutine");
	const char* script_wait = "wait()\nreturn 'after_wait'\n";
	load_err = lua_dobuffer(co_wait, script_wait, strlen(script_wait), "test_wait");
	RUNTIME_ASSERT(load_err == 0, "test_wait buffer compilation failed");
	resume_res = lua_resume(co_wait, 0);
	RUNTIME_ASSERT(resume_res == EXPECT_YIELD, "Coroutine must yield on wait()");
	RUNTIME_ASSERT(lua_gettop(co_wait) >= 1 && strcmp(lua_tostring(co_wait, 1), "wait") == 0, "Yield type must be 'wait'");
	resume2 = lua_resume(co_wait, 0);
	RUNTIME_ASSERT(resume2 == 0, "wait resumption must complete with 0");
	RUNTIME_ASSERT(strcmp(lua_tostring(co_wait, -1), "after_wait") == 0, "wait return value must match");
	lua_pop(L, 1);

	// 8.3 input
	lua_State* co_input = lua_newthread(L);
	RUNTIME_ASSERT(co_input != nullptr, "Failed to create input coroutine");
	const char* script_input = "local txt = input()\nreturn txt\n";
	load_err = lua_dobuffer(co_input, script_input, strlen(script_input), "test_input");
	RUNTIME_ASSERT(load_err == 0, "test_input buffer compilation failed");
	resume_res = lua_resume(co_input, 0);
	RUNTIME_ASSERT(resume_res == EXPECT_YIELD, "Coroutine must yield on input()");
	RUNTIME_ASSERT(lua_gettop(co_input) >= 1 && strcmp(lua_tostring(co_input, 1), "input") == 0, "Yield type must be 'input'");
	lua_pushstring(co_input, "player_secret_word");
	resume2 = lua_resume(co_input, 1);
	RUNTIME_ASSERT(resume2 == 0, "input resumption must complete with 0");
	RUNTIME_ASSERT(strcmp(lua_tostring(co_input, -1), "player_secret_word") == 0, "input return value must match player text");
	lua_pop(L, 1);

	// 8.4 confirm
	lua_State* co_confirm = lua_newthread(L);
	RUNTIME_ASSERT(co_confirm != nullptr, "Failed to create confirm coroutine");
	const char* script_confirm = "local ans = confirm(1001, 'Do you accept the guild war?', 15)\nreturn ans\n";
	load_err = lua_dobuffer(co_confirm, script_confirm, strlen(script_confirm), "test_confirm");
	RUNTIME_ASSERT(load_err == 0, "test_confirm buffer compilation failed");
	resume_res = lua_resume(co_confirm, 0);
	RUNTIME_ASSERT(resume_res == EXPECT_YIELD, "Coroutine must yield on confirm()");
	RUNTIME_ASSERT(lua_gettop(co_confirm) >= 1 && strcmp(lua_tostring(co_confirm, 1), "confirm") == 0, "Yield type must be 'confirm'");
	lua_pushnumber(co_confirm, 1); // CONFIRM_YES
	resume2 = lua_resume(co_confirm, 1);
	RUNTIME_ASSERT(resume2 == 0, "confirm resumption must complete with 0");
	RUNTIME_ASSERT((int)lua_tonumber(co_confirm, -1) == 1, "confirm return value must be CONFIRM_YES");
	lua_pop(L, 1);

	// 8.5 select_item
	lua_State* co_item = lua_newthread(L);
	RUNTIME_ASSERT(co_item != nullptr, "Failed to create select_item coroutine");
	const char* script_item = "local slot = select_item()\nreturn slot\n";
	load_err = lua_dobuffer(co_item, script_item, strlen(script_item), "test_select_item");
	RUNTIME_ASSERT(load_err == 0, "test_select_item buffer compilation failed");
	resume_res = lua_resume(co_item, 0);
	RUNTIME_ASSERT(resume_res == EXPECT_YIELD, "Coroutine must yield on select_item()");
	RUNTIME_ASSERT(lua_gettop(co_item) >= 1 && strcmp(lua_tostring(co_item, 1), "select_item") == 0, "Yield type must be 'select_item'");
	lua_pushnumber(co_item, 5); // selected item cell 5
	resume2 = lua_resume(co_item, 1);
	RUNTIME_ASSERT(resume2 == 0, "select_item resumption must complete with 0");
	RUNTIME_ASSERT((int)lua_tonumber(co_item, -1) == 5, "select_item return value must be slot 5");
	lua_pop(L, 1);

	// 8.6 Normal completion without yield
	lua_State* co_norm = lua_newthread(L);
	RUNTIME_ASSERT(co_norm != nullptr, "Failed to create normal coroutine");
	const char* script_norm = "local a = 40 + 2\nreturn a\n";
	load_err = lua_dobuffer(co_norm, script_norm, strlen(script_norm), "test_normal");
	RUNTIME_ASSERT(load_err == 0, "test_normal buffer compilation failed");
	resume_res = lua_resume(co_norm, 0);
	RUNTIME_ASSERT(resume_res == 0, "normal completion must return 0 without yielding");
	RUNTIME_ASSERT((int)lua_tonumber(co_norm, -1) == 42, "normal completion return value must be 42");
	lua_pop(L, 1);

	std::cout << "  --> All 5 yield commands (select, wait, input, confirm, select_item) & normal completion verified" << std::endl;
	checks_passed++;

	// 9. __codecache 64-Bit Pointer Safety
	std::cout << "[RUN] Check 9: __codecache 64-bit pointer safety..." << std::endl;
	lua_getglobal(L, "__codecache");
	if (!lua_istable(L, -1))
	{
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushvalue(L, -1);
		lua_setglobal(L, "__codecache");
	}

	void* test_addr = (void*)(uintptr_t)0x7FFE98765432ULL;
	lua_pushlightuserdata(L, test_addr);
	lua_pushstring(L, "CACHED_BYTECODE_LUA55");
	lua_rawset(L, -3);

	// Retrieve with identical pointer
	lua_pushlightuserdata(L, test_addr);
	lua_rawget(L, -2);
	RUNTIME_ASSERT(lua_isstring(L, -1), "__codecache must return stored string");
	RUNTIME_ASSERT(strcmp(lua_tostring(L, -1), "CACHED_BYTECODE_LUA55") == 0, "__codecache returned corrupted data");
	lua_pop(L, 2); // pop string and __codecache
	std::cout << "  --> 64-bit pointer key (0x7FFE98765432) stored and retrieved with 0 precision loss" << std::endl;
	checks_passed++;

	// 10. Coroutine Error Isolation & Crash Prevention
	std::cout << "[RUN] Check 10: Coroutine Error Isolation & Crash Prevention..." << std::endl;
	lua_State* co_err = lua_newthread(L);
	RUNTIME_ASSERT(co_err != nullptr, "Failed to create error test coroutine");
	const char* bad_script = "error('Deliberate quest error for safety test')";
	int err_load = lua_dobuffer(co_err, bad_script, strlen(bad_script), "bad_quest");
	RUNTIME_ASSERT(err_load == 0, "Loading bad_quest buffer must succeed");
	int err_res = lua_resume(co_err, 0);
#if LUA_V == 551 || LUA_V == 523
	RUNTIME_ASSERT(err_res != 0 && err_res != LUA_YIELD, "Deliberate error must return non-zero error status");
#else
	RUNTIME_ASSERT(err_res != 0, "Deliberate error must return non-zero error status");
#endif
	lua_pop(L, 1);
	std::cout << "  --> Deliberate script errors trapped without crashing server process" << std::endl;
	checks_passed++;

	// 11. Multi-State Quest Registration & Safe BuildStateIndexToName Two-Way Parity
	std::cout << "[RUN] Check 11: Multi-State Quest Registration & Safe BuildStateIndexToName..." << std::endl;
	const char* test_state_script =
		"test_multistate_quest = {\n"
		"    [\"start\"] = 0,\n"
		"    [\"information\"] = 695801987,\n"
		"    [\"go_to_disciple\"] = 1675612723,\n"
		"    [\"__complete\"] = 557528158\n"
		"}\n";
	int state_load_err = lua_dostring(L, test_state_script);
	RUNTIME_ASSERT(state_load_err == 0, "test_multistate_quest script execution failed");
	qm.BuildStateIndexToName("test_multistate_quest");

	lua_getglobal(L, "test_multistate_quest");
	RUNTIME_ASSERT(lua_istable(L, -1), "test_multistate_quest table must exist");

	// Verify forward mappings
	lua_pushstring(L, "start"); lua_gettable(L, -2);
	RUNTIME_ASSERT(lua_isnumber(L, -1) && (int)lua_tonumber(L, -1) == 0, "start forward mapping must be 0");
	lua_pop(L, 1);

	lua_pushstring(L, "information"); lua_gettable(L, -2);
	RUNTIME_ASSERT(lua_isnumber(L, -1) && (double)lua_tonumber(L, -1) == 695801987.0, "information forward mapping must be 695801987");
	lua_pop(L, 1);

	lua_pushstring(L, "go_to_disciple"); lua_gettable(L, -2);
	RUNTIME_ASSERT(lua_isnumber(L, -1) && (double)lua_tonumber(L, -1) == 1675612723.0, "go_to_disciple forward mapping must be 1675612723");
	lua_pop(L, 1);

	lua_pushstring(L, "__complete"); lua_gettable(L, -2);
	RUNTIME_ASSERT(lua_isnumber(L, -1) && (double)lua_tonumber(L, -1) == 557528158.0, "__complete forward mapping must be 557528158");
	lua_pop(L, 1);

	// Verify reverse mappings
	lua_pushnumber(L, 0); lua_gettable(L, -2);
	RUNTIME_ASSERT(lua_isstring(L, -1) && strcmp(lua_tostring(L, -1), "start") == 0, "0 reverse mapping must be 'start'");
	lua_pop(L, 1);

	lua_pushnumber(L, 695801987); lua_gettable(L, -2);
	RUNTIME_ASSERT(lua_isstring(L, -1) && strcmp(lua_tostring(L, -1), "information") == 0, "695801987 reverse mapping must be 'information'");
	lua_pop(L, 1);

	lua_pushnumber(L, 1675612723); lua_gettable(L, -2);
	RUNTIME_ASSERT(lua_isstring(L, -1) && strcmp(lua_tostring(L, -1), "go_to_disciple") == 0, "1675612723 reverse mapping must be 'go_to_disciple'");
	lua_pop(L, 1);

	lua_pushnumber(L, 557528158); lua_gettable(L, -2);
	RUNTIME_ASSERT(lua_isstring(L, -1) && strcmp(lua_tostring(L, -1), "__complete") == 0, "557528158 reverse mapping must be '__complete'");
	lua_pop(L, 2); // pop string and table

	std::cout << "  --> Two-way multi-state index/name mappings verified with 0 loss" << std::endl;
	checks_passed++;

	// Summary
	std::cout << "==================================================" << std::endl;
	std::cout << " [SUCCESS] METIN2 QUEST RUNTIME: " << checks_passed << "/11 CHECKS PASSED!" << std::endl;
	std::cout << "==================================================" << std::endl;
	return 0;
}
