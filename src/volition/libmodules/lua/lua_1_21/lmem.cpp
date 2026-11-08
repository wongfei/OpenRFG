// Original: volition/libmodules/lua/lua_1_21/lmem.cpp
//
// Lua 5.0: the memory manager. The lua.org 5.0 release (Copyright (C) 1994-2003 Tecgraf, PUC-Rio; MIT license, see the
// notice at the end of lua.h) as Volition compiled it, as C++ (lua_unity in rfg.exe; compiled here per file).
// Volition's changes are marked "Volition:". Each function carries its rfg.exe VA and its line in rfg.pdb.

/*
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/


#include <stdlib.h>

#define lmem_c

#include "volition/libmodules/lua/lua_1_21/lua.h"

#include "volition/libmodules/lua/lua_1_21/ldebug.h"
#include "volition/libmodules/lua/lua_1_21/ldo.h"
#include "volition/libmodules/lua/lua_1_21/lmem.h"
#include "volition/libmodules/lua/lua_1_21/lobject.h"
#include "volition/libmodules/lua/lua_1_21/lstate.h"



/*
** Volition: Lua allocates through two hooks, the CRT's realloc and free until lua_set_mem_calls installs others
** (lua_system_init gives it dlmalloc's dlrealloc and dlfree).
*/

// 0x00C17A40 (:52)
void *simple_realloc_wrapper (void *addy, size_t bytes) {
  return realloc(addy, bytes);
}


// 0x00C17A50 (:57)
void simple_free_wrapper (void *addy) {
  free(addy);
}


void *(*Lua_realloc) (void *addy, size_t bytes) = simple_realloc_wrapper; // 0x016EFE5C
void (*Lua_free) (void *addy) = simple_free_wrapper; // 0x016EFE60


// 0x00C17A60 (:68)
void lua_set_mem_calls (void *(*realloc_cb) (void *, size_t), void (*free_cb) (void *)) {
  if (realloc_cb != NULL && free_cb != NULL) {
    Lua_realloc = realloc_cb;
    Lua_free = free_cb;
  }
}


/*
** definition for realloc function. It must assure that l_realloc(NULL,
** 0, x) allocates a new block (ANSI C assures that). (`os' is the old
** block size; some allocators may use that.)
*/
#ifndef l_realloc
#define l_realloc(b,os,s)	Lua_realloc(b,s)  /* Volition: through the hook (stock: realloc(b,s)) */
#endif

/*
** definition for free function. (`os' is the old block size; some
** allocators may use that.)
*/
#ifndef l_free
#define l_free(b,os)	Lua_free(b)  /* Volition: through the hook (stock: free(b)) */
#endif


#define MINSIZEARRAY	4


// 0x00C21780 (:75)
void *luaM_growaux (lua_State *L, void *block, int *size, int size_elems,
                    int limit, const char *errormsg) {
  void *newblock;
  int newsize = (*size)*2;
  if (newsize < MINSIZEARRAY)
    newsize = MINSIZEARRAY;  /* minimum size */
  else if (*size >= limit/2) {  /* cannot double it? */
    if (*size < limit - MINSIZEARRAY)  /* try something smaller... */
      newsize = limit;  /* still have at least MINSIZEARRAY free places */
    else luaG_runerror(L, errormsg);
  }
  newblock = luaM_realloc(L, block,
                          cast(lu_mem, *size)*cast(lu_mem, size_elems),
                          cast(lu_mem, newsize)*cast(lu_mem, size_elems));
  *size = newsize;  /* update only when everything else is OK */
  return newblock;
}


/*
** generic allocation routine.
*/
// 0x00C20050 (:96)
void *luaM_realloc (lua_State *L, void *block, lu_mem oldsize, lu_mem size) {
  lua_assert((oldsize == 0) == (block == NULL));
  if (size == 0) {
    if (block != NULL) {
      l_free(block, oldsize);
      block = NULL;
    }
    else return NULL;  /* avoid `nblocks' computations when oldsize==size==0 */
  }
  else if (size >= MAX_SIZET)
    luaG_runerror(L, "memory allocation error: block too big");
  else {
    block = l_realloc(block, oldsize, size);
    if (block == NULL) {
      if (L)
        luaD_throw(L, LUA_ERRMEM);
      else return NULL;  /* error before creating state! */
    }
  }
  if (L) {
    lua_assert(G(L) != NULL && G(L)->nblocks > 0);
    G(L)->nblocks -= oldsize;
    G(L)->nblocks += size;
  }
  return block;
}
