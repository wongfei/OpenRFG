// Original: volition/lua_lib/code/lua_thread.cpp
//
// Script threads: Lua coroutines the game resumes once per frame (lua_thread_do_frame), up to 256, each started on a
// global function of its root state and kept alive on that state's stack. Threads end when their function returns,
// on an error (the root state's error callback is told) or when marked for termination; the last thread then moves
// into the freed slot. Thread_stack holds the threads being resumed (for lua_get_current_thread). Part of
// lua_lib_unity: the statics of lua_scripts.cpp are shared.

#include "volition/lua_lib/code/lua_thread.h"

#include "volition/libmodules/lua/lua_1_21/lua.h"
#include "volition/lua_lib/code/lua_scripts.h"

#include "keen/base/trace.hpp"

#include <string.h>

// ---- file-static globals
// lua_active_state Lua_states[4] (0x038B7180) is used here too; placed in lua_scripts.cpp.
static lua_thread* Thread_stack[16]; // 0x038B71A0
static lua_thread Script_threads[256]; // 0x038B71E0
static bool (*Check_thread_for_termination)(lua_thread*); // 0x038B920C
static bool (*Check_thread_for_skip)(lua_thread*); // 0x038B9210
// lua_State* Paused_lua_states[4] (0x038B91E0) is used here too; placed in lua_scripts.cpp.
// unsigned int Thread_stack_depth (0x038B91F0) is used here too; placed in lua_scripts.cpp.
// int (*Get_total_mem)() (0x038B91FC) is used here too; placed in lua_scripts.cpp.
// int Num_script_threads (0x038B9204) is used here too; placed in lua_scripts.cpp.
// unsigned short Next_thread_handle (0x038B9208) is used here too; placed in lua_scripts.cpp.
// void*  (*Realloc_cb)(void*, unsigned int) (0x038B9214) is used here too; placed in lua_scripts.cpp.
// void (*Free_cb)(void*) (0x038B9218) is used here too; placed in lua_scripts.cpp.

static const int Max_script_threads = 256;
static const unsigned int Max_thread_stack_depth = 16;

// 0x00C30600 (:142)
static bool does_prefix_match(const char* object_name, const char* prefix)
{
    if (object_name == NULL)
    {
        return false;
    }
    return strncmp(object_name, prefix, strlen(prefix)) == 0;
}

// 0x00C30630 (:155)
// A thread is active until its first resume, and afterwards while it has a call running or values on its stack.
static bool is_thread_active(lua_thread* t)
{
    lua_Debug ar;
    if (!t->first_run && lua_getstack(t->lua_state, 0, &ar) == 0 && lua_gettop(t->lua_state) == 0)
    {
        return false;
    }
    return true;
}

// 0x00C30690 (:187)
// Removes the thread from its root state's stack (so it can be collected), frees its function name and moves the last
// thread into its slot. t is cleared.
static void terminate_thread(lua_thread*& t)
{
    lua_State* thread_state = t->lua_state;
    int top = lua_gettop(t->root_state);
    for (int i = 1; i <= top; i++)
    {
        if (lua_type(t->root_state, i) == LUA_TTHREAD && lua_tothread(t->root_state, i) == thread_state)
        {
            lua_remove(t->root_state, i);
            top--;
            i--;
        }
    }
    if (Free_cb != NULL)
    {
        Free_cb(t->func_name);
    }
    const int index = (int)(t - Script_threads);
    if (index < Num_script_threads - 1)
    {
        Script_threads[index] = Script_threads[Num_script_threads - 1];
    }
    memset(&Script_threads[Num_script_threads - 1], 0, sizeof(lua_thread));
    Num_script_threads--;
    t = NULL;
}

// 0x00C307A0 (:252)
static void thread_stack_push(lua_thread* t)
{
    if (t != NULL && Thread_stack_depth < Max_thread_stack_depth)
    {
        Thread_stack[Thread_stack_depth] = t;
        Thread_stack_depth++;
    }
}

// 0x00C307C0 (:260)
static void thread_stack_pop()
{
    if (Thread_stack_depth != 0)
    {
        Thread_stack_depth--;
    }
}

