// Original: volition/libmodules/lua/lua_1_21/lfunc.h
// Implementation: volition/libmodules/lua/lua_1_21/lfunc.cpp
//
// Closures, upvalues and prototypes: creation, upvalue lookup and closing, freeing, local names.
//
// The lua.org Lua 5.0 header (MIT license, see the notice at the end of lua.h) as Volition compiled it; the
// declarations carry their rfg.exe VA. Volition's changes are marked "Volition:".
/*
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in lua.h
*/

#pragma once


#include "volition/libmodules/lua/lua_1_21/lobject.h"


Proto *luaF_newproto (lua_State *L); // 0x00C210C0 (lfunc.cpp:77)
Closure *luaF_newCclosure (lua_State *L, int nelems); // 0x00C21040 (lfunc.cpp:29)
Closure *luaF_newLclosure (lua_State *L, int nelems, TObject *e); // 0x00C1A750 (lfunc.cpp:38)
UpVal *luaF_findupval (lua_State *L, StkId level); // 0x00C172F0 (lfunc.cpp:48)
void luaF_close (lua_State *L, StkId level); // 0x00C1A7B0 (lfunc.cpp:66)
void luaF_freeproto (lua_State *L, Proto *f); // 0x00C17340 (lfunc.cpp:102)
void luaF_freeclosure (lua_State *L, Closure *c); // 0x00C173E0 (lfunc.cpp:113)

const char *luaF_getlocalname (const Proto *func, int local_number, int pc); // 0x00C17420 (lfunc.cpp:124)
