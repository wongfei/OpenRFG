// Original: volition/libmodules/lua/lua_1_21/lauxlib.h
// Implementation: volition/libmodules/lua/lua_1_21/lauxlib.cpp
//
// The auxiliary library: argument checks, metatables, library registration (luaL_reg), the string buffer, references,
// chunk loading and the 5.0 compatibility lua_dofile/lua_dobuffer/lua_dostring.
//
// The lua.org Lua 5.0 header (MIT license, see the notice at the end of lua.h) as Volition compiled it; the
// declarations carry their rfg.exe VA. Volition's changes are marked "Volition:".
/*
** Auxiliary functions for building Lua libraries
** See Copyright Notice in lua.h
*/


#pragma once


#include <stddef.h>
#include <stdio.h>

#include "volition/libmodules/lua/lua_1_21/lua.h"


#ifndef LUALIB_API
#define LUALIB_API	LUA_API
#endif



typedef struct luaL_reg {
  const char *name;
  lua_CFunction func;
} luaL_reg;


LUALIB_API void luaL_openlib (lua_State *L, const char *libname,
                               const luaL_reg *l, int nup); // 0x00C22280 (lauxlib.cpp:228)
LUALIB_API int luaL_getmetafield (lua_State *L, int obj, const char *e); // 0x00C1C8F0 (lauxlib.cpp:201)
LUALIB_API int luaL_callmeta (lua_State *L, int obj, const char *e); // 0x00C1EF70 (lauxlib.cpp:217)
LUALIB_API int luaL_typerror (lua_State *L, int narg, const char *tname); // 0x00C264D0 (lauxlib.cpp:64)
LUALIB_API int luaL_argerror (lua_State *L, int numarg, const char *extramsg); // 0x00C263E0 (lauxlib.cpp:48)
LUALIB_API const char *luaL_checklstring (lua_State *L, int numArg, size_t *l); // 0x00C265F0 (lauxlib.cpp:168)
LUALIB_API const char *luaL_optlstring (lua_State *L, int numArg,
                                           const char *def, size_t *l); // 0x00C266E0 (lauxlib.cpp:177)
LUALIB_API lua_Number luaL_checknumber (lua_State *L, int numArg); // 0x00C26740 (lauxlib.cpp:187)
LUALIB_API lua_Number luaL_optnumber (lua_State *L, int nArg, lua_Number def); // 0x00C26840 (lauxlib.cpp:195)

LUALIB_API void luaL_checkstack (lua_State *L, int sz, const char *msg); // 0x00C22220 (lauxlib.cpp:150)
LUALIB_API void luaL_checktype (lua_State *L, int narg, int t); // 0x00C26580 (lauxlib.cpp:156)
LUALIB_API void luaL_checkany (lua_State *L, int narg); // 0x00C265C0 (lauxlib.cpp:162)

LUALIB_API int   luaL_newmetatable (lua_State *L, const char *tname); // 0x00C1C660 (lauxlib.cpp:111)
LUALIB_API void  luaL_getmetatable (lua_State *L, const char *tname); // 0x00C1C7B0 (lauxlib.cpp:128)
LUALIB_API void *luaL_checkudata (lua_State *L, int ud, const char *tname); // 0x00C1C810 (lauxlib.cpp:134)

LUALIB_API void luaL_where (lua_State *L, int lvl); // 0x00C22100 (lauxlib.cpp:76)
LUALIB_API int luaL_error (lua_State *L, const char *fmt, ...); // 0x00C22190 (lauxlib.cpp:89)

LUALIB_API int luaL_findstring (const char *st, const char *const lst[]); // 0x00C15E20 (lauxlib.cpp:102)

LUALIB_API int luaL_ref (lua_State *L, int t); // 0x00C1CD90 (lauxlib.cpp:421)
LUALIB_API void luaL_unref (lua_State *L, int t, int ref); // 0x00C1BB10 (lauxlib.cpp:447)

