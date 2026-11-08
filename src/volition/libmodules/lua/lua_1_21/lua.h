// Original: volition/libmodules/lua/lua_1_21/lua.h
//
// The Lua C API (lapi.cpp, plus the lua_* functions of ldebug.cpp, ldo.cpp and lstate.cpp) and the debug record
// lua_Debug. Volition's additions are in lua_lib (lua_scripts.h).
//
// The lua.org Lua 5.0 header (MIT license, see the notice at the end of lua.h) as Volition compiled it; the
// declarations carry their rfg.exe VA. Volition's changes are marked "Volition:".
/*
** Lua - An Extensible Extension Language
** Tecgraf: Computer Graphics Technology Group, PUC-Rio, Brazil
** http://www.lua.org	mailto:info@lua.org
** See Copyright Notice at the end of this file
*/


#pragma once

#include <stdarg.h>
#include <stddef.h>


#define LUA_VERSION	"Lua 5.0"
#define LUA_COPYRIGHT	"Copyright (C) 1994-2003 Tecgraf, PUC-Rio"
#define LUA_AUTHORS 	"R. Ierusalimschy, L. H. de Figueiredo & W. Celes"



/* option for multiple returns in `lua_pcall' and `lua_call' */
#define LUA_MULTRET	(-1)


/*
** pseudo-indices
*/
#define LUA_REGISTRYINDEX	(-10000)
#define LUA_GLOBALSINDEX	(-10001)
#define lua_upvalueindex(i)	(LUA_GLOBALSINDEX-(i))


/* error codes for `lua_load' and `lua_pcall' */
#define LUA_ERRRUN	1
#define LUA_ERRFILE	2
#define LUA_ERRSYNTAX	3
#define LUA_ERRMEM	4
#define LUA_ERRERR	5


typedef struct lua_State lua_State;

typedef int (*lua_CFunction) (lua_State *L);


/*
** functions that read/write blocks when loading/dumping Lua chunks
*/
typedef const char * (*lua_Chunkreader) (lua_State *L, void *ud, size_t *sz);

typedef int (*lua_Chunkwriter) (lua_State *L, const void* p,
                                size_t sz, void* ud);


/*
** basic types
*/
#define LUA_TNONE	(-1)

#define LUA_TNIL	0
#define LUA_TBOOLEAN	1
#define LUA_TLIGHTUSERDATA	2
#define LUA_TNUMBER	3
#define LUA_TSTRING	4
#define LUA_TTABLE	5
#define LUA_TFUNCTION	6
#define LUA_TUSERDATA	7
#define LUA_TTHREAD	8


/* minimum Lua stack available to a C function */
#define LUA_MINSTACK	20


/*
** generic extra include file
*/
#ifdef LUA_USER_H
#include LUA_USER_H
#endif


/* type of numbers in Lua */
#ifndef LUA_NUMBER
typedef double lua_Number;
#else
typedef LUA_NUMBER lua_Number;
#endif


/* mark for all API functions */
#ifndef LUA_API
#define LUA_API		extern
#endif


/*
** state manipulation
*/
LUA_API lua_State *lua_open (void); // 0x00C219E0 (lstate.cpp:181)
LUA_API void       lua_close (lua_State *L); // 0x00C21A60 (lstate.cpp:206)
LUA_API lua_State *lua_newthread (lua_State *L); // 0x00C20E50 (lapi.cpp:132)

LUA_API lua_CFunction lua_atpanic (lua_State *L, lua_CFunction panicf); // 0x00C15780 (lapi.cpp:122)


/*
** basic stack manipulation
*/
LUA_API int   lua_gettop (lua_State *L); // 0x00C157A0 (lapi.cpp:151)
LUA_API void  lua_settop (lua_State *L, int idx); // 0x00C157B0 (lapi.cpp:156)
LUA_API void  lua_pushvalue (lua_State *L, int idx); // 0x00C158C0 (lapi.cpp:202)
LUA_API void  lua_remove (lua_State *L, int idx); // 0x00C15800 (lapi.cpp:172)
LUA_API void  lua_insert (lua_State *L, int idx); // 0x00C15840 (lapi.cpp:182)
LUA_API void  lua_replace (lua_State *L, int idx); // 0x00C15890 (lapi.cpp:193)
LUA_API int   lua_checkstack (lua_State *L, int sz); // 0x00C19AA0 (lapi.cpp:93)

