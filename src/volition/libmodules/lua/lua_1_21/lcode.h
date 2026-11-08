// Original: volition/libmodules/lua/lua_1_21/lcode.h
// Implementation: volition/libmodules/lua/lua_1_21/lcode.cpp
//
// The code generator: binary and unary operators, jumps and patch lists, registers, constants, expression discharge
// and the instruction encoders.
//
// The lua.org Lua 5.0 header (MIT license, see the notice at the end of lua.h) as Volition compiled it; the
// declarations carry their rfg.exe VA. Volition's changes are marked "Volition:".
/*
** Code generator for Lua
** See Copyright Notice in lua.h
*/

#pragma once

#include "volition/libmodules/lua/lua_1_21/llex.h"
#include "volition/libmodules/lua/lua_1_21/lobject.h"
#include "volition/libmodules/lua/lua_1_21/lopcodes.h"
#include "volition/libmodules/lua/lua_1_21/lparser.h"


/*
** Marks the end of a patch list. It is an invalid value both as an absolute
** address, and as a list link (would link an element to itself).
*/
#define NO_JUMP (-1)


/*
** grep "ORDER OPR" if you change these enums
*/
typedef enum BinOpr {
  OPR_ADD, OPR_SUB, OPR_MULT, OPR_DIV, OPR_POW,
  OPR_CONCAT,
  OPR_NE, OPR_EQ,
  OPR_LT, OPR_LE, OPR_GT, OPR_GE,
  OPR_AND, OPR_OR,
  OPR_NOBINOPR
} BinOpr;

#define binopistest(op)	((op) >= OPR_NE)

typedef enum UnOpr { OPR_MINUS, OPR_NOT, OPR_NOUNOPR } UnOpr;


#define getcode(fs,e)	((fs)->f->code[(e)->info])

#define luaK_codeAsBx(fs,o,A,sBx)	luaK_codeABx(fs,o,A,(sBx)+MAXARG_sBx)

int luaK_code (FuncState *fs, Instruction i, int line); // 0x00C22F00 (lcode.cpp:689)
int luaK_codeABx (FuncState *fs, OpCode o, int A, unsigned int Bx); // 0x00C22FE0 (lcode.cpp:710)
int luaK_codeABC (FuncState *fs, OpCode o, int A, int B, int C); // 0x00C22FB0 (lcode.cpp:704)
void luaK_fixline (FuncState *fs, int line); // 0x00C16130 (lcode.cpp:684)
void luaK_nil (FuncState *fs, int from, int n); // 0x00C283E0 (lcode.cpp:28)
void luaK_reserveregs (FuncState *fs, int n); // 0x00C22D40 (lcode.cpp:189)
void luaK_checkstack (FuncState *fs, int n); // 0x00C22D00 (lcode.cpp:179)
int luaK_stringK (FuncState *fs, TString *s); // 0x00C22E70 (lcode.cpp:226)
int luaK_numberK (FuncState *fs, lua_Number r); // 0x00C22EA0 (lcode.cpp:233)
void luaK_dischargevars (FuncState *fs, expdesc *e); // 0x00C28530 (lcode.cpp:259)
int luaK_exp2anyreg (FuncState *fs, expdesc *e); // 0x00C289B0 (lcode.cpp:374)
void luaK_exp2nextreg (FuncState *fs, expdesc *e); // 0x00C28940 (lcode.cpp:366)
void luaK_exp2val (FuncState *fs, expdesc *e); // 0x00C28A00 (lcode.cpp:388)
int luaK_exp2RK (FuncState *fs, expdesc *e); // 0x00C28A60 (lcode.cpp:396)
void luaK_self (FuncState *fs, expdesc *e, expdesc *key); // 0x00C28C20 (lcode.cpp:450)
void luaK_indexed (FuncState *fs, expdesc *t, expdesc *k); // 0x00C29050 (lcode.cpp:570)
void luaK_goiftrue (FuncState *fs, expdesc *e); // 0x00C28E10 (lcode.cpp:486)
void luaK_goiffalse (FuncState *fs, expdesc *e); // 0x00C28EC0 (lcode.cpp:512)
void luaK_storevar (FuncState *fs, expdesc *var, expdesc *e); // 0x00C28B30 (lcode.cpp:419)
void luaK_setcallreturns (FuncState *fs, expdesc *var, int nresults); // 0x00C160A0 (lcode.cpp:248)
int luaK_jump (FuncState *fs); // 0x00C28470 (lcode.cpp:44)
void luaK_patchlist (FuncState *fs, int list, int target); // 0x00C2E6B0 (lcode.cpp:149)
void luaK_patchtohere (FuncState *fs, int list); // 0x00C28510 (lcode.cpp:159)
void luaK_concat (FuncState *fs, int *l1, int l2); // 0x00C22CA0 (lcode.cpp:165)
int luaK_getlabel (FuncState *fs); // 0x00C15F50 (lcode.cpp:74)
void luaK_prefix (FuncState *fs, UnOpr op, expdesc *v); // 0x00C29070 (lcode.cpp:576)
void luaK_infix (FuncState *fs, BinOpr op, expdesc *v); // 0x00C29160 (lcode.cpp:593)
void luaK_posfix (FuncState *fs, BinOpr op, expdesc *v1, expdesc *v2); // 0x00C292C0 (lcode.cpp:640)
