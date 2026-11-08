// Original: volition/rfg/code/interface/vint/rfg_lua_actions.h
// Implementation: volition/rfg/code/interface/vint/rfg_lua_actions.cpp
//
// The game's natives for the documents' Lua scripts (rfg_*: team colors, mode names, player and match data, sounds,
// menus); rfg_lua_register_functions registers them in vint_lua_init.
#pragma once


struct lua_State;

void rfg_lua_register_functions(lua_State* lua_state); // 0x008CD060 (:580)
void rfg_lua_actions_cleanup(); // 0x008A1D50 (:629)
