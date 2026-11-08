// Original: volition/libmodules/lua/lua_1_21/lapi.h
// Implementation: volition/libmodules/lua/lua_1_21/lapi.cpp
//
// The one internal function of the API module.
//
// The lua.org Lua 5.0 header (MIT license, see the notice at the end of lua.h) as Volition compiled it; the
// declarations carry their rfg.exe VA. Volition's changes are marked "Volition:".
/*
** Auxiliary functions from Lua API
** See Copyright Notice in lua.h
*/

#pragma once


#include "volition/libmodules/lua/lua_1_21/lobject.h"


void luaA_pushobject (lua_State *L, const TObject *o); // 0x00C19A60 (lapi.cpp:87)
