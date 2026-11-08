// Original: volition/lua_lib/code/lua_thread.h
// Implementation: volition/lua_lib/code/lua_thread.cpp
//
// Script threads (coroutines run by the game each frame): creation, lookup, processing, termination and skip callbacks,
// contexts, and the push helpers for arguments.
#pragma once


struct lua_State;

// size 0x20
struct lua_thread
{
    void push_string(const char* string); // 0x00C32650 (:838)
    void push_boolean(bool param); // 0x00C31180 (:852)
    void push_number(double param); // 0x00C311D0 (:868)
    void push_number(float param); // 0x00C311A0 (:860)
    void push_nil(); // 0x00C31200 (:876)

    unsigned short handle; // 0x00
    lua_State* lua_state; // 0x04
    lua_State* root_state; // 0x08
    bool first_run; // 0x0C
    char* func_name; // 0x10
    unsigned int context; // 0x14
    struct
    {
        unsigned short processed_this_frame : 1; // 0x00:0
        unsigned short paused : 1; // 0x00:1
        unsigned short terminate : 1; // 0x00:2
        unsigned short level_init : 1; // 0x00:3
        unsigned short looping : 1; // 0x00:4
        unsigned short patrol : 1; // 0x00:5
        unsigned short remote_thread : 1; // 0x00:6
    } flags; // 0x18
    int num_pushed_args; // 0x1C
};

void lua_thread_system_init(); // 0x00C309A0 (:465)
void lua_clear_all_threads(); // 0x00C309C0 (:476)
void lua_thread_do_frame(); // 0x00C325E0 (:485)
lua_thread* lua_new_thread(const char* func_name, lua_thread* parent_thread, bool is_main, bool is_init); // 0x00C30AF0 (:620)
lua_thread* lua_new_thread(const char* func_name, lua_State* root_state, bool is_main, bool is_init); // 0x00C30CB0 (:625)
lua_thread* lua_get_thread(unsigned short handle); // 0x00C30CD0 (:636)
lua_thread* lua_get_thread(const lua_State* lua_state); // 0x00C30D70 (:661)
bool lua_process_thread(lua_thread*& t, bool* was_error); // 0x00C30DC0 (:689)
void lua_execute_thread_immediate(lua_thread*& t); // 0x00C30F30 (:788)
void lua_clear_threads(const char* prefix); // 0x00C30F50 (:809)
void lua_clear_threads(const lua_State* root_state); // 0x00C31000 (:825)
void lua_thread_for_each(bool (*functor)(lua_thread*)); // 0x00C31220 (:884)
void lua_clear_threads_in_context(unsigned int context); // 0x00C31250 (:893)
lua_thread* lua_get_current_thread(); // 0x00C31280 (:903)
void lua_set_thread_termination_cb(bool (*cb)(lua_thread*)); // 0x00C312A0 (:914)
void lua_set_thread_skip_cb(bool (*cb)(lua_thread*)); // 0x00C312B0 (:919)
bool lua_does_func_exist(const char* _func_name, lua_State* _state); // 0x00C312C0 (:924)
