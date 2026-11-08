// Original: volition/libmodules/lua/lua_1_21/lstring.h
// Implementation: volition/libmodules/lua/lua_1_21/lstring.cpp
//
// Interned strings and userdata allocation.
//
// The lua.org Lua 5.0 header (MIT license, see the notice at the end of lua.h) as Volition compiled it; the
// declarations carry their rfg.exe VA. Volition's changes are marked "Volition:".
/*
** String table (keep all strings handled by Lua)
** See Copyright Notice in lua.h
*/

#pragma once


#include "volition/libmodules/lua/lua_1_21/lobject.h"
#include "volition/libmodules/lua/lua_1_21/lstate.h"



#define sizestring(l)	(cast(lu_mem, sizeof(union TString))+ \
                         (cast(lu_mem, l)+1)*sizeof(char))

#define sizeudata(l)	(cast(lu_mem, sizeof(union Udata))+(l))

#define luaS_new(L, s)	(luaS_newlstr(L, s, strlen(s)))
#define luaS_newliteral(L, s)	(luaS_newlstr(L, "" s, \
                                 (sizeof(s)/sizeof(char))-1))

#define luaS_fix(s)	((s)->tsv.marked |= (1<<4))

void luaS_resize (lua_State *L, int newsize); // 0x00C18120 (lstring.cpp:27)
Udata *luaS_newudata (lua_State *L, size_t s); // 0x00C20420 (lstring.cpp:90)
void luaS_freeall (lua_State *L); // 0x00C203F0 (lstring.cpp:21)
TString *luaS_newlstr (lua_State *L, const char *str, size_t l); // 0x00C18250 (lstring.cpp:72)
