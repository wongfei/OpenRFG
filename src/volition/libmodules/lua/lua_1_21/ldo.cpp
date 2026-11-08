// Original: volition/libmodules/lua/lua_1_21/ldo.cpp
//
// Lua 5.0: the stack and the calls (protected calls, coroutines, chunk parsing). The lua.org 5.0 release (Copyright (C)
// 1994-2003 Tecgraf, PUC-Rio; MIT license, see the notice at the end of lua.h) as Volition compiled it, as C++
// (lua_unity in rfg.exe; compiled here per file). Volition's changes are marked "Volition:". Each function carries its
// rfg.exe VA and its line in rfg.pdb.

/*
** Stack and Call structure of Lua
** See Copyright Notice in lua.h
*/


#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#define ldo_c

#include "volition/libmodules/lua/lua_1_21/lua.h"

#include "volition/libmodules/lua/lua_1_21/ldebug.h"
#include "volition/libmodules/lua/lua_1_21/ldo.h"
#include "volition/libmodules/lua/lua_1_21/lfunc.h"
#include "volition/libmodules/lua/lua_1_21/lgc.h"
#include "volition/libmodules/lua/lua_1_21/lmem.h"
#include "volition/libmodules/lua/lua_1_21/lobject.h"
#include "volition/libmodules/lua/lua_1_21/lopcodes.h"
#include "volition/libmodules/lua/lua_1_21/lparser.h"
#include "volition/libmodules/lua/lua_1_21/lstate.h"
#include "volition/libmodules/lua/lua_1_21/lstring.h"
#include "volition/libmodules/lua/lua_1_21/ltable.h"
#include "volition/libmodules/lua/lua_1_21/ltm.h"
#include "volition/libmodules/lua/lua_1_21/lundump.h"
#include "volition/libmodules/lua/lua_1_21/lvm.h"
#include "volition/libmodules/lua/lua_1_21/lzio.h"




/*
** {======================================================
** Error-recovery functions (based on long jumps)
** =======================================================
*/


/* chain list of long jump buffers */
struct lua_longjmp {
  struct lua_longjmp *previous;
  jmp_buf b;
  volatile int status;  /* error code */
};


// 0x00C1A440 (:51)
static void seterrorobj (lua_State *L, int errcode, StkId oldtop) {
  switch (errcode) {
    case LUA_ERRMEM: {
      setsvalue2s(oldtop, luaS_new(L, MEMERRMSG));
      break;
    }
    case LUA_ERRERR: {
      setsvalue2s(oldtop, luaS_new(L, "error in error handling"));
      break;
    }
    case LUA_ERRSYNTAX:
    case LUA_ERRRUN: {
      setobjs2s(oldtop, L->top - 1);  /* error message on current top */
      break;
    }
  }
  L->top = oldtop + 1;
}


// 0x00C1D1A0 (:70)
void luaD_throw (lua_State *L, int errcode) {
  /* Volition: the error message is converted to a string first (for a debug print the release build leaves out:
     only the lua_tostring call remains, which turns a number message into a string in place). The NULL test is
     always true (kept as compiled). */
  if (L->top - 1 != NULL && (ttisstring(L->top - 1) || ttisnumber(L->top - 1)))
    lua_tostring(L, -1);
  if (L->errorJmp) {
    L->errorJmp->status = errcode;
    longjmp(L->errorJmp->b, 1);
  }
  else {
    G(L)->panic(L);
    /* Volition: no exit(EXIT_FAILURE) after the panic function; luaD_throw returns to its caller. */
  }
}


// 0x00C168D0 (:114)
int luaD_rawrunprotected (lua_State *L, Pfunc f, void *ud) {
  struct lua_longjmp lj;
  lj.status = 0;
  lj.previous = L->errorJmp;  /* chain new error handler */
  L->errorJmp = &lj;
#pragma warning(suppress : 4611) // Lua reports errors with longjmp
  if (setjmp(lj.b) == 0)
    (*f)(L, ud);
  L->errorJmp = lj.previous;  /* restore old error handler */
  return lj.status;
}


// 0x00C1A4D0 (:126)
static void restore_stack_limit (lua_State *L) {
  L->stack_last = L->stack+L->stacksize-1;
  if (L->size_ci > LUA_MAXCALLS) {  /* there was an overflow? */
    int inuse = (L->ci - L->base_ci);
    if (inuse + 1 < LUA_MAXCALLS)  /* can `undo' overflow? */
      luaD_reallocCI(L, LUA_MAXCALLS);
  }
}

/* }====================================================== */


