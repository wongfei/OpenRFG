// Original: volition/libmodules/lua/lua_1_21/lapi.cpp
//
// Lua 5.0: the C API. The lua.org 5.0 release (Copyright (C) 1994-2003 Tecgraf, PUC-Rio; MIT license, see the notice at
// the end of lua.h) as Volition compiled it, as C++ (lua_unity in rfg.exe; compiled here per file). Volition's changes
// are marked "Volition:". Each function carries its rfg.exe VA and its line in rfg.pdb.

/*
** Lua API
** See Copyright Notice in lua.h
*/


#include <assert.h>
#include <string.h>

#define lapi_c

#include "volition/libmodules/lua/lua_1_21/lua.h"

#include "volition/libmodules/lua/lua_1_21/lapi.h"
#include "volition/libmodules/lua/lua_1_21/ldebug.h"
#include "volition/libmodules/lua/lua_1_21/ldo.h"
#include "volition/libmodules/lua/lua_1_21/lfunc.h"
#include "volition/libmodules/lua/lua_1_21/lgc.h"
#include "volition/libmodules/lua/lua_1_21/lmem.h"
#include "volition/libmodules/lua/lua_1_21/lobject.h"
#include "volition/libmodules/lua/lua_1_21/lstate.h"
#include "volition/libmodules/lua/lua_1_21/lstring.h"
#include "volition/libmodules/lua/lua_1_21/ltable.h"
#include "volition/libmodules/lua/lua_1_21/ltm.h"
#include "volition/libmodules/lua/lua_1_21/lundump.h"
#include "volition/libmodules/lua/lua_1_21/lvm.h"


/* Volition: no lua_ident string (rfg.exe holds no "$Lua: ..." text). */



#ifndef api_check
#define api_check(L, o)		/*{ assert(o); }*/
#endif

#define api_checknelems(L, n)	api_check(L, (n) <= (L->top - L->base))

#define api_incr_top(L)   {api_check(L, L->top < L->ci->top); L->top++;}




// 0x00C15620 (:42)
static TObject *negindex (lua_State *L, int idx) {
  if (idx > LUA_REGISTRYINDEX) {
    api_check(L, idx != 0 && -idx <= L->top - L->base);
    return L->top+idx;
  }
  else switch (idx) {  /* pseudo-indices */
    case LUA_REGISTRYINDEX: return registry(L);
    case LUA_GLOBALSINDEX: return gt(L);
    default: {
      TObject *func = (L->base - 1);
      idx = LUA_GLOBALSINDEX - idx;
      lua_assert(iscfunction(func));
      return (idx <= clvalue(func)->c.nupvalues)
                ? &clvalue(func)->c.upvalue[idx-1]
                : NULL;
    }
  }
}


// 0x00C15670 (:62)
static TObject *luaA_index (lua_State *L, int idx) {
  if (idx > 0) {
    api_check(L, idx <= L->top - L->base);
    return L->base + idx - 1;
  }
  else {
    TObject *o = negindex(L, idx);
    api_check(L, o != NULL);
    return o;
  }
}


// 0x00C156D0 (:75)
static TObject *luaA_indexAcceptable (lua_State *L, int idx) {
  if (idx > 0) {
    TObject *o = L->base+(idx-1);
    api_check(L, idx <= L->stack_last - L->base);
    if (o >= L->top) return NULL;
    else return o;
  }
  else
    return negindex(L, idx);
}


// 0x00C19A60 (:87)
void luaA_pushobject (lua_State *L, const TObject *o) {
  setobj2s(L->top, o);
  incr_top(L);
}


// 0x00C19AA0 (:93)
LUA_API int lua_checkstack (lua_State *L, int size) {
  int res;
  lua_lock(L);
  if ((L->top - L->base + size) > LUA_MAXCSTACK)
    res = 0;  /* stack overflow */
  else {
    luaD_checkstack(L, size);
    if (L->ci->top < L->top + size)
      L->ci->top = L->top + size;
    res = 1;
  }
  lua_unlock(L);
  return res;
}