LUALIB_API int luaL_getn (lua_State *L, int t); // 0x00C1CC20 (lauxlib.cpp:303)
LUALIB_API void luaL_setn (lua_State *L, int t, int n); // 0x00C1CB00 (lauxlib.cpp:284)

LUALIB_API int luaL_loadfile (lua_State *L, const char *filename); // 0x00C15E90 (lauxlib.cpp:494)
LUALIB_API int luaL_loadbuffer (lua_State *L, const char *buff, size_t sz,
                                const char *name); // 0x00C2FCF0 (lauxlib.cpp:551)



/*
** ===============================================================
** some useful macros
** ===============================================================
*/

#define luaL_argcheck(L, cond,numarg,extramsg) if (!(cond)) \
                                               luaL_argerror(L, numarg,extramsg)
#define luaL_checkstring(L,n)	(luaL_checklstring(L, (n), NULL))
#define luaL_optstring(L,n,d)	(luaL_optlstring(L, (n), (d), NULL))
#define luaL_checkint(L,n)	((int)luaL_checknumber(L, n))
#define luaL_checklong(L,n)	((long)luaL_checknumber(L, n))
#define luaL_optint(L,n,d)	((int)luaL_optnumber(L, n,(lua_Number)(d)))
#define luaL_optlong(L,n,d)	((long)luaL_optnumber(L, n,(lua_Number)(d)))


/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/


#ifndef LUAL_BUFFERSIZE
#define LUAL_BUFFERSIZE	  BUFSIZ
#endif


typedef struct luaL_Buffer {
  char *p;			/* current position in buffer */
  int lvl;  /* number of strings in the stack (level) */
  lua_State *L;
  char buffer[LUAL_BUFFERSIZE];
} luaL_Buffer;

#define luaL_putchar(B,c) \
  ((void)((B)->p < ((B)->buffer+LUAL_BUFFERSIZE) || luaL_prepbuffer(B)), \
   (*(B)->p++ = (char)(c)))

#define luaL_addsize(B,n)	((B)->p += (n))

LUALIB_API void luaL_buffinit (lua_State *L, luaL_Buffer *B); // 0x00C15E70 (lauxlib.cpp:412)
LUALIB_API char *luaL_prepbuffer (luaL_Buffer *B); // 0x00C1F1A0 (lauxlib.cpp:370)
LUALIB_API void luaL_addlstring (luaL_Buffer *B, const char *s, size_t l); // 0x00C1F1E0 (lauxlib.cpp:377)
LUALIB_API void luaL_addstring (luaL_Buffer *B, const char *s); // 0x00C1F2B0 (lauxlib.cpp:383)
LUALIB_API void luaL_addvalue (luaL_Buffer *B); // 0x00C1F320 (lauxlib.cpp:395)
LUALIB_API void luaL_pushresult (luaL_Buffer *B); // 0x00C1F2E0 (lauxlib.cpp:388)


/* }====================================================== */



/*
** Compatibility macros and functions
*/

LUALIB_API int   lua_dofile (lua_State *L, const char *filename); // 0x00C1F490 (lauxlib.cpp:592)
LUALIB_API int   lua_dostring (lua_State *L, const char *str); // 0x00C2FDA0 (lauxlib.cpp:603)
LUALIB_API int   lua_dobuffer (lua_State *L, const char *buff, size_t sz,
                               const char *n); // 0x00C2FD20 (lauxlib.cpp:598)


#define luaL_check_lstr 	luaL_checklstring
#define luaL_opt_lstr 	luaL_optlstring 
#define luaL_check_number 	luaL_checknumber 
#define luaL_opt_number	luaL_optnumber
#define luaL_arg_check	luaL_argcheck
#define luaL_check_string	luaL_checkstring
#define luaL_opt_string	luaL_optstring
#define luaL_check_int	luaL_checkint
#define luaL_check_long	luaL_checklong
#define luaL_opt_int	luaL_optint
#define luaL_opt_long	luaL_optlong