// 0x00C16960 (:138)
static void correctstack (lua_State *L, TObject *oldstack) {
  CallInfo *ci;
  GCObject *up;
  L->top = (L->top - oldstack) + L->stack;
  for (up = L->openupval; up != NULL; up = up->gch.next)
    gcotouv(up)->v = (gcotouv(up)->v - oldstack) + L->stack;
  for (ci = L->base_ci; ci <= L->ci; ci++) {
    ci->top = (ci->top - oldstack) + L->stack;
    ci->base = (ci->base - oldstack) + L->stack;
  }
  L->base = L->ci->base;
}


// 0x00C169C0 (:152)
void luaD_reallocstack (lua_State *L, int newsize) {
  TObject *oldstack = L->stack;
  luaM_reallocvector(L, L->stack, L->stacksize, newsize, TObject);
  L->stacksize = newsize;
  L->stack_last = L->stack+newsize-1-EXTRA_STACK;
  correctstack(L, oldstack);
}


// 0x00C16A10 (:161)
void luaD_reallocCI (lua_State *L, int newsize) {
  CallInfo *oldci = L->base_ci;
  luaM_reallocvector(L, L->base_ci, L->size_ci, newsize, CallInfo);
  L->size_ci = cast(unsigned short, newsize);
  L->ci = (L->ci - oldci) + L->base_ci;
  L->end_ci = L->base_ci + L->size_ci;
}


// 0x00C16A80 (:170)
void luaD_growstack (lua_State *L, int n) {
  if (n <= L->stacksize)  /* double size is enough? */
    luaD_reallocstack(L, 2*L->stacksize);
  else
    luaD_reallocstack(L, L->stacksize + n + EXTRA_STACK);
}


// 0x00C1D1F0 (:178)
static void luaD_growCI (lua_State *L) {
  if (L->size_ci > LUA_MAXCALLS)  /* overflow while handling overflow? */
    luaD_throw(L, LUA_ERRERR);
  else {
    luaD_reallocCI(L, 2*L->size_ci);
    if (L->size_ci > LUA_MAXCALLS)
      luaG_runerror(L, "stack overflow");
  }
}


// 0x00C16B00 (:189)
void luaD_callhook (lua_State *L, int event, int line) {
  lua_Hook hook = L->hook;
  if (hook && L->allowhook) {
    ptrdiff_t top = savestack(L, L->top);
    ptrdiff_t ci_top = savestack(L, L->ci->top);
    lua_Debug ar;
    ar.event = event;
    ar.currentline = line;
    if (event == LUA_HOOKTAILRET)
      ar.i_ci = 0;  /* tail call; no debug information about it */
    else
      ar.i_ci = L->ci - L->base_ci;
    luaD_checkstack(L, LUA_MINSTACK);  /* ensure minimum stack size */
    L->ci->top = L->top + LUA_MINSTACK;
    L->allowhook = 0;  /* cannot call hooks inside a hook */
    lua_unlock(L);
    (*hook)(L, &ar);
    lua_lock(L);
    lua_assert(!L->allowhook);
    L->allowhook = 1;
    L->ci->top = restorestack(L, ci_top);
    L->top = restorestack(L, top);
  }
}


// 0x00C1BD50 (:215)
static void adjust_varargs (lua_State *L, int nfixargs, StkId base) {
  int i;
  Table *htab;
  TObject nname;
  int actual = L->top - base;  /* actual number of arguments */
  if (actual < nfixargs) {
    luaD_checkstack(L, nfixargs - actual);
    for (; actual < nfixargs; ++actual)
      setnilvalue(L->top++);
  }
  actual -= nfixargs;  /* number of extra arguments */
  htab = luaH_new(L, actual, 1);  /* create `arg' table */
  for (i=0; i<actual; i++)  /* put extra arguments into `arg' table */
    setobj2n(luaH_setnum(L, htab, i+1), L->top - actual + i);
  /* store counter in field `n' */
  setsvalue(&nname, luaS_newliteral(L, "n"));
  setnvalue(luaH_set(L, htab, &nname), cast(lua_Number, actual));
  L->top -= actual;  /* remove extra elements from the stack */
  sethvalue(L->top, htab);
  incr_top(L);
}


// 0x00C1A520 (:238)
static StkId tryfuncTM (lua_State *L, StkId func) {
  const TObject *tm = luaT_gettmbyobj(L, func, TM_CALL);
  StkId p;
  ptrdiff_t funcr = savestack(L, func);
  if (!ttisfunction(tm))
    luaG_typeerror(L, func, "call");
  /* Open a hole inside the stack at `func' */
  for (p = L->top; p > func; p--) setobjs2s(p, p-1);
  incr_top(L);
  func = restorestack(L, funcr);  /* previous call may change stack */
  setobj2s(func, tm);  /* tag method is the new function to be called */
  return func;
}


