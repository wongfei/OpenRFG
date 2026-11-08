// Original: volition/libmodules/lua/lua_1_21/llex.h
// Implementation: volition/libmodules/lua/lua_1_21/llex.cpp
//
// The lexer: reserved words, tokens with their semantic info, and the lexer state the parser drives.
//
// The lua.org Lua 5.0 header (MIT license, see the notice at the end of lua.h) as Volition compiled it; the
// declarations carry their rfg.exe VA. Volition's changes are marked "Volition:".
/*
** Lexical Analyzer
** See Copyright Notice in lua.h
*/

#pragma once

#include "volition/libmodules/lua/lua_1_21/lobject.h"
#include "volition/libmodules/lua/lua_1_21/lzio.h"


#define FIRST_RESERVED	257

/* maximum length of a reserved word */
#define TOKEN_LEN	(sizeof("function")/sizeof(char))


/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER RESERVED"
*/
enum RESERVED {
  /* terminal symbols denoted by reserved words */
  TK_AND = FIRST_RESERVED, TK_BREAK,
  TK_DO, TK_ELSE, TK_ELSEIF, TK_END, TK_FALSE, TK_FOR, TK_FUNCTION,
  TK_IF, TK_IN, TK_LOCAL, TK_NIL, TK_NOT, TK_OR, TK_REPEAT,
  TK_RETURN, TK_THEN, TK_TRUE, TK_UNTIL, TK_WHILE,
  /* other terminal symbols */
  TK_NAME, TK_CONCAT, TK_DOTS, TK_EQ, TK_GE, TK_LE, TK_NE, TK_NUMBER,
  TK_STRING, TK_EOS
};

/* number of reserved words */
#define NUM_RESERVED	(cast(int, TK_WHILE-FIRST_RESERVED+1))


typedef union {
  lua_Number r;
  TString *ts;
} SemInfo;  /* semantics information */


typedef struct Token {
  int token;
  SemInfo seminfo;
} Token;


typedef struct LexState {
  int current;  /* current character (charint) */
  int linenumber;  /* input line counter */
  int lastline;  /* line of last token `consumed' */
  Token t;  /* current token */
  Token lookahead;  /* look ahead token */
  struct FuncState *fs;  /* `FuncState' is private to the parser */
  struct lua_State *L;
  ZIO *z;  /* input stream */
  Mbuffer *buff;  /* buffer for tokens */
  TString *source;  /* current source name */
  int nestlevel;  /* level of nested non-terminals */
} LexState;


void luaX_init (lua_State *L); // 0x00C1ADC0 (llex.cpp:40)
void luaX_setinput (lua_State *L, LexState *LS, ZIO *z, TString *source); // 0x00C1AF30 (llex.cpp:119)
int luaX_lex (LexState *LS, SemInfo *seminfo); // 0x00C23D30 (llex.cpp:314)
void luaX_checklimit (LexState *ls, int val, int limit, const char *msg); // 0x00C23350 (llex.cpp:54)
void luaX_syntaxerror (LexState *ls, const char *s); // 0x00C21140 (llex.cpp:76)
void luaX_errorline (LexState *ls, const char *s, const char *token, int line); // 0x00C1FF10 (llex.cpp:62)
const char *luaX_token2str (LexState *ls, int token); // 0x00C1FFE0 (llex.cpp:94)
