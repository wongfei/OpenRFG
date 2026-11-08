// Original: volition/libmodules/lua/lua_1_21/ltable.h
// Implementation: volition/libmodules/lua/lua_1_21/ltable.cpp
//
// Tables: main positions, traversal, creation, lookups and stores by number, string and value.
//
// The lua.org Lua 5.0 header (MIT license, see the notice at the end of lua.h) as Volition compiled it; the
// declarations carry their rfg.exe VA. Volition's changes are marked "Volition:".
/*
** Lua tables (hash)
** See Copyright Notice in lua.h
*/

#pragma once

#include "volition/libmodules/lua/lua_1_21/lobject.h"


#define gnode(t,i)	(&(t)->node[i])
#define gkey(n)		(&(n)->i_key)
#define gval(n)		(&(n)->i_val)


const TObject *luaH_getnum (Table *t, int key); // 0x00C18EC0 (ltable.cpp:437)
TObject *luaH_setnum (lua_State *L, Table *t, int key); // 0x00C1B440 (ltable.cpp:499)
const TObject *luaH_getstr (Table *t, TString *key); // 0x00C18F50 (ltable.cpp:456)
const TObject *luaH_get (Table *t, const TObject *key); // 0x00C18F90 (ltable.cpp:470)
TObject *luaH_set (lua_State *L, Table *t, const TObject *key); // 0x00C18FE0 (ltable.cpp:485)
Table *luaH_new (lua_State *L, int narray, int lnhash); // 0x00C18D40 (ltable.cpp:327)
void luaH_free (lua_State *L, Table *t); // 0x00C18DE0 (ltable.cpp:343)
int luaH_next (lua_State *L, Table *t, StkId key); // 0x00C20560 (ltable.cpp:153)

/* exported only for debugging */
Node *luaH_mainposition (const Table *t, const TObject *key); // 0x00C188F0 (ltable.cpp:99)