// 0x00C15730 (:109)
LUA_API void lua_xmove (lua_State *from, lua_State *to, int n) {
  int i;
  lua_lock(to);
  api_checknelems(from, n);
  from->top -= n;
  for (i = 0; i < n; i++) {
    setobj2s(to->top, from->top + i);
    api_incr_top(to);
  }
  lua_unlock(to);
}


// 0x00C15780 (:122)
LUA_API lua_CFunction lua_atpanic (lua_State *L, lua_CFunction panicf) {
  lua_CFunction old;
  lua_lock(L);
  old = G(L)->panic;
  G(L)->panic = panicf;
  lua_unlock(L);
  return old;
}


// 0x00C20E50 (:132)
LUA_API lua_State *lua_newthread (lua_State *L) {
  lua_State *L1;
  lua_lock(L);
  luaC_checkGC(L);
  L1 = luaE_newthread(L);
  setthvalue(L->top, L1);
  api_incr_top(L);
  lua_unlock(L);
  lua_userstateopen(L1);
  return L1;
}



/*
** basic stack manipulation
*/


// 0x00C157A0 (:151)
LUA_API int lua_gettop (lua_State *L) {
  return (L->top - L->base);
}


// 0x00C157B0 (:156)
LUA_API void lua_settop (lua_State *L, int idx) {
  lua_lock(L);
  if (idx >= 0) {
    api_check(L, idx <= L->stack_last - L->base);
    while (L->top < L->base + idx)
      setnilvalue(L->top++);
    L->top = L->base + idx;
  }
  else {
    api_check(L, -(idx+1) <= (L->top - L->base));
    L->top += idx+1;  /* `subtract' index (index is negative) */
  }
  lua_unlock(L);
}


// 0x00C15800 (:172)
LUA_API void lua_remove (lua_State *L, int idx) {
  StkId p;
  lua_lock(L);
  p = luaA_index(L, idx);
  while (++p < L->top) setobjs2s(p-1, p);
  L->top--;
  lua_unlock(L);
}


// 0x00C15840 (:182)
LUA_API void lua_insert (lua_State *L, int idx) {
  StkId p;
  StkId q;
  lua_lock(L);
  p = luaA_index(L, idx);
  for (q = L->top; q>p; q--) setobjs2s(q, q-1);
  setobjs2s(p, L->top);
  lua_unlock(L);
}


// 0x00C15890 (:193)
LUA_API void lua_replace (lua_State *L, int idx) {
  lua_lock(L);
  api_checknelems(L, 1);
  setobj(luaA_index(L, idx), L->top - 1);  /* write barrier */
  L->top--;
  lua_unlock(L);
}


// 0x00C158C0 (:202)
LUA_API void lua_pushvalue (lua_State *L, int idx) {
  lua_lock(L);
  setobj2s(L->top, luaA_index(L, idx));
  api_incr_top(L);
  lua_unlock(L);
}



/*
** access functions (stack -> C)
*/


// 0x00C158F0 (:216)
LUA_API int lua_type (lua_State *L, int idx) {
  StkId o = luaA_indexAcceptable(L, idx);
  return (o == NULL) ? LUA_TNONE : ttype(o);
}


// 0x00C15910 (:222)
LUA_API const char *lua_typename (lua_State *L, int t) {
  UNUSED(L);
  return (t == LUA_TNONE) ? "no value" : luaT_typenames[t];
}


// 0x00C15930 (:228)
LUA_API int lua_iscfunction (lua_State *L, int idx) {
  StkId o = luaA_indexAcceptable(L, idx);
  return (o == NULL) ? 0 : iscfunction(o);
}


// 0x00C19B00 (:234)
LUA_API int lua_isnumber (lua_State *L, int idx) {
  TObject n;
  const TObject *o = luaA_indexAcceptable(L, idx);
  return (o != NULL && tonumber(o, &n));
}


// 0x00C15960 (:241)
LUA_API int lua_isstring (lua_State *L, int idx) {
  int t = lua_type(L, idx);
  return (t == LUA_TSTRING || t == LUA_TNUMBER);
}


// 0x00C15990 (:247)
LUA_API int lua_isuserdata (lua_State *L, int idx) {
  const TObject *o = luaA_indexAcceptable(L, idx);
  return (o != NULL && (ttisuserdata(o) || ttislightuserdata(o)));
}


