// Original: volition/lua_lib/code/lua_system_actions.cpp
//
// The 21 system natives every root state gets (lua_system_actions_register_functions): math helpers on floats, the
// frame time, random numbers from vlib's generator, table size, thread control and string case. Each native reads its
// arguments with macros_v2.h's lua_get_arg from the first of the last N stack values (N = the arguments it expects).
// Part of lua_lib_unity: the statics of lua_scripts.cpp and lua_thread.cpp are shared.

#include "volition/lua_lib/code/lua_system_actions.h"

#include "volition/libmodules/lua/lua_1_21/lua.h"
#include "volition/lua_lib/code/lua_scripts.h"
#include "volition/lua_lib/code/lua_thread.h"
#include "volition/lua_lib/code/macros_v2.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/util/string_util.h"

#include <math.h>

// ---- file-static globals
// lua_active_state Lua_states[4] (0x038B7180) is used here too; placed in lua_scripts.cpp.
// unsigned int Thread_stack_depth (0x038B91F0) is used here too; placed in lua_scripts.cpp.

// The position of the first of the last num_args stack values (the natives' argument cursor).
static int first_arg_pos(lua_State* lua_state, int num_args)
{
    const int top = lua_gettop(lua_state);
    return top > num_args ? num_args : top;
}

// 0x00C31AD0 (:430)
static int acos(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 1);
    const float value = lua_get_arg<float>(lua_state, pos, true, 0.0f);
    return lua_add_return<float>(fl_acos(value), lua_state);
}

// 0x00C31B30 (:437)
// x87: the float goes through the CRT's double ceil, then truncates to an int.
static int ceil(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 1);
    const float value = lua_get_arg<float>(lua_state, pos, true, 0.0f);
    return lua_add_return<int>((int)::ceil((double)value), lua_state);
}

// 0x00C31B90 (:443)
static int round(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 1);
    const float value = lua_get_arg<float>(lua_state, pos, true, 0.0f);
    return lua_add_return<int>(fl_round(value), lua_state);
}

// 0x00C31BF0 (:449)
static int max(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 2);
    const float a = lua_get_arg<float>(lua_state, pos, true, 0.0f);
    const float b = lua_get_arg<float>(lua_state, pos, true, 0.0f);
    return lua_add_return<float>(a - b >= 0.0f ? a : b, lua_state);
}

// 0x00C31C80 (:456)
static int min(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 2);
    const float a = lua_get_arg<float>(lua_state, pos, true, 0.0f);
    const float b = lua_get_arg<float>(lua_state, pos, true, 0.0f);
    return lua_add_return<float>(a - b >= 0.0f ? b : a, lua_state);
}

// 0x00C31D10 (:463)
// x87: __CIcos of the float.
static int cos(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 1);
    const float value = lua_get_arg<float>(lua_state, pos, true, 0.0f);
    return lua_add_return<float>((float)::cos((double)value), lua_state);
}

// 0x00C31D70 (:469)
// debug_print(channel, text): the print itself is left out of the release build.
static int debug_print(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 2);
    const char* channel = lua_get_arg<const char*>(lua_state, pos, true, NULL);
    const char* text = lua_get_arg<const char*>(lua_state, pos, true, NULL);
    (void)channel;
    (void)text;
    return 0;
}

// 0x00C31DB0 (:482)
static int floor(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 1);
    const float value = lua_get_arg<float>(lua_state, pos, true, 0.0f);
    return lua_add_return<int>((int)::floor((double)value), lua_state);
}

// 0x00C31E60 (:488)
static int get_frame_time_real(lua_State* lua_state)
{
    first_arg_pos(lua_state, 0);
    return lua_add_return<float>(Frametime_real, lua_state);
}

// 0x00C31E90 (:492)
// Original kept: returns Frametime_real too, not Frametime.
static int get_frame_time(lua_State* lua_state)
{
    first_arg_pos(lua_state, 0);
    return lua_add_return<float>(Frametime_real, lua_state);
}

// 0x00C31EC0 (:503)
// include(name): lua_add_include_script (inlined), a critical failure in the game, with the name given ".lua" first.
static int include(lua_State* lua_state)
{
    // the TLS scratch string of rfg.exe (thread-local block offset 0x38, 0x400 bytes)
    static thread_local char filename[0x400];
    int pos = first_arg_pos(lua_state, 1);
    const char* name = lua_get_arg<const char*>(lua_state, pos, true, NULL);
    if (str_stri(name, ".lua") != NULL)
    {
        lua_add_include_script(name);
        return 0;
    }
    _snprintf_safe(filename, sizeof(filename), "%s%s", name, ".lua");
    lua_add_include_script(filename);
    return 0;
}

// 0x00C31F60 (:516)
static int rand_float(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 2);
    float minimum = lua_get_arg<float>(lua_state, pos, true, 0.0f);
    float maximum = lua_get_arg<float>(lua_state, pos, true, 0.0f);
    if (maximum < minimum)
    {
        const float swap = minimum;
        minimum = maximum;
        maximum = swap;
    }
    return lua_add_return<float>(fl_rand_range(minimum, maximum), lua_state);
}

// 0x00C32010 (:535)
static int rand_int(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 2);
    int minimum = lua_get_arg<int>(lua_state, pos, true, 0);
    int maximum = lua_get_arg<int>(lua_state, pos, true, 0);
    if (maximum < minimum)
    {
        const int swap = minimum;
        minimum = maximum;
        maximum = swap;
    }
    return lua_add_return<int>(int_rand_range(minimum, maximum), lua_state);
}

