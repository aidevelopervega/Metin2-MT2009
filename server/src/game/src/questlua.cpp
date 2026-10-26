#include "stdafx.h"

#include <sstream>

#include "questmanager.h"
#include "questlua.h"
#include "config.h"
#include "desc.h"
#include "char.h"
#include "char_manager.h"
#include "buffer_manager.h"
#include "db.h"
#include "xmas_event.h"
#include "locale_service.h"
#include "regen.h"
#include "affect.h"
#include "guild.h"
#include "guild_manager.h"
#include "sectree_manager.h"

#undef sys_err
#ifndef __WIN32__
#define sys_err(fmt, args...) quest::CQuestManager::instance().QuestError(__FUNCTION__, __LINE__, fmt, ##args)
#else
#define sys_err(fmt, ...) quest::CQuestManager::instance().QuestError(__FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#endif

namespace quest
{
	using namespace std;

	string ScriptToString(const string& str)
	{
		lua_State* L = CQuestManager::instance().GetLuaState();
		int x = lua_gettop(L);

		int errcode = lua_dobuffer(L, ("return "+str).c_str(), str.size()+7, "ScriptToString");
		string retstr;
		if (!errcode)
		{
			if (lua_isstring(L,-1))
				retstr = lua_tostring(L, -1);
		}
		else
		{
			sys_err("LUA ScriptRunError (code:%d src:[%s])", errcode, str.c_str());
		}
		lua_settop(L,x);
		return retstr;
	}

	void FSetWarpLocation::operator() (LPCHARACTER ch)
	{
		if (ch->IsPC())
		{
			ch->SetWarpLocation (map_index, x, y);
		}
	}

	void FSetQuestFlag::operator() (LPCHARACTER ch)
	{
		if (!ch->IsPC())
			return;

		PC * pPC = CQuestManager::instance().GetPCForce(ch->GetPlayerID());

		if (pPC)
		{
			pPC->SetFlag(flagname, advance ? (pPC->GetFlag(flagname) + value) : value);
		}
	}

	bool FPartyCheckFlagLt::operator() (LPCHARACTER ch)
	{
		if (!ch->IsPC())
			return false;

		PC * pPC = CQuestManager::instance().GetPCForce(ch->GetPlayerID());
		bool returnBool = false;
		if (pPC)
		{
			int flagValue = pPC->GetFlag(flagname);
			if (value > flagValue)
				returnBool = true;
			else
				returnBool = false;
		}

		return returnBool;
	}

	FPartyChat::FPartyChat(int ChatType, const char* in_str) : iChatType(ChatType), str(in_str)
	{
	}

	void FPartyChat::operator() (LPCHARACTER ch)
	{
		ch->ChatPacket(iChatType, "%s", str);
	}

	void FPartyClearReady::operator() (LPCHARACTER ch)
	{
		ch->RemoveAffect(AFFECT_DUNGEON_READY);
	}

	void FSendPacket::operator() (LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			LPCHARACTER ch = (LPCHARACTER) ent;

			if (ch->GetDesc())
			{
				ch->GetDesc()->Packet(buf.read_peek(), buf.size());
			}
		}
	}

#ifdef ENABLE_NEWSTUFF
	void FSendChatPacket::operator() (LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			LPCHARACTER ch = (LPCHARACTER) ent;
			ch->ChatPacket(m_chat_type, "%s", m_text.c_str());
		}
	}
