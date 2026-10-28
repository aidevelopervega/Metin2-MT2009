#ifndef __INC_METIN2_LUA_INCL_H__
#define __INC_METIN2_LUA_INCL_H__

#if !defined(_MSC_VER) && defined(__cplusplus)
extern "C" {
#endif

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#if LUA_V != 551
	#error "Only Lua 5.5.1 (LUA_V == 551) is supported"
#endif

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

static inline int m2_lua_resume(lua_State* L, int nargs) {
	int nresults = 0;
	return (lua_resume)(L, NULL, nargs, &nresults);
}
#undef lua_resume
#define lua_resume(a,b)	m2_lua_resume(a,b)

#define lua_ref(L,lock)	((lock) ? luaL_ref(L, LUA_REGISTRYINDEX) : \
  (lua_pushstring(L, "unlocked references are obsolete"), lua_error(L), 0))
#define lua_unref(L,ref)	luaL_unref(L, LUA_REGISTRYINDEX, (ref))

#ifndef lua_String
#define lua_String const char*
#endif

#ifndef ALUA
#define ALUA(name) int name([[maybe_unused]] lua_State* L)
#endif

#ifndef LUA_YIELD
#define LUA_YIELD 1
#endif

#include <stdint.h>
#include <stdlib.h>

static inline uint64_t m2_lua_touint64(lua_State* L, int idx) {
	if (lua_isinteger(L, idx))
		return static_cast<uint64_t>(lua_tointeger(L, idx));
	if (lua_isnumber(L, idx))
		return static_cast<uint64_t>(lua_tonumber(L, idx));
	if (lua_isstring(L, idx)) {
		const char* s = lua_tostring(L, idx);
		if (s)
			return static_cast<uint64_t>(strtoull(s, NULL, 10));
	}
	return 0ULL;
}

static inline void m2_lua_pushuint64(lua_State* L, uint64_t v) {
	lua_pushinteger(L, static_cast<lua_Integer>(v));
}

#if !defined(_MSC_VER) && defined(__cplusplus)
}
#endif

#endif // __INC_METIN2_LUA_INCL_H__