// 0x00C19B50 (:253)
LUA_API int lua_rawequal (lua_State *L, int index1, int index2) {
  StkId o1 = luaA_indexAcceptable(L, index1);
  StkId o2 = luaA_indexAcceptable(L, index2);
  return (o1 == NULL || o2 == NULL) ? 0  /* index out of range */
                                    : luaO_rawequalObj(o1, o2);
}


// 0x00C19B90 (:261)
LUA_API int lua_equal (lua_State *L, int index1, int index2) {
  StkId o1, o2;
  int i;
  lua_lock(L);  /* may call tag method */
  o1 = luaA_indexAcceptable(L, index1);
  o2 = luaA_indexAcceptable(L, index2);
  i = (o1 == NULL || o2 == NULL) ? 0  /* index out of range */
                                 : equalobj(L, o1, o2);
  lua_unlock(L);
  return i;
}


// 0x00C19BE0 (:274)
LUA_API int lua_lessthan (lua_State *L, int index1, int index2) {
  StkId o1, o2;
  int i;
  lua_lock(L);  /* may call tag method */
  o1 = luaA_indexAcceptable(L, index1);
  o2 = luaA_indexAcceptable(L, index2);
  i = (o1 == NULL || o2 == NULL) ? 0  /* index out-of-range */
                                 : luaV_lessthan(L, o1, o2);
  lua_unlock(L);
  return i;
}



// 0x00C19C20 (:288)
LUA_API lua_Number lua_tonumber (lua_State *L, int idx) {
  TObject n;
  const TObject *o = luaA_indexAcceptable(L, idx);
  if (o != NULL && tonumber(o, &n))
    return nvalue(o);
  else
    return 0;
}


// 0x00C159C0 (:298)
LUA_API int lua_toboolean (lua_State *L, int idx) {
  const TObject *o = luaA_indexAcceptable(L, idx);
  return (o != NULL) && !l_isfalse(o);
}


// 0x00C1C470 (:304)
LUA_API const char *lua_tostring (lua_State *L, int idx) {
  StkId o = luaA_indexAcceptable(L, idx);
  if (o == NULL)
    return NULL;
  else if (ttisstring(o))
    return svalue(o);
  else {
    const char *s;
    lua_lock(L);  /* `luaV_tostring' may create a new string */
    s = (luaV_tostring(L, o) ? svalue(o) : NULL);
    luaC_checkGC(L);
    lua_unlock(L);
    return s;
  }
}


// 0x00C19C80 (:321)
LUA_API size_t lua_strlen (lua_State *L, int idx) {
  StkId o = luaA_indexAcceptable(L, idx);
  if (o == NULL)
    return 0;
  else if (ttisstring(o))
    return tsvalue(o)->tsv.len;
  else {
    size_t l;
    lua_lock(L);  /* `luaV_tostring' may create a new string */
    l = (luaV_tostring(L, o) ? tsvalue(o)->tsv.len : 0);
    lua_unlock(L);
    return l;
  }
}


// 0x00C159F0 (:337)
LUA_API lua_CFunction lua_tocfunction (lua_State *L, int idx) {
  StkId o = luaA_indexAcceptable(L, idx);
  return (o == NULL || !iscfunction(o)) ? NULL : clvalue(o)->c.f;
}


// 0x00C15A20 (:343)
LUA_API void *lua_touserdata (lua_State *L, int idx) {
  StkId o = luaA_indexAcceptable(L, idx);
  if (o == NULL) return NULL;
  switch (ttype(o)) {
    case LUA_TUSERDATA: return (uvalue(o) + 1);
    case LUA_TLIGHTUSERDATA: return pvalue(o);
    default: return NULL;
  }
}


// 0x00C15A50 (:354)
LUA_API lua_State *lua_tothread (lua_State *L, int idx) {
  StkId o = luaA_indexAcceptable(L, idx);
  return (o == NULL || !ttisthread(o)) ? NULL : thvalue(o);
}