#endif

	void FSendPacketToEmpire::operator() (LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			LPCHARACTER ch = (LPCHARACTER) ent;

			if (ch->GetDesc())
			{
				if (ch->GetEmpire() == bEmpire)
					ch->GetDesc()->Packet(buf.read_peek(), buf.size());
			}
		}
	}

	void FWarpEmpire::operator() (LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			LPCHARACTER ch = (LPCHARACTER) ent;

			if (ch->IsPC() && ch->GetEmpire() == m_bEmpire)
			{
				ch->WarpSet(m_x, m_y, m_lMapIndexTo);
			}
		}
	}

	FBuildLuaGuildWarList::FBuildLuaGuildWarList(lua_State * lua_state) : L(lua_state), m_count(1)
	{
		lua_newtable(lua_state);
	}

	void FBuildLuaGuildWarList::operator() (DWORD g1, DWORD g2)
	{
		CGuild* g = CGuildManager::instance().FindGuild(g1);

		if (!g)
			return;

		if (g->GetGuildWarType(g2) == GUILD_WAR_TYPE_FIELD)
			return;

		if (g->GetGuildWarState(g2) != GUILD_WAR_ON_WAR)
			return;

		lua_newtable(L);
		lua_pushnumber(L, g1);
		lua_rawseti(L, -2, 1);
		lua_pushnumber(L, g2);
		lua_rawseti(L, -2, 2);
		lua_rawseti(L, -2, m_count++);
	}

	bool IsScriptTrue(const char* code, int size)
	{
		if (size==0)
			return true;

		lua_State* L = CQuestManager::instance().GetLuaState();
		int x = lua_gettop(L);
		int errcode = lua_dobuffer(L, code, size, "IsScriptTrue");
		int bStart = lua_toboolean(L, -1);
		if (errcode)
		{
			char buf[100];
			snprintf(buf, sizeof(buf), "LUA ScriptRunError (code:%%d src:[%%%ds])", size);
			sys_err(buf, errcode, code);
		}
		lua_settop(L,x);
		return bStart != 0;
	}

	void combine_lua_string(lua_State * L, ostringstream & s)
	{
		char buf[32];

		int n = lua_gettop(L);
		int i;

		for (i = 1; i <= n; ++i)
		{
			if (lua_isstring(L,i))
				//printf("%s\n",lua_tostring(L,i));
				s << lua_tostring(L, i);
			else if (lua_isnumber(L, i))
			{
				snprintf(buf, sizeof(buf), "%.14g\n", lua_tonumber(L,i));
				s << buf;
			}
		}
	}

	// "member" Lua functions

	ALUA(member_chat)
	{
		ostringstream s;
		combine_lua_string(L, s);
		CQuestManager::Instance().GetCurrentPartyMember()->ChatPacket(CHAT_TYPE_TALKING, "%s", s.str().c_str());
		return 0;
	}

	ALUA(member_clear_ready)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentPartyMember();
		ch->RemoveAffect(AFFECT_DUNGEON_READY);
		return 0;
	}

	ALUA(member_set_ready)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentPartyMember();
		ch->AddAffect(AFFECT_DUNGEON_READY, POINT_NONE, 0, AFF_DUNGEON_READY, 65535, 0, true);
		return 0;
	}

	ALUA(mob_spawn)
	{
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4))
		{
			sys_err("invalid argument");
			return 0;
		}

		DWORD mob_vnum = (DWORD)lua_tonumber(L, 1);
		long local_x = (long) lua_tonumber(L, 2)*100;
		long local_y = (long) lua_tonumber(L, 3)*100;
		float radius = (float) lua_tonumber(L, 4)*100;
		bool bAggressive = lua_toboolean(L, 5);
		DWORD count = (lua_isnumber(L, 6))?(DWORD) lua_tonumber(L, 6):1;

		if (count == 0)
			count = 1;
		else if (count > 10)
		{
			sys_err("count bigger than 10");
			count = 10;
		}

		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();
		LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(ch->GetMapIndex());
		if (pMap == NULL) {
			return 0;
		}
		DWORD dwQuestIdx = CQuestManager::instance().GetCurrentPC()->GetCurrentQuestIndex();

		bool ret = false;
		LPCHARACTER mob = NULL;

		while (count--)
		{
			for (int loop = 0; loop < 8; ++loop)
			{
				float angle = number(0, 999) * M_PI * 2 / 1000;
				float r = number(0, 999) * radius / 1000;

				long x = local_x + pMap->m_setting.iBaseX + (long)(r * cos(angle));
				long y = local_y + pMap->m_setting.iBaseY + (long)(r * sin(angle));

				mob = CHARACTER_MANAGER::instance().SpawnMob(mob_vnum, ch->GetMapIndex(), x, y, 0);

				if (mob)
					break;
			}

			if (mob)
			{
				if (bAggressive)
					mob->SetAggressive();

				mob->SetQuestBy(dwQuestIdx);

				if (!ret)
				{
					ret = true;
					lua_pushnumber(L, (DWORD) mob->GetVID());
				}
			}
		}

		if (!ret)
			lua_pushnumber(L, 0);

		return 1;
	}

	ALUA(mob_spawn_group)
	{
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4) || !lua_isnumber(L, 6))
		{
			sys_err("invalid argument");
			lua_pushnumber(L, 0);
			return 1;
		}

		DWORD group_vnum = (DWORD)lua_tonumber(L, 1);
		long local_x = (long) lua_tonumber(L, 2) * 100;
		long local_y = (long) lua_tonumber(L, 3) * 100;
		float radius = (float) lua_tonumber(L, 4) * 100;
		bool bAggressive = lua_toboolean(L, 5);
		DWORD count = (DWORD) lua_tonumber(L, 6);

		if (count == 0)
			count = 1;
		else if (count > 10)
		{
			sys_err("count bigger than 10");
			count = 10;
		}

		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();
		LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(ch->GetMapIndex());
		if (pMap == NULL) {
			lua_pushnumber(L, 0);
			return 1;
		}
		DWORD dwQuestIdx = CQuestManager::instance().GetCurrentPC()->GetCurrentQuestIndex();

		bool ret = false;
		LPCHARACTER mob = NULL;

		while (count--)
		{
			for (int loop = 0; loop < 8; ++loop)
			{
				float angle = number(0, 999) * M_PI * 2 / 1000;
				float r = number(0, 999)*radius/1000;

				long x = local_x + pMap->m_setting.iBaseX + (long)(r * cos(angle));
				long y = local_y + pMap->m_setting.iBaseY + (long)(r * sin(angle));

				mob = CHARACTER_MANAGER::instance().SpawnGroup(group_vnum, ch->GetMapIndex(), x, y, x, y, NULL, bAggressive);

				if (mob)
					break;
			}

			if (mob)
			{
				mob->SetQuestBy(dwQuestIdx);

				if (!ret)
				{
					ret = true;
					lua_pushnumber(L, (DWORD) mob->GetVID());
				}
			}
		}

		if (!ret)
			lua_pushnumber(L, 0);

		return 1;
	}

	// global Lua functions

	// Registers Lua function table

	void CQuestManager::AddLuaFunctionTable(const char * c_pszName, luaL_reg * preg, bool bCheckIfExists)
	{
#ifdef ENABLE_NEWSTUFF
		bool bIsExists = false;
		if (bCheckIfExists)
		{
			int x = lua_gettop(L);
			lua_getglobal(L, c_pszName);
			if (!lua_istable(L, -1))
			{
				lua_settop(L, x);
				bIsExists = true;
			}
		}
		if (!bIsExists)
			lua_newtable(L);
#else
		lua_newtable(L);
#endif

		while ((preg->name))
		{
			lua_pushstring(L, preg->name);
			lua_pushcfunction(L, preg->func);
			lua_rawset(L, -3);
			preg++;
		}

		lua_setglobal(L, c_pszName);
	}

	void CQuestManager::AddLuaFunctionSubTable(const char * c_pszName, const char * c_pszSubName, luaL_reg * preg)
	{
		// lua_State* L = CQuestManager::instance().GetLuaState();
		int x = lua_gettop(L);
		{
			lua_getglobal(L, c_pszName);
			if (!lua_istable(L, -1))
			{
				sys_err("%s global index not found for %s", c_pszName, c_pszSubName);
				lua_settop(L, x);
				return;
			}
			lua_pushstring(L, c_pszSubName);
			{
				lua_newtable(L);
				while ((preg->name))
				{
					lua_pushstring(L, preg->name);
					lua_pushcfunction(L, preg->func);
					lua_rawset(L, -3);
					preg++;
				}
			}
			lua_rawset(L, -3);
			lua_setglobal(L, c_pszName);
		}
		lua_settop(L, x);
	}

