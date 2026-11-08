// Original: volition/lua_lib/code/macros_v2.h
//
// Argument and return helpers for natives: lua_get_arg<T> reads the argument at the cursor current_pos (the number of
// stack values from the top: -current_pos is its index) and moves the cursor down; an optional argument that is nil or
// missing gives val_default. lua_add_return<T> pushes a result and returns 1. The functions are defined here (inline;
// rfg.exe keeps one copy of each, the instantiations are listed with their VAs), plus the nil marker type.
#pragma once

#include "volition/libmodules/lua/lua_1_21/lua.h"

// size 0x1
struct lua_nil
{
};

// 0x00461960 (.h:128)
// The type of the argument at the cursor (nil when the arguments are used up).
inline int xlua_get_arg_type(lua_State* lua_state, int current_pos)
{
    if (current_pos > 0)
    {
        return lua_type(lua_state, -current_pos);
    }
    return LUA_TNIL;
}

// 0x00461980 (.h:137)
// The type check is compiled out of the release build.
inline bool test_expected_type(lua_State* lua_state, int current_pos, int lua_type, const char* const type_name)
{
    (void)lua_state;
    (void)current_pos;
    (void)lua_type;
    (void)type_name;
    return true;
}

// A Lua number as an integer or enum argument. rfg.exe converts with _ftol2: truncated to 64 bits, the low bits kept. A
// plain (int) cast is cvttsd2si in this SSE2 build, which turns everything from 2^31 up into 0x80000000. That matters
// for the crc hashes Lua passes back to natives through int arguments (vint text_tag_crc reads its uint as an int).
template <typename T>
inline T lua_number_to_arg(lua_Number value)
{
    return (T)(long long)value;
}

template <>
inline double lua_number_to_arg<double>(lua_Number value)
{
    return value;
}

// The generic version (numbers and enums), with the instantiations in rfg.exe: <unsigned short> 0x00C31360,
// <unsigned int> 0x00467890, <unsigned __int64> 0x00467820, <human_teams> 0x008A5ED0, <int> 0x004677B0,
// <double> 0x00467900 (.h:163). x87: the number converts with _ftol2 (truncation).
template <typename T>
T lua_get_arg(lua_State* lua_state, int& current_pos, bool is_required, const T& val_default)
{
    if (!is_required && xlua_get_arg_type(lua_state, current_pos) == LUA_TNIL)
    {
        if (current_pos > 0)
        {
            current_pos--;
        }
        return val_default;
    }
    T value = val_default;
    if (is_required || current_pos > 0)
    {
        value = lua_number_to_arg<T>(lua_tonumber(lua_state, -current_pos));
        current_pos--;
    }
    return value;
}

// 0x00461990 (.h:199)
template <>
inline float lua_get_arg<float>(lua_State* lua_state, int& current_pos, bool is_required, const float& val_default)
{
    if (!is_required && xlua_get_arg_type(lua_state, current_pos) == LUA_TNIL)
    {
        if (current_pos > 0)
        {
            current_pos--;
        }
        return val_default;
    }
    float value = val_default;
    if (is_required || current_pos > 0)
    {
        value = (float)lua_tonumber(lua_state, -current_pos);
        current_pos--;
    }
    return value;
}

// 0x00461A00 (.h:226)
template <>
inline lua_nil lua_get_arg<lua_nil>(lua_State* lua_state, int& current_pos, bool is_required, const lua_nil& val_default)
{
    (void)lua_state;
    (void)val_default;
    if (is_required || current_pos > 0)
    {
        current_pos--;
    }
    return lua_nil();
}

// 0x00461A20 (.h:241)
template <>
inline bool lua_get_arg<bool>(lua_State* lua_state, int& current_pos, bool is_required, const bool& val_default)
{
    if (!is_required && xlua_get_arg_type(lua_state, current_pos) == LUA_TNIL)
    {
        if (current_pos > 0)
        {
            current_pos--;
        }
        return val_default;
    }
    bool value = val_default;
    if (is_required || current_pos > 0)
    {
        value = lua_toboolean(lua_state, -current_pos) != 0;
        current_pos--;
    }
    return value;
}

// 0x00461A90 (.h:267)
template <>
inline const char* lua_get_arg<const char*>(lua_State* lua_state, int& current_pos, bool is_required, const char* const& val_default)
{
    if (!is_required && xlua_get_arg_type(lua_state, current_pos) == LUA_TNIL)
    {
        if (current_pos > 0)
        {
            current_pos--;
        }
        return val_default;
    }
    const char* value = val_default;
    if (is_required || current_pos > 0)
    {
        value = lua_tostring(lua_state, -current_pos);
        current_pos--;
    }
    return value;
}

// The generic version, with the instantiations in rfg.exe: <unsigned int> 0x00467A30, <int> 0x00467A00,
// <double> 0x00467990, <float> 0x004679C0, <unsigned short> 0x00C31330, <keen::LanguageId> 0x008A5F40 (.h:296).
template <typename T>
int lua_add_return(const T& value, lua_State* lua_state)
{
    lua_pushnumber(lua_state, (lua_Number)value);
    return 1;
}

// 0x00461AF0 (.h:304)
template <>
inline int lua_add_return<const char*>(const char* const& value, lua_State* lua_state)
{
    lua_pushstring(lua_state, value);
    return 1;
}

// 0x00461B10 (.h:311)
template <>
inline int lua_add_return<lua_nil>(const lua_nil& value, lua_State* lua_state)
{
    (void)value;
    lua_pushnil(lua_state);
    return 1;
}

// 0x00461B30 (.h:319)
template <>
inline int lua_add_return<bool>(const bool& value, lua_State* lua_state)
{
    lua_pushboolean(lua_state, value);
    return 1;
}

// 0x00C2FE70 (.h:407)
// The size of the table argument at the cursor: its "n" field when that is a number, else its number of pairs; -1
// without an argument. x87: "n" truncates with _ftol2.
inline int xlua_get_arg_table_length(lua_State* lua_state, int current_pos)
{
    if (current_pos < 1)
    {
        return -1;
    }
    const int idx = -1 - current_pos;
    lua_pushstring(lua_state, "n");
    lua_gettable(lua_state, idx);
    if (lua_type(lua_state, -1) == LUA_TNUMBER)
    {
        const int length = (int)lua_tonumber(lua_state, -1);
        lua_settop(lua_state, -2);
        return length;
    }
    lua_settop(lua_state, -2);
    int length = 0;
    lua_pushnil(lua_state);
    while (lua_next(lua_state, idx) != 0)
    {
        lua_settop(lua_state, -2);
        length++;
    }
    return length;
}

// 0x00461B50 (.h:438)
// Records the native being run for error reports; empty in the release build.
inline void lua_set_current_function(const char* _func_name)
{
    (void)_func_name;
}
