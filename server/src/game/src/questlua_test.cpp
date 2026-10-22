#include "stdafx.h"
#include <iostream>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "questmanager.h"
#include "questlua.h"
#include "packet.h"
#include "char.h"
#include "char_manager.h"
#include "affect.h"
#include "arena.h"
#include "BlueDragon_Binder.h"

// ==============================================================================
// METIN2 MT2009 - GAME SERVER LUA 5.5.1 QUEST RUNTIME VERIFICATION
// Executed via: ./game_r41023 --check-lua
// Tests:
//   1. 32-Bit Client wire protocol packet_script struct size guarantee
//   2. QuestManager & Lua 5.5.1 State initialization
//   3. Compatibility shims (math.mod, math.pow, string.gfind, table.getn, unpack)
//   4. Metin2 Quest C API registration (pc, npc, item, q, say, select)
//   5. select(...) nil-argument preservation & m2_lua_getn contract
//   6. Real questlib.lua, libs/*, settings.lua, BlueDragon.lua, translate.lua,
//      locale.lua, oxquiz.lua, monkey_dungeon.lua & object/* verification
//   7. Gameforge translation proxy & missing locale detection
//   8. Interactive Coroutine Dialog State Machine (yield on select, resume with choice)
//   9. __codecache 64-bit pointer key safety (no precision loss)
//  10. Coroutine error isolation & crash prevention (fail-closed)
//  11. Multi-State Quest Registration & Safe BuildStateIndexToName
// ==============================================================================

#define RUNTIME_ASSERT(cond, msg) \
	do { \
		if (!(cond)) { \
			std::cerr << "[FAIL] Line " << __LINE__ << ": " << msg << std::endl; \
			return 1; \
		} \
	} while (0)

extern std::string g_stQuestDir;
extern std::string g_stServiceBasePath;

static int DummyAddOxQuiz(lua_State* L)
{
	lua_pushboolean(L, 1);
	return 1;
}

static int DummyAddGotoNpcForMonkeyDungeon(lua_State* L)
{
	(void)L;
	return 0;
}