// 0x00C307D0 (:293)
// A thread on the global Lua function func_name of root_state (or of the parent's root state). Main and init threads
// look the function up raw; other threads go through the globals' metatable. The handle is the next one no live
// thread uses. Original bug kept: without allocator callbacks the name copy writes through NULL.
static lua_thread* new_thread_internal(const char* func_name, lua_thread* parent_thread, bool is_main, bool is_init, lua_State* root_state)
{
    if (func_name == NULL || Num_script_threads >= Max_script_threads)
    {
        return NULL;
    }
    if (root_state == NULL && (parent_thread == NULL || (root_state = parent_thread->root_state) == NULL))
    {
        keen::debug::nativePrintArgs("No Lua state provided");
        return NULL;
    }
    // 0: a normal thread, 1: an init thread, 2: the main thread
    int thread_type;
    if (is_main)
    {
        thread_type = 2;
    }
    else if (is_init)
    {
        thread_type = 1;
    }
    else
    {
        thread_type = 0;
    }
    lua_pushstring(root_state, func_name);
    if (thread_type != 0)
    {
        lua_rawget(root_state, LUA_GLOBALSINDEX);
    }
    else
    {
        lua_gettable(root_state, LUA_GLOBALSINDEX);
    }
    const bool is_lua_function = lua_type(root_state, -1) == LUA_TFUNCTION && lua_iscfunction(root_state, -1) == 0;
    lua_settop(root_state, -2);
    if (!is_lua_function)
    {
        return NULL;
    }
    lua_State* thread_state = lua_newthread(root_state);
    if (thread_state == NULL)
    {
        return NULL;
    }
    unsigned short handle;
    bool in_use;
    do
    {
        handle = Next_thread_handle++;
        in_use = false;
        for (int i = 0; i < Num_script_threads; i++)
        {
            if (Script_threads[i].handle == handle)
            {
                in_use = true;
                break;
            }
        }
    } while (in_use);
    lua_thread* t = &Script_threads[Num_script_threads];
    Num_script_threads++;
    t->lua_state = thread_state;
    t->handle = handle;
    t->root_state = root_state;
    t->context = parent_thread != NULL ? parent_thread->context : 0;
    memset(&t->flags, 0, sizeof(t->flags));
    t->first_run = true;
    t->num_pushed_args = 0;
    t->flags.level_init = thread_type == 1;
    const unsigned int size = (unsigned int)strlen(func_name) + 1;
    t->func_name = Realloc_cb != NULL ? (char*)Realloc_cb(NULL, size) : NULL;
    strcpy(t->func_name, func_name);
    return t;
}

// 0x00C309A0 (:465)
void lua_thread_system_init()
{
    Thread_stack_depth = 0;
    Num_script_threads = 0;
    Next_thread_handle = 0;
}

// 0x00C309C0 (:476)
void lua_clear_all_threads()
{
    while (Num_script_threads > 0)
    {
        lua_thread* t = &Script_threads[Num_script_threads - 1];
        terminate_thread(t);
    }
}

// 0x00C325E0 (:485)
// Resumes every thread whose root state is not paused. A thread that ends leaves its slot to the last thread, which is
// then processed in the same pass.
void lua_thread_do_frame()
{
    for (int i = 0; i < Num_script_threads; i++)
    {
        lua_thread* t = &Script_threads[i];
        t->flags.processed_this_frame = 0;
        if (lua_root_state_is_paused(t->root_state))
        {
            continue;
        }
        bool was_error;
        if (!lua_process_thread(t, &was_error))
        {
            i--;
        }
    }
}

// 0x00C30AF0 (:620)
lua_thread* lua_new_thread(const char* func_name, lua_thread* parent_thread, bool is_main, bool is_init)
{
    return new_thread_internal(func_name, parent_thread, is_main, is_init, NULL);
}

// 0x00C30CB0 (:625)
lua_thread* lua_new_thread(const char* func_name, lua_State* root_state, bool is_main, bool is_init)
{
    return new_thread_internal(func_name, NULL, is_main, is_init, root_state);
}

// 0x00C30CD0 (:636)
// A live thread (not marked for termination, still active) by handle.
lua_thread* lua_get_thread(unsigned short handle)
{
    for (int i = 0; i < Num_script_threads; i++)
    {
        lua_thread* t = &Script_threads[i];
        if (!t->flags.terminate && is_thread_active(t) && t->handle == handle)
        {
            return t;
        }
    }
    return NULL;
}

// 0x00C30D70 (:661)
lua_thread* lua_get_thread(const lua_State* lua_state)
{
    if (lua_state == NULL)
    {
        return NULL;
    }
    for (int i = 0; i < Num_script_threads; i++)
    {
        lua_thread* t = &Script_threads[i];
        if (!t->flags.terminate && is_thread_active(t) && t->lua_state == lua_state)
        {
            return t;
        }
    }
    return NULL;
}