// 0x00C15A70 (:360)
LUA_API const void *lua_topointer (lua_State *L, int idx) {
  StkId o = luaA_indexAcceptable(L, idx);
  if (o == NULL) return NULL;
  else {
    switch (ttype(o)) {
      case LUA_TTABLE: return hvalue(o);
      case LUA_TFUNCTION: return clvalue(o);
      case LUA_TTHREAD: return thvalue(o);
      case LUA_TUSERDATA:
      case LUA_TLIGHTUSERDATA:
        return lua_touserdata(L, idx);
      default: return NULL;
    }
  }
}



/*
** push functions (C -> stack)
*/


// 0x00C15AD0 (:383)
LUA_API void lua_pushnil (lua_State *L) {
  lua_lock(L);
  setnilvalue(L->top);
  api_incr_top(L);
  lua_unlock(L);
}


// 0x00C15AF0 (:391)
LUA_API void lua_pushnumber (lua_State *L, lua_Number n) {
  lua_lock(L);
  setnvalue(L->top, n);
  api_incr_top(L);
  lua_unlock(L);
}


// 0x00C1C4D0 (:399)
LUA_API void lua_pushlstring (lua_State *L, const char *s, size_t len) {
  lua_lock(L);
  luaC_checkGC(L);
  setsvalue2s(L->top, luaS_newlstr(L, s, len));
  api_incr_top(L);
  lua_unlock(L);
}


// 0x00C1C540 (:408)
LUA_API void lua_pushstring (lua_State *L, const char *s) {
  if (s == NULL)
    lua_pushnil(L);
  else
    lua_pushlstring(L, s, strlen(s));
}


// 0x00C1FBB0 (:417)
LUA_API const char *lua_pushvfstring (lua_State *L, const char *fmt,
                                      va_list argp) {
  const char *ret;
  lua_lock(L);
  luaC_checkGC(L);
  ret = luaO_pushvfstring(L, fmt, argp);
  lua_unlock(L);
  return ret;
}


// 0x00C1FC10 (:427)
LUA_API const char *lua_pushfstring (lua_State *L, const char *fmt, ...) {
  const char *ret;
  va_list argp;
  lua_lock(L);
  luaC_checkGC(L);
  va_start(argp, fmt);
  ret = luaO_pushvfstring(L, fmt, argp);
  va_end(argp);
  lua_unlock(L);
  return ret;
}


// 0x00C21F90 (:440)
LUA_API void lua_pushcclosure (lua_State *L, lua_CFunction fn, int n) {
  Closure *cl;
  lua_lock(L);
  luaC_checkGC(L);
  api_checknelems(L, n);
  cl = luaF_newCclosure(L, n);
  cl->c.f = fn;
  L->top -= n;
  while (n--)
    setobj2n(&cl->c.upvalue[n], L->top+n);
  setclvalue(L->top, cl);
  api_incr_top(L);
  lua_unlock(L);
}


// 0x00C15B10 (:456)
LUA_API void lua_pushboolean (lua_State *L, int b) {
  lua_lock(L);
  setbvalue(L->top, (b != 0));  /* ensure that true is 1 */
  api_incr_top(L);
  lua_unlock(L);
}


// 0x00C15B30 (:464)
LUA_API void lua_pushlightuserdata (lua_State *L, void *p) {
  lua_lock(L);
  setpvalue(L->top, p);
  api_incr_top(L);
  lua_unlock(L);
}



/*
** get functions (Lua -> stack)
*/


// 0x00C1B960 (:478)
LUA_API void lua_gettable (lua_State *L, int idx) {
  StkId t;
  lua_lock(L);
  t = luaA_index(L, idx);
  setobj2s(L->top - 1, luaV_gettable(L, t, L->top - 1, 0));
  lua_unlock(L);
}


// 0x00C19CD0 (:487)
LUA_API void lua_rawget (lua_State *L, int idx) {
  StkId t;
  lua_lock(L);
  t = luaA_index(L, idx);
  api_check(L, ttistable(t));
  setobj2s(L->top - 1, luaH_get(hvalue(t), L->top - 1));
  lua_unlock(L);
}


