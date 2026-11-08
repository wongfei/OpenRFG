// Original: volition/libmodules/lua/lua_1_21/lparser.h
// Implementation: volition/libmodules/lua/lua_1_21/lparser.cpp
//
// The parser: expression descriptors and the per-function state. The scripts in the packfiles are source text, so the
// parser runs at every script load.
//
// The lua.org Lua 5.0 header (MIT license, see the notice at the end of lua.h) as Volition compiled it; the
// declarations carry their rfg.exe VA. Volition's changes are marked "Volition:".
/*
** Lua Parser
** See Copyright Notice in lua.h
*/

#pragma once

#include "volition/libmodules/lua/lua_1_21/llimits.h"
#include "volition/libmodules/lua/lua_1_21/lobject.h"
#include "volition/libmodules/lua/lua_1_21/ltable.h"
#include "volition/libmodules/lua/lua_1_21/lzio.h"


/*
** Expression descriptor
*/

typedef enum {
  VVOID,	/* no value */
  VNIL,
  VTRUE,
  VFALSE,
  VK,		/* info = index of constant in `k' */
  VLOCAL,	/* info = local register */
  VUPVAL,       /* info = index of upvalue in `upvalues' */
  VGLOBAL,	/* info = index of table; aux = index of global name in `k' */
  VINDEXED,	/* info = table register; aux = index register (or `k') */
  VJMP,		/* info = instruction pc */
  VRELOCABLE,	/* info = instruction pc */
  VNONRELOC,	/* info = result register */
  VCALL		/* info = result register */
} expkind;

typedef struct expdesc {
  expkind k;
  int info, aux;
  int t;  /* patch list of `exit when true' */
  int f;  /* patch list of `exit when false' */
} expdesc;


struct BlockCnt;  /* defined in lparser.c */


/* state needed to generate code for a given function */
typedef struct FuncState {
  Proto *f;  /* current function header */
  Table *h;  /* table to find (and reuse) elements in `k' */
  struct FuncState *prev;  /* enclosing function */
  struct LexState *ls;  /* lexical state */
  struct lua_State *L;  /* copy of the Lua state */
  struct BlockCnt *bl;  /* chain of current blocks */
  int pc;  /* next position to code (equivalent to `ncode') */
  int lasttarget;   /* `pc' of last `jump target' */
  int jpc;  /* list of pending jumps to `pc' */
  int freereg;  /* first free register */
  int nk;  /* number of elements in `k' */
  int np;  /* number of elements in `p' */
  int nlocvars;  /* number of elements in `locvars' */
  int nactvar;  /* number of active local variables */
  expdesc upvalues[MAXUPVALUES];  /* upvalues */
  int actvar[MAXVARS];  /* declared-variable stack */
} FuncState;


Proto *luaY_parser (lua_State *L, ZIO *z, Mbuffer *buff); // 0x00C2F910 (lparser.cpp:366)