// 0x00C30DC0 (:689)
// Resumes the thread once. Returns false when the thread ended (t is then NULL and its slot holds the last thread).
// On the first run the function is pushed below the arguments pushed so far. Original behaviour kept: every resume
// passes num_pushed_args again.
bool lua_process_thread(lua_thread*& t, bool* was_error)
{
    if (t->flags.remote_thread)
    {
        return false;
    }
    if (t->flags.terminate || (Check_thread_for_termination != NULL && Check_thread_for_termination(t)))
    {
        terminate_thread(t);
        return false;
    }
    if (t->flags.processed_this_frame || t->flags.paused || (Check_thread_for_skip != NULL && Check_thread_for_skip(t)))
    {
        return true;
    }
    if (t->first_run)
    {
        lua_pushstring(t->lua_state, t->func_name);
        lua_gettable(t->lua_state, LUA_GLOBALSINDEX);
        if (t->num_pushed_args > 0)
        {
            lua_insert(t->lua_state, -1 - t->num_pushed_args);
        }
        t->first_run = false;
    }
    t->flags.processed_this_frame = 1;
    thread_stack_push(t);
    const bool error = lua_resume(t->lua_state, t->num_pushed_args) != 0;
    thread_stack_pop();
    lua_total_mem(); // a memory statistic whose use the release build leaves out
    if (was_error != NULL)
    {
        *was_error = error;
    }
    if (is_thread_active(t))
    {
        if (!error)
        {
            return true;
        }
    }
    else if (!error)
    {
        terminate_thread(t);
        return false;
    }
    if (t != NULL && t->lua_state != NULL)
    {
        void (*error_cb)(lua_thread*) = lua_get_error_cb(t->root_state);
        if (error_cb != NULL)
        {
            error_cb(t);
        }
    }
    terminate_thread(t);
    return false;
}

// 0x00C30F30 (:788)
void lua_execute_thread_immediate(lua_thread*& t)
{
    if (t != NULL && !t->flags.remote_thread)
    {
        lua_process_thread(t, NULL);
    }
}

// 0x00C30F50 (:809)
// Marks the threads whose function name starts with prefix + "_". Original bug kept: the prefix is copied into a
// 64-byte buffer unchecked.
void lua_clear_threads(const char* prefix)
{
    char full_prefix[64];
    strcpy(full_prefix, prefix);
    strcat(full_prefix, "_");
    for (int i = 0; i < Num_script_threads; i++)
    {
        if (does_prefix_match(Script_threads[i].func_name, full_prefix))
        {
            Script_threads[i].flags.terminate = 1;
        }
    }
}

// 0x00C31000 (:825)
// Ends every thread of the root state at once.
void lua_clear_threads(const lua_State* root_state)
{
    for (int i = 0; i < Num_script_threads; i++)
    {
        if (Script_threads[i].root_state == root_state)
        {
            lua_thread* t = &Script_threads[i];
            terminate_thread(t);
            i--;
        }
    }
}

// 0x00C32650 (:838)
// The push helpers add an argument for the thread's first resume (nil for a NULL string).
void lua_thread::push_string(const char* string)
{
    if (string == NULL)
    {
        lua_pushnil(lua_state);
    }
    else
    {
        lua_pushstring(lua_state, string);
    }
    num_pushed_args++;
}

// 0x00C31180 (:852)
void lua_thread::push_boolean(bool param)
{
    lua_pushboolean(lua_state, param);
    num_pushed_args++;
}

// 0x00C311A0 (:860)
void lua_thread::push_number(float param)
{
    lua_pushnumber(lua_state, param);
    num_pushed_args++;
}

// 0x00C311D0 (:868)
void lua_thread::push_number(double param)
{
    lua_pushnumber(lua_state, param);
    num_pushed_args++;
}

// 0x00C31200 (:876)
void lua_thread::push_nil()
{
    lua_pushnil(lua_state);
    num_pushed_args++;
}

// 0x00C31220 (:884)
// Stops at the first thread the functor returns false for.
void lua_thread_for_each(bool (*functor)(lua_thread*))
{
    for (int i = 0; i < Num_script_threads; i++)
    {
        if (!functor(&Script_threads[i]))
        {
            return;
        }
    }
}

// 0x00C31250 (:893)
void lua_clear_threads_in_context(unsigned int context)
{
    for (int i = 0; i < Num_script_threads; i++)
    {
        if (Script_threads[i].context == context)
        {
            Script_threads[i].flags.terminate = 1;
        }
    }
}

// 0x00C31280 (:903)
// The thread being resumed (the innermost one).
lua_thread* lua_get_current_thread()
{
    if (Thread_stack_depth != 0 && Thread_stack_depth <= Max_thread_stack_depth)
    {
        return Thread_stack[Thread_stack_depth - 1];
    }
    return NULL;
}

// 0x00C312A0 (:914)
void lua_set_thread_termination_cb(bool (*cb)(lua_thread*))
{
    Check_thread_for_termination = cb;
}

// 0x00C312B0 (:919)
void lua_set_thread_skip_cb(bool (*cb)(lua_thread*))
{
    Check_thread_for_skip = cb;
}

// 0x00C312C0 (:924)
// Through system_lib.lua's _GetAnyGlobalSilent, so dynamic globals count.
bool lua_does_func_exist(const char* _func_name, lua_State* _state)
{
    if (_func_name == NULL || _state == NULL)
    {
        return false;
    }
    lua_pushstring(_state, "_GetAnyGlobalSilent");
    lua_rawget(_state, LUA_GLOBALSINDEX);
    lua_pushstring(_state, _func_name);
    lua_call(_state, 1, 1);
    const int type = lua_type(_state, -1);
    lua_settop(_state, -2);
    return type == LUA_TFUNCTION;
}
