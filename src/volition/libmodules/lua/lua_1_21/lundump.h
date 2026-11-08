// Original: volition/libmodules/lua/lua_1_21/lundump.h
// Implementation: volition/libmodules/lua/lua_1_21/lundump.cpp
//
// Precompiled chunks: load (lundump.cpp) and dump (ldump.cpp), and the endianness probe.
//
// The lua.org Lua 5.0 header (MIT license, see the notice at the end of lua.h) as Volition compiled it; the
// declarations carry their rfg.exe VA. Volition's changes are marked "Volition:".
/*
** load pre-compiled Lua chunks
** See Copyright Notice in lua.h
*/

#pragma once

#include "volition/libmodules/lua/lua_1_21/lobject.h"
#include "volition/libmodules/lua/lua_1_21/lzio.h"

/* load one chunk; from lundump.c */
Proto* luaU_undump (lua_State* L, ZIO* Z, Mbuffer* buff); // 0x00C21F30 (lundump.cpp:264)

/* find byte order; from lundump.c */
int luaU_endianness (void); // 0x00C192D0 (lundump.cpp:283)

/* dump one chunk; from ldump.c */
void luaU_dump (lua_State* L, const Proto* Main, lua_Chunkwriter w, void* data); // 0x00C1A710 (ldump.cpp:162)

/* print one chunk; from print.c */
void luaU_print (const Proto* Main);

/* definitions for headers of binary files */
#define	LUA_SIGNATURE	"\033Lua"	/* binary files start with "<esc>Lua" */
#define	VERSION		0x50		/* last format change was in 5.0 */
#define	VERSION0	0x50		/* last major  change was in 5.0 */

/* a multiple of PI for testing native format */
/* multiplying by 1E7 gives non-trivial integer values */
#define	TEST_NUMBER	((lua_Number)3.14159265358979323846E7)
