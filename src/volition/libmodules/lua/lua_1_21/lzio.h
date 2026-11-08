// Original: volition/libmodules/lua/lua_1_21/lzio.h
// Implementation: volition/libmodules/lua/lua_1_21/lzio.cpp
//
// Buffered input streams (Zio) and the growable buffer (Mbuffer) the lexer and undump use.
//
// The lua.org Lua 5.0 header (MIT license, see the notice at the end of lua.h) as Volition compiled it; the
// declarations carry their rfg.exe VA. Volition's changes are marked "Volition:".
/*
** Buffered streams
** See Copyright Notice in lua.h
*/


#pragma once

#include "volition/libmodules/lua/lua_1_21/lua.h"


#define EOZ	(-1)			/* end of stream */

typedef struct Zio ZIO;


#define char2int(c)	cast(int, cast(unsigned char, (c)))

#define zgetc(z)  (((z)->n--)>0 ?  char2int(*(z)->p++) : luaZ_fill(z))

#define zname(z)	((z)->name)

void luaZ_init (ZIO *z, lua_Chunkreader reader, void *data, const char *name); // 0x00C199B0 (lzio.cpp:40)
size_t luaZ_read (ZIO* z, void* b, size_t n);	/* read next n bytes */
int luaZ_lookahead (ZIO *z); // 0x00C19960 (lzio.cpp:29)



typedef struct Mbuffer {
  char *buffer;
  size_t buffsize;
} Mbuffer;


char *luaZ_openspace (lua_State *L, Mbuffer *buff, size_t n); // 0x00C20E10 (lzio.cpp:72)

#define luaZ_initbuffer(L, buff) ((buff)->buffer = NULL, (buff)->buffsize = 0)

#define luaZ_sizebuffer(buff)	((buff)->buffsize)
#define luaZ_buffer(buff)	((buff)->buffer)

#define luaZ_resizebuffer(L, buff, size) \
	(luaM_reallocvector(L, (buff)->buffer, (buff)->buffsize, size, char), \
	(buff)->buffsize = size)

#define luaZ_freebuffer(L, buff)	luaZ_resizebuffer(L, buff, 0)


/* --------- Private Part ------------------ */

struct Zio {
  size_t n;			/* bytes still unread */
  const char *p;		/* current position in buffer */
  lua_Chunkreader reader;
  void* data;			/* additional data */
  const char *name;
};


int luaZ_fill (ZIO *z); // 0x00C19920 (lzio.cpp:19)