int RunGameQuestRuntimeTests()
{
	std::cout << "==================================================" << std::endl;
	std::cout << " METIN2 MT2009 - GAME SERVER QUEST RUNTIME SUITE" << std::endl;
	std::cout << " Platform: 64-bit amd64" << std::endl;
	std::cout << " Lua Engine: Lua 5.5.1 Exclusive Production" << std::endl;
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

	// Stage fixture path to ensure fail-closed loading of settings.lua, questlib.lua, translate.lua, locale.lua
	const char* candidate_dirs[] = {
		"tests/fixtures",
		"../tests/fixtures",
		"../../tests/fixtures",
		"../../../tests/fixtures"
	};
	std::string staging_dir = "";
	for (const char* dir : candidate_dirs) {
		std::string test_path = std::string(dir) + "/questlib.lua";
		FILE* fp = fopen(test_path.c_str(), "r");
		if (fp) {
			fclose(fp);
			staging_dir = dir;
			break;
		}
	}
	RUNTIME_ASSERT(!staging_dir.empty(), "Mandatory tests/fixtures directory with questlib.lua must exist");
	g_stQuestDir = staging_dir;
	g_stServiceBasePath = staging_dir;

	std::unique_ptr<CArenaManager> local_am;
	if (CArenaManager::instance_ptr() == nullptr)
	{
		local_am = std::make_unique<CArenaManager>();
	}

	std::unique_ptr<quest::CQuestManager> local_qm;
	if (quest::CQuestManager::instance_ptr() == nullptr)
	{
		local_qm = std::make_unique<quest::CQuestManager>();
	}
	quest::CQuestManager& qm = quest::CQuestManager::instance();
	lua_State* L = qm.GetLuaState();
	if (!L)
	{
		bool init_ok = qm.InitializeLua();
		RUNTIME_ASSERT(init_ok, "InitializeLua() must succeed and load all mandatory lua files fail-closed");
		L = qm.GetLuaState();
	}
	RUNTIME_ASSERT(L != nullptr, "Lua state must be initialized and non-null");
	std::cout << "  --> Lua state initialized at: " << (void*)L << " from staging: " << staging_dir << std::endl;
	checks_passed++;

	// 3. Compatibility Shims & Lua 5.5.1 Numeric/String Semantics
	std::cout << "[RUN] Check 3: Standard compatibility shims & Lua 5.5.1 numeric semantics..." << std::endl;
	int shim_err = lua_dostring(L,
		"assert(math.mod(10, 3) == 1, 'math.mod failed')\n"
		"assert(math.pow(2, 4) == 16, 'math.pow failed')\n"
		"local t = {10, 20, 30}\n"
		"assert(table.getn(t) == 3, 'table.getn failed')\n"
		"local count = 0\n"
		"table.foreach(t, function(k, v) count = count + 1 end)\n"
		"assert(count == 3, 'table.foreach failed')\n"
		"assert(string.format('%d', 31 - 45 / 60) == '30', '%d positive fractional float truncation failed')\n"
		"assert(string.format('%d', -1.9) == '-1', '%d negative fractional float truncation toward zero failed')\n"
		"assert(math.random(1.9, 1.9) == 1, 'math.random fractional float bound failed')\n"
		"local ok_bit = pcall(function() return 1.5 | 0 end)\n"
		"assert(not ok_bit, 'Standard Lua 5.5 F2Ieq must reject non-integral float in bitwise op')\n"
		"local kt = {}\n"
		"kt[50] = 'fifty'\n"
		"assert(kt[50.0] == 'fifty', 'kt[50.0] must match kt[50]')\n"
		"assert(tostring(-0.0) == '-0' and 1.0 / (-0.0) == -math.huge, '-0.0 sign preservation failed')\n"
		"assert(tostring(math.maxinteger) == '9223372036854775807', '64-bit maxinteger failed')\n"
		"assert(tostring(math.mininteger) == '-9223372036854775808', '64-bit mininteger failed')\n"
		"local nan = 0.0 / 0.0\n"
		"assert(nan ~= nan and not pcall(function() kt[nan] = 1 end), 'NaN semantics failed')\n"
		"assert(string.format('%s', 500000 / 10000) == '50', '%s integral float formatting failed')\n"
		"assert(tostring(1000000.0) == '1000000', 'integral float tostring failed')\n"
	);
	RUNTIME_ASSERT(shim_err == 0, "Compatibility shims & numeric boundary execution must return 0");
	std::cout << "  --> Shims, fractional/negative float truncation, table indexing, -0.0, 64-bit limits, NaN/Inf & integral float formatting verified" << std::endl;
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

	{
		std::unique_ptr<CHARACTER> test_ch = std::make_unique<CHARACTER>(false);
		qm.SetCurrentCharacterPtr(test_ch.get());

		// Add unrelated affects to verify affect.remove_hair() never touches other affects
		RUNTIME_ASSERT(test_ch->AddAffect(AFFECT_MOV_SPEED, POINT_MOV_SPEED, 20, 0, 3600, 0, false), "Baseline AFFECT_MOV_SPEED must be added");
		RUNTIME_ASSERT(test_ch->AddAffect(AFFECT_COLLECT, POINT_ATTBONUS_HUMAN, 10, 0, INFINITE_AFFECT_DURATION, 0, false), "Baseline AFFECT_COLLECT must be added");
		RUNTIME_ASSERT(test_ch->GetPoint(POINT_MOV_SPEED) == 20 && test_ch->GetPoint(POINT_ATTBONUS_HUMAN) == 10, "Baseline affect points must be active");

		int hair_behav_err = lua_dostring(L,
			"assert(ay_reward == nil, 'temporary ay_reward C++ alias must not exist')\n"
			"assert(affect.get_count(514) == 0, 'initial AFFECT_HAIR count must be 0')\n"
			"assert(affect.get_apply_on(514) == nil, 'initial AFFECT_HAIR apply_on must be nil')\n"
			"local r0_a, r0_v, r0_d = affect.remove_hair()\n"
			"assert(r0_a == nil and r0_v == nil and r0_d == nil, 'remove_hair with no active hair affect must return nil')\n"
			"-- Add hair affect using questlib apply.ATTBONUS_MONSTER (63 -> POINT_ATTBONUS_MONSTER 53)\n"
			"affect.add_hair(63, 10, 604800)\n"
			"assert(affect.get_count(514) == 1, 'AFFECT_HAIR count must be 1 after add_hair')\n"
			"assert(affect.get_apply_on(514) == 53, 'AFFECT_HAIR apply_on must map 63 -> 53 (POINT_ATTBONUS_MONSTER)')\n"
			"assert(affect.get_duration(514) == 604800, 'AFFECT_HAIR duration must be 604800')\n"
			"-- Replacing hair affect without remove_hair must not stack duplicate AFFECT_HAIR entries\n"
			"affect.add_hair(59, 15, 86400)\n"
			"assert(affect.get_count(514) == 1, 'AFFECT_HAIR count must remain 1 when replaced')\n"
			"assert(affect.get_apply_on(514) == 54, 'AFFECT_HAIR apply_on must map 59 -> 54 (POINT_ATTBONUS_WARRIOR)')\n"
			"assert(affect.get_duration(514) == 86400, 'AFFECT_HAIR duration must be 86400')\n"
			"-- Remove hair affect and verify returned (applyOn, value, duration) tuple\n"
			"local ra, rv, rd = affect.remove_hair()\n"
			"assert(ra == 54 and rv == 15 and rd == 86400, 'remove_hair must return (54, 15, 86400)')\n"
			"assert(affect.get_count(514) == 0 and affect.get_apply_on(514) == nil, 'AFFECT_HAIR must be removed')\n"
			"-- Invalid types, out-of-range apply (0, negative, >= POINT_MAX_NUM, utility point), and <= 0 durations must be rejected\n"
			"affect.add_hair()\n"
			"affect.add_hair(nil, 10, 600)\n"
			"affect.add_hair('invalid', 10, 600)\n"
			"affect.add_hair({}, 10, 600)\n"
			"affect.add_hair(0, 10, 600)\n"
			"affect.add_hair(-5, 10, 600)\n"
			"affect.add_hair(300, 10, 600)\n"
			"affect.add_hair(1, 10, 600)\n"
			"affect.add_hair(53, 10, 0)\n"
			"affect.add_hair(53, 10, -604800)\n"
			"affect.add_hair(0/0, 10, 600)\n"
			"affect.add_hair(1/0, 10, 600)\n"
			"assert(affect.get_count(514) == 0 and affect.get_apply_on(514) == nil, 'invalid add_hair calls must not add AFFECT_HAIR')\n"
		);
		RUNTIME_ASSERT(hair_behav_err == 0, "affect.add_hair & affect.remove_hair behavioral assertions must pass");
		RUNTIME_ASSERT(test_ch->FindAffect(AFFECT_HAIR) == nullptr, "AFFECT_HAIR must not remain on test character");
		RUNTIME_ASSERT(test_ch->GetPoint(POINT_ATTBONUS_MONSTER) == 0 && test_ch->GetPoint(POINT_ATTBONUS_WARRIOR) == 0, "Removed hair affect stat bonus must be reverted to 0");
		RUNTIME_ASSERT(test_ch->FindAffect(AFFECT_MOV_SPEED) != nullptr && test_ch->GetPoint(POINT_MOV_SPEED) == 20, "Unrelated AFFECT_MOV_SPEED must remain untouched");
		RUNTIME_ASSERT(test_ch->FindAffect(AFFECT_COLLECT) != nullptr && test_ch->GetPoint(POINT_ATTBONUS_HUMAN) == 10, "Unrelated AFFECT_COLLECT must remain untouched");

		test_ch->ClearAffect();
		qm.SetCurrentCharacterPtr(nullptr);
	}
	std::cout << "  --> pc, q, say, select & affect.add_hair/remove_hair runtime behavior confirmed" << std::endl;
	checks_passed++;

	// 5. select(...) Nil-Argument Preservation & m2_lua_getn
	std::cout << "[RUN] Check 5: select(...) nil-argument preservation & m2_lua_getn contract..." << std::endl;
	int pack_err = lua_dostring(L, "test_packed = table.pack('A', nil, 'C')");
	RUNTIME_ASSERT(pack_err == 0, "table.pack code evaluation must succeed");
	lua_getglobal(L, "test_packed");
	RUNTIME_ASSERT(lua_istable(L, -1), "test_packed must be a table");
	size_t n = luaL_getn(L, -1);
	RUNTIME_ASSERT(n == 3, "luaL_getn must return exactly 3 despite nil element in middle");
	lua_pop(L, 1);
	std::cout << "  --> Packed table with nil argument preserved count n = 3" << std::endl;
	checks_passed++;

	// 6. Real questlib.lua, libs/*, settings.lua, BlueDragon.lua, translate.lua, locale.lua, oxquiz.lua & object/* execution
	std::cout << "[RUN] Check 6: questlib.lua, libs/*, locale.lua & object/* verification (Synthetic & Real Serverfiles)..." << std::endl;

	// 6.1 Synthetic Fixture Verification (Loaded in InitializeLua)
	lua_getglobal(L, "CONFIRM_YES");
	RUNTIME_ASSERT(lua_isnumber(L, -1) && (int)lua_tonumber(L, -1) == 1, "CONFIRM_YES must be 1 in questlib.lua");
	lua_pop(L, 1);

	lua_getglobal(L, "locale");
	RUNTIME_ASSERT(lua_istable(L, -1), "locale table must be defined in locale.lua");
	lua_pushstring(L, "yes");
	lua_gettable(L, -2);
	RUNTIME_ASSERT(lua_isstring(L, -1), "locale.yes must be a string");
	lua_pop(L, 2);
	std::cout << "  --> [PASS] Synthetic fixtures (questlib.lua, locale.lua, settings.lua, translate.lua) verified" << std::endl;

	// 6.2 Real Serverfiles Verification (when server/files/share/locale/poland is present)
	const char* real_poland_dirs[] = {
		"server/files/share/locale/poland",
		"../server/files/share/locale/poland",
		"../../server/files/share/locale/poland",
		"../../../server/files/share/locale/poland",
		"/usr/home/mt2/share/locale/poland",
		"share/locale/poland",
		"../share/locale/poland",
		"../../share/locale/poland",
		"scratch/ftp_quests/usr/home/mt2/share/locale/poland",
		"../scratch/ftp_quests/usr/home/mt2/share/locale/poland",
		"../../scratch/ftp_quests/usr/home/mt2/share/locale/poland"
	};
	std::string found_poland_dir = "";
	for (const char* d : real_poland_dirs) {
		std::string ql_path = std::string(d) + "/quest/questlib.lua";
		FILE* fp = fopen(ql_path.c_str(), "r");
		if (fp) {
			fclose(fp);
			found_poland_dir = d;
			break;
		}
	}
	if (!found_poland_dir.empty()) {
		std::string prev_service_path = g_stServiceBasePath;
		std::string prev_quest_dir = g_stQuestDir;
		g_stServiceBasePath = found_poland_dir;
		g_stQuestDir = found_poland_dir + "/quest";

		// 6.2.1 settings.lua & BlueDragon.lua (loaded via dofile inside settings.lua)
		std::string settings_path = found_poland_dir + "/settings.lua";
		RUNTIME_ASSERT(lua_dofile(L, settings_path.c_str()) == 0, "Real serverfiles settings.lua + BlueDragon.lua execution must succeed");

		int bd_err = lua_dostring(L, "assert(type(BlueDragonSetting) == 'table', 'BlueDragonSetting table missing')\n");
		RUNTIME_ASSERT(bd_err == 0, "BlueDragonSetting table must be loaded by settings.lua");
		RUNTIME_ASSERT(BlueDragon_GetSkillFactor(3, "Skill0", "period", "min") == 30, "BlueDragon_GetSkillFactor(Skill0.period.min) must return 30");
		RUNTIME_ASSERT(BlueDragon_GetRangeFactor("hp_damage", 60) == 10, "BlueDragon_GetRangeFactor(hp_damage, 60) must return 10");
		RUNTIME_ASSERT(BlueDragon_GetIndexFactor("DragonStone", 1, "effect_type") == 1, "BlueDragon_GetIndexFactor(DragonStone, 1, effect_type) must return 1");

		// 6.2.2 questlib.lua + GFquestlib.lua + libModuleLoader.lua (36 active libs/* submodules)
		std::string ql_path = found_poland_dir + "/quest/questlib.lua";
		RUNTIME_ASSERT(lua_dofile(L, ql_path.c_str()) == 0, "Real serverfiles questlib.lua + libs/* execution must succeed");

		// 6.2.3 translate.lua & quest/locale.lua
		std::string trans_path = found_poland_dir + "/translate.lua";
		RUNTIME_ASSERT(lua_dofile(L, trans_path.c_str()) == 0, "Real serverfiles translate.lua execution must succeed");

		std::string loc_path = found_poland_dir + "/quest/locale.lua";
		RUNTIME_ASSERT(lua_dofile(L, loc_path.c_str()) == 0, "Real serverfiles quest/locale.lua execution must succeed");

		int helper_err = lua_dostring(L,
			"assert(parse_number(1000000) == '1.000.000', 'parse_number(1000000) failed: ' .. tostring(parse_number(1000000)))\n"
			"assert(parse_number(1000000.0) == '1.000.000', 'parse_number(1000000.0) failed')\n"
			"assert(type(get_time_remaining(3665)) == 'string', 'get_time_remaining failed')\n"
			"assert(type(busy_action) == 'table', 'busy_action module missing')\n"
			"assert(type(crafting) == 'table', 'crafting module missing')\n"
			"assert(type(fishing_manage) == 'table', 'fishing_manage module missing')\n"
			"assert(type(itemshop) == 'table', 'itemshop module missing')\n"
			"assert(type(quest_book) == 'table', 'quest_book module missing')\n"
			"assert(type(affect) == 'table' and type(affect.add_hair) == 'function' and type(affect.remove_hair) == 'function', 'affect.add_hair/remove_hair missing')\n"
			"assert(ay_reward == nil, 'ay_reward alias must be removed after pony_levelup.quest source fix')\n"
		);
		RUNTIME_ASSERT(helper_err == 0, "questlib.lua & libs/* helper assertions must pass");

		// 6.2.4 oxquiz.lua & monkey_dungeon.lua
		lua_register(L, "add_ox_quiz", DummyAddOxQuiz);
		std::string ox_path = found_poland_dir + "/oxquiz.lua";
		RUNTIME_ASSERT(lua_dofile(L, ox_path.c_str()) == 0, "Real serverfiles oxquiz.lua execution must succeed");

		lua_register(L, "add_goto_npc_for_monkey_dungeon", DummyAddGotoNpcForMonkeyDungeon);
		std::string md_path = found_poland_dir + "/monkey_dungeon.lua";
		FILE* fp_md = fopen(md_path.c_str(), "r");
		if (fp_md) {
			fclose(fp_md);
			RUNTIME_ASSERT(lua_dofile(L, md_path.c_str()) == 0, "Real serverfiles monkey_dungeon.lua execution must succeed");
		}

		// 6.2.5 Load all object/state/* files and verify BuildStateIndexToName
		namespace fs = std::filesystem;
		fs::path obj_root = fs::path(found_poland_dir) / "quest" / "object";
		fs::path state_root = obj_root / "state";
		int state_files_loaded = 0;
		if (fs::exists(state_root) && fs::is_directory(state_root)) {
			for (const auto& entry : fs::directory_iterator(state_root)) {
				if (!entry.is_regular_file()) continue;
				std::string qname = entry.path().filename().string();
				std::string spath = entry.path().string();
				RUNTIME_ASSERT(lua_dofile(L, spath.c_str()) == 0, ("Failed to execute object/state/" + qname).c_str());
				qm.BuildStateIndexToName(qname.c_str());
				state_files_loaded++;
			}
			RUNTIME_ASSERT(state_files_loaded >= 202, "Must load all 202 object/state/* quest state tables");
			RUNTIME_ASSERT(lua_dostring(L, "assert(type(guild_building) == 'table' and type(guild_manage) == 'table' and guild_building ~= guild_manage, 'guild_building and guild_manage state tables must both exist and be distinct')") == 0, "guild_building and guild_manage state tables must be separate");

			// Verify event_halloween_hair.get_hair_option(1..7) end-to-end with affect.add_hair / affect.remove_hair
			std::unique_ptr<CHARACTER> hw_ch = std::make_unique<CHARACTER>(false);
			qm.SetCurrentCharacterPtr(hw_ch.get());
			int hw_err = lua_dostring(L,
				"local expected_points = { [1]=53, [2]=56, [3]=57, [4]=54, [5]=55, [6]=119, [7]=120 }\n"
				"for idx = 1, 7 do\n"
				"    local opt = event_halloween_hair.get_hair_option(idx)\n"
				"    assert(type(opt) == 'table', 'missing hair option ' .. idx)\n"
				"    affect.remove_hair()\n"
				"    affect.add_hair(opt.apply, opt.value, opt.duration)\n"
				"    assert(affect.get_apply_on(514) == expected_points[idx], 'mismatch for option ' .. idx)\n"
				"    local ra, rv, rd = affect.remove_hair()\n"
				"    assert(ra == expected_points[idx] and rv == opt.value and rd == opt.duration, 'remove_hair mismatch for option ' .. idx)\n"
				"end\n"
			);
			qm.SetCurrentCharacterPtr(nullptr);
			RUNTIME_ASSERT(hw_err == 0, "event_halloween_hair.get_hair_option(1..7) must map and apply/remove AFFECT_HAIR accurately");
		}

		// 6.2.6 Syntax-check all 6,900 compiled files in quest/object/ and evaluate all chat/*.arg expressions
		int obj_files_checked = 0;
		int chat_args_evaluated = 0;
		if (fs::exists(obj_root) && fs::is_directory(obj_root)) {
			for (const auto& entry : fs::recursive_directory_iterator(obj_root)) {
				if (!entry.is_regular_file()) continue;
				std::string fpath = entry.path().string();
				std::string fname = entry.path().filename().string();
				std::ifstream ifs(entry.path(), std::ios::binary);
				std::ostringstream ss;
				ss << ifs.rdbuf();
				std::string content = ss.str();

				if (entry.path().extension() == ".arg") {
					if (entry.path().parent_path().filename() == "chat") {
						std::string line = content;
						while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
							line.pop_back();
						std::string expr = "return " + line;
						int top_before = lua_gettop(L);
						int aerr = lua_dobuffer(L, expr.c_str(), expr.size(), fpath.c_str());
						RUNTIME_ASSERT(aerr == 0, ("Failed to evaluate chat .arg expression: " + fpath).c_str());
						RUNTIME_ASSERT(lua_isstring(L, -1), ("chat .arg expression did not evaluate to string: " + fpath).c_str());
						const char* res_s = lua_tostring(L, -1);
						RUNTIME_ASSERT(res_s != nullptr && strstr(res_s, "MISSING_LOCALE") == nullptr, ("Missing locale in chat .arg: " + fpath).c_str());
						lua_settop(L, top_before);
						chat_args_evaluated++;
					}
					obj_files_checked++;
				} else {
					int lerr = luaL_loadbuffer(L, content.c_str(), content.size(), fpath.c_str());
					RUNTIME_ASSERT(lerr == 0, ("Syntax error in compiled object file: " + fpath).c_str());
					lua_pop(L, 1);
					obj_files_checked++;
				}
			}
			RUNTIME_ASSERT(obj_files_checked >= 6900, "Must syntax-verify all 6,900 compiled object/ files");
			RUNTIME_ASSERT(chat_args_evaluated >= 696, "Must evaluate all 696 chat/*.arg NPC dialog title expressions");
		}

		g_stServiceBasePath = prev_service_path;
		g_stQuestDir = prev_quest_dir;
		std::cout << "  --> [PASS] Real serverfiles locale/poland verified: settings.lua, BlueDragon.lua, questlib.lua + 36 libs/*, translate.lua, locale.lua, oxquiz.lua, "
		          << state_files_loaded << " state tables, " << chat_args_evaluated << " chat/*.arg titles, and " << obj_files_checked << " compiled object/ files (0 errors)" << std::endl;
	} else {
		std::cout << "  --> [INFO] Real serverfiles not unpacked in working path; mandatory synthetic fixtures verified" << std::endl;
	}
	checks_passed++;

	// 7. Gameforge Localization Proxy
	std::cout << "[RUN] Check 7: Gameforge translation proxy..." << std::endl;
	int gf_err = lua_dostring(L,
		"local function ensure_gf_proxy(path, tbl)\n"
		"    local t = tbl or {}\n"
		"    return setmetatable(t, {\n"
		"        __index = function(self, k)\n"
		"            local p = (path and path ~= '' and path ~= 'gameforge') and (path .. '.' .. tostring(k)) or tostring(k)\n"
		"            return ensure_gf_proxy(p)\n"
		"        end,\n"
		"        __concat = function(a, b) return tostring(a) .. tostring(b) end,\n"
		"        __tostring = function() return '[MISSING_LOCALE: ' .. tostring(path) .. ']' end\n"
		"    })\n"
		"end\n"
		"if not gameforge or type(gameforge) ~= 'table' then\n"
		"    gameforge = ensure_gf_proxy('gameforge')\n"
		"elseif not getmetatable(gameforge) then\n"
		"    ensure_gf_proxy('gameforge', gameforge)\n"
		"    if type(gameforge.locale) == 'table' and not getmetatable(gameforge.locale) then\n"
		"        ensure_gf_proxy('locale', gameforge.locale)\n"
		"    end\n"
		"end\n"
		"missing_str = tostring(gameforge.locale.unknown_test_key_xyz)\n"
	);
	RUNTIME_ASSERT(gf_err == 0, "Gameforge proxy code evaluation must succeed");
	lua_getglobal(L, "missing_str");
	RUNTIME_ASSERT(lua_isstring(L, -1), "missing_str must be a string");
	RUNTIME_ASSERT(strstr(lua_tostring(L, -1), "MISSING_LOCALE") != nullptr, "Missing keys must not be silently masked");
	lua_pop(L, 1);

	int audit_err = lua_dostring(L,
		"local missing_count = 0\n"
		"local real_str_count = 0\n"
		"local visited = {}\n"
		"local function scan_tbl(t, depth)\n"
		"    if depth > 6 or visited[t] then return end\n"
		"    visited[t] = true\n"
		"    for k, v in pairs(t) do\n"
		"        if type(v) == 'string' then\n"
		"            if string.find(v, 'MISSING_LOCALE', 1, true) then missing_count = missing_count + 1 else real_str_count = real_str_count + 1 end\n"
		"        elseif type(v) == 'table' then\n"
		"            if string.find(tostring(v), 'MISSING_LOCALE', 1, true) then missing_count = missing_count + 1 else scan_tbl(v, depth + 1) end\n"
		"        end\n"
		"    end\n"
		"end\n"
		"if type(locale) == 'table' then scan_tbl(locale, 1) end\n"
		"g_check7_missing = missing_count\n"
		"g_check7_real = real_str_count\n"
	);
	RUNTIME_ASSERT(audit_err == 0, "locale.* audit script must succeed");
	lua_getglobal(L, "g_check7_missing");
	int check7_missing = (int)lua_tonumber(L, -1);
	lua_pop(L, 1);
	lua_getglobal(L, "g_check7_real");
	int check7_real = (int)lua_tonumber(L, -1);
	lua_pop(L, 1);
	RUNTIME_ASSERT(check7_missing == 0, "locale.* must have 0 [MISSING_LOCALE] masked entries");
	std::cout << "  --> Gameforge proxy transparently resolves and flags missing keys (locale.* audit: real_strings=" << check7_real << ", missing=0)" << std::endl;
	checks_passed++;

	// 8. Interactive Coroutine Dialog State Machine (select, wait, input, confirm, select_item, normal completion)
	std::cout << "[RUN] Check 8: Interactive Coroutine Dialog State Machine..." << std::endl;
	const int EXPECT_YIELD = LUA_YIELD;

	// 8.1 select (must compile with luaL_loadbuffer onto coroutine stack, then execute via lua_resume)
	lua_State* co = lua_newthread(L);
	RUNTIME_ASSERT(co != nullptr, "Failed to create coroutine thread");
	const char* quest_script =
		"local choice = select('Accept Biologist Task', 'Refuse Task')\n"
		"return choice\n";
	int load_err = luaL_loadbuffer(co, quest_script, strlen(quest_script), "test_biologist");
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
	load_err = luaL_loadbuffer(co_wait, script_wait, strlen(script_wait), "test_wait");
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
	load_err = luaL_loadbuffer(co_input, script_input, strlen(script_input), "test_input");
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
	load_err = luaL_loadbuffer(co_confirm, script_confirm, strlen(script_confirm), "test_confirm");
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
	load_err = luaL_loadbuffer(co_item, script_item, strlen(script_item), "test_select_item");
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
	load_err = luaL_loadbuffer(co_norm, script_norm, strlen(script_norm), "test_normal");
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
	int err_load = luaL_loadbuffer(co_err, bad_script, strlen(bad_script), "bad_quest");
	RUNTIME_ASSERT(err_load == 0, "Loading bad_quest buffer must succeed");
	int err_res = lua_resume(co_err, 0);
	RUNTIME_ASSERT(err_res != 0 && err_res != LUA_YIELD, "Deliberate error must return non-zero error status");
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
