// Original: volition/libmodules/lua/lua_1_21/lgc.h
// Implementation: volition/libmodules/lua/lua_1_21/lgc.cpp
//
// The mark and sweep collector: userdata finalizers, sweep, collection and linking new objects.
//
// The lua.org Lua 5.0 header (MIT license, see the notice at the end of lua.h) as Volition compiled it; the
// declarations carry their rfg.exe VA. Volition's changes are marked "Volition:".
/*
** Garbage Collector
** See Copyright Notice in lua.h
*/

#pragma once


#include "volition/libmodules/lua/lua_1_21/lobject.h"


#define luaC_checkGC(L) { lua_assert(!(L->ci->state & CI_CALLING)); \
	if (G(L)->nblocks >= G(L)->GCthreshold) luaC_collectgarbage(L); }


void luaC_separateudata (lua_State *L); // 0x00C1A810 (lgc.cpp:114)
void luaC_callGCTM (lua_State *L); // 0x00C1ABC0 (lgc.cpp:419)
void luaC_sweep (lua_State *L, int all); // 0x00C1C2B0 (lgc.cpp:439)
void luaC_collectgarbage (lua_State *L); // 0x00C1C2F0 (lgc.cpp:485)
void luaC_link (lua_State *L, GCObject *o, lu_byte tt); // 0x00C17A10 (lgc.cpp:513)
