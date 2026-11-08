// Original: volition/libmodules/lua/lua_1_21/lmem.h
// Implementation: volition/libmodules/lua/lua_1_21/lmem.cpp
//
// Memory: luaM_realloc and growaux, plus Volition's allocator hooks (lua_set_mem_calls and the default
// simple_realloc_wrapper/simple_free_wrapper); lua_system_init installs dlmalloc through them.
//
// The lua.org Lua 5.0 header (MIT license, see the notice at the end of lua.h) as Volition compiled it; the
// declarations carry their rfg.exe VA. Volition's changes are marked "Volition:".
/*
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/

#pragma once


#include <stddef.h>

#include "volition/libmodules/lua/lua_1_21/llimits.h"
#include "volition/libmodules/lua/lua_1_21/lua.h"

#define MEMERRMSG	"not enough memory"


/* Volition: the allocator hooks (lmem.cpp) */
void *simple_realloc_wrapper (void *addy, size_t bytes); // 0x00C17A40 (lmem.cpp:52)
void simple_free_wrapper (void *addy); // 0x00C17A50 (lmem.cpp:57)
void lua_set_mem_calls (void *(*realloc_cb) (void *, size_t), void (*free_cb) (void *)); // 0x00C17A60 (lmem.cpp:68)
extern void *(*Lua_realloc) (void *addy, size_t bytes); // 0x016EFE5C
extern void (*Lua_free) (void *addy); // 0x016EFE60


void *luaM_realloc (lua_State *L, void *oldblock, lu_mem oldsize, lu_mem size); // 0x00C20050 (lmem.cpp:96)

void *luaM_growaux (lua_State *L, void *block, int *size, int size_elem,
                    int limit, const char *errormsg); // 0x00C21780 (lmem.cpp:75)

#define luaM_free(L, b, s)	luaM_realloc(L, (b), (s), 0)
#define luaM_freelem(L, b)	luaM_realloc(L, (b), sizeof(*(b)), 0)
#define luaM_freearray(L, b, n, t)	luaM_realloc(L, (b), \
                                      cast(lu_mem, n)*cast(lu_mem, sizeof(t)), 0)

#define luaM_malloc(L, t)	luaM_realloc(L, NULL, 0, (t))
#define luaM_new(L, t)          cast(t *, luaM_malloc(L, sizeof(t)))
#define luaM_newvector(L, n,t)  cast(t *, luaM_malloc(L, \
                                         cast(lu_mem, n)*cast(lu_mem, sizeof(t))))

#define luaM_growvector(L,v,nelems,size,t,limit,e) \
          if (((nelems)+1) > (size)) \
            ((v)=cast(t *, luaM_growaux(L,v,&(size),sizeof(t),limit,e)))

#define luaM_reallocvector(L, v,oldn,n,t) \
   ((v)=cast(t *, luaM_realloc(L, v,cast(lu_mem, oldn)*cast(lu_mem, sizeof(t)), \
                                    cast(lu_mem, n)*cast(lu_mem, sizeof(t)))))
