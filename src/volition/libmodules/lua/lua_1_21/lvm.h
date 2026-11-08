// Original: volition/libmodules/lua/lua_1_21/lvm.h
// Implementation: volition/libmodules/lua/lua_1_21/lvm.cpp
//
// The virtual machine: conversions, table access with tag methods, comparisons, concatenation and luaV_execute.
//
// The lua.org Lua 5.0 header (MIT license, see the notice at the end of lua.h) as Volition compiled it; the
// declarations carry their rfg.exe VA. Volition's changes are marked "Volition:".
/*
** Lua virtual machine
** See Copyright Notice in lua.h
*/

#pragma once


#include "volition/libmodules/lua/lua_1_21/ldo.h"
#include "volition/libmodules/lua/lua_1_21/lobject.h"
#include "volition/libmodules/lua/lua_1_21/ltm.h"


#define tostring(L,o) ((ttype(o) == LUA_TSTRING) || (luaV_tostring(L, o)))

#define tonumber(o,n)	(ttype(o) == LUA_TNUMBER || \
                         (((o) = luaV_tonumber(o,n)) != NULL))

#define equalobj(L,o1,o2) \
	(ttype(o1) == ttype(o2) && luaV_equalval(L, o1, o2))


int luaV_lessthan (lua_State *L, const TObject *l, const TObject *r); // 0x00C19700 (lvm.cpp:258)
int luaV_equalval (lua_State *L, const TObject *t1, const TObject *t2); // 0x00C19820 (lvm.cpp:288)
const TObject *luaV_tonumber (const TObject *obj, TObject *n); // 0x00C192E0 (lvm.cpp:43)
int luaV_tostring (lua_State *L, StkId obj); // 0x00C19330 (lvm.cpp:55)
const TObject *luaV_gettable (lua_State *L, const TObject *t, TObject *key,
                              int loop); // 0x00C1B520 (lvm.cpp:153)
void luaV_settable (lua_State *L, const TObject *t, TObject *key, StkId val); // 0x00C1B5C0 (lvm.cpp:169)
StkId luaV_execute (lua_State *L); // 0x00C1D3C0 (lvm.cpp:397)
void luaV_concat (lua_State *L, int total, int last); // 0x00C1EB90 (lvm.cpp:315)