// 0x00C19D60 (:497)
LUA_API void lua_rawgeti (lua_State *L, int idx, int n) {
  StkId o;
  lua_lock(L);
  o = luaA_index(L, idx);
  api_check(L, ttistable(o));
  setobj2s(L->top, luaH_getnum(hvalue(o), n));
  api_incr_top(L);
  lua_unlock(L);
}


// 0x00C1C580 (:508)
LUA_API void lua_newtable (lua_State *L) {
  lua_lock(L);
  luaC_checkGC(L);
  sethvalue(L->top, luaH_new(L, 0, 0));
  api_incr_top(L);
  lua_unlock(L);
}


// 0x00C15B50 (:517)
LUA_API int lua_getmetatable (lua_State *L, int objindex) {
  const TObject *obj;
  Table *mt = NULL;
  int res;
  lua_lock(L);
  obj = luaA_indexAcceptable(L, objindex);
  if (obj != NULL) {
    switch (ttype(obj)) {
      case LUA_TTABLE:
        mt = hvalue(obj)->metatable;
        break;
      case LUA_TUSERDATA:
        mt = uvalue(obj)->uv.metatable;
        break;
    }
  }
  if (mt == NULL || mt == hvalue(defaultmeta(L)))
    res = 0;
  else {
    sethvalue(L->top, mt);
    api_incr_top(L);
    res = 1;
  }
  lua_unlock(L);
  return res;
}


// 0x00C15BB0 (:545)
LUA_API void lua_getfenv (lua_State *L, int idx) {
  StkId o;
  lua_lock(L);
  o = luaA_index(L, idx);
  setobj2s(L->top, isLfunction(o) ? &clvalue(o)->l.g : gt(L));
  api_incr_top(L);
  lua_unlock(L);
}


/*
** set functions (stack -> Lua)
*/


// 0x00C1BA00 (:560)
LUA_API void lua_settable (lua_State *L, int idx) {
  StkId t;
  lua_lock(L);
  api_checknelems(L, 2);
  t = luaA_index(L, idx);
  luaV_settable(L, t, L->top - 2, L->top - 1);
  L->top -= 2;  /* pop index and value */
  lua_unlock(L);
}


// 0x00C19DA0 (:571)
LUA_API void lua_rawset (lua_State *L, int idx) {
  StkId t;
  lua_lock(L);
  api_checknelems(L, 2);
  t = luaA_index(L, idx);
  api_check(L, ttistable(t));
  setobj2t(luaH_set(L, hvalue(t), L->top-2), L->top-1);  /* write barrier */
  L->top -= 2;
  lua_unlock(L);
}


// 0x00C1BA30 (:583)
LUA_API void lua_rawseti (lua_State *L, int idx, int n) {
  StkId o;
  lua_lock(L);
  api_checknelems(L, 1);
  o = luaA_index(L, idx);
  api_check(L, ttistable(o));
  setobj2t(luaH_setnum(L, hvalue(o), n), L->top-1);  /* write barrier */
  L->top--;
  lua_unlock(L);
}


// 0x00C15BF0 (:595)
LUA_API int lua_setmetatable (lua_State *L, int objindex) {
  TObject *obj, *mt;
  int res = 1;
  lua_lock(L);
  api_checknelems(L, 1);
  obj = luaA_index(L, objindex);
  mt = (!ttisnil(L->top - 1)) ? L->top - 1 : defaultmeta(L);
  api_check(L, ttistable(mt));
  switch (ttype(obj)) {
    case LUA_TTABLE: {
      hvalue(obj)->metatable = hvalue(mt);  /* write barrier */
      break;
    }
    case LUA_TUSERDATA: {
      uvalue(obj)->uv.metatable = hvalue(mt);  /* write barrier */
      break;
    }
    default: {
      res = 0;  /* cannot set */
      break;
    }
  }
  L->top--;
  lua_unlock(L);
  return res;
}


// 0x00C15C50 (:623)
LUA_API int lua_setfenv (lua_State *L, int idx) {
  StkId o;
  int res = 0;
  lua_lock(L);
  api_checknelems(L, 1);
  o = luaA_index(L, idx);
  L->top--;
  api_check(L, ttistable(L->top));
  if (isLfunction(o)) {
    res = 1;
    clvalue(o)->l.g = *(L->top);
  }
  lua_unlock(L);
  return res;
}


