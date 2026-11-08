// Original: volition/libmodules/lua/lua_1_21/lgc.cpp
//
// Lua 5.0: the garbage collector. The lua.org 5.0 release (Copyright (C) 1994-2003 Tecgraf, PUC-Rio; MIT license, see
// the notice at the end of lua.h) as Volition compiled it, as C++ (lua_unity in rfg.exe; compiled here per file).
// Volition's changes are marked "Volition:". Each function carries its rfg.exe VA and its line in rfg.pdb.

/*
** Garbage Collector
** See Copyright Notice in lua.h
*/

#include <string.h>

#define lgc_c

#include "volition/libmodules/lua/lua_1_21/lua.h"

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


typedef struct GCState {
  GCObject *tmark;  /* list of marked objects to be traversed */
  GCObject *wk;  /* list of traversed key-weak tables (to be cleared) */
  GCObject *wv;  /* list of traversed value-weak tables */
  GCObject *wkv;  /* list of traversed key-value weak tables */
  global_State *g;
} GCState;


/*
** some userful bit tricks
*/
#define setbit(x,b)	((x) |= (1<<(b)))
#define resetbit(x,b)	((x) &= cast(lu_byte, ~(1<<(b))))
#define testbit(x,b)	((x) & (1<<(b)))

#define unmark(x)	resetbit((x)->gch.marked, 0)
#define ismarked(x)	((x)->gch.marked & ((1<<4)|1))

#define stringmark(s)	setbit((s)->tsv.marked, 0)


#define isfinalized(u)		(!testbit((u)->uv.marked, 1))
#define markfinalized(u)	resetbit((u)->uv.marked, 1)


#define KEYWEAKBIT    1
#define VALUEWEAKBIT  2
#define KEYWEAK         (1<<KEYWEAKBIT)
#define VALUEWEAK       (1<<VALUEWEAKBIT)



#define markobject(st,o) { checkconsistency(o); \
  if (iscollectable(o) && !ismarked(gcvalue(o))) reallymarkobject(st,gcvalue(o)); }

#define condmarkobject(st,o,c) { checkconsistency(o); \
  if (iscollectable(o) && !ismarked(gcvalue(o)) && (c)) \
    reallymarkobject(st,gcvalue(o)); }

#define markvalue(st,t) { if (!ismarked(valtogco(t))) \
		reallymarkobject(st, valtogco(t)); }



// 0x00C17470 (:71)
static void reallymarkobject (GCState *st, GCObject *o) {
  lua_assert(!ismarked(o));
  setbit(o->gch.marked, 0);  /* mark object */
  switch (o->gch.tt) {
    case LUA_TUSERDATA: {
      markvalue(st, gcotou(o)->uv.metatable);
      break;
    }
    case LUA_TFUNCTION: {
      gcotocl(o)->c.gclist = st->tmark;
      st->tmark = o;
      break;
    }
    case LUA_TTABLE: {
      gcotoh(o)->gclist = st->tmark;
      st->tmark = o;
      break;
    }
    case LUA_TTHREAD: {
      gcototh(o)->gclist = st->tmark;
      st->tmark = o;
      break;
    }
    case LUA_TPROTO: {
      gcotop(o)->gclist = st->tmark;
      st->tmark = o;
      break;
    }
    default: lua_assert(o->gch.tt == LUA_TSTRING);
  }
}


// 0x00C174E0 (:104)
static void marktmu (GCState *st) {
  GCObject *u;
  for (u = st->g->tmudata; u; u = u->gch.next) {
    unmark(u);  /* may be marked, if left from previous GC */
    reallymarkobject(st, u);
  }
}


/* move `dead' udata that need finalization to list `tmudata' */
// 0x00C1A810 (:114)
void luaC_separateudata (lua_State *L) {
  GCObject **p = &G(L)->rootudata;
  GCObject *curr;
  GCObject *collected = NULL;  /* to collect udata with gc event */
  GCObject **lastcollected = &collected;
  while ((curr = *p) != NULL) {
    lua_assert(curr->gch.tt == LUA_TUSERDATA);
    if (ismarked(curr) || isfinalized(gcotou(curr)))
      p = &curr->gch.next;  /* don't bother with them */

    else if (fasttm(L, gcotou(curr)->uv.metatable, TM_GC) == NULL) {
      markfinalized(gcotou(curr));  /* don't need finalization */
      p = &curr->gch.next;
    }
    else {  /* must call its gc method */
      *p = curr->gch.next;
      curr->gch.next = NULL;  /* link `curr' at the end of `collected' list */
      *lastcollected = curr;
      lastcollected = &curr->gch.next;
    }
  }
  /* insert collected udata with gc event into `tmudata' list */
  *lastcollected = G(L)->tmudata;
  G(L)->tmudata = collected;
}