LUA_API void  lua_xmove (lua_State *from, lua_State *to, int n); // 0x00C15730 (lapi.cpp:109)


/*
** access functions (stack -> C)
*/

LUA_API int             lua_isnumber (lua_State *L, int idx); // 0x00C19B00 (lapi.cpp:234)
LUA_API int             lua_isstring (lua_State *L, int idx); // 0x00C15960 (lapi.cpp:241)
LUA_API int             lua_iscfunction (lua_State *L, int idx); // 0x00C15930 (lapi.cpp:228)
LUA_API int             lua_isuserdata (lua_State *L, int idx); // 0x00C15990 (lapi.cpp:247)
LUA_API int             lua_type (lua_State *L, int idx); // 0x00C158F0 (lapi.cpp:216)
LUA_API const char     *lua_typename (lua_State *L, int tp); // 0x00C15910 (lapi.cpp:222)

LUA_API int            lua_equal (lua_State *L, int idx1, int idx2); // 0x00C19B90 (lapi.cpp:261)
LUA_API int            lua_rawequal (lua_State *L, int idx1, int idx2); // 0x00C19B50 (lapi.cpp:253)
LUA_API int            lua_lessthan (lua_State *L, int idx1, int idx2); // 0x00C19BE0 (lapi.cpp:274)

LUA_API lua_Number      lua_tonumber (lua_State *L, int idx); // 0x00C19C20 (lapi.cpp:288)
LUA_API int             lua_toboolean (lua_State *L, int idx); // 0x00C159C0 (lapi.cpp:298)
LUA_API const char     *lua_tostring (lua_State *L, int idx); // 0x00C1C470 (lapi.cpp:304)
LUA_API size_t          lua_strlen (lua_State *L, int idx); // 0x00C19C80 (lapi.cpp:321)
LUA_API lua_CFunction   lua_tocfunction (lua_State *L, int idx); // 0x00C159F0 (lapi.cpp:337)
LUA_API void	       *lua_touserdata (lua_State *L, int idx); // 0x00C15A20 (lapi.cpp:343)
LUA_API lua_State      *lua_tothread (lua_State *L, int idx); // 0x00C15A50 (lapi.cpp:354)
LUA_API const void     *lua_topointer (lua_State *L, int idx); // 0x00C15A70 (lapi.cpp:360)


/*
** push functions (C -> stack)
*/
LUA_API void  lua_pushnil (lua_State *L); // 0x00C15AD0 (lapi.cpp:383)
LUA_API void  lua_pushnumber (lua_State *L, lua_Number n); // 0x00C15AF0 (lapi.cpp:391)
LUA_API void  lua_pushlstring (lua_State *L, const char *s, size_t l); // 0x00C1C4D0 (lapi.cpp:399)
LUA_API void  lua_pushstring (lua_State *L, const char *s); // 0x00C1C540 (lapi.cpp:408)
LUA_API const char *lua_pushvfstring (lua_State *L, const char *fmt,
                                                    va_list argp); // 0x00C1FBB0 (lapi.cpp:417)
LUA_API const char *lua_pushfstring (lua_State *L, const char *fmt, ...); // 0x00C1FC10 (lapi.cpp:427)
LUA_API void  lua_pushcclosure (lua_State *L, lua_CFunction fn, int n); // 0x00C21F90 (lapi.cpp:440)
LUA_API void  lua_pushboolean (lua_State *L, int b); // 0x00C15B10 (lapi.cpp:456)
LUA_API void  lua_pushlightuserdata (lua_State *L, void *p); // 0x00C15B30 (lapi.cpp:464)


/*
** get functions (Lua -> stack)
*/
LUA_API void  lua_gettable (lua_State *L, int idx); // 0x00C1B960 (lapi.cpp:478)
LUA_API void  lua_rawget (lua_State *L, int idx); // 0x00C19CD0 (lapi.cpp:487)
LUA_API void  lua_rawgeti (lua_State *L, int idx, int n); // 0x00C19D60 (lapi.cpp:497)
LUA_API void  lua_newtable (lua_State *L); // 0x00C1C580 (lapi.cpp:508)
LUA_API void *lua_newuserdata (lua_State *L, size_t sz); // 0x00C20F00 (lapi.cpp:840)
LUA_API int   lua_getmetatable (lua_State *L, int objindex); // 0x00C15B50 (lapi.cpp:517)
LUA_API void  lua_getfenv (lua_State *L, int idx); // 0x00C15BB0 (lapi.cpp:545)