// 0x00C1D270 (:253)
StkId luaD_precall (lua_State *L, StkId func) {
  LClosure *cl;
  ptrdiff_t funcr = savestack(L, func);
  if (!ttisfunction(func)) /* `func' is not a function? */
    func = tryfuncTM(L, func);  /* check the `function' tag method */
  if (L->ci + 1 == L->end_ci) luaD_growCI(L);
  else condhardstacktests(luaD_reallocCI(L, L->size_ci));
  cl = &clvalue(func)->l;
  if (!cl->isC) {  /* Lua function? prepare its call */
    CallInfo *ci;
    Proto *p = cl->p;
    if (p->is_vararg)  /* varargs? */
      adjust_varargs(L, p->numparams, func+1);
    luaD_checkstack(L, p->maxstacksize);
    ci = ++L->ci;  /* now `enter' new function */
    L->base = L->ci->base = restorestack(L, funcr) + 1;
    ci->top = L->base + p->maxstacksize;
    ci->u.l.savedpc = p->code;  /* starting point */
    ci->u.l.tailcalls = 0;
    ci->state = CI_SAVEDPC;
    while (L->top < ci->top)
      setnilvalue(L->top++);
    L->top = ci->top;
    return NULL;
  }
  else {  /* if is a C function, call it */
    CallInfo *ci;
    int n;
    luaD_checkstack(L, LUA_MINSTACK);  /* ensure minimum stack size */
    ci = ++L->ci;  /* now `enter' new function */
    L->base = L->ci->base = restorestack(L, funcr) + 1;
    ci->top = L->top + LUA_MINSTACK;
    ci->state = CI_C;  /* a C function */
    if (L->hookmask & LUA_MASKCALL)
      luaD_callhook(L, LUA_HOOKCALL, -1);
    lua_unlock(L);
#ifdef LUA_COMPATUPVALUES
    lua_pushupvalues(L);
#endif
    n = (*clvalue(L->base - 1)->c.f)(L);  /* do the actual call */
    lua_lock(L);
    return L->top - n;
  }
}


// 0x00C16BE0 (:299)
static StkId callrethooks (lua_State *L, StkId firstResult) {
  ptrdiff_t fr = savestack(L, firstResult);  /* next call may change stack */
  luaD_callhook(L, LUA_HOOKRET, -1);
  if (!(L->ci->state & CI_C)) {  /* Lua function? */
    while (L->ci->u.l.tailcalls--)  /* call hook for eventual tail calls */
      luaD_callhook(L, LUA_HOOKTAILRET, -1);
  }
  return restorestack(L, fr);
}


// 0x00C16CE0 (:310)
void luaD_poscall (lua_State *L, int wanted, StkId firstResult) { 
  StkId res;
  if (L->hookmask & LUA_MASKRET)
    firstResult = callrethooks(L, firstResult);
  res = L->base - 1;  /* res == final position of 1st result */
  L->ci--;
  L->base = L->ci->base;  /* restore base */
  /* move results to correct place */
  while (wanted != 0 && firstResult < L->top) {
    setobjs2s(res++, firstResult++);
    wanted--;
  }
  while (wanted-- > 0)
    setnilvalue(res++);
  L->top = res;
}


/*
** Call a function (C or Lua). The function to be called is at *func.
** The arguments are on the stack, right after the function.
** When returns, all the results are on the stack, starting at the original
** function position.
*/ 
// 0x00C1E990 (:334)
void luaD_call (lua_State *L, StkId func, int nResults) {
  StkId firstResult;
  lua_assert(!(L->ci->state & CI_CALLING));
  if (++L->nCcalls >= LUA_MAXCCALLS) {
    if (L->nCcalls == LUA_MAXCCALLS)
      luaG_runerror(L, "C stack overflow");
    else if (L->nCcalls >= (LUA_MAXCCALLS + (LUA_MAXCCALLS>>3)))
      luaD_throw(L, LUA_ERRERR);  /* error while handing stack error */
  }
  firstResult = luaD_precall(L, func);
  if (firstResult == NULL)  /* is a Lua function? */
    firstResult = luaV_execute(L);  /* call it */
  luaD_poscall(L, nResults, firstResult);
  L->nCcalls--;
  luaC_checkGC(L);
}