// 0x00C17550 (:141)
static void removekey (Node *n) {
  setnilvalue(gval(n));  /* remove corresponding value ... */
  if (iscollectable(gkey(n)))
    setttype(gkey(n), LUA_TNONE);  /* dead key; remove it */
}


// 0x00C1A8C0 (:148)
static void traversetable (GCState *st, Table *h) {
  int i;
  int weakkey = 0;
  int weakvalue = 0;
  const TObject *mode;
  markvalue(st, h->metatable);
  lua_assert(h->lsizenode || h->node == st->g->dummynode);
  mode = gfasttm(st->g, h->metatable, TM_MODE);
  if (mode && ttisstring(mode)) {  /* is there a weak mode? */
    weakkey = (strchr(svalue(mode), 'k') != NULL);
    weakvalue = (strchr(svalue(mode), 'v') != NULL);
    if (weakkey || weakvalue) {  /* is really weak? */
      GCObject **weaklist;
      h->marked &= ~(KEYWEAK | VALUEWEAK);  /* clear bits */
      h->marked |= cast(lu_byte, (weakkey << KEYWEAKBIT) |
                                 (weakvalue << VALUEWEAKBIT));
      weaklist = (weakkey && weakvalue) ? &st->wkv :
                              (weakkey) ? &st->wk :
                                          &st->wv;
      h->gclist = *weaklist;  /* must be cleared after GC, ... */
      *weaklist = valtogco(h);  /* ... so put in the appropriate list */
    }
  }
  if (!weakvalue) {
    i = h->sizearray;
    while (i--)
      markobject(st, &h->array[i]);
  }
  i = sizenode(h);
  while (i--) {
    Node *n = gnode(h, i);
    if (!ttisnil(gval(n))) {
      lua_assert(!ttisnil(gkey(n)));
      condmarkobject(st, gkey(n), !weakkey);
      condmarkobject(st, gval(n), !weakvalue);
    }
  }
}


// 0x00C17570 (:188)
static void traverseproto (GCState *st, Proto *f) {
  int i;
  stringmark(f->source);
  for (i=0; i<f->sizek; i++) {  /* mark literal strings */
    if (ttisstring(f->k+i))
      stringmark(tsvalue(f->k+i));
  }
  for (i=0; i<f->sizeupvalues; i++)  /* mark upvalue names */
    stringmark(f->upvalues[i]);
  for (i=0; i<f->sizep; i++)  /* mark nested protos */
    markvalue(st, f->p[i]);
  for (i=0; i<f->sizelocvars; i++)  /* mark local-variable names */
    stringmark(f->locvars[i].varname);
  lua_assert(luaG_checkcode(f));
}



// 0x00C17660 (:206)
static void traverseclosure (GCState *st, Closure *cl) {
  if (cl->c.isC) {
    int i;
    for (i=0; i<cl->c.nupvalues; i++)  /* mark its upvalues */
      markobject(st, &cl->c.upvalue[i]);
  }
  else {
    int i;
    lua_assert(cl->l.nupvalues == cl->l.p->nups);
    markvalue(st, hvalue(&cl->l.g));
    markvalue(st, cl->l.p);
    for (i=0; i<cl->l.nupvalues; i++) {  /* mark its upvalues */
      UpVal *u = cl->l.upvals[i];
      if (!u->marked) {
        markobject(st, &u->value);
        u->marked = 1;
      }
    }
  }
}


// 0x00C176F0 (:228)
static void checkstacksizes (lua_State *L, StkId max) {
  int used = L->ci - L->base_ci;  /* number of `ci' in use */
  if (4*used < L->size_ci && 2*BASIC_CI_SIZE < L->size_ci)
    luaD_reallocCI(L, L->size_ci/2);  /* still big enough... */
  else condhardstacktests(luaD_reallocCI(L, L->size_ci));
  used = max - L->stack;  /* part of stack in use */
  if (4*used < L->stacksize && 2*(BASIC_STACK_SIZE+EXTRA_STACK) < L->stacksize)
    luaD_reallocstack(L, L->stacksize/2);  /* still big enough... */
  else condhardstacktests(luaD_reallocstack(L, L->stacksize));
}


