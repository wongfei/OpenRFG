// Original: volition/libmodules/lua/lua_1_21/lzio.cpp
//
// Lua 5.0: the buffered streams. The lua.org 5.0 release (Copyright (C) 1994-2003 Tecgraf, PUC-Rio; MIT license, see
// the notice at the end of lua.h) as Volition compiled it, as C++ (lua_unity in rfg.exe; compiled here per file).
// Volition's changes are marked "Volition:". Each function carries its rfg.exe VA and its line in rfg.pdb.

/*
** a generic input stream interface
** See Copyright Notice in lua.h
*/


#include <string.h>

#define lzio_c

#include "volition/libmodules/lua/lua_1_21/lua.h"

#include "volition/libmodules/lua/lua_1_21/llimits.h"
#include "volition/libmodules/lua/lua_1_21/lmem.h"
#include "volition/libmodules/lua/lua_1_21/lzio.h"


// 0x00C19920 (:19)
int luaZ_fill (ZIO *z) {
  size_t size;
  const char *buff = z->reader(NULL, z->data, &size);
  if (buff == NULL || size == 0) return EOZ;
  z->n = size - 1;
  z->p = buff;
  return char2int(*(z->p++));
}


// 0x00C19960 (:29)
int luaZ_lookahead (ZIO *z) {
  if (z->n == 0) {
    int c = luaZ_fill(z);
    if (c == EOZ) return c;
    z->n++;
    z->p--;
  }
  return char2int(*z->p);
}


// 0x00C199B0 (:40)
void luaZ_init (ZIO *z, lua_Chunkreader reader, void *data, const char *name) {
  z->reader = reader;
  z->data = data;
  z->name = name;
  z->n = 0;
  z->p = NULL;
}


/* --------------------------------------------------------------- read --- */
// 0x00C199E0 (:50)
size_t luaZ_read (ZIO *z, void *b, size_t n) {
  while (n) {
    size_t m;
    if (z->n == 0) {
      if (luaZ_fill(z) == EOZ)
        return n;  /* return number of missing bytes */
      else {
        ++z->n;  /* filbuf removed first byte; put back it */
        --z->p;
      }
    }
    m = (n <= z->n) ? n : z->n;  /* min. between n and z->n */
    memcpy(b, z->p, m);
    z->n -= m;
    z->p += m;
    b = (char *)b + m;
    n -= m;
  }
  return 0;
}

/* ------------------------------------------------------------------------ */
// 0x00C20E10 (:72)
char *luaZ_openspace (lua_State *L, Mbuffer *buff, size_t n) {
  if (n > buff->buffsize) {
    if (n < LUA_MINBUFFER) n = LUA_MINBUFFER;
    luaM_reallocvector(L, buff->buffer, buff->buffsize, n, char);
    buff->buffsize = n;
  }
  return buff->buffer;
}