#ifdef ENABLE_NEWSTUFF
	void CQuestManager::AppendLuaFunctionTable(const char * c_pszName, luaL_reg * preg, bool bForceCreation)
	{
		int x = lua_gettop(L);
		{
			lua_getglobal(L, c_pszName);
			if (!lua_istable(L, -1))
			{
				sys_err("%s global index not found (force=%d)", c_pszName, bForceCreation);
				lua_settop(L, x);
				if (bForceCreation)
					AddLuaFunctionTable(c_pszName, preg);
				return;
			}

			while ((preg->name))
			{
				lua_pushstring(L, preg->name);
				lua_pushcfunction(L, preg->func);
				lua_rawset(L, -3);
				preg++;
			}

			lua_setglobal(L, c_pszName);
		}
		lua_settop(L, x);
	}

	void CQuestManager::AddLuaConstantGlobal(const char * c_pszName, lua_Number lNumber, bool bOverwrite)
	{
		int x = lua_gettop(L);
		{
			lua_getglobal(L, c_pszName);
			if (lua_isnumber(L, -1))
			{
				if (!bOverwrite)
				{
					sys_err("%s global index already defined", c_pszName);
					lua_settop(L, x);
					return;
				}
			}
			lua_pushnumber(L, lNumber);
			lua_setglobal(L, c_pszName);
		}
		lua_settop(L, x);
	}

	void CQuestManager::AddLuaConstantInTable(const char * c_pszName, const char * c_pszSubName, lua_Number lNumber, bool bForceCreation)
	{
		int x = lua_gettop(L);
		{
			lua_getglobal(L, c_pszName);
			if (!lua_istable(L, -1))
			{
				if (!bForceCreation)
				{
					sys_err("%s global index for %s already defined", c_pszName, c_pszSubName);
					lua_settop(L, x);
					return;
				}
				lua_newtable(L);
			}
			{
				lua_pushstring(L, c_pszSubName);
				lua_pushnumber(L, lNumber);
				lua_rawset(L, -3);
			}
			lua_setglobal(L, c_pszName);
		}
		lua_settop(L, x);
	}

	void CQuestManager::AddLuaConstantInTable(const char * c_pszName, const char * c_pszSubName, const char * szString, bool bForceCreation)
	{
		int x = lua_gettop(L);
		{
			lua_getglobal(L, c_pszName);
			if (!lua_istable(L, -1))
			{
				if (!bForceCreation)
				{
					sys_err("%s global index for %s already defined", c_pszName, c_pszSubName);
					lua_settop(L, x);
					return;
				}
				lua_newtable(L);
			}
			{
				lua_pushstring(L, c_pszSubName);
				lua_pushstring(L, szString);
				lua_rawset(L, -3);
			}
			lua_setglobal(L, c_pszName);
		}
		lua_settop(L, x);
	}

	void CQuestManager::AddLuaConstantSubTable(const char * c_pszName, const char * c_pszSubName, luaC_tab * preg)
	{
		// lua_State* L = CQuestManager::instance().GetLuaState();
		int x = lua_gettop(L);
		{
			lua_getglobal(L, c_pszName);
			if (!lua_istable(L, -1))
			{
				sys_err("%s global index not found for %s", c_pszName, c_pszSubName);
				lua_settop(L, x);
				return;
			}
			lua_pushstring(L, c_pszSubName);
			{
				lua_newtable(L);
				while ((preg->name))
				{
					lua_pushstring(L, preg->name);
					switch (preg->val.type)
					{
						case ETL_CFUN:
							lua_pushcfunction(L, preg->val.cfVal);
							break;
						case ETL_LNUM:
							lua_pushnumber(L, preg->val.lnVal);
							break;
						case ETL_LSTR:
							lua_pushstring(L, preg->val.lsVal);
							break;
						case ETL_NIL:
							lua_pushnil(L);
							break;
					}
					lua_rawset(L, -3);
					preg++;
				}
			}
			lua_rawset(L, -3);
			lua_setglobal(L, c_pszName);
		}
		lua_settop(L, x);
	}
