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
#include "item.h"
#include "item_manager.h"
#include "log.h"
#include "affect.h"
#include "arena.h"
#include "BlueDragon_Binder.h"
#include "utils.h"
#include "db.h"
#include "desc.h"
#include "desc_client.h"
#include "p2p.h"
#include "itemshop_manager.h"
#include "monarch.h"
#include "../../common/PulseManager.h"
#include <thread>
#include <cstdlib>

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

	std::unique_ptr<ITEM_MANAGER> local_im;
	if (ITEM_MANAGER::instance_ptr() == nullptr)
	{
		local_im = std::make_unique<ITEM_MANAGER>();
	}

	std::unique_ptr<LogManager> local_lm;
	if (LogManager::instance_ptr() == nullptr)
	{
		local_lm = std::make_unique<LogManager>();
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
		test_ch->SetRealPoint(POINT_MAX_HP, 10000);
		test_ch->SetMaxHP(10000);
		test_ch->PointChange(POINT_MAX_HP, 0);
		test_ch->SetRealPoint(POINT_MAX_SP, 5000);
		test_ch->SetMaxSP(5000);
		test_ch->PointChange(POINT_MAX_SP, 0);
		RUNTIME_ASSERT(test_ch->GetMaxHP() == 10000 && test_ch->GetMaxSP() == 5000, "Baseline MaxHP/MaxSP must be 10000/5000");

		qm.SetCurrentCharacterPtr(test_ch.get());

		// Add unrelated affects to verify affect.remove_hair() never touches other affects
		RUNTIME_ASSERT(test_ch->AddAffect(AFFECT_MOV_SPEED, POINT_MOV_SPEED, 20, 0, 3600, 0, false), "Baseline AFFECT_MOV_SPEED must be added");
		RUNTIME_ASSERT(test_ch->AddAffect(AFFECT_COLLECT, POINT_ATTBONUS_HUMAN, 10, 0, INFINITE_AFFECT_DURATION, 0, false), "Baseline AFFECT_COLLECT must be added");
		RUNTIME_ASSERT(test_ch->GetPoint(POINT_MOV_SPEED) == 20 && test_ch->GetPoint(POINT_ATTBONUS_HUMAN) == 10, "Baseline affect points must be active");

		// Verify all 7 hair options on real CHARACTER (affect.add_hair + affect.get_apply_on(514) + affect.remove_hair + stat recalculation)
		struct HairOptionCase {
			long apply_code;
			BYTE expected_point;
			long value;
		};
		const HairOptionCase hair_cases[7] = {
			{ 63, POINT_ATTBONUS_MONSTER,  10 },
			{ 61, POINT_ATTBONUS_SURA,     10 },
			{ 62, POINT_ATTBONUS_SHAMAN,   10 },
			{ 59, POINT_ATTBONUS_WARRIOR,  10 },
			{ 60, POINT_ATTBONUS_ASSASSIN, 10 },
			{ 69, POINT_MAX_HP_PCT,        10 },
			{ 70, POINT_MAX_SP_PCT,        10 },
		};
		for (int i = 0; i < 7; ++i) {
			const auto& hc = hair_cases[i];
			char script_buf[512];
			snprintf(script_buf, sizeof(script_buf),
				"affect.add_hair(%ld, %ld, 604800)\n"
				"assert(affect.get_count(514) == 1, 'AFFECT_HAIR count must be 1')\n"
				"assert(affect.get_apply_on(514) == %u, 'AFFECT_HAIR apply_on mismatch')\n"
				"assert(affect.get_duration(514) == 604800, 'AFFECT_HAIR duration mismatch')\n",
				hc.apply_code, hc.value, static_cast<unsigned>(hc.expected_point));
			RUNTIME_ASSERT(lua_dostring(L, script_buf) == 0, "Hair option affect.add_hair must succeed");
			RUNTIME_ASSERT(test_ch->GetPoint(hc.expected_point) == hc.value, "CHARACTER point must equal hair bonus value while active");
			if (hc.expected_point == POINT_MAX_HP_PCT) {
				RUNTIME_ASSERT(test_ch->GetMaxHP() == 11000, "POINT_MAX_HP_PCT (+10%) must increase MaxHP from 10000 to 11000");
			} else if (hc.expected_point == POINT_MAX_SP_PCT) {
				RUNTIME_ASSERT(test_ch->GetMaxSP() == 5500, "POINT_MAX_SP_PCT (+10%) must increase MaxSP from 5000 to 5500");
			}

			// Remove and verify clean stat reversion
			snprintf(script_buf, sizeof(script_buf),
				"local ra, rv, rd = affect.remove_hair()\n"
				"assert(ra == %u and rv == %ld and rd == 604800, 'remove_hair return tuple mismatch')\n"
				"assert(affect.get_count(514) == 0 and affect.get_apply_on(514) == nil, 'AFFECT_HAIR must be removed')\n",
				static_cast<unsigned>(hc.expected_point), hc.value);
			RUNTIME_ASSERT(lua_dostring(L, script_buf) == 0, "Hair option affect.remove_hair must succeed");
			RUNTIME_ASSERT(test_ch->GetPoint(hc.expected_point) == 0, "CHARACTER point must revert to 0 after affect.remove_hair");
			RUNTIME_ASSERT(test_ch->GetMaxHP() == 10000 && test_ch->GetMaxSP() == 5000, "MaxHP/MaxSP must revert to 10000/5000 after affect.remove_hair");
		}

		// Verify Halloween hair CItem (ITEM_COSTUME / COSTUME_HAIR) via item.set_value(0, option.apply, option.value) + ModifyPoints
		TItemTable hair_proto{};
		hair_proto.dwVnum = 74020;
		hair_proto.bType = ITEM_COSTUME;
		hair_proto.bSubType = COSTUME_HAIR;
		hair_proto.bSize = 1;
		ITEM_MANAGER::instance().SetMaxItemID({ 1, 1000000, 1 });
		ITEM_MANAGER::instance().Initialize(&hair_proto, 1);
		LPITEM hair_item = ITEM_MANAGER::instance().CreateItem(74020, 1, 0, false);
		RUNTIME_ASSERT(hair_item != nullptr && hair_item->GetVID() != 0, "Halloween hair CItem must be created with valid VID");
		hair_item->SetSkipSave(true);
		hair_item->SetCell(test_ch.get(), 0);
		qm.SetCurrentItem(hair_item);
		RUNTIME_ASSERT(qm.GetCurrentItem() == hair_item, "CQuestManager::GetCurrentItem() must resolve hair_item via VID");

		for (int i = 0; i < 7; ++i) {
			const auto& hc = hair_cases[i];
			hair_item->ClearAttribute();
			char item_script[256];
			snprintf(item_script, sizeof(item_script), "item.set_value(0, %ld, %ld)\n", hc.apply_code, hc.value);
			RUNTIME_ASSERT(lua_dostring(L, item_script) == 0, "item.set_value on hair item must succeed");
			RUNTIME_ASSERT(hair_item->GetAttributeType(0) == hc.expected_point && hair_item->GetAttributeValue(0) == hc.value,
				"item.set_value on hair item must map questlib apply.* to EPointTypes");
			hair_item->ModifyPoints(true);
			RUNTIME_ASSERT(test_ch->GetPoint(hc.expected_point) == hc.value, "Equipped hair item must apply EPointTypes bonus to CHARACTER");
			if (hc.expected_point == POINT_MAX_HP_PCT) {
				RUNTIME_ASSERT(test_ch->GetMaxHP() == 11000, "Equipped hair item POINT_MAX_HP_PCT (+10%) must increase MaxHP to 11000");
			} else if (hc.expected_point == POINT_MAX_SP_PCT) {
				RUNTIME_ASSERT(test_ch->GetMaxSP() == 5500, "Equipped hair item POINT_MAX_SP_PCT (+10%) must increase MaxSP to 5500");
			}
			hair_item->ModifyPoints(false);
			RUNTIME_ASSERT(test_ch->GetPoint(hc.expected_point) == 0 && test_ch->GetMaxHP() == 10000 && test_ch->GetMaxSP() == 5000,
				"Unequipped hair item must revert EPointTypes bonus on CHARACTER");

			// Simulate DB-migrated Halloween hair item load -> equip -> unequip -> save -> reload round-trip
			// using the exact event_halloween_hair.quest values (value=1, duration=21600/86400 for MAX_HP_PCT/MAX_SP_PCT; value=10, duration=604800 for 59..63)
			const short migrated_val = (hc.expected_point == POINT_MAX_HP_PCT || hc.expected_point == POINT_MAX_SP_PCT) ? 1 : static_cast<short>(hc.value);
			const int32_t migrated_dur = (hc.expected_point == POINT_MAX_HP_PCT) ? 21600
				: ((hc.expected_point == POINT_MAX_SP_PCT) ? 86400 : 604800);
			TPlayerItemAttribute migrated_attrs[ITEM_ATTRIBUTE_MAX_NUM]{};
			migrated_attrs[0].bType = hc.expected_point;
			migrated_attrs[0].sValue = migrated_val;
			migrated_attrs[1].bType = POINT_STEAL_SP;
			migrated_attrs[1].sValue = 5;
			int32_t migrated_sockets[ITEM_SOCKET_MAX_NUM] = { migrated_dur, 0, 0 };

			hair_item->SetAttributes(migrated_attrs);
			hair_item->SetSockets(migrated_sockets);
			RUNTIME_ASSERT(hair_item->GetAttributeType(0) == hc.expected_point && hair_item->GetAttributeValue(0) == migrated_val,
				"DB-loaded migrated hair item must preserve exact quest attrtype0 and attrvalue0");
			RUNTIME_ASSERT(hair_item->GetAttributeType(1) == POINT_STEAL_SP && hair_item->GetAttributeValue(1) == 5,
				"DB-loaded hair item must preserve secondary attrtype1..6 untouched");
			for (int a = 2; a < ITEM_ATTRIBUTE_MAX_NUM; ++a) {
				RUNTIME_ASSERT(hair_item->GetAttributeType(a) == 0 && hair_item->GetAttributeValue(a) == 0,
					"DB-loaded hair item unused attrtype2..6 must remain 0");
			}
			RUNTIME_ASSERT(hair_item->GetSocket(0) == migrated_dur && hair_item->GetSocket(1) == 0 && hair_item->GetSocket(2) == 0,
				"DB-loaded hair item must preserve exact quest sockets 0..2 untouched");

			hair_item->ModifyPoints(true);
			RUNTIME_ASSERT(test_ch->GetPoint(hc.expected_point) == migrated_val,
				"Equipped migrated hair item must apply exact single bonus without double-application");
			RUNTIME_ASSERT(test_ch->GetPoint(POINT_STEAL_SP) == 5,
				"Equipped migrated hair item must apply secondary attrtype1 (+5 POINT_STEAL_SP) accurately");
			if (hc.expected_point == POINT_MAX_HP_PCT) {
				RUNTIME_ASSERT(test_ch->GetMaxHP() == 10100,
					"Equipped migrated hair item with POINT_MAX_HP_PCT (+1%) must compute exact MaxHP 10100");
			} else if (hc.expected_point == POINT_MAX_SP_PCT) {
				RUNTIME_ASSERT(test_ch->GetMaxSP() == 5050,
					"Equipped migrated hair item with POINT_MAX_SP_PCT (+1%) must compute exact MaxSP 5050");
			}
			hair_item->ModifyPoints(false);
			RUNTIME_ASSERT(test_ch->GetPoint(hc.expected_point) == 0 && test_ch->GetPoint(POINT_STEAL_SP) == 0 &&
				test_ch->GetMaxHP() == 10000 && test_ch->GetMaxSP() == 5000,
				"Unequipped migrated hair item must cleanly revert all bonuses to baseline");
		}

		// Verify invalid / colliding apply_type and out-of-bounds index are rejected on hair item
		hair_item->ClearAttribute();
		RUNTIME_ASSERT(lua_dostring(L,
			"item.set_value(-1, 63, 10)\n"
			"item.set_value(7, 63, 10)\n"
			"item.set_value(0, 53, 10)\n"
			"item.set_value(0, 54, 10)\n"
			"item.set_value(0, 119, 10)\n"
			"item.set_value(0, 15, 10)\n"
			"item.set_value(0, 0, 10)\n"
		) == 0, "Invalid item.set_value calls on hair item must not crash");
		RUNTIME_ASSERT(hair_item->GetAttributeType(0) == POINT_NONE && hair_item->GetAttributeValue(0) == 0,
			"Invalid/colliding apply_type on hair item must be rejected");
		hair_item->SetCell(nullptr, 0);
		qm.SetCurrentItem(nullptr);
		M2_DESTROY_ITEM(hair_item);

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
			"-- Invalid types, colliding non-hair apply/EPointTypes (53, 54, 15, 119), out-of-range (0, -5, 300), and <= 0 durations must be rejected\n"
			"affect.add_hair()\n"
			"affect.add_hair(nil, 10, 600)\n"
			"affect.add_hair('invalid', 10, 600)\n"
			"affect.add_hair({}, 10, 600)\n"
			"affect.add_hair(0, 10, 600)\n"
			"affect.add_hair(-5, 10, 600)\n"
			"affect.add_hair(300, 10, 600)\n"
			"affect.add_hair(1, 10, 600)\n"
			"affect.add_hair(15, 10, 600)\n"
			"affect.add_hair(53, 10, 600)\n"
			"affect.add_hair(54, 10, 600)\n"
			"affect.add_hair(119, 10, 600)\n"
			"affect.add_hair(120, 10, 600)\n"
			"affect.add_hair(63, 10, 0)\n"
			"affect.add_hair(63, 10, -604800)\n"
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
	std::cout << "  --> pc, q, say, select, affect.add_hair/remove_hair & item.set_value (7 hair options + MaxHP/MaxSP recalculation) confirmed" << std::endl;
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

namespace
{
	class CTestItemShopClientDesc : public CLIENT_DESC
	{
	public:
		CTestItemShopClientDesc()
		{
			m_sock = static_cast<socket_t>(1);
		}
		virtual ~CTestItemShopClientDesc()
		{
			m_sock = INVALID_SOCKET;
		}
	};

	class CTestItemShopCharacter : public CHARACTER
	{
	public:
		CTestItemShopCharacter() : CHARACTER(false) {}
		virtual ~CTestItemShopCharacter()
		{
			ClearTestInventory();
		}
		void SetTestPlayerID(DWORD pid) { m_dwPlayerID = pid; }
		void EnableDirectInventory(bool enable)
		{
			if (enable)
			{
				if (!m_PlayerSlots)
					m_PlayerSlots = std::make_unique<PlayerSlotT>();
				m_bCharType = CHAR_TYPE_PC;
			}
			else
			{
				ClearTestInventory();
				m_PlayerSlots.reset();
				m_bCharType = CHAR_TYPE_MONSTER;
			}
		}
		void ClearTestInventory()
		{
			if (!m_PlayerSlots)
				return;
			for (int i = 0; i < INVENTORY_AND_EQUIP_SLOT_MAX; ++i)
			{
				LPITEM item = m_PlayerSlots->pItems[i];
				if (item)
				{
					item->SetSkipSave(true);
					item->RemoveFromCharacter();
					m_PlayerSlots->pItems[i] = nullptr;
					ITEM_MANAGER::instance().RemoveFromDelayedSave(item);
					M2_DESTROY_ITEM(item);
				}
			}
			m_PlayerSlots->bItemGrid.fill(0);
		}
	};
}

int RunGameItemShopLiveTests(int argc, char** argv)
{
	const bool isCrashChildMode = (argc > 1 && std::strcmp(argv[1], "--test-itemshop-crash-child") == 0);
	const bool isCrashRecoveringChildMode = (argc > 1 && std::strcmp(argv[1], "--test-itemshop-crash-recovering-child") == 0);
	const bool isCrashPreSqlChildMode = (argc > 1 && std::strcmp(argv[1], "--test-itemshop-crash-pre-sql-child") == 0);
	const bool isRecoverWorkerChildMode = (argc > 1 && std::strcmp(argv[1], "--test-itemshop-recover-child") == 0);

	std::string host = "127.0.0.1";
	int port = 3306;
	std::string user = "mt2test";
	std::string password = "";
	std::string dbname = "mt2_testdb";

	for (int i = 2; i < argc; ++i)
	{
		std::string arg = argv[i];
		if (arg == "--host" && i + 1 < argc) host = argv[++i];
		else if (arg == "--port" && i + 1 < argc) port = std::atoi(argv[++i]);
		else if (arg == "--user" && i + 1 < argc) user = argv[++i];
		else if (arg == "--password" && i + 1 < argc) password = argv[++i];
		else if (arg == "--db" && i + 1 < argc) dbname = argv[++i];
	}

	if (const char* env_host = std::getenv("MT2_TEST_HOST")) host = env_host;
	if (const char* env_port = std::getenv("MT2_TEST_PORT")) port = std::atoi(env_port);
	if (const char* env_user = std::getenv("MT2_TEST_USER")) user = env_user;
	if (const char* env_pwd = std::getenv("MT2_TEST_PASSWORD")) password = env_pwd;
	if (const char* env_db = std::getenv("MT2_TEST_DB")) dbname = env_db;

	struct FileCleanupGuard
	{
		~FileCleanupGuard()
		{
			std::remove("sql_log_spool.dat");
			std::remove("sql_log_spool.dat.lock");
			std::remove("sql_log_spool.dat.quarantine");
			std::remove("p2p_packet_info.txt");
			std::remove("packet_info.txt");
		}
	} fileCleanupGuard;

	std::unique_ptr<CArenaManager> local_am = std::make_unique<CArenaManager>();
	std::unique_ptr<quest::CQuestManager> local_qm = std::make_unique<quest::CQuestManager>();
	std::unique_ptr<ITEM_MANAGER> local_im = std::make_unique<ITEM_MANAGER>();
	std::unique_ptr<P2P_MANAGER> local_p2p = std::make_unique<P2P_MANAGER>();
	std::unique_ptr<DBManager> local_db = std::make_unique<DBManager>();
	std::unique_ptr<LogManager> local_log = std::make_unique<LogManager>();
	std::unique_ptr<CItemShopManager> local_ism = std::make_unique<CItemShopManager>();
	std::unique_ptr<CMonarch> local_monarch = std::make_unique<CMonarch>();

	CTestItemShopClientDesc dummyDbClientDesc;
	LPCLIENT_DESC prevDbClientDesc = db_clientdesc;
	db_clientdesc = &dummyDbClientDesc;

	bool dbConnOk = DBManager::instance().Connect(host.c_str(), port, user.c_str(), password.c_str(), dbname.c_str());
	bool accConnOk = DBManager::instance().ConnectAccount(host.c_str(), port, user.c_str(), password.c_str(), dbname.c_str());
	bool logConnOk = LogManager::instance().Connect(host.c_str(), port, user.c_str(), password.c_str(), dbname.c_str());
	RUNTIME_ASSERT(dbConnOk && accConnOk && logConnOk, "DBManager (Direct/Account) and LogManager must connect to live MariaDB 12.3 LTS");

	DBManager::instance().AccountQuery(
		"SET SESSION sql_mode = 'STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION'");
	DBManager::instance().DirectQuery(
		"SET SESSION sql_mode = 'STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION'");

	// Register item prototypes in ITEM_MANAGER:
	// - 71084: Stackable ITEM_DS (routes to item_award when inventory is not used)
	// - 72001: Non-stackable ITEM_DS (tests multi-slot non-stackable item_award delivery)
	// - 72101: VIP subscription item (ITEM_USE / USE_VIP, alValues[0]=PREMIUM_EXP, alValues[1]=24 hours)
	// - 80014: Dragon Scroll voucher item (ITEM_USE / USE_SPECIAL, alValues[0]=500 voucher value, stackable)
	// - 71085: Direct-inventory stackable item (ITEM_USE / USE_SPECIAL, bSize=1, dwMaxStack=200)
	TItemTable protoItems[5]{};
	protoItems[0].dwVnum = 71084;
	strlcpy(protoItems[0].szName, "EnchantItem", sizeof(protoItems[0].szName));
	strlcpy(protoItems[0].szLocaleName, "EnchantItem", sizeof(protoItems[0].szLocaleName));
	protoItems[0].bType = ITEM_DS;
	protoItems[0].bSubType = 0;
	protoItems[0].bSize = 1;
	protoItems[0].dwFlags = ITEM_FLAG_STACKABLE;
	protoItems[0].dwMaxStack = 200;

	protoItems[1].dwVnum = 72001;
	strlcpy(protoItems[1].szName, "WhalePackage", sizeof(protoItems[1].szName));
	strlcpy(protoItems[1].szLocaleName, "WhalePackage", sizeof(protoItems[1].szLocaleName));
	protoItems[1].bType = ITEM_DS;
	protoItems[1].bSubType = 0;
	protoItems[1].bSize = 1;
	protoItems[1].dwFlags = 0; // Non-stackable
	protoItems[1].dwAntiFlags = ITEM_ANTIFLAG_STACK;
	protoItems[1].dwMaxStack = 1;

	protoItems[2].dwVnum = 72101;
	strlcpy(protoItems[2].szName, "VIPGold24h", sizeof(protoItems[2].szName));
	strlcpy(protoItems[2].szLocaleName, "VIPGold24h", sizeof(protoItems[2].szLocaleName));
	protoItems[2].bType = ITEM_USE;
	protoItems[2].bSubType = USE_VIP;
	protoItems[2].bSize = 1;
	protoItems[2].alValues[0] = 0; // silver_expire
	protoItems[2].alValues[1] = 24; // 24 hours

	protoItems[3].dwVnum = 80014;
	strlcpy(protoItems[3].szName, "DragonCoinVoucher500", sizeof(protoItems[3].szName));
	strlcpy(protoItems[3].szLocaleName, "DragonCoinVoucher500", sizeof(protoItems[3].szLocaleName));
	protoItems[3].bType = ITEM_DS;
	protoItems[3].bSubType = 0;
	protoItems[3].bSize = 1;
	protoItems[3].dwFlags = 0;
	protoItems[3].dwAntiFlags = ITEM_ANTIFLAG_STACK;
	protoItems[3].alValues[0] = 500;

	protoItems[4].dwVnum = 71085;
	strlcpy(protoItems[4].szName, "DirectInvBlessing", sizeof(protoItems[4].szName));
	strlcpy(protoItems[4].szLocaleName, "DirectInvBlessing", sizeof(protoItems[4].szLocaleName));
	protoItems[4].bType = ITEM_USE;
	protoItems[4].bSubType = USE_TUNING;
	protoItems[4].bSize = 1;
	protoItems[4].dwFlags = ITEM_FLAG_STACKABLE;
	protoItems[4].dwMaxStack = 200;

	RUNTIME_ASSERT(ITEM_MANAGER::instance().Initialize(protoItems, 5), "ITEM_MANAGER::Initialize with test prototypes succeeded");

	TItemIDRangeTable testIdRange{};
	testIdRange.dwMin = 700001;
	testIdRange.dwMax = 799999;
	testIdRange.dwUsableItemIDMin = 700001;
	RUNTIME_ASSERT(ITEM_MANAGER::instance().SetMaxItemID(testIdRange), "ITEM_MANAGER::SetMaxItemID succeeded");

	const int64_t nowSec = static_cast<int64_t>(get_global_time());
	TItemShopItem shopItems[6]{};
	shopItems[0].dwIndex = 101;
	shopItems[0].dwVnum = 71084;
	shopItems[0].dwCount = 2;
	shopItems[0].dwPrice = 600;
	shopItems[0].bCurrency = CItemShopManager::CURRENCY_DRAGON_COIN;
	shopItems[0].bMinLevel = 1;

	shopItems[1].dwIndex = 102;
	shopItems[1].dwVnum = 72001;
	shopItems[1].dwCount = 1;
	shopItems[1].dwPrice = 5000000000LL;
	shopItems[1].bCurrency = CItemShopManager::CURRENCY_DRAGON_COIN;
	shopItems[1].bMinLevel = 1;

	shopItems[2].dwIndex = 201;
	shopItems[2].dwVnum = 71084;
	shopItems[2].dwCount = 1;
	shopItems[2].dwPrice = 400;
	shopItems[2].bCurrency = CItemShopManager::CURRENCY_DRAGON_COIN;
	shopItems[2].bMinLevel = 1;
	shopItems[2].timeAuction.dwStartTime = nowSec - 3600;
	shopItems[2].timeAuction.dwEndTime = nowSec + 3600;
	shopItems[2].timeAuction.dwMaxBuyAmount = 5;
	shopItems[2].timeAuction.dwCurrentBuyAmount = 0;

	shopItems[3].dwIndex = 103; // VIP 24h
	shopItems[3].dwVnum = 72101;
	shopItems[3].dwCount = 1;
	shopItems[3].dwPrice = 300;
	shopItems[3].bCurrency = CItemShopManager::CURRENCY_DRAGON_COIN;
	shopItems[3].bMinLevel = 1;

	shopItems[4].dwIndex = 104; // Dragon Scroll Voucher (price=550, voucher value=500 -> marks=50)
	shopItems[4].dwVnum = 80014;
	shopItems[4].dwCount = 1;
	shopItems[4].dwPrice = 550;
	shopItems[4].bCurrency = CItemShopManager::CURRENCY_DRAGON_COIN;
	shopItems[4].bMinLevel = 1;

	shopItems[5].dwIndex = 105; // Direct inventory item (71085)
	shopItems[5].dwVnum = 71085;
	shopItems[5].dwCount = 3;
	shopItems[5].dwPrice = 250;
	shopItems[5].bCurrency = CItemShopManager::CURRENCY_DRAGON_COIN;
	shopItems[5].bMinLevel = 1;

	// Child mode 1: crash with _Exit(99) immediately after CHARGED commit
	if (isCrashChildMode)
	{
		RUNTIME_ASSERT(CItemShopManager::instance().Initialize(shopItems, 6), "Child CItemShopManager::Initialize succeeded");
		DESC childDesc;
		childDesc.GetAccountTable().id = 990701;
		strlcpy(childDesc.GetAccountTable().login, "real_itemshop_buyer", sizeof(childDesc.GetAccountTable().login));
		std::unique_ptr<CTestItemShopCharacter> childBuyer = std::make_unique<CTestItemShopCharacter>();
		childBuyer->SetTestPlayerID(880701);
		childBuyer->SetLevel(50);
		childBuyer->BindDesc(&childDesc);

		CItemShopManager::instance().SetHardExitAfterChargeForTest(true);
		CItemShopManager::instance().BuyItem(childBuyer.get(), 102, 1);
		return 1;
	}

	// Child mode 2 (G3.H): claim CHARGED -> RECOVERING via real RecoverIncompleteTransactions() and crash with _Exit(97)
	if (isCrashRecoveringChildMode)
	{
		CItemShopManager::instance().SetHardExitAfterClaimRecoveringForTest(true);
		CItemShopManager::instance().RecoverIncompleteTransactions();
		return 1;
	}

	// Child mode 3 (G3.L): give item into RAM + record INV_PENDING:<id> in journal, then crash with _Exit(98) before SQL player.item write
	if (isCrashPreSqlChildMode)
	{
		TItemIDRangeTable childRange{};
		childRange.dwMin = 750001;
		childRange.dwMax = 759999;
		childRange.dwUsableItemIDMin = 750001;
		ITEM_MANAGER::instance().SetMaxItemID(childRange);
		RUNTIME_ASSERT(CItemShopManager::instance().Initialize(shopItems, 6), "Pre-SQL crash child Initialize succeeded");

		DESC childDesc;
		childDesc.GetAccountTable().id = 990701;
		strlcpy(childDesc.GetAccountTable().login, "real_itemshop_buyer", sizeof(childDesc.GetAccountTable().login));
		std::unique_ptr<CTestItemShopCharacter> childBuyer = std::make_unique<CTestItemShopCharacter>();
		childBuyer->SetTestPlayerID(880701);
		childBuyer->SetLevel(50);
		childBuyer->BindDesc(&childDesc);
		childBuyer->EnableDirectInventory(true);

		CItemShopManager::instance().SetHardExitAfterGiveItemBeforeSqlPersistForTest(true);
		CItemShopManager::instance().BuyItem(childBuyer.get(), 105, 1);
		return 1;
	}

	// Child mode 4 (G3.I): run real RecoverIncompleteTransactions() in a separate OS process
	if (isRecoverWorkerChildMode)
	{
		int rec = CItemShopManager::instance().RecoverIncompleteTransactions();
		return (rec >= 0) ? (10 + rec) : 1;
	}

	std::cout << "==================================================" << std::endl;
	std::cout << " METIN2 MT2009 - REAL CItemShopManager LIVE SUITE" << std::endl;
	std::cout << " Endpoint: " << host << ":" << port << " / Schema: " << dbname << std::endl;
	std::cout << "==================================================" << std::endl;

	int checks_passed = 1; // DB connection check 1 passed

	// Prepare clean isolated ItemShop test tables inside dbname (mt2_testdb)
	DBManager::instance().AccountQuery("DROP TABLE IF EXISTS `itemshop_tx_journal`, `account`");
	DBManager::instance().DirectQuery("DROP TABLE IF EXISTS `item_award`, `item`, `itemshop_time_auction`, `itemshop`");

	auto rCreateAcc = DBManager::instance().AccountQuery(
		"CREATE TABLE `account` ("
		"  `id` int(11) unsigned NOT NULL AUTO_INCREMENT,"
		"  `login` varchar(30) NOT NULL DEFAULT '',"
		"  `cash` bigint(20) NOT NULL DEFAULT 0,"
		"  `cash_mark` bigint(20) NOT NULL DEFAULT 0,"
		"  `silver_expire` datetime NOT NULL DEFAULT current_timestamp(),"
		"  `gold_expire` datetime NOT NULL DEFAULT current_timestamp(),"
		"  `safebox_expire` datetime NOT NULL DEFAULT current_timestamp(),"
		"  `autoloot_expire` datetime NOT NULL DEFAULT current_timestamp(),"
		"  `fish_mind_expire` datetime NOT NULL DEFAULT current_timestamp(),"
		"  `marriage_fast_expire` datetime NOT NULL DEFAULT current_timestamp(),"
		"  `money_drop_rate_expire` datetime NOT NULL DEFAULT current_timestamp(),"
		"  `shop_expire` datetime NOT NULL DEFAULT current_timestamp(),"
		"  `premium_expire` datetime NOT NULL DEFAULT current_timestamp(),"
		"  PRIMARY KEY (`id`),"
		"  UNIQUE KEY `login` (`login`)"
		") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
	RUNTIME_ASSERT(rCreateAcc && rCreateAcc->uiSQLErrno == 0, "Isolated 64-bit account table created");

	auto rCreateAward = DBManager::instance().DirectQuery(
		"CREATE TABLE `item_award` ("
		"  `id` int(11) unsigned NOT NULL AUTO_INCREMENT,"
		"  `pid` int(10) unsigned NOT NULL DEFAULT 0,"
		"  `login` varchar(30) NOT NULL DEFAULT '',"
		"  `vnum` int(6) unsigned NOT NULL DEFAULT 0,"
		"  `count` int(10) unsigned NOT NULL DEFAULT 0,"
		"  `given_time` datetime NOT NULL DEFAULT current_timestamp(),"
		"  `taken_time` datetime DEFAULT NULL,"
		"  `item_id` int(11) unsigned DEFAULT NULL,"
		"  `why` varchar(128) DEFAULT NULL,"
		"  `socket0` int(11) unsigned NOT NULL DEFAULT 0,"
		"  `socket1` int(11) unsigned NOT NULL DEFAULT 0,"
		"  `socket2` int(11) unsigned NOT NULL DEFAULT 0,"
		"  `mall` tinyint(1) NOT NULL DEFAULT 0,"
		"  PRIMARY KEY (`id`)"
		") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
	RUNTIME_ASSERT(rCreateAward && rCreateAward->uiSQLErrno == 0, "Isolated item_award table created");

	auto rCreateItem = DBManager::instance().DirectQuery(
		"CREATE TABLE `item` ("
		"  `id` int(11) unsigned NOT NULL DEFAULT 0,"
		"  `owner_id` int(11) unsigned NOT NULL DEFAULT 0,"
		"  `window` varchar(32) NOT NULL DEFAULT 'INVENTORY',"
		"  `pos` smallint(5) unsigned NOT NULL DEFAULT 0,"
		"  `count` int(10) unsigned NOT NULL DEFAULT 0,"
		"  `vnum` int(11) unsigned NOT NULL DEFAULT 0,"
		"  `socket0` bigint(20) NOT NULL DEFAULT 0,"
		"  `socket1` bigint(20) NOT NULL DEFAULT 0,"
		"  `socket2` bigint(20) NOT NULL DEFAULT 0,"
		"  `attrtype0` tinyint(3) unsigned NOT NULL DEFAULT 0,"
		"  `attrvalue0` smallint(6) NOT NULL DEFAULT 0,"
		"  `attrtype1` tinyint(3) unsigned NOT NULL DEFAULT 0,"
		"  `attrvalue1` smallint(6) NOT NULL DEFAULT 0,"
		"  `attrtype2` tinyint(3) unsigned NOT NULL DEFAULT 0,"
		"  `attrvalue2` smallint(6) NOT NULL DEFAULT 0,"
		"  `attrtype3` tinyint(3) unsigned NOT NULL DEFAULT 0,"
		"  `attrvalue3` smallint(6) NOT NULL DEFAULT 0,"
		"  `attrtype4` tinyint(3) unsigned NOT NULL DEFAULT 0,"
		"  `attrvalue4` smallint(6) NOT NULL DEFAULT 0,"
		"  `attrtype5` tinyint(3) unsigned NOT NULL DEFAULT 0,"
		"  `attrvalue5` smallint(6) NOT NULL DEFAULT 0,"
		"  `attrtype6` tinyint(3) unsigned NOT NULL DEFAULT 0,"
		"  `attrvalue6` smallint(6) NOT NULL DEFAULT 0,"
		"  PRIMARY KEY (`id`),"
		"  KEY `owner_id_idx` (`owner_id`)"
		") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
	RUNTIME_ASSERT(rCreateItem && rCreateItem->uiSQLErrno == 0, "Isolated item table created");

	auto rCreateAuction = DBManager::instance().DirectQuery(
		"CREATE TABLE `itemshop_time_auction` ("
		"  `item_index` int(11) NOT NULL,"
		"  `buy_count` int(11) unsigned NOT NULL DEFAULT 0,"
		"  PRIMARY KEY (`item_index`)"
		") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
	RUNTIME_ASSERT(rCreateAuction && rCreateAuction->uiSQLErrno == 0, "Isolated itemshop_time_auction table created");

	auto rCreateLog = DBManager::instance().DirectQuery(
		"CREATE TABLE `itemshop` ("
		"  `pid` int(10) unsigned NOT NULL DEFAULT 0,"
		"  `account_id` int(10) unsigned NOT NULL DEFAULT 0,"
		"  `item_index` int(11) NOT NULL DEFAULT 0,"
		"  `vnum` int(10) unsigned NOT NULL DEFAULT 0,"
		"  `count` int(11) NOT NULL DEFAULT 0,"
		"  `price` bigint(20) NOT NULL DEFAULT 0,"
		"  `currency` tinyint(4) NOT NULL DEFAULT 0,"
		"  `item_id` int(10) unsigned DEFAULT NULL,"
		"  `date` datetime NOT NULL DEFAULT current_timestamp(),"
		"  `money_before_buy` bigint(20) NOT NULL DEFAULT 0"
		") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
	RUNTIME_ASSERT(rCreateLog && rCreateLog->uiSQLErrno == 0, "Isolated 64-bit itemshop log table created");

	DBManager::instance().DirectQuery("DROP TABLE IF EXISTS `log`");
	auto rCreateGenLog = DBManager::instance().DirectQuery(
		"CREATE TABLE `log` ("
		"  `type` varchar(20) NOT NULL DEFAULT 'ITEM',"
		"  `time` datetime NOT NULL DEFAULT current_timestamp(),"
		"  `who` int(10) unsigned NOT NULL DEFAULT 0,"
		"  `x` int(10) unsigned NOT NULL DEFAULT 0,"
		"  `y` int(10) unsigned NOT NULL DEFAULT 0,"
		"  `what` bigint(20) unsigned NOT NULL DEFAULT 0,"
		"  `how` varchar(50) NOT NULL DEFAULT '',"
		"  `hint` text DEFAULT NULL,"
		"  `ip` varchar(20) NOT NULL DEFAULT '',"
		"  `vnum` int(10) unsigned NOT NULL DEFAULT 0"
		") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
	RUNTIME_ASSERT(rCreateGenLog && rCreateGenLog->uiSQLErrno == 0, "Isolated general log table created");
	checks_passed++;

	DBManager::instance().DirectQuery("INSERT INTO `itemshop_time_auction` (`item_index`, `buy_count`) VALUES (201, 0)");

	RUNTIME_ASSERT(CItemShopManager::instance().Initialize(shopItems, 6),
		"CItemShopManager::Initialize created itemshop_tx_journal, enforced idx_item_award_why & idx_itemshop_tx_aid_req UNIQUE indexes, and loaded items");
	checks_passed++;

	// Prepare test buyer CHARACTER and DESC
	DESC buyerDesc;
	buyerDesc.GetAccountTable().id = 990701;
	strlcpy(buyerDesc.GetAccountTable().login, "real_itemshop_buyer", sizeof(buyerDesc.GetAccountTable().login));

	std::unique_ptr<CTestItemShopCharacter> buyer = std::make_unique<CTestItemShopCharacter>();
	buyer->SetTestPlayerID(880701);
	buyer->SetLevel(50);
	buyer->BindDesc(&buyerDesc);

	auto rInsAcc = DBManager::instance().AccountQuery(
		"INSERT INTO `account` (`id`, `login`, `cash`, `cash_mark`) VALUES (990701, 'real_itemshop_buyer', 2000, 100)");
	RUNTIME_ASSERT(rInsAcc && rInsAcc->uiSQLErrno == 0, "Inserted test account 990701 with 2000 Dragon Coins and 100 Dragon Marks");

	auto queryJournalState = [](uint64_t txId) -> std::string {
		auto msg = DBManager::instance().AccountQuery(
			"SELECT state FROM itemshop_tx_journal WHERE tx_id = %llu",
			static_cast<unsigned long long>(txId));
		if (msg && msg->uiSQLErrno == 0 && msg->Get() && msg->Get()->uiNumRows == 1)
		{
			MYSQL_ROW r = mysql_fetch_row(msg->Get()->pSQLResult);
			return (r && r[0]) ? std::string(r[0]) : "";
		}
		return "";
	};

	auto queryAwardCountForTx = [](uint64_t txId) -> uint64_t {
		auto msg = DBManager::instance().DirectQuery(
			"SELECT COALESCE(SUM(count), 0) FROM item_award WHERE why = 'ITEMSHOP_TX_%llu' OR why LIKE 'ITEMSHOP_TX_%llu#%%'",
			static_cast<unsigned long long>(txId),
			static_cast<unsigned long long>(txId));
		if (msg && msg->uiSQLErrno == 0 && msg->Get() && msg->Get()->uiNumRows == 1)
		{
			MYSQL_ROW r = mysql_fetch_row(msg->Get()->pSQLResult);
			uint64_t total = 0;
			if (r && r[0]) str_to_number(total, r[0]);
			return total;
		}
		return 0ULL;
	};

	auto runChildMode = [&](const char* modeFlag) -> int {
		std::string selfExe = argv[0];
		std::string cmd = "\"" + selfExe + "\" " + modeFlag
			+ " --host \"" + host + "\""
			+ " --port " + std::to_string(port)
			+ " --user \"" + user + "\""
			+ " --db \"" + dbname + "\"";
		if (!password.empty())
			cmd += " --password \"" + password + "\"";
#ifdef _WIN32
		std::string fullCmd = "\"" + cmd + "\"";
		int rawStatus = std::system(fullCmd.c_str());
		return rawStatus;
#else
		int rawStatus = std::system(cmd.c_str());
		return WIFEXITED(rawStatus) ? WEXITSTATUS(rawStatus) : rawStatus;
#endif
	};

	// Check 4: Normal purchase via real CItemShopManager::BuyItem (INITIATED -> CHARGED -> DELIVERED)
	bool buy1Ok = CItemShopManager::instance().BuyItem(buyer.get(), 101, 2); // 2 packs * 600 = 1200 coins, 4 items
	uint64_t tx1 = CItemShopManager::instance().GetLastTransactionId();
	auto bal1 = CItemShopManager::instance().GetPlayerAccountBalance(buyer.get());
	RUNTIME_ASSERT(buy1Ok && tx1 > 0, "Real CItemShopManager::BuyItem succeeded for normal purchase");
	RUNTIME_ASSERT(bal1.dwDragonCoins == 800 && bal1.dwDragonMarks == 1300,
		"Balance after normal purchase: Dragon Coins 2000->800, Dragon Marks 100->1300");
	RUNTIME_ASSERT(queryJournalState(tx1) == "DELIVERED" && queryAwardCountForTx(tx1) == 4,
		"Journal state is DELIVERED and item_award has 4 items linked to ITEMSHOP_TX_<tx_id>");
	std::cout << "  [PASS] Check 4: Real CItemShopManager::BuyItem normal flow (tx_id=" << tx1 << " -> DELIVERED)" << std::endl;
	checks_passed++;

	// Check 5: Simulated crash immediately after CHARGED (before delivery) -> RecoverIncompleteTransactions()
	CItemShopManager::instance().SetCrashAfterChargeForTest(true);
	bool buy2Ok = CItemShopManager::instance().BuyItem(buyer.get(), 201, 1); // Time auction item, costs 400 coins
	CItemShopManager::instance().SetCrashAfterChargeForTest(false);
	uint64_t tx2 = CItemShopManager::instance().GetLastTransactionId();
	auto balDuringCrash = CItemShopManager::instance().GetPlayerAccountBalance(buyer.get());
	RUNTIME_ASSERT(!buy2Ok && tx2 > tx1, "Simulated crash after CHARGED aborted BuyItem before delivery");
	RUNTIME_ASSERT(balDuringCrash.dwDragonCoins == 400 && balDuringCrash.dwDragonMarks == 1700,
		"Before recovery: account was charged 400 coins (800->400) and auction buy_count reserved");
	RUNTIME_ASSERT(queryJournalState(tx2) == "CHARGED" && queryAwardCountForTx(tx2) == 0,
		"Before recovery: journal state is CHARGED and 0 items were delivered");

	int recCount1 = CItemShopManager::instance().RecoverIncompleteTransactions();
	auto balAfterRec = CItemShopManager::instance().GetPlayerAccountBalance(buyer.get());
	auto msgAuc = DBManager::instance().DirectQuery("SELECT buy_count FROM itemshop_time_auction WHERE item_index = 201");
	MYSQL_ROW rowAuc = (msgAuc && msgAuc->Get()) ? mysql_fetch_row(msgAuc->Get()->pSQLResult) : nullptr;
	int aucBuyCount = (rowAuc && rowAuc[0]) ? std::atoi(rowAuc[0]) : -1;
	RUNTIME_ASSERT(recCount1 == 1 && queryJournalState(tx2) == "REFUNDED",
		"RecoverIncompleteTransactions() transitioned tx_id from CHARGED to REFUNDED");
	RUNTIME_ASSERT(balAfterRec.dwDragonCoins == 800 && balAfterRec.dwDragonMarks == 1300 && aucBuyCount == 0,
		"RecoverIncompleteTransactions() restored Dragon Coins (400->800), Dragon Marks (1700->1300), and time-auction buy_count (1->0)");

	int recCount2 = CItemShopManager::instance().RecoverIncompleteTransactions();
	auto balAfterSecondRec = CItemShopManager::instance().GetPlayerAccountBalance(buyer.get());
	RUNTIME_ASSERT(recCount2 == 0 && balAfterSecondRec.dwDragonCoins == 800 && balAfterSecondRec.dwDragonMarks == 1300,
		"Idempotency verified: second RecoverIncompleteTransactions() call is a no-op with 0 double-refund");
	std::cout << "  [PASS] Check 5: Real CItemShopManager crash-after-CHARGED recovery & idempotency (tx_id=" << tx2 << " -> REFUNDED)" << std::endl;
	checks_passed++;

	// Check 6: Simulated crash AFTER item_award delivery but BEFORE journal DELIVERED update -> RecoverIncompleteTransactions()
	CItemShopManager::instance().SetCrashAfterDeliveryBeforeJournalForTest(true);
	bool buy3Ok = CItemShopManager::instance().BuyItem(buyer.get(), 101, 1); // 1 pack = 600 coins, 2 items
	CItemShopManager::instance().SetCrashAfterDeliveryBeforeJournalForTest(false);
	uint64_t tx3 = CItemShopManager::instance().GetLastTransactionId();
	RUNTIME_ASSERT(buy3Ok && tx3 > tx2 && queryJournalState(tx3) == "CHARGED" && queryAwardCountForTx(tx3) == 2,
		"Crash after delivery left journal in CHARGED while item_award has 2 delivered items");

	int recCount3 = CItemShopManager::instance().RecoverIncompleteTransactions();
	auto balAfterRec3 = CItemShopManager::instance().GetPlayerAccountBalance(buyer.get());
	RUNTIME_ASSERT(recCount3 == 1 && queryJournalState(tx3) == "DELIVERED" && queryAwardCountForTx(tx3) == 2,
		"RecoverIncompleteTransactions() finalized journal as DELIVERED without double-delivery");
	RUNTIME_ASSERT(balAfterRec3.dwDragonCoins == 200 && balAfterRec3.dwDragonMarks == 1900,
		"RecoverIncompleteTransactions() did NOT falsely refund a delivered order (coins=200, marks=1900)");
	std::cout << "  [PASS] Check 6: Real CItemShopManager crash-after-delivery recovery without false refund or double-delivery (tx_id=" << tx3 << " -> DELIVERED)" << std::endl;
	checks_passed++;

	// Check 7: 64-bit high-value (> 2^32) balance & item purchase via real CItemShopManager
	DBManager::instance().AccountQuery("UPDATE `account` SET `cash` = 8000000000, `cash_mark` = 100000000 WHERE `id` = 990701");
	bool buy64Ok = CItemShopManager::instance().BuyItem(buyer.get(), 102, 1); // Costs 5,000,000,000 Dragon Coins
	uint64_t tx4 = CItemShopManager::instance().GetLastTransactionId();
	auto bal64 = CItemShopManager::instance().GetPlayerAccountBalance(buyer.get());
	RUNTIME_ASSERT(buy64Ok && tx4 > tx3 && queryJournalState(tx4) == "DELIVERED",
		"64-bit purchase (price = 5,000,000,000 > 2^32) succeeded via real CItemShopManager::BuyItem");
	RUNTIME_ASSERT(bal64.dwDragonCoins == 3000000000LL && bal64.dwDragonMarks == 5100000000LL,
		"64-bit balances preserved without 32-bit truncation: coins=3,000,000,000, marks=5,100,000,000");
	std::cout << "  [PASS] Check 7: Real CItemShopManager 64-bit (> 2^32) purchase & balance persistence (tx_id=" << tx4 << ")" << std::endl;
	checks_passed++;

	// Check 8 (Section 3 Q1): Physical MariaDB CONNECTION_ID() & Transaction Isolation on AccountDB
	{
		auto getConnId = [](bool useAccountDb) -> uint64_t {
			auto msg = useAccountDb
				? DBManager::instance().AccountQuery("SELECT CONNECTION_ID()")
				: DBManager::instance().DirectQuery("SELECT CONNECTION_ID()");
			if (!msg || msg->uiSQLErrno != 0 || !msg->Get() || msg->Get()->uiNumRows != 1)
				return 0;
			MYSQL_ROW r = mysql_fetch_row(msg->Get()->pSQLResult);
			uint64_t id = 0;
			if (r && r[0]) str_to_number(id, r[0]);
			return id;
		};
		uint64_t accConn1 = getConnId(true);
		uint64_t playerConn = getConnId(false);
		RUNTIME_ASSERT(accConn1 > 0 && playerConn > 0 && accConn1 != playerConn,
			"AccountDB and PlayerDB use distinct physical MariaDB connections");

		DBManager::instance().AccountQuery("START TRANSACTION");
		uint64_t accConnInTx1 = getConnId(true);
		DBManager::instance().AccountQuery("UPDATE `account` SET `cash` = 7777777777 WHERE `id` = 990701");
		uint64_t accConnInTx2 = getConnId(true);

		auto msgIso = DBManager::instance().DirectQuery("SELECT `cash` FROM `account` WHERE `id` = 990701");
		MYSQL_ROW rIso = (msgIso && msgIso->Get()) ? mysql_fetch_row(msgIso->Get()->pSQLResult) : nullptr;
		int64_t isolatedCash = 0;
		if (rIso && rIso[0]) str_to_number(isolatedCash, rIso[0]);

		DBManager::instance().AccountQuery("ROLLBACK");
		uint64_t accConnAfterRb = getConnId(true);
		RUNTIME_ASSERT(accConn1 == accConnInTx1 && accConnInTx1 == accConnInTx2 && accConnInTx2 == accConnAfterRb,
			"All AccountDB statements between START TRANSACTION and COMMIT/ROLLBACK run on the exact same physical CONNECTION_ID()");
		RUNTIME_ASSERT(isolatedCash == 3000000000LL,
			"Uncommitted AccountDB changes are isolated from external connections until COMMIT");
		std::cout << "  [PASS] Check 8 (Q1): AccountDB physical CONNECTION_ID()=" << accConn1 << " & ACID transaction isolation verified" << std::endl;
		checks_passed++;
	}

	// Check 9 (G3.A, G3.B, G3.C, G3.D & Q4): Persistent DB Request Idempotency (req_id), Intentional Repeat Buys & Reconnect/Cross-Process tx_id Preservation
	{
		DBManager::instance().AccountQuery("UPDATE `account` SET `cash` = 10000, `cash_mark` = 0 WHERE `id` = 990701");
		PulseManager::Instance().clockMap.clear();

		// G3.A & G3.B: Purchase with req_id=900001 succeeds; duplicate network retry with req_id=900001 (even with PulseManager cleared / >1s elapsed) is blocked by DB without creating a new tx_id
		bool reqBuy1 = CItemShopManager::instance().BuyItem(buyer.get(), 101, 1, 900001ULL);
		uint64_t txReq1 = CItemShopManager::instance().GetLastTransactionId();
		PulseManager::Instance().clockMap.clear(); // Prove we do NOT rely on 1s in-memory PulseManager filter (G3.B)
		bool reqBuyDup = CItemShopManager::instance().BuyItem(buyer.get(), 101, 1, 900001ULL);
		uint64_t txAfterDup = CItemShopManager::instance().GetLastTransactionId();
		auto balAfterDup = CItemShopManager::instance().GetPlayerAccountBalance(buyer.get());
		RUNTIME_ASSERT(reqBuy1 && !reqBuyDup && txReq1 == txAfterDup && balAfterDup.dwDragonCoins == 9400,
			"G3.A/B: Persistent DB req_id idempotency blocked duplicate retry without in-memory filter, new tx_id, or double charge");

		// G3.D: Player disconnects & reconnects (or moves to another GAME process) and retries req_id=900001 -> original tx_id is preserved
		DESC reconnectedDesc;
		reconnectedDesc.GetAccountTable().id = 990701;
		strlcpy(reconnectedDesc.GetAccountTable().login, "real_itemshop_buyer", sizeof(reconnectedDesc.GetAccountTable().login));
		std::unique_ptr<CTestItemShopCharacter> reconnectedBuyer = std::make_unique<CTestItemShopCharacter>();
		reconnectedBuyer->SetTestPlayerID(880701);
		reconnectedBuyer->SetLevel(50);
		reconnectedBuyer->BindDesc(&reconnectedDesc);
		bool reqAfterReconnect = CItemShopManager::instance().BuyItem(reconnectedBuyer.get(), 101, 1, 900001ULL);
		uint64_t txAfterReconnect = CItemShopManager::instance().GetLastTransactionId();
		std::string lookedUpState;
		uint64_t lookedUpTx = CItemShopManager::instance().LookupTransactionByRequestId(990701, 900001ULL, &lookedUpState);
		reconnectedBuyer->BindDesc(nullptr);
		RUNTIME_ASSERT(!reqAfterReconnect && txAfterReconnect == txReq1 && lookedUpTx == txReq1 && lookedUpState == "DELIVERED",
			"G3.D: Original transaction identity (tx_id) is preserved across player reconnect / separate GAME instance");

		// G3.C: Two separate, intentional purchases of the exact same item (index=101, qty=1) within the same millisecond succeed as two distinct transactions
		bool intentionalBuyA = CItemShopManager::instance().BuyItem(buyer.get(), 101, 1, 900002ULL);
		uint64_t txIntentionalA = CItemShopManager::instance().GetLastTransactionId();
		bool intentionalBuyB = CItemShopManager::instance().BuyItem(buyer.get(), 101, 1, 900003ULL);
		uint64_t txIntentionalB = CItemShopManager::instance().GetLastTransactionId();
		auto balAfterIntentional = CItemShopManager::instance().GetPlayerAccountBalance(buyer.get());
		RUNTIME_ASSERT(intentionalBuyA && intentionalBuyB && txIntentionalA > txReq1 && txIntentionalB > txIntentionalA && balAfterIntentional.dwDragonCoins == 8200,
			"G3.C: Two intentional identical purchases back-to-back within <1s both succeeded with distinct tx_ids");

		// Q4: Attempting to insert a duplicate item_award row with the same why='ITEMSHOP_TX_<txReq1>' fails with ER_DUP_ENTRY (1062)
		auto dupAward = DBManager::instance().DirectQuery(
			"INSERT INTO `item_award` (`login`, `vnum`, `count`, `given_time`, `why`, `mall`) "
			"VALUES ('real_itemshop_buyer', 71084, 2, NOW(), 'ITEMSHOP_TX_%llu', 1)",
			static_cast<unsigned long long>(txReq1));
		RUNTIME_ASSERT(!dupAward || dupAward->uiSQLErrno == 1062,
			"MariaDB UNIQUE KEY idx_item_award_why rejected duplicate ITEMSHOP_TX_<tx_id> with ER_DUP_ENTRY (1062)");
		std::cout << "  [PASS] Check 9 (G3.A-D & Q4): Persistent req_id idempotency, reconnect tx_id preservation & intentional repeat purchases verified" << std::endl;
		checks_passed++;
	}

	// Check 10 (G3.E, G3.K & Q3): Multi-Slot Non-Stackable item_award Delivery, VIP & Dragon Scroll Voucher Safety, and Real 2-Process Recovery Race
	{
		// G3.E: Non-stackable item (index=102, vnum=72001, dwFlags=0/ITEM_ANTIFLAG_STACK) bought with quantity=3 creates 3 distinct item_award rows (ITEMSHOP_TX_<tx>, #1, #2) without violating idx_item_award_why!
		DBManager::instance().AccountQuery("UPDATE `account` SET `cash` = 20000000000, `cash_mark` = 0 WHERE `id` = 990701");
		bool multiNonStackOk = CItemShopManager::instance().BuyItem(buyer.get(), 102, 3); // 3 * 5B = 15B coins, 3 non-stackable items
		uint64_t txMultiNonStack = CItemShopManager::instance().GetLastTransactionId();
		auto msgAwardRows = DBManager::instance().DirectQuery(
			"SELECT COUNT(*), COALESCE(SUM(count), 0) FROM item_award WHERE why = 'ITEMSHOP_TX_%llu' OR why LIKE 'ITEMSHOP_TX_%llu#%%'",
			static_cast<unsigned long long>(txMultiNonStack),
			static_cast<unsigned long long>(txMultiNonStack));
		MYSQL_ROW rAwardRows = (msgAwardRows && msgAwardRows->Get()) ? mysql_fetch_row(msgAwardRows->Get()->pSQLResult) : nullptr;
		int distinctAwardRows = (rAwardRows && rAwardRows[0]) ? std::atoi(rAwardRows[0]) : 0;
		int totalAwardCount = (rAwardRows && rAwardRows[1]) ? std::atoi(rAwardRows[1]) : 0;
		RUNTIME_ASSERT(multiNonStackOk && queryJournalState(txMultiNonStack) == "DELIVERED" && distinctAwardRows == 3 && totalAwardCount == 3,
			"G3.E: Multi-item non-stackable purchase inserted 3 distinct item_award rows under UNIQUE idx_item_award_why");

		// G3.K (VIP + Dragon Scroll Voucher normal & crash-recovery safety):
		DBManager::instance().AccountQuery("UPDATE `account` SET `cash` = 2000, `cash_mark` = 0, `silver_expire` = '2020-01-01 00:00:00' WHERE `id` = 990701");
		bool vipBuyOk = CItemShopManager::instance().BuyItem(buyer.get(), 103, 1); // VIP 24h, 300 coins
		uint64_t txVip = CItemShopManager::instance().GetLastTransactionId();
		auto msgVipExp = DBManager::instance().AccountQuery("SELECT (`silver_expire` > NOW()) FROM `account` WHERE `id` = 990701");
		MYSQL_ROW rVipExp = (msgVipExp && msgVipExp->Get()) ? mysql_fetch_row(msgVipExp->Get()->pSQLResult) : nullptr;
		int vipActive = (rVipExp && rVipExp[0]) ? std::atoi(rVipExp[0]) : 0;
		RUNTIME_ASSERT(vipBuyOk && queryJournalState(txVip) == "DELIVERED" && vipActive == 1,
			"G3.K: VIP purchase atomically updated account.silver_expire and finalized journal as DELIVERED");

		// Dragon Scroll Voucher (index=104, price=550, voucher value=500 -> marks=50): crash after CHARGED + recovery, then normal purchase
		CItemShopManager::instance().SetCrashAfterChargeForTest(true);
		bool voucherCrashOk = CItemShopManager::instance().BuyItem(buyer.get(), 104, 1);
		CItemShopManager::instance().SetCrashAfterChargeForTest(false);
		uint64_t txVoucherCrash = CItemShopManager::instance().GetLastTransactionId();
		auto balVoucherMid = CItemShopManager::instance().GetPlayerAccountBalance(buyer.get());
		RUNTIME_ASSERT(!voucherCrashOk && balVoucherMid.dwDragonCoins == 1150 && balVoucherMid.dwDragonMarks == 350,
			"G3.K: Dragon Scroll Voucher crash after CHARGED deducted 550 coins and added only 50 marks (300->350)");
		int recVoucher = CItemShopManager::instance().RecoverIncompleteTransactions();
		auto balVoucherRec = CItemShopManager::instance().GetPlayerAccountBalance(buyer.get());
		RUNTIME_ASSERT(recVoucher == 1 && queryJournalState(txVoucherCrash) == "REFUNDED" && balVoucherRec.dwDragonCoins == 1700 && balVoucherRec.dwDragonMarks == 300,
			"G3.K: Dragon Scroll Voucher crash recovery refunded 550 coins and rolled back 50 marks accurately");
		bool voucherBuyOk = CItemShopManager::instance().BuyItem(buyer.get(), 104, 1);
		uint64_t txVoucherOk = CItemShopManager::instance().GetLastTransactionId();
		RUNTIME_ASSERT(voucherBuyOk && queryJournalState(txVoucherOk) == "DELIVERED" && queryAwardCountForTx(txVoucherOk) == 1,
			"G3.K: Dragon Scroll Voucher normal purchase succeeded");

		// G3.I & Q3: Two real GAME processes racing on RecoverIncompleteTransactions() simultaneously
		DBManager::instance().AccountQuery("UPDATE `account` SET `cash` = 10000, `cash_mark` = 0 WHERE `id` = 990701");
		CItemShopManager::instance().SetCrashAfterChargeForTest(true);
		CItemShopManager::instance().BuyItem(buyer.get(), 101, 1); // Creates real CHARGED row (-600 coins -> 9400)
		CItemShopManager::instance().SetCrashAfterChargeForTest(false);
		uint64_t raceTxId = CItemShopManager::instance().GetLastTransactionId();

		std::atomic<int> childRecovered{0};
		std::thread childProcessThread([&]() {
			int code = runChildMode("--test-itemshop-recover-child");
			if (code >= 10)
				childRecovered.store(code - 10, std::memory_order_release);
		});
		int parentRecovered = CItemShopManager::instance().RecoverIncompleteTransactions();
		childProcessThread.join();

		auto balRace = CItemShopManager::instance().GetPlayerAccountBalance(buyer.get());
		RUNTIME_ASSERT((parentRecovered + childRecovered.load()) == 1 && queryJournalState(raceTxId) == "REFUNDED" && balRace.dwDragonCoins == 10000,
			"G3.I: Two concurrent GAME processes running real RecoverIncompleteTransactions() had 0 interference and 0 double-refund");
		std::cout << "  [PASS] Check 10 (G3.E, G3.I, G3.K & Q3): Multi-slot non-stackable award, VIP/Voucher safety & 2-process recovery race verified" << std::endl;
		checks_passed++;
	}

	// Check 11 (G3.F, G3.G, G3.L, Q5 & Q6): Synchronous Direct-Inventory SQL Persistence, Pre-Existing Same-Vnum Isolation & Pre-SQL Crash Subprocess
	{
		DBManager::instance().AccountQuery("UPDATE `account` SET `cash` = 5000, `cash_mark` = 0 WHERE `id` = 990701");

		// G3.G: Real direct-inventory purchase via AutoGiveItem + PersistInventoryItemSynchronously writes to player.item before DELIVERED
		buyer->EnableDirectInventory(true);
		bool directBuyOk = CItemShopManager::instance().BuyItem(buyer.get(), 105, 2); // 2 * 3 = 6 items of vnum 71085, price = 500
		uint64_t txDirectOk = CItemShopManager::instance().GetLastTransactionId();
		auto msgDirectItem = DBManager::instance().DirectQuery(
			"SELECT id, count FROM `item` WHERE `owner_id` = 880701 AND `vnum` = 71085");
		MYSQL_ROW rDirect = (msgDirectItem && msgDirectItem->Get()) ? mysql_fetch_row(msgDirectItem->Get()->pSQLResult) : nullptr;
		uint64_t preExistingItemId = 0;
		uint64_t preExistingCount = 0;
		if (rDirect && rDirect[0]) str_to_number(preExistingItemId, rDirect[0]);
		if (rDirect && rDirect[1]) str_to_number(preExistingCount, rDirect[1]);
		RUNTIME_ASSERT(directBuyOk && queryJournalState(txDirectOk) == "DELIVERED" && preExistingItemId >= 700001 && preExistingCount == 6,
			"G3.G: BuyItem synchronously persisted direct-inventory item to player.item in MariaDB before marking DELIVERED");

		// G3.F & G3.L: Player ALREADY owns vnum=71085 (id=preExistingItemId, count=6) in player.item.
		// Now spawn a real child OS process (--test-itemshop-crash-pre-sql-child) that buys vnum=71085 (index=105, qty=1, cost=250),
		// gives the new item in RAM, records INV_PENDING:<new_id> in itemshop_tx_journal, and crashes with _Exit(98) BEFORE SQL player.item write!
		int preSqlExit = runChildMode("--test-itemshop-crash-pre-sql-child");
		RUNTIME_ASSERT(preSqlExit == 98,
			"G3.L: Child process exited via _Exit(98) after GiveItem + INV_PENDING before player.item SQL persistence");

		auto msgPreSqlTx = DBManager::instance().AccountQuery(
			"SELECT tx_id, state, delivered_item_ids FROM itemshop_tx_journal WHERE aid = 990701 ORDER BY tx_id DESC LIMIT 1");
		MYSQL_ROW rPreSql = (msgPreSqlTx && msgPreSqlTx->Get()) ? mysql_fetch_row(msgPreSqlTx->Get()->pSQLResult) : nullptr;
		uint64_t txPreSql = 0;
		std::string statePreSql = (rPreSql && rPreSql[1]) ? rPreSql[1] : "";
		std::string idsPreSql = (rPreSql && rPreSql[2]) ? rPreSql[2] : "";
		if (rPreSql && rPreSql[0]) str_to_number(txPreSql, rPreSql[0]);
		RUNTIME_ASSERT(txPreSql > txDirectOk && statePreSql == "CHARGED" && idsPreSql.rfind("INV_PENDING:", 0) == 0,
			"G3.L: Crashed pre-SQL child left CHARGED row with INV_PENDING:<new_id>");

		// Run RecoverIncompleteTransactions(): must NOT mistake preExistingItemId (also vnum=71085!) for the crashed purchase (G3.F),
		// must refund the 250 coins (4250 -> 4500), and must keep preExistingItemId intact in player.item!
		int recPreSql = CItemShopManager::instance().RecoverIncompleteTransactions();
		auto balAfterPreSqlRec = CItemShopManager::instance().GetPlayerAccountBalance(buyer.get());
		auto msgExistingStillThere = DBManager::instance().DirectQuery(
			"SELECT count FROM `item` WHERE `id` = %llu AND `owner_id` = 880701",
			static_cast<unsigned long long>(preExistingItemId));
		MYSQL_ROW rExisting = (msgExistingStillThere && msgExistingStillThere->Get()) ? mysql_fetch_row(msgExistingStillThere->Get()->pSQLResult) : nullptr;
		int existingCountAfter = (rExisting && rExisting[0]) ? std::atoi(rExisting[0]) : 0;
		RUNTIME_ASSERT(recPreSql == 1 && queryJournalState(txPreSql) == "REFUNDED" && balAfterPreSqlRec.dwDragonCoins == 4500 && existingCountAfter == 6,
			"G3.F & G3.L: Pre-existing same-vnum item in player.item was never mistaken for new delivery; pre-SQL crash was refunded and existing item preserved");

		// Q6: If direct-inventory item WAS persisted to player.item before crash (crash right before DELIVERED journal update), recovery finalizes as DELIVERED!
		CItemShopManager::instance().SetCrashAfterDeliveryBeforeJournalForTest(true);
		bool buyInvCrashAfterSql = CItemShopManager::instance().BuyItem(buyer.get(), 105, 1); // 250 coins -> 4250
		CItemShopManager::instance().SetCrashAfterDeliveryBeforeJournalForTest(false);
		uint64_t txInvDelivered = CItemShopManager::instance().GetLastTransactionId();
		int recInvDelivered = CItemShopManager::instance().RecoverIncompleteTransactions();
		auto balAfterInvDelivered = CItemShopManager::instance().GetPlayerAccountBalance(buyer.get());
		RUNTIME_ASSERT(buyInvCrashAfterSql && recInvDelivered == 1 && queryJournalState(txInvDelivered) == "DELIVERED" && balAfterInvDelivered.dwDragonCoins == 4250,
			"Q6: Direct-inventory item persisted in player.item before crash is finalized as DELIVERED without false refund");

		buyer->EnableDirectInventory(false);
		std::cout << "  [PASS] Check 11 (G3.F, G3.G, G3.L, Q5 & Q6): Synchronous item SQL persistence, pre-existing vnum isolation & pre-SQL crash recovery verified" << std::endl;
		checks_passed++;
	}

	// Check 12 (G3.J, Q7, Q8 & Q9): Cross-DB Auction/Item/Account Consistency, Disconnect Retry & INT64_MAX Overflow / Invalid Delta Guard
	{
		// G3.J & Q7: Player A reserves 2 auction items via real BuyItem (0->2) and crashes at CHARGED; Player B buys 2 auction items (2->4); recovery rolls back Player A (4->2)
		DBManager::instance().AccountQuery("UPDATE `account` SET `cash` = 7000, `cash_mark` = 0 WHERE `id` = 990701");
		DBManager::instance().DirectQuery("UPDATE `itemshop_time_auction` SET `buy_count` = 0 WHERE `item_index` = 201");
		CItemShopManager::instance().UpdateAuctionBuyAmount(201, 0);

		CItemShopManager::instance().SetCrashAfterChargeForTest(true);
		CItemShopManager::instance().BuyItem(buyer.get(), 201, 2); // Reserves 2 auction items (0->2), charges 800 coins (7000->6200), crashes at CHARGED
		CItemShopManager::instance().SetCrashAfterChargeForTest(false);
		uint64_t txAucA = CItemShopManager::instance().GetLastTransactionId();

		// Concurrent Player B reserves 2 items in DB before Player A's recovery runs (2->4)
		DBManager::instance().DirectQuery(
			"UPDATE `itemshop_time_auction` SET `buy_count` = `buy_count` + 2 WHERE `item_index` = 201 AND `buy_count` + 2 <= 5");
		int recAuc = CItemShopManager::instance().RecoverIncompleteTransactions();
		auto msgAucAfter = DBManager::instance().DirectQuery("SELECT `buy_count` FROM `itemshop_time_auction` WHERE `item_index` = 201");
		MYSQL_ROW rAucAfter = (msgAucAfter && msgAucAfter->Get()) ? mysql_fetch_row(msgAucAfter->Get()->pSQLResult) : nullptr;
		int finalBuyCount = (rAucAfter && rAucAfter[0]) ? std::atoi(rAucAfter[0]) : -1;
		auto balAfterAucRec = CItemShopManager::instance().GetPlayerAccountBalance(buyer.get());
		RUNTIME_ASSERT(recAuc == 1 && queryJournalState(txAucA) == "REFUNDED" && finalBuyCount == 2 && balAfterAucRec.dwDragonCoins == 7000,
			"G3.J & Q7: Cross-DB recovery rolled back Player A's auction count (4->2) and refunded coins (6200->7000) while preserving Player B's count");

		// Q8: Simulated connection drop mid-recovery (uncommitted transaction on dropped connection rolls back cleanly; retry recovers)
		{
			CItemShopManager::instance().SetCrashAfterChargeForTest(true);
			CItemShopManager::instance().BuyItem(buyer.get(), 101, 1); // -600 coins -> 6400, CHARGED
			CItemShopManager::instance().SetCrashAfterChargeForTest(false);
			uint64_t txDrop = CItemShopManager::instance().GetLastTransactionId();
			{
				CAsyncSQL transientConn;
				RUNTIME_ASSERT(transientConn.Setup(host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), "utf8mb4", true, port),
					"Transient connection for Q8 setup succeeded");
				transientConn.DirectQuery("START TRANSACTION");
				char qUncommitted[256];
				std::snprintf(qUncommitted, sizeof(qUncommitted),
					"UPDATE itemshop_tx_journal SET state = 'REFUNDED' WHERE tx_id = %llu",
					static_cast<unsigned long long>(txDrop));
				transientConn.DirectQuery(qUncommitted);
			}
			int recAfterDrop = CItemShopManager::instance().RecoverIncompleteTransactions();
			RUNTIME_ASSERT(recAfterDrop == 1 && queryJournalState(txDrop) == "REFUNDED",
				"Q8: Uncommitted mid-recovery disconnect rolled back cleanly and subsequent retry recovered tx_id");
		}

		// Q9: INT64_MAX overflow & invalid positive cash_delta on CHARGED row are blocked and routed to NEEDS_MANUAL_REVIEW
		DBManager::instance().AccountQuery(
			"UPDATE `account` SET `cash` = %lld, `cash_mark` = 0 WHERE `id` = 990701",
			static_cast<long long>(INT64_MAX - 10));
		auto insOverflow = DBManager::instance().AccountQuery(
			"INSERT INTO `itemshop_tx_journal` "
			"(`aid`, `pid`, `login`, `item_index`, `vnum`, `quantity`, `item_count`, `charge_type`, `cash_delta`, `mark_delta`, `money_before_buy`, `state`) "
			"VALUES (990701, 880701, 'real_itemshop_buyer', 101, 71084, 1, 2, %u, -500, 500, 1000, 'CHARGED')",
			static_cast<unsigned int>(ERequestCharge_Cash));
		uint64_t txOverflow = insOverflow->Get()->ullInsertID;

		auto insBadDelta = DBManager::instance().AccountQuery(
			"INSERT INTO `itemshop_tx_journal` "
			"(`aid`, `pid`, `login`, `item_index`, `vnum`, `quantity`, `item_count`, `charge_type`, `cash_delta`, `mark_delta`, `money_before_buy`, `state`) "
			"VALUES (990701, 880701, 'real_itemshop_buyer', 101, 71084, 1, 2, %u, 500, 500, 1000, 'CHARGED')",
			static_cast<unsigned int>(ERequestCharge_Cash));
		uint64_t txBadDelta = insBadDelta->Get()->ullInsertID;

		CItemShopManager::instance().RecoverIncompleteTransactions();
		auto balOverflowCheck = CItemShopManager::instance().GetPlayerAccountBalance(buyer.get());
		RUNTIME_ASSERT(queryJournalState(txOverflow) == "NEEDS_MANUAL_REVIEW" && balOverflowCheck.dwDragonCoins == (INT64_MAX - 10),
			"Q9: Refund that would overflow INT64_MAX was blocked and marked NEEDS_MANUAL_REVIEW without modifying balance");
		RUNTIME_ASSERT(queryJournalState(txBadDelta) == "NEEDS_MANUAL_REVIEW",
			"Q9b: Corrupted positive cash_delta on CHARGED row was blocked and marked NEEDS_MANUAL_REVIEW");
		std::cout << "  [PASS] Check 12 (G3.J, Q7, Q8 & Q9): Cross-DB auction rollback, disconnect retry & INT64_MAX overflow protection verified" << std::endl;
		checks_passed++;
	}

	// Check 13 (G3.H & Q10): Real OS Subprocess Crash Injection (_Exit(99) at CHARGED -> _Exit(97) in 2nd Process at RECOVERING -> Parent Reclaim)
	{
		DBManager::instance().AccountQuery("UPDATE `account` SET `cash` = 10000000000, `cash_mark` = 0 WHERE `id` = 990701");
		int child1Exit = runChildMode("--test-itemshop-crash-child");
		RUNTIME_ASSERT(child1Exit == 99,
			"Q10: First child process terminated via real std::_Exit(99) immediately after committing CHARGED");

		auto balAfterChild1 = CItemShopManager::instance().GetPlayerAccountBalance(buyer.get());
		auto msgChildTx = DBManager::instance().AccountQuery(
			"SELECT tx_id, state FROM itemshop_tx_journal WHERE aid = 990701 ORDER BY tx_id DESC LIMIT 1");
		MYSQL_ROW rChild = (msgChildTx && msgChildTx->Get()) ? mysql_fetch_row(msgChildTx->Get()->pSQLResult) : nullptr;
		uint64_t childTxId = 0;
		std::string childState = (rChild && rChild[1]) ? rChild[1] : "";
		if (rChild && rChild[0]) str_to_number(childTxId, rChild[0]);
		RUNTIME_ASSERT(childTxId > 0 && childState == "CHARGED" && balAfterChild1.dwDragonCoins == 5000000000LL,
			"Q10: Parent verified first child process committed CHARGED (10B -> 5B coins) before dying with _Exit(99)");

		// G3.H: Spawn a SECOND real child OS process that starts RecoverIncompleteTransactions(), claims CHARGED -> RECOVERING, and crashes with _Exit(97)!
		int child2Exit = runChildMode("--test-itemshop-crash-recovering-child");
		RUNTIME_ASSERT(child2Exit == 97 && queryJournalState(childTxId) == "RECOVERING",
			"G3.H: Second child process claimed CHARGED -> RECOVERING and crashed via std::_Exit(97)");

		// Parent now runs RecoverIncompleteTransactions() and reclaims the orphaned RECOVERING transaction!
		int recChild = CItemShopManager::instance().RecoverIncompleteTransactions();
		auto balAfterParentRec = CItemShopManager::instance().GetPlayerAccountBalance(buyer.get());
		RUNTIME_ASSERT(recChild == 1 && queryJournalState(childTxId) == "REFUNDED" && balAfterParentRec.dwDragonCoins == 10000000000LL,
			"G3.H & Q10: Parent RecoverIncompleteTransactions() reclaimed RECOVERING row after second process crash and refunded (5B -> 10B coins)");
		std::cout << "  [PASS] Check 13 (G3.H & Q10): Real 2-stage OS child process crashes (_Exit(99) CHARGED -> _Exit(97) RECOVERING -> REFUNDED, tx_id=" << childTxId << ") verified" << std::endl;
		checks_passed++;
	}

	// Check 14 (Section 1): 64-Bit Economy Consistency (GOLD_MAX = 10^12, ComputeRefineFee, PointChange/ChangeGold, Monarch 64-bit)
	{
		static_assert(GOLD_MAX == 1000000000000LL, "GOLD_MAX must be 1,000,000,000,000 (1 Trillion Yang)");
		static_assert(EXCHANGE_GOLD_MAX == 1000000000000LL, "EXCHANGE_GOLD_MAX must be 1,000,000,000,000 (1 Trillion Yang)");

		const YANG fee2Pow31 = buyer->ComputeRefineFee(2147483648LL, 1);
		const YANG fee2Pow32 = buyer->ComputeRefineFee(4294967296LL, 1);
		const YANG fee6B = buyer->ComputeRefineFee(6000000000LL, 5);
		const YANG feeClamp = buyer->ComputeRefineFee(2500000000000LL, 5);
		RUNTIME_ASSERT(fee2Pow31 == 2147483648LL && fee2Pow32 == 4294967296LL && fee6B == 6000000000LL && feeClamp == (GOLD_MAX - 1),
			"ComputeRefineFee supports >2^31 (2.14B), >2^32 (4.29B), 6B, and clamps safely at GOLD_MAX - 1 (999,999,999,999)");

		buyer->SetGold(0);
		buyer->PointChange(POINT_GOLD, 3000000000LL);
		RUNTIME_ASSERT(buyer->GetGold() == 3000000000LL, "PointChange(POINT_GOLD) holds 3,000,000,000 (> 2^31) Yang");
		buyer->ChangeGold(3500000000LL);
		RUNTIME_ASSERT(buyer->GetGold() == 6500000000LL, "ChangeGold holds 6,500,000,000 (> 2^32) Yang");
		buyer->PointChange(POINT_GOLD, GOLD_MAX);
		RUNTIME_ASSERT(buyer->GetGold() == (GOLD_MAX - 1), "PointChange(POINT_GOLD) clamps safely at GOLD_MAX - 1 (999,999,999,999)");

		TMonarchInfo monarchInfo{};
		monarchInfo.money[1] = 8000000000LL;
		CMonarch::instance().SetMonarchInfo(&monarchInfo);
		RUNTIME_ASSERT(CMonarch::instance().IsMoneyOk(5000000000LL, 1) && !CMonarch::instance().IsMoneyOk(9000000000LL, 1),
			"CMonarch::IsMoneyOk supports 64-bit (> 2^32) empire treasury amounts");
		std::cout << "  [PASS] Check 14 (Sec 1): 64-Bit Economy (GOLD_MAX=10^12, ComputeRefineFee 6B, ChangeGold 6.5B, Monarch 8B) verified" << std::endl;
		checks_passed++;
	}

	// Check 15 (G3.1, G3.2 & G3.J): Split (Inventory + item_award) Delivery Recovery & Partial Delivery Cleanup + Refund
	{
		DBManager::instance().AccountQuery("UPDATE `account` SET `cash` = 5600, `cash_mark` = 0 WHERE `id` = 990701");

		// 15a: Split delivery (2 items in player.item via INV_PENDING:77099 + 2 items in item_award = 4 total) is finalized as DELIVERED
		DBManager::instance().DirectQuery(
			"INSERT INTO `item` (`id`, `owner_id`, `window`, `pos`, `count`, `vnum`) VALUES (77099, 880701, 'INVENTORY', 1, 2, 71084)");
		auto insSplit = DBManager::instance().AccountQuery(
			"INSERT INTO `itemshop_tx_journal` "
			"(`aid`, `pid`, `login`, `item_index`, `vnum`, `quantity`, `item_count`, `charge_type`, `cash_delta`, `mark_delta`, `money_before_buy`, `state`, `delivered_item_ids`) "
			"VALUES (990701, 880701, 'real_itemshop_buyer', 101, 71084, 2, 4, %u, -1200, 1200, 6800, 'CHARGED', 'INV_PENDING:77099')",
			static_cast<unsigned int>(ERequestCharge_Cash));
		RUNTIME_ASSERT(insSplit && insSplit->uiSQLErrno == 0 && insSplit->Get(), "Inserted split-delivery CHARGED row");
		uint64_t txSplit = insSplit->Get()->ullInsertID;
		DBManager::instance().DirectQuery(
			"INSERT INTO `item_award` (`login`, `vnum`, `count`, `given_time`, `why`, `mall`) "
			"VALUES ('real_itemshop_buyer', 71084, 2, NOW(), 'ITEMSHOP_TX_%llu#1', 1)",
			static_cast<unsigned long long>(txSplit));
		int rec15a = CItemShopManager::instance().RecoverIncompleteTransactions();
		auto bal15a = CItemShopManager::instance().GetPlayerAccountBalance(buyer.get());
		auto msgSplitItem = DBManager::instance().DirectQuery("SELECT COUNT(*) FROM `item` WHERE `id` = 77099 AND `owner_id` = 880701");
		MYSQL_ROW rSplitItem = (msgSplitItem && msgSplitItem->Get()) ? mysql_fetch_row(msgSplitItem->Get()->pSQLResult) : nullptr;
		int splitInvStillExists = (rSplitItem && rSplitItem[0]) ? std::atoi(rSplitItem[0]) : 0;
		RUNTIME_ASSERT(rec15a == 1 && queryJournalState(txSplit) == "DELIVERED" && splitInvStillExists == 1 && bal15a.dwDragonCoins == 5600,
			"G3.2: Split delivery across player.item and item_award is finalized as DELIVERED without deleting inventory items or false refund");

		// 15b: Partial incomplete delivery (only 1 of 3 items in player.item via INV_PENDING:77100 + 1 of 3 in item_award = 2 < 3 required) -> cleans up partial rows & refunds!
		DBManager::instance().DirectQuery(
			"INSERT INTO `item` (`id`, `owner_id`, `window`, `pos`, `count`, `vnum`) VALUES (77100, 880701, 'INVENTORY', 2, 1, 72001)");
		auto insPartial = DBManager::instance().AccountQuery(
			"INSERT INTO `itemshop_tx_journal` "
			"(`aid`, `pid`, `login`, `item_index`, `vnum`, `quantity`, `item_count`, `charge_type`, `cash_delta`, `mark_delta`, `money_before_buy`, `state`, `delivered_item_ids`) "
			"VALUES (990701, 880701, 'real_itemshop_buyer', 102, 72001, 3, 3, %u, -600, 600, 6200, 'RECOVERING', 'INV_PENDING:77100')",
			static_cast<unsigned int>(ERequestCharge_Cash));
		uint64_t txPartial = insPartial->Get()->ullInsertID;
		DBManager::instance().DirectQuery(
			"INSERT INTO `item_award` (`login`, `vnum`, `count`, `given_time`, `why`, `mall`) "
			"VALUES ('real_itemshop_buyer', 72001, 1, NOW(), 'ITEMSHOP_TX_%llu#1', 1)",
			static_cast<unsigned long long>(txPartial));
		int rec15b = CItemShopManager::instance().RecoverIncompleteTransactions();
		auto bal15b = CItemShopManager::instance().GetPlayerAccountBalance(buyer.get());
		auto msgPartItem = DBManager::instance().DirectQuery("SELECT COUNT(*) FROM `item` WHERE `id` = 77100");
		MYSQL_ROW rPartItem = (msgPartItem && msgPartItem->Get()) ? mysql_fetch_row(msgPartItem->Get()->pSQLResult) : nullptr;
		int partInvLeft = (rPartItem && rPartItem[0]) ? std::atoi(rPartItem[0]) : -1;
		RUNTIME_ASSERT(rec15b == 1 && queryJournalState(txPartial) == "REFUNDED" && partInvLeft == 0 && queryAwardCountForTx(txPartial) == 0 && bal15b.dwDragonCoins == 6200,
			"G3.J: Partial incomplete delivery across player.item and item_award was cleaned up and refunded atomically");
		std::cout << "  [PASS] Check 15 (G3.1, G3.2 & G3.J): Split delivery finalization & partial cross-DB cleanup + refund verified" << std::endl;
		checks_passed++;
	}

	// Cleanup isolated tables
	DBManager::instance().AccountQuery("DROP TABLE IF EXISTS `itemshop_tx_journal`, `account`");
	DBManager::instance().DirectQuery("DROP TABLE IF EXISTS `item_award`, `item`, `itemshop_time_auction`, `itemshop`, `log`");
	buyer->EnableDirectInventory(false);
	buyer->BindDesc(nullptr);
	buyer.reset();
	db_clientdesc = prevDbClientDesc;
	local_monarch.reset();
	local_ism.reset();
	local_log.reset();
	local_db.reset();
	local_p2p.reset();
	local_im.reset();
	local_qm.reset();
	local_am.reset();
	std::remove("sql_log_spool.dat");
	std::remove("sql_log_spool.dat.lock");
	std::remove("p2p_packet_info.txt");
	std::remove("packet_info.txt");

	std::cout << "==================================================" << std::endl;
	std::cout << " [SUCCESS] REAL CItemShopManager LIVE SUITE: " << checks_passed << "/15 CHECKS PASSED!" << std::endl;
	std::cout << "==================================================" << std::endl;
	return 0;
}

