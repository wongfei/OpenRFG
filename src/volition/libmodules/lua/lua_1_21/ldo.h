// Original: volition/libmodules/lua/lua_1_21/ldo.h
// Implementation: volition/libmodules/lua/lua_1_21/ldo.cpp
//
// Stack and call handling: errors (longjmp), stack and call info growth, hooks, calls, protected calls and the
// protected parser. lua_resume/lua_yield are in lua.h.
//
// The lua.org Lua 5.0 header (MIT license, see the notice at the end of lua.h) as Volition compiled it; the
// declarations carry their rfg.exe VA. Volition's changes are marked "Volition:".
/*
** Stack and Call structure of Lua
** See Copyright Notice in lua.h
*/

#pragma once


#include "volition/libmodules/lua/lua_1_21/lobject.h"
#include "volition/libmodules/lua/lua_1_21/lstate.h"
#include "volition/libmodules/lua/lua_1_21/lzio.h"


/*
** macro to control inclusion of some hard tests on stack reallocation
*/ 
#ifndef HARDSTACKTESTS
#define condhardstacktests(x)	{ /* empty */ }
#else
#define condhardstacktests(x)	x
#endif


#define luaD_checkstack(L,n)	\
  if ((char *)L->stack_last - (char *)L->top <= (n)*(int)sizeof(TObject)) \
    luaD_growstack(L, n); \
  else condhardstacktests(luaD_reallocstack(L, L->stacksize)); // 0x00C169C0 (ldo.cpp:152)


#define incr_top(L) {luaD_checkstack(L,1); L->top++;}

#define savestack(L,p)		((char *)(p) - (char *)L->stack)
#define restorestack(L,n)	((TObject *)((char *)L->stack + (n)))

#define saveci(L,p)		((char *)(p) - (char *)L->base_ci)
#define restoreci(L,n)		((CallInfo *)((char *)L->base_ci + (n)))


/* type of protected functions, to be ran by `runprotected' */
typedef void (*Pfunc) (lua_State *L, void *ud);

void luaD_resetprotection (lua_State *L);
int luaD_protectedparser (lua_State *L, ZIO *z, int bin); // 0x00C2FBE0 (ldo.cpp:475)
void luaD_callhook (lua_State *L, int event, int line); // 0x00C16B00 (ldo.cpp:189)
StkId luaD_precall (lua_State *L, StkId func); // 0x00C1D270 (ldo.cpp:253)
void luaD_call (lua_State *L, StkId func, int nResults); // 0x00C1E990 (ldo.cpp:334)
int luaD_pcall (lua_State *L, Pfunc func, void *u,
                ptrdiff_t oldtop, ptrdiff_t ef); // 0x00C1BEF0 (ldo.cpp:428)
void luaD_poscall (lua_State *L, int wanted, StkId firstResult); // 0x00C16CE0 (ldo.cpp:310)
void luaD_reallocCI (lua_State *L, int newsize); // 0x00C16A10 (ldo.cpp:161)
void luaD_reallocstack (lua_State *L, int newsize); // 0x00C169C0 (ldo.cpp:152)
void luaD_growstack (lua_State *L, int n); // 0x00C16A80 (ldo.cpp:170)

void luaD_throw (lua_State *L, int errcode); // 0x00C1D1A0 (ldo.cpp:70)
int luaD_rawrunprotected (lua_State *L, Pfunc f, void *ud); // 0x00C168D0 (ldo.cpp:114)