// 0x00C17790 (:240)
static void traversestack (GCState *st, lua_State *L1) {
  StkId o, lim;
  CallInfo *ci;
  markobject(st, gt(L1));
  lim = L1->top;
  for (ci = L1->base_ci; ci <= L1->ci; ci++) {
    lua_assert(ci->top <= L1->stack_last);
    lua_assert(ci->state & (CI_C | CI_HASFRAME | CI_SAVEDPC));
    if (!(ci->state & CI_C) && lim < ci->top)
      lim = ci->top;
  }
  for (o = L1->stack; o < L1->top; o++)
    markobject(st, o);
  for (; o <= lim; o++)
    setnilvalue(o);
  checkstacksizes(L1, lim);
}


// 0x00C1AA30 (:259)
static void propagatemarks (GCState *st) {
  while (st->tmark) {  /* traverse marked objects */
    switch (st->tmark->gch.tt) {
      case LUA_TTABLE: {
        Table *h = gcotoh(st->tmark);
        st->tmark = h->gclist;
        traversetable(st, h);
        break;
      }
      case LUA_TFUNCTION: {
        Closure *cl = gcotocl(st->tmark);
        st->tmark = cl->c.gclist;
        traverseclosure(st, cl);
        break;
      }
      case LUA_TTHREAD: {
        lua_State *th = gcototh(st->tmark);
        st->tmark = th->gclist;
        traversestack(st, th);
        break;
      }
      case LUA_TPROTO: {
        Proto *p = gcotop(st->tmark);
        st->tmark = p->gclist;
        traverseproto(st, p);
        break;
      }
      default: lua_assert(0);
    }
  }
}


// 0x00C17870 (:292)
static int valismarked (const TObject *o) {
  if (ttisstring(o))
    stringmark(tsvalue(o));  /* strings are `values', so are never weak */
  return !iscollectable(o) || testbit(o->value.gc->gch.marked, 0);
}


/*
** clear collected keys from weaktables
*/
// 0x00C178A0 (:302)
static void cleartablekeys (GCObject *l) {
  while (l) {
    Table *h = gcotoh(l);
    int i = sizenode(h);
    lua_assert(h->marked & KEYWEAK);
    while (i--) {
      Node *n = gnode(h, i);
      if (!valismarked(gkey(n)))  /* key was collected? */
        removekey(n);  /* remove entry from table */
    }
    l = h->gclist;
  }
}


/*
** clear collected values from weaktables
*/
// 0x00C17900 (:320)
static void cleartablevalues (GCObject *l) {
  while (l) {
    Table *h = gcotoh(l);
    int i = h->sizearray;
    lua_assert(h->marked & VALUEWEAK);
    while (i--) {
      TObject *o = &h->array[i];
      if (!valismarked(o))  /* value was collected? */
        setnilvalue(o);  /* remove value */
    }
    i = sizenode(h);
    while (i--) {
      Node *n = gnode(h, i);
      if (!valismarked(gval(n)))  /* value was collected? */
        removekey(n);  /* remove entry from table */
    }
    l = h->gclist;
  }
}


// 0x00C1BFA0 (:341)
static void freeobj (lua_State *L, GCObject *o) {
  switch (o->gch.tt) {
    case LUA_TPROTO: luaF_freeproto(L, gcotop(o)); break;
    case LUA_TFUNCTION: luaF_freeclosure(L, gcotocl(o)); break;
    case LUA_TUPVAL: luaM_freelem(L, gcotouv(o)); break;
    case LUA_TTABLE: luaH_free(L, gcotoh(o)); break;
    case LUA_TTHREAD: {
      lua_assert(gcototh(o) != L && gcototh(o) != G(L)->mainthread);
      luaE_freethread(L, gcototh(o));
      break;
    }
    case LUA_TSTRING: {
      luaM_free(L, o, sizestring(gcotots(o)->tsv.len));
      break;
    }
    case LUA_TUSERDATA: {
      luaM_free(L, o, sizeudata(gcotou(o)->uv.len));
      break;
    }
    default: lua_assert(0);
  }
}


// 0x00C1C040 (:365)
static int sweeplist (lua_State *L, GCObject **p, int limit) {
  GCObject *curr;
  int count = 0;  /* number of collected items */
  while ((curr = *p) != NULL) {
    if (curr->gch.marked > limit) {
      unmark(curr);
      p = &curr->gch.next;
    }
    else {
      count++;
      *p = curr->gch.next;
      freeobj(L, curr);
    }
  }
  return count;
}