/*
** set functions (stack -> Lua)
*/
LUA_API void  lua_settable (lua_State *L, int idx); // 0x00C1BA00 (lapi.cpp:560)
LUA_API void  lua_rawset (lua_State *L, int idx); // 0x00C19DA0 (lapi.cpp:571)
LUA_API void  lua_rawseti (lua_State *L, int idx, int n); // 0x00C1BA30 (lapi.cpp:583)
LUA_API int   lua_setmetatable (lua_State *L, int objindex); // 0x00C15BF0 (lapi.cpp:595)
LUA_API int   lua_setfenv (lua_State *L, int idx); // 0x00C15C50 (lapi.cpp:623)


/*
** `load' and `call' functions (load and run Lua code)
*/
LUA_API void  lua_call (lua_State *L, int nargs, int nresults); // 0x00C1EE30 (lapi.cpp:644)
LUA_API int   lua_pcall (lua_State *L, int nargs, int nresults, int errfunc); // 0x00C1EE70 (lapi.cpp:671)
LUA_API int lua_cpcall (lua_State *L, lua_CFunction func, void *ud); // 0x00C220C0 (lapi.cpp:707)
LUA_API int   lua_load (lua_State *L, lua_Chunkreader reader, void *dt,
                        const char *chunkname); // 0x00C2FC60 (lapi.cpp:720)

LUA_API int lua_dump (lua_State *L, lua_Chunkwriter writer, void *data); // 0x00C1BAB0 (lapi.cpp:734)


/*
** coroutine functions
*/
LUA_API int  lua_yield (lua_State *L, int nresults); // 0x00C1FE60 (ldo.cpp:405)
LUA_API int  lua_resume (lua_State *L, int narg); // 0x00C1FE00 (ldo.cpp:384)

/*
** garbage-collection functions
*/
LUA_API int   lua_getgcthreshold (lua_State *L); // 0x00C15CA0 (lapi.cpp:760)
LUA_API int   lua_getgccount (lua_State *L); // 0x00C15CB0 (lapi.cpp:768)
LUA_API void  lua_setgcthreshold (lua_State *L, int newthreshold); // 0x00C1C5F0 (lapi.cpp:776)

/*
** miscellaneous functions
*/

LUA_API const char *lua_version (void); // 0x00C15CC0 (lapi.cpp:792)

LUA_API int   lua_error (lua_State *L); // 0x00C1FC70 (lapi.cpp:797)

LUA_API int   lua_next (lua_State *L, int idx); // 0x00C20EC0 (lapi.cpp:806)

LUA_API void  lua_concat (lua_State *L, int n); // 0x00C1EED0 (lapi.cpp:823)



/* 
** ===============================================================
** some useful macros
** ===============================================================
*/

#define lua_boxpointer(L,u) \
	(*(void **)(lua_newuserdata(L, sizeof(void *))) = (u))

#define lua_unboxpointer(L,i)	(*(void **)(lua_touserdata(L, i)))

#define lua_pop(L,n)		lua_settop(L, -(n)-1)

#define lua_register(L,n,f) \
	(lua_pushstring(L, n), \
	 lua_pushcfunction(L, f), \
	 lua_settable(L, LUA_GLOBALSINDEX))

#define lua_pushcfunction(L,f)	lua_pushcclosure(L, f, 0)

#define lua_isfunction(L,n)	(lua_type(L,n) == LUA_TFUNCTION)
#define lua_istable(L,n)	(lua_type(L,n) == LUA_TTABLE)
#define lua_islightuserdata(L,n)	(lua_type(L,n) == LUA_TLIGHTUSERDATA)
#define lua_isnil(L,n)		(lua_type(L,n) == LUA_TNIL)
#define lua_isboolean(L,n)	(lua_type(L,n) == LUA_TBOOLEAN)
#define lua_isnone(L,n)		(lua_type(L,n) == LUA_TNONE)
#define lua_isnoneornil(L, n)	(lua_type(L,n) <= 0)

#define lua_pushliteral(L, s)	\
	lua_pushlstring(L, "" s, (sizeof(s)/sizeof(char))-1)



/*
** compatibility macros and functions
*/