// 0x00C1FD40 (:352)
static void resume (lua_State *L, void *ud) {
  StkId firstResult;
  int nargs = *cast(int *, ud);
  CallInfo *ci = L->ci;
  if (ci == L->base_ci) {  /* no activation record? */
    if (nargs >= L->top - L->base)
      luaG_runerror(L, "cannot resume dead coroutine");
    luaD_precall(L, L->top - (nargs + 1));  /* start coroutine */
  }
  else if (ci->state & CI_YIELD) {  /* inside a yield? */
    if (ci->state & CI_C) {  /* `common' yield? */
      /* finish interrupted execution of `OP_CALL' */
      int nresults;
      lua_assert((ci-1)->state & CI_SAVEDPC);
      lua_assert(GET_OPCODE(*((ci-1)->u.l.savedpc - 1)) == OP_CALL ||
                 GET_OPCODE(*((ci-1)->u.l.savedpc - 1)) == OP_TAILCALL);
      nresults = GETARG_C(*((ci-1)->u.l.savedpc - 1)) - 1;
      luaD_poscall(L, nresults, L->top - nargs);  /* complete it */
      if (nresults >= 0) L->top = L->ci->top;
    }
    else {  /* yielded inside a hook: just continue its execution */
      ci->state &= ~CI_YIELD;
    }
  }
  else
    luaG_runerror(L, "cannot resume non-suspended coroutine");
  firstResult = luaV_execute(L);
  if (firstResult != NULL)   /* return? */
    luaD_poscall(L, LUA_MULTRET, firstResult);  /* finalize this coroutine */
}


// 0x00C1FE00 (:384)
LUA_API int lua_resume (lua_State *L, int nargs) {
  int status;
  lu_byte old_allowhooks;
  lua_lock(L);
  old_allowhooks = L->allowhook;
  lua_assert(L->errfunc == 0 && L->nCcalls == 0);
  status = luaD_rawrunprotected(L, resume, &nargs);
  if (status != 0) {  /* error? */
    L->ci = L->base_ci;  /* go back to initial level */
    L->base = L->ci->base;
    L->nCcalls = 0;
    luaF_close(L, L->base);  /* close eventual pending closures */
    seterrorobj(L, status, L->base);
    L->allowhook = old_allowhooks;
    restore_stack_limit(L);
  }
  lua_unlock(L);
  return status;
}


// 0x00C1FE60 (:405)
LUA_API int lua_yield (lua_State *L, int nresults) {
  CallInfo *ci;
  lua_lock(L);
  ci = L->ci;
  if (L->nCcalls > 0)
    luaG_runerror(L, "attempt to yield across metamethod/C-call boundary");
  if (ci->state & CI_C) {  /* usual yield */
    if ((ci-1)->state & CI_C)
      luaG_runerror(L, "cannot yield a C function");
    if (L->top - nresults > L->base) {  /* is there garbage in the stack? */
      int i;
      for (i=0; i<nresults; i++)  /* move down results */
        setobjs2s(L->base + i, L->top - nresults + i);
      L->top = L->base + nresults;
    }
  } /* else it's an yield inside a hook: nothing to do */
  ci->state |= CI_YIELD;
  lua_unlock(L);
  return -1;
}


// 0x00C1BEF0 (:428)
int luaD_pcall (lua_State *L, Pfunc func, void *u,
                ptrdiff_t old_top, ptrdiff_t ef) {
  int status;
  unsigned short oldnCcalls = L->nCcalls;
  ptrdiff_t old_ci = saveci(L, L->ci);
  lu_byte old_allowhooks = L->allowhook;
  ptrdiff_t old_errfunc = L->errfunc;
  L->errfunc = ef;
  status = luaD_rawrunprotected(L, func, u);
  if (status != 0) {  /* an error occurred? */
    StkId oldtop = restorestack(L, old_top);
    luaF_close(L, oldtop);  /* close eventual pending closures */
    seterrorobj(L, status, oldtop);
    L->nCcalls = oldnCcalls;
    L->ci = restoreci(L, old_ci);
    L->base = L->ci->base;
    L->allowhook = old_allowhooks;
    restore_stack_limit(L);
  }
  L->errfunc = old_errfunc;
  return status;
}



/*
** Execute a protected parser.
*/
struct SParser {  /* data to `f_parser' */
  ZIO *z;
  Mbuffer buff;  /* buffer to be used by the scanner */
  int bin;
};

// 0x00C2FAA0 (:461)
static void f_parser (lua_State *L, void *ud) {
  struct SParser *p;
  Proto *tf;
  Closure *cl;
  luaC_checkGC(L);
  p = cast(struct SParser *, ud);
  tf = p->bin ? luaU_undump(L, p->z, &p->buff) : luaY_parser(L, p->z, &p->buff);
  cl = luaF_newLclosure(L, 0, gt(L));
  cl->l.p = tf;
  setclvalue(L->top, cl);
  incr_top(L);
}


// 0x00C2FBE0 (:475)
int luaD_protectedparser (lua_State *L, ZIO *z, int bin) {
  struct SParser p;
  int status;
  ptrdiff_t oldtopr = savestack(L, L->top);  /* save current top */
  p.z = z; p.bin = bin;
  luaZ_initbuffer(L, &p.buff);
  status = luaD_rawrunprotected(L, f_parser, &p);
  luaZ_freebuffer(L, &p.buff);
  if (status != 0) {  /* error? */
    StkId oldtop = restorestack(L, oldtopr);
    seterrorobj(L, status, oldtop);
  }
  return status;
}