// 0x00C32090 (:554)
static int sizeof_table(lua_State* lua_state)
{
    const int pos = first_arg_pos(lua_state, 1);
    return lua_add_return<int>(xlua_get_arg_table_length(lua_state, pos), lua_state);
}

// 0x00C320D0 (:563)
// x87: __CIsqrt of the float.
static int sqrt(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 1);
    const float value = lua_get_arg<float>(lua_state, pos, true, 0.0f);
    return lua_add_return<float>((float)::sqrt((double)value), lua_state);
}

// 0x00C32130 (:571)
static int thread_kill(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 1);
    const unsigned short handle = lua_get_arg<unsigned short>(lua_state, pos, true, 0);
    lua_thread* t = lua_get_thread(handle);
    if (t != NULL)
    {
        t->flags.terminate = 1;
    }
    return 0;
}

// 0x00C32170 (:583)
// thread_new(function name, up to 19 arguments): a thread in the current thread's root state and context, run at once.
// Returns its handle, or 0xFFFF once it has ended (or could not start). Original bug kept: called outside a script
// thread, the missing current thread is dereferenced.
static int thread_new(lua_State* lua_state)
{
    const int num_args = lua_gettop(lua_state);
    int pos = first_arg_pos(lua_state, 20);
    const char* func_name = lua_get_arg<const char*>(lua_state, pos, true, NULL);
    lua_thread* current = lua_get_current_thread();
    lua_thread* t = new_thread_internal(func_name, NULL, false, false, current->root_state);
    if (t != NULL)
    {
        t->context = current->context;
        lua_xmove(lua_state, t->lua_state, num_args - 1);
        t->num_pushed_args = num_args - 1;
        lua_process_thread(t, NULL);
        if (t != NULL)
        {
            return lua_add_return<unsigned short>(t->handle, lua_state);
        }
    }
    return lua_add_return<unsigned short>(0xFFFF, lua_state);
}

// 0x00C32240 (:634)
static int thread_check_done(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 1);
    const unsigned short handle = lua_get_arg<unsigned short>(lua_state, pos, true, 0);
    return lua_add_return<bool>(lua_get_thread(handle) == NULL, lua_state);
}

// 0x00C32290 (:642)
static int thread_yield(lua_State* lua_state)
{
    first_arg_pos(lua_state, 0);
    return lua_yield(lua_state, 0);
}

// 0x00C322B0 (:645)
static int to_upper(lua_State* lua_state)
{
    char buffer[512];
    int pos = first_arg_pos(lua_state, 1);
    const char* string = lua_get_arg<const char*>(lua_state, pos, true, NULL);
    if (string == NULL)
    {
        buffer[0] = 0;
    }
    else
    {
        _snprintf_safe(buffer, sizeof(buffer), "%s", string);
        str_make_upper(buffer);
    }
    return lua_add_return<const char*>(buffer, lua_state);
}

// 0x00C32350 (:660)
static int to_lower(lua_State* lua_state)
{
    char buffer[512];
    int pos = first_arg_pos(lua_state, 1);
    const char* string = lua_get_arg<const char*>(lua_state, pos, true, NULL);
    if (string == NULL)
    {
        buffer[0] = 0;
    }
    else
    {
        _snprintf_safe(buffer, sizeof(buffer), "%s", string);
        str_make_lower(buffer);
    }
    return lua_add_return<const char*>(buffer, lua_state);
}

// 0x00C323F0 (:676)
// Each native as a global of the state (lua_register).
void lua_system_actions_register_functions(lua_State* lua_state)
{
    const struct
    {
        const char* name;
        lua_CFunction func;
    } natives[] = {
        { "acos", acos },
        { "ceil", ceil },
        { "round", round },
        { "cos", cos },
        { "max", max },
        { "min", min },
        { "debug_print", debug_print },
        { "floor", floor },
        { "get_frame_time", get_frame_time },
        { "get_frame_time_real", get_frame_time_real },
        { "include", include },
        { "rand_float", rand_float },
        { "rand_int", rand_int },
        { "sizeof_table", sizeof_table },
        { "sqrt", sqrt },
        { "thread_kill", thread_kill },
        { "thread_new", thread_new },
        { "thread_check_done", thread_check_done },
        { "thread_yield", thread_yield },
        { "to_upper", to_upper },
        { "to_lower", to_lower },
    };
    for (int i = 0; i < (int)(sizeof(natives) / sizeof(natives[0])); i++)
    {
        lua_pushstring(lua_state, natives[i].name);
        lua_pushcclosure(lua_state, natives[i].func, 0);
        lua_settable(lua_state, LUA_GLOBALSINDEX);
    }
}

// 0x00C30580 (:713)
const char* lua_get_arg_type_name(int lua_arg_type)
{
    switch (lua_arg_type)
    {
    case LUA_TNIL:
        return "nil";
    case LUA_TBOOLEAN:
        return "boolean";
    case LUA_TLIGHTUSERDATA:
        return "light user data";
    case LUA_TNUMBER:
        return "number";
    case LUA_TSTRING:
        return "string";
    case LUA_TTABLE:
        return "table";
    case LUA_TFUNCTION:
        return "function";
    case LUA_TUSERDATA:
        return "user data";
    case LUA_TTHREAD:
        return "thread";
    case LUA_TNONE:
        return "none";
    default:
        return "unknown";
    }
}