#endif

	void CQuestManager::InitializeQuestReward(TQuestRewardItem* table, int size)
	{
		m_mapQuestReward.clear();
		for (int i = 0; i < size; ++i, ++table)
		{
			m_mapQuestReward.insert(std::make_pair(table->questName, *table));
		}

		sys_log(0, "QUEST_REWARD: COUNT %d", m_mapQuestReward.size());
	}

	const TQuestRewardItem* CQuestManager::GetQuestReward(const char* questName)
	{
		auto it = m_mapQuestReward.find(questName);
		if (it == m_mapQuestReward.end())
			return nullptr;

		return &it->second;
	}

	void CQuestManager::BuildStateIndexToName(const char* questName)
	{
		int x = lua_gettop(L);
		lua_getglobal(L, questName);

		if (lua_isnil(L,-1))
		{
			sys_err("QUEST wrong quest state file for quest %s",questName);
			lua_settop(L,x);
			return;
		}

		// Phase 1: Collect mappings without modifying table during lua_next traversal (prevents table rehashing corruption)
		std::vector<std::pair<std::string, double>> state_mappings;
		for (lua_pushnil(L); lua_next(L, -2);)
		{
			if (lua_isstring(L, -2) && lua_isnumber(L, -1))
			{
				state_mappings.push_back({lua_tostring(L, -2), lua_tonumber(L, -1)});
			}
			lua_pop(L, 1);
		}

		// Phase 2: Insert reverse mappings (table[state_index] = state_name)
		for (const auto& sm : state_mappings)
		{
			lua_pushnumber(L, sm.second);
			lua_pushstring(L, sm.first.c_str());
			lua_rawset(L, -3);
		}

		lua_settop(L, x);
	}

	bool CQuestManager::InitializeLua()
	{
		L = luaL_newstate();

		luaL_openlibs(L);
		//luaopen_debug(L);
		{
			const char* shims =
				"if not math.mod then math.mod = math.fmod end\n"
				"if not math.pow then math.pow = function(x, y) return x ^ y end end\n"
				"if not string.gfind then string.gfind = string.gmatch end\n"
				"if not table.getn then\n"
				"    table.getn = function(t)\n"
				"        if type(t) ~= 'table' then return 0 end\n"
				"        if type(t.n) == 'number' then return t.n end\n"
				"        return #t\n"
				"    end\n"
				"end\n"
				"if not table.foreach then\n"
				"    table.foreach = function(t, f)\n"
				"        for k, v in pairs(t) do\n"
				"            local r = f(k, v)\n"
				"            if r ~= nil then return r end\n"
				"        end\n"
				"    end\n"
				"end\n"
				"if not table.foreachi then\n"
				"    table.foreachi = function(t, f)\n"
				"        for i, v in ipairs(t) do\n"
				"            local r = f(i, v)\n"
				"            if r ~= nil then return r end\n"
				"        end\n"
				"    end\n"
				"end\n"
				"if not unpack then unpack = table.unpack end\n"
				"do\n"
				"    local raw_random = math.random\n"
				"    math.random = function(a, b)\n"
				"        if type(a) == 'number' and math.type(a) == 'float' then a = math.tointeger(math.modf(a)) or a end\n"
				"        if type(b) == 'number' and math.type(b) == 'float' then b = math.tointeger(math.modf(b)) or b end\n"
				"        if a == nil then return raw_random()\n"
				"        elseif b == nil then return raw_random(a)\n"
				"        else return raw_random(a, b) end\n"
				"    end\n"
				"    local raw_format = string.format\n"
				"    string.format = function(fmt, ...)\n"
				"        local ok, res = pcall(raw_format, fmt, ...)\n"
				"        if ok then return res end\n"
				"        local args = table.pack(...)\n"
				"        local arg_idx = 0\n"
				"        local i = 1\n"
				"        local len = type(fmt) == 'string' and #fmt or 0\n"
				"        while i <= len do\n"
				"            local s, e, spec = string.find(fmt, '%%[-+ #0]*%d*%.?%d*([A-Za-z%%])', i)\n"
				"            if not s then break end\n"
				"            if spec ~= '%' then\n"
				"                arg_idx = arg_idx + 1\n"
				"                if (spec == 'd' or spec == 'i' or spec == 'u' or spec == 'o' or spec == 'x' or spec == 'X' or spec == 'c') and type(args[arg_idx]) == 'number' and math.type(args[arg_idx]) == 'float' then\n"
				"                    local ipart = math.modf(args[arg_idx])\n"
				"                    args[arg_idx] = math.tointeger(ipart) or ipart\n"
				"                end\n"
				"            end\n"
				"            i = e + 1\n"
				"        end\n"
				"        return raw_format(fmt, table.unpack(args, 1, args.n))\n"
				"    end\n"
				"end\n";
			luaL_dostring(L, shims);
		}

		RegisterAffectFunctionTable();
		RegisterBuildingFunctionTable();
		RegisterDungeonFunctionTable();
		RegisterGameFunctionTable();
		RegisterGuildFunctionTable();
		RegisterHorseFunctionTable();
#ifdef __PET_SYSTEM__
		RegisterPetFunctionTable();
#endif
		RegisterITEMFunctionTable();
		RegisterMarriageFunctionTable();
		RegisterNPCFunctionTable();
		RegisterPartyFunctionTable();
		RegisterPCFunctionTable();
		RegisterQuestFunctionTable();
		RegisterTargetFunctionTable();
		RegisterArenaFunctionTable();
		RegisterForkedFunctionTable();
		RegisterMonarchFunctionTable();
		RegisterOXEventFunctionTable();
		RegisterMgmtFunctionTable();
		RegisterDanceEventFunctionTable();
		RegisterDragonLairFunctionTable();
		RegisterDragonSoulFunctionTable();
#ifdef ENABLE_QUEST_DND_EVENT
		RegisterDNDFunctionTable();
#endif
		RegisterCommandFunctionTable();
		{
			luaL_reg member_functions[] =
			{
				{ "chat",			member_chat		},
				{ "set_ready",			member_set_ready	},
				{ "clear_ready",		member_clear_ready	},
				{ NULL,				NULL			}
			};

			AddLuaFunctionTable("member", member_functions);
		}

		{
			luaL_reg mob_functions[] =
			{
				{ "spawn",			mob_spawn		},
				{ "spawn_group",		mob_spawn_group		},
				{ NULL,				NULL			}
			};

			AddLuaFunctionTable("mob", mob_functions);
		}

		// global namespace functions

		RegisterGlobalFunctionTable(L);

		// LUA_INIT_ERROR_MESSAGE
		{
			char settingsFileName[256];
			snprintf(settingsFileName, sizeof(settingsFileName), "%s/settings.lua", LocaleService_GetBasePath().c_str());

			int settingsLoadingResult = lua_dofile(L, settingsFileName);
			sys_log(0, "LoadSettings(%s), returns %d", settingsFileName, settingsLoadingResult);
			if (settingsLoadingResult != 0)
			{
				sys_err("LOAD_SETTINS_FAILURE(%s)", settingsFileName);
				return false;
			}
		}

		{
			char questlibFileName[256];
			snprintf(questlibFileName, sizeof(questlibFileName), "%s/questlib.lua", LocaleService_GetQuestPath().c_str());

			int questlibLoadingResult = lua_dofile(L, questlibFileName);
			sys_log(0, "LoadQuestlib(%s), returns %d", questlibFileName, questlibLoadingResult);
			if (questlibLoadingResult != 0)
			{
				sys_err("LOAD_QUESTLIB_FAILURE(%s)", questlibFileName);
				return false;
			}
		}

		lua_dostring(L,
			"select = function(...) local a = table.pack(...); return q.yield('select', a) end\n"
			"debug = function(text, ...) if is_test_server and is_test_server() then syschat(string.format('[qDEBUG] '..text, ...)) end end\n"
		);

#define ENABLE_TRANSLATE_LUA
#ifdef ENABLE_TRANSLATE_LUA
		{
			char translateFileName[256];
			snprintf(translateFileName, sizeof(translateFileName), "%s/translate.lua", LocaleService_GetBasePath().c_str());

			int translateLoadingResult = lua_dofile(L, translateFileName);
			sys_log(0, "LoadTranslate(%s), returns %d", translateFileName, translateLoadingResult);
			if (translateLoadingResult != 0)
			{
				sys_err("LOAD_TRANSLATE_ERROR(%s)", translateFileName);
				return false;
			}
		}
#endif

		{
			lua_dostring(L,
				"local function make_gf_proxy(path, existing)\n"
				"    local t = existing or {}\n"
				"    local mt = {\n"
				"        __index = function(self, k)\n"
				"            local str_k = tostring(k)\n"
				"            local full_path = (path and path ~= '' and path ~= 'gameforge') and (path .. '.' .. str_k) or str_k\n"
				"            if translate and type(translate) == 'table' then\n"
				"                if not path or path == '' or path == 'gameforge' then\n"
				"                    if translate[k] ~= nil then return translate[k] end\n"
				"                elseif path == 'locale' or path == 'gameforge.locale' then\n"
				"                    if translate.general and translate.general[k] ~= nil then return translate.general[k] end\n"
				"                    if translate.reward and translate.reward[k] ~= nil then return translate.reward[k] end\n"
				"                    if translate[k] ~= nil then return translate[k] end\n"
				"                else\n"
				"                    local sub = translate[path]\n"
				"                    if sub and type(sub) == 'table' and sub[k] ~= nil then return sub[k] end\n"
				"                end\n"
				"            end\n"
				"            return make_gf_proxy(full_path)\n"
				"        end,\n"
				"        __concat = function(a, b)\n"
				"            return tostring(a) .. tostring(b)\n"
				"        end,\n"
				"        __tostring = function(self)\n"
				"            return string.format('[MISSING_LOCALE: %s]', tostring(path))\n"
				"        end\n"
				"    }\n"
				"    return setmetatable(t, mt)\n"
				"end\n"
				"if not gameforge or type(gameforge) ~= 'table' then\n"
				"    gameforge = make_gf_proxy('gameforge')\n"
				"else\n"
				"    make_gf_proxy('gameforge', gameforge)\n"
				"    for k, v in pairs(gameforge) do\n"
				"        if type(v) == 'table' then\n"
				"            make_gf_proxy(tostring(k), v)\n"
				"            for k2, v2 in pairs(v) do\n"
				"                if type(v2) == 'table' then\n"
				"                    make_gf_proxy(tostring(k) .. '.' .. tostring(k2), v2)\n"
				"                end\n"
				"            end\n"
				"        end\n"
				"    end\n"
				"end\n"
			);

			char questLocaleFileName[256];
			snprintf(questLocaleFileName, sizeof(questLocaleFileName), "%s/locale.lua", g_stQuestDir.c_str());

			int questLocaleLoadingResult = lua_dofile(L, questLocaleFileName);
			sys_log(0, "LoadQuestLocale(%s), returns %d", questLocaleFileName, questLocaleLoadingResult);
			if (questLocaleLoadingResult != 0)
			{
				sys_err("LoadQuestLocale(%s) FAILURE", questLocaleFileName);
				return false;
			}
		}
		// END_OF_LUA_INIT_ERROR_MESSAGE

		for (itertype(g_setQuestObjectDir) it = g_setQuestObjectDir.begin(); it != g_setQuestObjectDir.end(); ++it)
		{
			const string& stQuestObjectDir = *it;
			char buf[PATH_MAX];
			snprintf(buf, sizeof(buf), "%s/state/", stQuestObjectDir.c_str());
			DIR * pdir = opendir(buf);
			int iQuestIdx = 0;

			if (pdir)
			{
				dirent * pde;

				while ((pde = readdir(pdir)))
				{
					if (pde->d_name[0] == '.')
						continue;

					snprintf(buf + 11, sizeof(buf) - 11, "%s", pde->d_name);

					RegisterQuest(pde->d_name, ++iQuestIdx);
					int ret = lua_dofile(L, (stQuestObjectDir + "/state/" + pde->d_name).c_str());
					sys_log(0, "QUEST: loading %s, returns %d", (stQuestObjectDir + "/state/" + pde->d_name).c_str(), ret);
					if (ret != 0)
					{
						sys_err("QUEST: failed to load state %s: %s", (stQuestObjectDir + "/state/" + pde->d_name).c_str(), lua_tostring(L, -1));
						closedir(pdir);
						return false;
					}

					BuildStateIndexToName(pde->d_name);
				}

				closedir(pdir);
			}
		}

		lua_newtable(L);
		lua_setglobal(L, "__codecache");
		return true;
	}

	void CQuestManager::GotoSelectState(QuestState& qs)
	{
		lua_checkstack(qs.co, 1);

		//int n = lua_gettop(L);
		int n = luaL_getn(qs.co, -1);
		qs.args = n;
		//cout << "select here (1-" << qs.args << ")" << endl;

		ostringstream os;
		os << "[QUESTION ";

		for (int i=1; i<=n; i++)
		{
			lua_rawgeti(qs.co,-1,i);
			if (lua_isstring(qs.co,-1))
			{
				//printf("%d\t%s\n",i,lua_tostring(qs.co,-1));
				if (i != 1)
					os << "|";
				os << i << ";" << lua_tostring(qs.co,-1);
			}
			else
			{
				sys_err("SELECT wrong data %s", lua_typename(qs.co, -1));
				sys_err("here");
			}
			lua_pop(qs.co,1);
		}
		os << "]";

		AddScript(os.str());
		qs.suspend_state = SUSPEND_STATE_SELECT;
		if ( test_server )
			sys_log( 0, "%s", m_strScript.c_str() );
		SendScript();
	}

	EVENTINFO(confirm_timeout_event_info)
	{
		DWORD dwWaitPID;
		DWORD dwReplyPID;

		confirm_timeout_event_info()
		: dwWaitPID( 0 )
		, dwReplyPID( 0 )
		{
		}
	};

	EVENTFUNC(confirm_timeout_event)
	{
		confirm_timeout_event_info * info = dynamic_cast<confirm_timeout_event_info *>(event->info);

		if ( info == NULL )
		{
			sys_err( "confirm_timeout_event> <Factor> Null pointer" );
			return 0;
		}

		LPCHARACTER chWait = CHARACTER_MANAGER::instance().FindByPID(info->dwWaitPID);
		LPCHARACTER chReply = NULL; //CHARACTER_MANAGER::info().FindByPID(info->dwReplyPID);

		if (chReply)
		{
		}

		if (chWait)
		{
			CQuestManager::instance().Confirm(info->dwWaitPID, CONFIRM_TIMEOUT);
		}

		return 0;
	}

	void CQuestManager::GotoConfirmState(QuestState & qs)
	{
		qs.suspend_state = SUSPEND_STATE_CONFIRM;
		DWORD dwVID = (DWORD) lua_tonumber(qs.co, -3);
		const char* szMsg = lua_tostring(qs.co, -2);
		int iTimeout = (int) lua_tonumber(qs.co, -1);

		sys_log(0, "GotoConfirmState vid %u msg '%s', timeout %d", dwVID, szMsg, iTimeout);

		// 1
		LPCHARACTER ch = CHARACTER_MANAGER::instance().Find(dwVID);
		if (ch && ch->IsPC())
		{
			ch->ConfirmWithMsg(szMsg, iTimeout, GetCurrentCharacterPtr()->GetPlayerID());
		}

		// 2
		GetCurrentPC()->SetConfirmWait((ch && ch->IsPC())?ch->GetPlayerID():0);
		ostringstream os;
		os << "[CONFIRM_WAIT timeout;" << iTimeout << "]";
		AddScript(os.str());
		SendScript();

		// 3
		confirm_timeout_event_info* info = AllocEventInfo<confirm_timeout_event_info>();

		info->dwWaitPID = GetCurrentCharacterPtr()->GetPlayerID();
		info->dwReplyPID = (ch && ch->IsPC()) ? ch->GetPlayerID() : 0;

		event_create(confirm_timeout_event, info, PASSES_PER_SEC(iTimeout));
	}

	void CQuestManager::GotoSelectItemState(QuestState& qs)
	{
		qs.suspend_state = SUSPEND_STATE_SELECT_ITEM;
		AddScript("[SELECT_ITEM]");
		SendScript();
	}

	void CQuestManager::GotoInputState(QuestState & qs)
	{
		qs.suspend_state = SUSPEND_STATE_INPUT;
		AddScript("[INPUT]");
		SendScript();
	}

	void CQuestManager::GotoPauseState(QuestState & qs)
	{
		qs.suspend_state = SUSPEND_STATE_PAUSE;
		AddScript("[NEXT]");
		SendScript();
	}

	void CQuestManager::GotoEndState([[maybe_unused]] QuestState & qs)
	{
		AddScript("[DONE]");
		SendScript();
	}

	// * OpenState

	// The beginning of script

	QuestState CQuestManager::OpenState([[maybe_unused]] const string& quest_name, int state_index)
	{
		QuestState qs;
		qs.args=0;
		qs.st = state_index;
		qs.co = lua_newthread(L);
		qs.ico = lua_ref(L, 1/*qs.co*/);
		return qs;
	}

	// * RunState

	// decides script to wait for user input, or finish

	bool CQuestManager::RunState(QuestState & qs)
	{
		ClearError();

		m_CurrentRunningState = &qs;
		int ret = lua_resume(qs.co, qs.args);

		if (ret == LUA_YIELD)
		{
			const char* cmd = lua_tostring(qs.co, 1);
			if (cmd)
			{
				if (!strcmp(cmd, "select"))
				{
					GotoSelectState(qs);
					return true;
				}

				if (!strcmp(cmd, "wait"))
				{
					GotoPauseState(qs);
					return true;
				}

				if (!strcmp(cmd, "input"))
				{
					GotoInputState(qs);
					return true;
				}

				if (!strcmp(cmd, "confirm"))
				{
					GotoConfirmState(qs);
					return true;
				}

				if (!strcmp(cmd, "select_item"))
				{
					GotoSelectItemState(qs);
					return true;
				}
			}

			sys_err("LUA_ERROR: unknown yield command '%s'", cmd ? cmd : "NULL");
			WriteRunningStateToSyserr();
			SetError();
			GotoEndState(qs);
			return false;
		}
		else if (ret == 0)
		{
			GotoEndState(qs);
			return false;
		}
		else
		{
			sys_err("LUA_ERROR: %s", lua_tostring(qs.co, -1));
			WriteRunningStateToSyserr();
			SetError();
			GotoEndState(qs);
			return false;
		}
	}

	// * CloseState

	// makes script end

	void CQuestManager::CloseState(QuestState& qs)
	{
		if (qs.co)
		{
			//cerr << "ICO "<<qs.ico <<endl;
			lua_unref(L, qs.ico);
			qs.co = 0;
		}
	}
}