// 0x00C1C110 (:383)
static void sweepstrings (lua_State *L, int all) {
  int i;
  for (i=0; i<G(L)->strt.size; i++) {  /* for each list */
    G(L)->strt.nuse -= sweeplist(L, &G(L)->strt.hash[i], all);
  }
}


// 0x00C1AAB0 (:391)
static void checkSizes (lua_State *L) {
  /* check size of string hash */
  if (G(L)->strt.nuse < cast(ls_nstr, G(L)->strt.size/4) &&
      G(L)->strt.size > MINSTRTABSIZE*2)
    luaS_resize(L, G(L)->strt.size/2);  /* table is too big */
  /* check size of buffer */
  if (luaZ_sizebuffer(&G(L)->buff) > LUA_MINBUFFER*2) {  /* buffer too big? */
    size_t newsize = luaZ_sizebuffer(&G(L)->buff) / 2;
    luaZ_resizebuffer(L, &G(L)->buff, newsize);
  }
  /* Volition: the next collection comes after 25% growth instead of 100% (stock: 2*G(L)->nblocks). The x87 product
     is truncated through a 64-bit integer. */
  G(L)->GCthreshold = cast(lu_mem, G(L)->nblocks * 1.25);  /* new threshold */
}


// 0x00C1AB60 (:408)
static void do1gcTM (lua_State *L, Udata *udata) {
  const TObject *tm = fasttm(L, udata->uv.metatable, TM_GC);
  if (tm != NULL) {
    setobj2s(L->top, tm);
    setuvalue(L->top+1, udata);
    L->top += 2;
    luaD_call(L, L->top - 2, 0);
  }
}


// 0x00C1ABC0 (:419)
void luaC_callGCTM (lua_State *L) {
  lu_byte oldah = L->allowhook;
  L->allowhook = 0;  /* stop debug hooks during GC tag methods */
  L->top++;  /* reserve space to keep udata while runs its gc method */
  while (G(L)->tmudata != NULL) {
    GCObject *o = G(L)->tmudata;
    Udata *udata = gcotou(o);
    G(L)->tmudata = udata->uv.next;  /* remove udata from `tmudata' */
    udata->uv.next = G(L)->rootudata;  /* return it to `root' list */
    G(L)->rootudata = o;
    setuvalue(L->top - 1, udata);  /* keep a reference to it */
    unmark(o);
    markfinalized(udata);
    do1gcTM(L, udata);
  }
  L->top--;
  L->allowhook = oldah;  /* restore hooks */
}


// 0x00C1C2B0 (:439)
void luaC_sweep (lua_State *L, int all) {
  if (all) all = 256;  /* larger than any mark */
  sweeplist(L, &G(L)->rootudata, all);
  sweepstrings(L, all);
  sweeplist(L, &G(L)->rootgc, all);
}


/* mark root set */
// 0x00C179B0 (:448)
static void markroot (GCState *st, lua_State *L) {
  global_State *g = st->g;
  markobject(st, defaultmeta(L));
  markobject(st, registry(L));
  traversestack(st, g->mainthread);
  if (L != g->mainthread)  /* another thread is running? */
    markvalue(st, L);  /* cannot collect it */
}


// 0x00C1ACD0 (:458)
static void mark (lua_State *L) {
  GCState st;
  GCObject *wkv;
  st.g = G(L);
  st.tmark = NULL;
  st.wkv = st.wk = st.wv = NULL;
  markroot(&st, L);
  propagatemarks(&st);  /* mark all reachable objects */
  cleartablevalues(st.wkv);
  cleartablevalues(st.wv);
  wkv = st.wkv;  /* keys must be cleared after preserving udata */
  st.wkv = NULL;
  st.wv = NULL;
  luaC_separateudata(L);  /* separate userdata to be preserved */
  marktmu(&st);  /* mark `preserved' userdata */
  propagatemarks(&st);  /* remark, to propagate `preserveness' */
  cleartablekeys(wkv);
  /* `propagatemarks' may resuscitate some weak tables; clear them too */
  cleartablekeys(st.wk);
  cleartablevalues(st.wv);
  cleartablekeys(st.wkv);
  cleartablevalues(st.wkv);
}


// 0x00C1C2F0 (:485)
void luaC_collectgarbage (lua_State *L) {
  mark(L);
  luaC_sweep(L, 0);
  checkSizes(L);
  luaC_callGCTM(L);
}


// 0x00C17A10 (:513)
void luaC_link (lua_State *L, GCObject *o, lu_byte tt) {
  o->gch.next = G(L)->rootgc;
  G(L)->rootgc = o;
  o->gch.marked = 0;
  o->gch.tt = tt;
}