/*
** `load' and `call' functions (run Lua code)
*/

// 0x00C1EE30 (:644)
LUA_API void lua_call (lua_State *L, int nargs, int nresults) {
  StkId func;
  lua_lock(L);
  api_checknelems(L, nargs+1);
  func = L->top - (nargs+1);
  luaD_call(L, func, nresults);
  lua_unlock(L);
}



/*
** Execute a protected call.
*/
struct CallS {  /* data to `f_call' */
  StkId func;
  int nresults;
};


// 0x00C1EE50 (:664)
static void f_call (lua_State *L, void *ud) {
  struct CallS *c = cast(struct CallS *, ud);
  luaD_call(L, c->func, c->nresults);
}



// 0x00C1EE70 (:671)
LUA_API int lua_pcall (lua_State *L, int nargs, int nresults, int errfunc) {
  struct CallS c;
  int status;
  ptrdiff_t func;
  lua_lock(L);
  func = (errfunc == 0) ? 0 : savestack(L, luaA_index(L, errfunc));
  c.func = L->top - (nargs+1);  /* function to be called */
  c.nresults = nresults;
  status = luaD_pcall(L, f_call, &c, savestack(L, c.func), func);
  lua_unlock(L);
  return status;
}


/*
** Execute a protected C call.
*/
struct CCallS {  /* data to `f_Ccall' */
  lua_CFunction func;
  void *ud;
};


// 0x00C22040 (:694)
static void f_Ccall (lua_State *L, void *ud) {
  struct CCallS *c = cast(struct CCallS *, ud);
  Closure *cl;
  cl = luaF_newCclosure(L, 0);
  cl->c.f = c->func;
  setclvalue(L->top, cl);  /* push function */
  incr_top(L);
  setpvalue(L->top, c->ud);  /* push only argument */
  incr_top(L);
  luaD_call(L, L->top - 2, 0);
}


// 0x00C220C0 (:707)
LUA_API int lua_cpcall (lua_State *L, lua_CFunction func, void *ud) {
  struct CCallS c;
  int status;
  lua_lock(L);
  c.func = func;
  c.ud = ud;
  status = luaD_pcall(L, f_Ccall, &c, savestack(L, L->top), 0);
  lua_unlock(L);
  return status;
}


// 0x00C2FC60 (:720)
LUA_API int lua_load (lua_State *L, lua_Chunkreader reader, void *data,
                      const char *chunkname) {
  ZIO z;
  int status;
  int c;
  lua_lock(L);
  if (!chunkname) chunkname = "?";
  luaZ_init(&z, reader, data, chunkname);
  c = luaZ_lookahead(&z);
  status = luaD_protectedparser(L, &z, (c == LUA_SIGNATURE[0]));
  lua_unlock(L);
  return status;
}


// 0x00C1BAB0 (:734)
LUA_API int lua_dump (lua_State *L, lua_Chunkwriter writer, void *data) {
  int status;
  TObject *o;
  lua_lock(L);
  api_checknelems(L, 1);
  o = L->top - 1;
  if (isLfunction(o) && clvalue(o)->l.nupvalues == 0) {
    luaU_dump(L, clvalue(o)->l.p, writer, data);
    status = 1;
  }
  else
    status = 0;
  lua_unlock(L);
  return status;
}


/*
** Garbage-collection functions
*/

/* GC values are expressed in Kbytes: #bytes/2^10 */
#define GCscalel(x)		((x)>>10)
#define GCscale(x)		(cast(int, GCscalel(x)))
#define GCunscale(x)		(cast(lu_mem, x)<<10)

// 0x00C15CA0 (:760)
LUA_API int lua_getgcthreshold (lua_State *L) {
  int threshold;
  lua_lock(L);
  threshold = GCscale(G(L)->GCthreshold);
  lua_unlock(L);
  return threshold;
}

// 0x00C15CB0 (:768)
LUA_API int lua_getgccount (lua_State *L) {
  int count;
  lua_lock(L);
  count = GCscale(G(L)->nblocks);
  lua_unlock(L);
  return count;
}

