// Original: volition/libmodules/lua/lua_1_21/ldump.cpp
//
// Lua 5.0: precompiled chunk saving. The lua.org 5.0 release (Copyright (C) 1994-2003 Tecgraf, PUC-Rio; MIT license,
// see the notice at the end of lua.h) as Volition compiled it, as C++ (lua_unity in rfg.exe; compiled here per file).
// Volition's changes are marked "Volition:". Each function carries its rfg.exe VA and its line in rfg.pdb.

/*
** save bytecodes
** See Copyright Notice in lua.h
*/

#include <stddef.h>

#define ldump_c

#include "volition/libmodules/lua/lua_1_21/lua.h"

#include "volition/libmodules/lua/lua_1_21/lobject.h"
#include "volition/libmodules/lua/lua_1_21/lopcodes.h"
#include "volition/libmodules/lua/lua_1_21/lstate.h"
#include "volition/libmodules/lua/lua_1_21/lundump.h"

#define DumpVector(b,n,size,D)	DumpBlock(b,(n)*(size),D)
#define DumpLiteral(s,D)	DumpBlock("" s,(sizeof(s))-1,D)

typedef struct {
 lua_State* L;
 lua_Chunkwriter write;
 void* data;
} DumpState;

// 0x00C16D60 (:28)
static void DumpBlock(const void* b, size_t size, DumpState* D)
{
 lua_unlock(D->L);
 (*D->write)(D->L,b,size,D->data);
 lua_lock(D->L);
}

// 0x00C16D80 (:35)
static void DumpByte(int y, DumpState* D)
{
 char x=(char)y;
 DumpBlock(&x,sizeof(x),D);
}

// 0x00C16DA0 (:41)
static void DumpInt(int x, DumpState* D)
{
 DumpBlock(&x,sizeof(x),D);
}

// 0x00C16DC0 (:46)
static void DumpSize(size_t x, DumpState* D)
{
 DumpBlock(&x,sizeof(x),D);
}

// 0x00C16DE0 (:51)
static void DumpNumber(lua_Number x, DumpState* D)
{
 DumpBlock(&x,sizeof(x),D);
}

// 0x00C16E00 (:56)
static void DumpString(TString* s, DumpState* D)
{
 if (s==NULL || getstr(s)==NULL)
  DumpSize(0,D);
 else
 {
  size_t size=s->tsv.len+1;		/* include trailing '\0' */
  DumpSize(size,D);
  DumpBlock(getstr(s),size,D);
 }
}

// 0x00C16E60 (:68)
static void DumpCode(const Proto* f, DumpState* D)
{
 DumpInt(f->sizecode,D);
 DumpVector(f->code,f->sizecode,sizeof(*f->code),D);
}

// 0x00C16EA0 (:74)
static void DumpLocals(const Proto* f, DumpState* D)
{
 int i,n=f->sizelocvars;
 DumpInt(n,D);
 for (i=0; i<n; i++)
 {
  DumpString(f->locvars[i].varname,D);
  DumpInt(f->locvars[i].startpc,D);
  DumpInt(f->locvars[i].endpc,D);
 }
}

// 0x00C16F90 (:86)
static void DumpLines(const Proto* f, DumpState* D)
{
 DumpInt(f->sizelineinfo,D);
 DumpVector(f->lineinfo,f->sizelineinfo,sizeof(*f->lineinfo),D);
}

// 0x00C16FD0 (:92)
static void DumpUpvalues(const Proto* f, DumpState* D)
{
 int i,n=f->sizeupvalues;
 DumpInt(n,D);
 for (i=0; i<n; i++) DumpString(f->upvalues[i],D);
}

static void DumpFunction(const Proto* f, const TString* p, DumpState* D);

// 0x00C17080 (:101)
static void DumpConstants(const Proto* f, DumpState* D)
{
 int i,n;
 DumpInt(n=f->sizek,D);
 for (i=0; i<n; i++)
 {
  const TObject* o=&f->k[i];
  DumpByte(ttype(o),D);
  switch (ttype(o))
  {
   case LUA_TNUMBER:
	DumpNumber(nvalue(o),D);
	break;
   case LUA_TSTRING:
	DumpString(tsvalue(o),D);
	break;
   case LUA_TNIL:
	break;
   default:
	lua_assert(0);			/* cannot happen */
	break;
  }
 }
 DumpInt(n=f->sizep,D);
 for (i=0; i<n; i++) DumpFunction(f->p[i],f->source,D);
}

// 0x00C171C0 (:128)
static void DumpFunction(const Proto* f, const TString* p, DumpState* D)
{
 DumpString((f->source==p) ? NULL : f->source,D);
 DumpInt(f->lineDefined,D);
 DumpByte(f->nups,D);
 DumpByte(f->numparams,D);
 DumpByte(f->is_vararg,D);
 DumpByte(f->maxstacksize,D);
 DumpLines(f,D);
 DumpLocals(f,D);
 DumpUpvalues(f,D);
 DumpConstants(f,D);
 DumpCode(f,D);
}

// 0x00C1A5D0 (:143)
static void DumpHeader(DumpState* D)
{
 DumpLiteral(LUA_SIGNATURE,D);
 DumpByte(VERSION,D);
 DumpByte(luaU_endianness(),D);
 DumpByte(sizeof(int),D);
 DumpByte(sizeof(size_t),D);
 DumpByte(sizeof(Instruction),D);
 DumpByte(SIZE_OP,D);
 DumpByte(SIZE_A,D);
 DumpByte(SIZE_B,D);
 DumpByte(SIZE_C,D);
 DumpByte(sizeof(lua_Number),D);
 DumpNumber(TEST_NUMBER,D);
}

/*
** dump function as precompiled chunk
*/
// 0x00C1A710 (:162)
void luaU_dump (lua_State* L, const Proto* Main, lua_Chunkwriter w, void* data)
{
 DumpState D;
 D.L=L;
 D.write=w;
 D.data=data;
 DumpHeader(&D);
 DumpFunction(Main,NULL,&D);
}
