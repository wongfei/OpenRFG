// Original: volition/libmodules/lua/lua_1_21/ltm.h
// Implementation: volition/libmodules/lua/lua_1_21/ltm.cpp
//
// Tag methods (metamethods): the event enum and the lookups.
//
// The lua.org Lua 5.0 header (MIT license, see the notice at the end of lua.h) as Volition compiled it; the
// declarations carry their rfg.exe VA. Volition's changes are marked "Volition:".
/*
** Tag methods
** See Copyright Notice in lua.h
*/

#pragma once


#include "volition/libmodules/lua/lua_1_21/lobject.h"


/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER TM"
*/
typedef enum {
  TM_INDEX,
  TM_NEWINDEX,
  TM_GC,
  TM_MODE,
  TM_EQ,  /* last tag method with `fast' access */
  TM_ADD,
  TM_SUB,
  TM_MUL,
  TM_DIV,
  TM_POW,
  TM_UNM,
  TM_LT,
  TM_LE,
  TM_CONCAT,
  TM_CALL,
  TM_N		/* number of elements in the enum */
} TMS;



#define gfasttm(g,et,e) \
  (((et)->flags & (1u<<(e))) ? NULL : luaT_gettm(et, e, (g)->tmname[e]))

#define fasttm(l,et,e)	gfasttm(G(l), et, e)


const TObject *luaT_gettm (Table *events, TMS event, TString *ename); // 0x00C19220 (ltm.cpp:48)
const TObject *luaT_gettmbyobj (lua_State *L, const TObject *o, TMS event); // 0x00C19280 (ltm.cpp:59)
void luaT_init (lua_State *L); // 0x00C19090 (ltm.cpp:28)

extern const char *const luaT_typenames[];
