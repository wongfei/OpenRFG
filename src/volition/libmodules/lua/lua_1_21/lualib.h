// Original: volition/libmodules/lua/lua_1_21/lualib.h
//
// The standard libraries' entry points (base, table, string, math, debug). Lua 5.0's io and os libraries are not in
// rfg.exe.
//
// The lua.org Lua 5.0 header (MIT license, see the notice at the end of lua.h) as Volition compiled it; the
// declarations carry their rfg.exe VA. Volition's changes are marked "Volition:".
/*
** Lua standard libraries
** See Copyright Notice in lua.h
*/


#pragma once

#include "volition/libmodules/lua/lua_1_21/lua.h"


#ifndef LUALIB_API
#define LUALIB_API	LUA_API
#endif


#define LUA_COLIBNAME	"coroutine"
LUALIB_API int luaopen_base (lua_State *L); // 0x00C28350 (lbaselib.cpp:671)

#define LUA_TABLIBNAME	"table"
LUALIB_API int luaopen_table (lua_State *L); // 0x00C263C0 (ltablib.cpp:246)

#define LUA_IOLIBNAME	"io"
#define LUA_OSLIBNAME	"os"
LUALIB_API int luaopen_io (lua_State *L);

#define LUA_STRLIBNAME	"string"
LUALIB_API int luaopen_string (lua_State *L); // 0x00C26050 (lstrlib.cpp:766)

#define LUA_MATHLIBNAME	"math"
LUALIB_API int luaopen_math (lua_State *L); // 0x00C2A880 (lmathlib.cpp:239)

#define LUA_DBLIBNAME	"debug"
LUALIB_API int luaopen_debug (lua_State *L); // 0x00C232F0 (ldblib.cpp:301)


LUALIB_API int luaopen_loadlib (lua_State *L);


/* to help testing the libraries */
#ifndef lua_assert
#define lua_assert(c)		/* empty */
#endif


/* compatibility code */
#define lua_baselibopen	luaopen_base
#define lua_tablibopen	luaopen_table
#define lua_iolibopen	luaopen_io
#define lua_strlibopen	luaopen_string
#define lua_mathlibopen	luaopen_math
#define lua_dblibopen	luaopen_debug