LUA_API int lua_pushupvalues (lua_State *L); // 0x00C19DE0 (lapi.cpp:852)

#define lua_getregistry(L)	lua_pushvalue(L, LUA_REGISTRYINDEX)
#define lua_setglobal(L,s)	\
   (lua_pushstring(L, s), lua_insert(L, -2), lua_settable(L, LUA_GLOBALSINDEX))

#define lua_getglobal(L,s)	\
		(lua_pushstring(L, s), lua_gettable(L, LUA_GLOBALSINDEX))


/* compatibility with ref system */

/* pre-defined references */
#define LUA_NOREF	(-2)
#define LUA_REFNIL	(-1)

#define lua_ref(L,lock)	((lock) ? luaL_ref(L, LUA_REGISTRYINDEX) : \
      (lua_pushstring(L, "unlocked references are obsolete"), lua_error(L), 0))

#define lua_unref(L,ref)	luaL_unref(L, LUA_REGISTRYINDEX, (ref))

#define lua_getref(L,ref)	lua_rawgeti(L, LUA_REGISTRYINDEX, ref)



/*
** {======================================================================
** useful definitions for Lua kernel and libraries
** =======================================================================
*/

/* formats for Lua numbers */
#ifndef LUA_NUMBER_SCAN
#define LUA_NUMBER_SCAN		"%lf"
#endif

#ifndef LUA_NUMBER_FMT
#define LUA_NUMBER_FMT		"%.14g"
#endif

/* }====================================================================== */


/*
** {======================================================================
** Debug API
** =======================================================================
*/


/*
** Event codes
*/
#define LUA_HOOKCALL	0
#define LUA_HOOKRET	1
#define LUA_HOOKLINE	2
#define LUA_HOOKCOUNT	3
#define LUA_HOOKTAILRET 4


/*
** Event masks
*/
#define LUA_MASKCALL	(1 << LUA_HOOKCALL)
#define LUA_MASKRET	(1 << LUA_HOOKRET)
#define LUA_MASKLINE	(1 << LUA_HOOKLINE)
#define LUA_MASKCOUNT	(1 << LUA_HOOKCOUNT)

typedef struct lua_Debug lua_Debug;  /* activation record */

typedef void (*lua_Hook) (lua_State *L, lua_Debug *ar);


LUA_API int lua_getstack (lua_State *L, int level, lua_Debug *ar); // 0x00C16310 (ldebug.cpp:94)
LUA_API int lua_getinfo (lua_State *L, const char *what, lua_Debug *ar); // 0x00C20F90 (ldebug.cpp:238)
LUA_API const char *lua_getlocal (lua_State *L, const lua_Debug *ar, int n); // 0x00C19EF0 (ldebug.cpp:122)
LUA_API const char *lua_setlocal (lua_State *L, const lua_Debug *ar, int n); // 0x00C19F90 (ldebug.cpp:140)
LUA_API const char *lua_getupvalue (lua_State *L, int funcindex, int n); // 0x00C15D20 (lapi.cpp:889)
LUA_API const char *lua_setupvalue (lua_State *L, int funcindex, int n); // 0x00C15DA0 (lapi.cpp:903)

LUA_API int lua_sethook (lua_State *L, lua_Hook func, int mask, int count); // 0x00C162A0 (ldebug.cpp:65)
LUA_API lua_Hook lua_gethook (lua_State *L); // 0x00C162E0 (ldebug.cpp:79)
LUA_API int lua_gethookmask (lua_State *L); // 0x00C162F0 (ldebug.cpp:84)
LUA_API int lua_gethookcount (lua_State *L); // 0x00C16300 (ldebug.cpp:89)


#define LUA_IDSIZE	60

struct lua_Debug {
  int event;
  const char *name;	/* (n) */
  const char *namewhat;	/* (n) `global', `local', `field', `method' */
  const char *what;	/* (S) `Lua', `C', `main', `tail' */
  const char *source;	/* (S) */
  int currentline;	/* (l) */
  int nups;		/* (u) number of upvalues */
  int linedefined;	/* (S) */
  char short_src[LUA_IDSIZE]; /* (S) */
  /* private part */
  int i_ci;  /* active function */
};

/* }====================================================================== */


/******************************************************************************
* Copyright (C) 1994-2003 Tecgraf, PUC-Rio.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/
