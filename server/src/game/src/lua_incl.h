#if !defined(_MSC_VER) && defined(__cplusplus)
extern "C" {
#endif

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#if LUA_V == 523 || LUA_V == 551
	#define luaL_reg		luaL_Reg

	#define lua_dobuffer(L, buff, sz, name)	(luaL_loadbuffer(L, buff, sz, name) || lua_pcall(L, 0, LUA_MULTRET, 0))
	#define lua_dofile		luaL_dofile
	#define lua_dostring	luaL_dostring

	static inline size_t m2_lua_getn(lua_State* L, int t) {
		if (!lua_istable(L, t))
			return 0;
		int abs_t = lua_absindex(L, t);
		lua_pushliteral(L, "n");
		lua_rawget(L, abs_t);
		if (lua_type(L, -1) == LUA_TNUMBER) {
			lua_Number n = lua_tonumber(L, -1);
			lua_pop(L, 1);
			if (n >= 0)
				return (size_t)n;
		} else {
			lua_pop(L, 1);
		}
		return lua_rawlen(L, abs_t);
	}
	#define luaL_getn		m2_lua_getn

#if LUA_V == 523
	#define lua_resume(a,b)	lua_resume(a,0,b)
#elif LUA_V == 551
	static inline int m2_lua_resume(lua_State* L, int nargs) {
		int nresults = 0;
		return lua_resume(L, NULL, nargs, &nresults);
	}
	#define lua_resume(a,b)	m2_lua_resume(a,b)
#endif

	#define lua_ref(L,lock)	((lock) ? luaL_ref(L, LUA_REGISTRYINDEX) : \
	  (lua_pushstring(L, "unlocked references are obsolete"), lua_error(L), 0))
	#define lua_unref(L,ref)	luaL_unref(L, LUA_REGISTRYINDEX, (ref))
#endif

// #ifdef ENABLE_NEWSTUFF
#ifndef lua_String
#define lua_String const char*
#endif

#ifndef ALUA
#define ALUA(name) int name(lua_State* L)
#endif
// #endif

#ifndef LUA_YIELD
#define LUA_YIELD 1
#endif

#if !defined(_MSC_VER) && defined(__cplusplus)
}
#endif
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