// 0x00C1C5F0 (:776)
LUA_API void lua_setgcthreshold (lua_State *L, int newthreshold) {
  lua_lock(L);
  if (cast(lu_mem, newthreshold) > GCscalel(MAX_LUMEM))
    G(L)->GCthreshold = MAX_LUMEM;
  else
    G(L)->GCthreshold = GCunscale(newthreshold);
  luaC_checkGC(L);
  lua_unlock(L);
}


/*
** miscellaneous functions
*/


// 0x00C15CC0 (:792)
LUA_API const char *lua_version (void) {
  return LUA_VERSION;
}


// 0x00C1FC70 (:797)
LUA_API int lua_error (lua_State *L) {
  lua_lock(L);
  api_checknelems(L, 1);
  luaG_errormsg(L);
  lua_unlock(L);
  return 0;  /* to avoid warnings */
}


// 0x00C20EC0 (:806)
LUA_API int lua_next (lua_State *L, int idx) {
  StkId t;
  int more;
  lua_lock(L);
  t = luaA_index(L, idx);
  api_check(L, ttistable(t));
  more = luaH_next(L, hvalue(t), L->top - 1);
  if (more) {
    api_incr_top(L);
  }
  else  /* no more elements */
    L->top -= 1;  /* remove key */
  lua_unlock(L);
  return more;
}


// 0x00C1EED0 (:823)
LUA_API void lua_concat (lua_State *L, int n) {
  lua_lock(L);
  luaC_checkGC(L);
  api_checknelems(L, n);
  if (n >= 2) {
    luaV_concat(L, n, L->top - L->base - 1);
    L->top -= (n-1);
  }
  else if (n == 0) {  /* push empty string */
    setsvalue2s(L->top, luaS_newlstr(L, NULL, 0));
    api_incr_top(L);
  }
  /* else n == 1; nothing to do */
  lua_unlock(L);
}


// 0x00C20F00 (:840)
LUA_API void *lua_newuserdata (lua_State *L, size_t size) {
  Udata *u;
  lua_lock(L);
  luaC_checkGC(L);
  u = luaS_newudata(L, size);
  setuvalue(L->top, u);
  api_incr_top(L);
  lua_unlock(L);
  return u + 1;
}


// 0x00C19DE0 (:852)
LUA_API int lua_pushupvalues (lua_State *L) {
  Closure *func;
  int n, i;
  lua_lock(L);
  api_check(L, iscfunction(L->base - 1));
  func = clvalue(L->base - 1);
  n = func->c.nupvalues;
  luaD_checkstack(L, n + LUA_MINSTACK);
  for (i=0; i<n; i++) {
    setobj2s(L->top, &func->c.upvalue[i]);
    L->top++;
  }
  lua_unlock(L);
  return n;
}


// 0x00C15CD0 (:870)
static const char *aux_upvalue (lua_State *L, int funcindex, int n,
                                TObject **val) {
  Closure *f;
  StkId fi = luaA_index(L, funcindex);
  if (!ttisfunction(fi)) return NULL;
  f = clvalue(fi);
  if (f->c.isC) {
    if (n > f->c.nupvalues) return NULL;
    *val = &f->c.upvalue[n-1];
    return "";
  }
  else {
    Proto *p = f->l.p;
    if (n > p->sizeupvalues) return NULL;
    *val = f->l.upvals[n-1]->v;
    return getstr(p->upvalues[n-1]);
  }
}


// 0x00C15D20 (:889)
LUA_API const char *lua_getupvalue (lua_State *L, int funcindex, int n) {
  const char *name;
  TObject *val;
  lua_lock(L);
  name = aux_upvalue(L, funcindex, n, &val);
  if (name) {
    setobj2s(L->top, val);
    api_incr_top(L);
  }
  lua_unlock(L);
  return name;
}


// 0x00C15DA0 (:903)
LUA_API const char *lua_setupvalue (lua_State *L, int funcindex, int n) {
  const char *name;
  TObject *val;
  lua_lock(L);
  api_checknelems(L, 1);
  name = aux_upvalue(L, funcindex, n, &val);
  if (name) {
    L->top--;
    setobj(val, L->top);  /* write barrier */
  }
  lua_unlock(L);
  return name;
}
