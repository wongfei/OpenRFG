// Original: volition/lua_lib/code/lua_system_actions.h
// Implementation: volition/lua_lib/code/lua_system_actions.cpp
//
// The system natives every Lua state gets (registered by lua_system_actions_register_functions).
#pragma once


struct lua_State;

void lua_system_actions_register_functions(lua_State* lua_state); // 0x00C323F0 (:676)
const char* lua_get_arg_type_name(int lua_arg_type); // 0x00C30580 (:713)
