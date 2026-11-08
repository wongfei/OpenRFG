// Original: volition/libmodules/lua/lua_1_21/ldebug.h
// Implementation: volition/libmodules/lua/lua_1_21/ldebug.cpp
//
// Debug support inside the VM: hooks, the code checker and the runtime error helpers. The lua_* debug API is in lua.h.
//
// The lua.org Lua 5.0 header (MIT license, see the notice at the end of lua.h) as Volition compiled it; the
// declarations carry their rfg.exe VA. Volition's changes are marked "Volition:".
/*
** Auxiliary functions from Debug Interface module
** See Copyright Notice in lua.h
*/

#pragma once


#include "volition/libmodules/lua/lua_1_21/lstate.h"


#define pcRel(pc, p)	(cast(int, (pc) - (p)->code) - 1)

#define getline(f,pc)	(((f)->lineinfo) ? (f)->lineinfo[pc] : 0)

#define resethookcount(L)	(L->hookcount = L->basehookcount)


void luaG_inithooks (lua_State *L); // 0x00C16260 (ldebug.cpp:54)
void luaG_typeerror (lua_State *L, const TObject *o, const char *opname); // 0x00C1A340 (ldebug.cpp:515)
void luaG_concaterror (lua_State *L, StkId p1, StkId p2); // 0x00C1A3D0 (ldebug.cpp:528)
void luaG_aritherror (lua_State *L, const TObject *p1, const TObject *p2); // 0x00C1A3F0 (ldebug.cpp:535)
int luaG_ordererror (lua_State *L, const TObject *p1, const TObject *p2); // 0x00C16880 (ldebug.cpp:543)
void luaG_runerror (lua_State *L, const char *fmt, ...); // 0x00C1FD10 (ldebug.cpp:578)
void luaG_errormsg (lua_State *L); // 0x00C1F690 (ldebug.cpp:565)
int luaG_checkcode (const Proto *pt); // 0x00C16810 (ldebug.cpp:438)
