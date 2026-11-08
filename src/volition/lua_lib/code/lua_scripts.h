// Original: volition/lua_lib/code/lua_scripts.h
// Implementation: volition/lua_lib/code/lua_scripts.cpp
//
// Volition's script layer: lua_system_init (allocator callbacks), state creation and destruction, script loading
// (static and dynamic, include scripts), the per-frame garbage collection, globals by name, the nil-assignment and
// global-variable locks, memory statistics and the debug print helpers.
#pragma once


struct lua_State;
struct lua_thread;
class mempool_base;

void unused_dcf_dump_lua_globals(); // 0x00C32690 (:241)
void (*lua_get_error_cb(lua_State* root_state))(lua_thread*); // 0x00C300D0 (:265)
void lua_print_var(lua_State* L, int idx, int depth); // 0x00C313E0 (:277)
void lua_dump_state(lua_State* L, int depth); // 0x00C315F0 (:339)
void lua_system_init(void* (*realloc_cb)(void*, unsigned int), void (*free_cb)(void*), int (*get_total_mem)(), int (*get_free_mem)()); // 0x00C31740 (:401)
void lua_do_garbage_collection(lua_State* lua_state); // 0x00C30100 (:429)
bool lua_get_global_boolean(char* name, lua_State* lua_state); // 0x00C30120 (:444)
float lua_get_global_number(char* name, lua_State* lua_state); // 0x00C317E0 (:470)
double lua_get_global_number_large(char* name, lua_State* lua_state); // 0x00C30180 (:475)
void lua_set_global_boolean(char* name, bool value, lua_State* lua_state); // 0x00C301F0 (:501)
void lua_set_global_number(char* name, float value, lua_State* lua_state); // 0x00C30260 (:523)
void lua_set_global_string(char* name, const char* string, lua_State* lua_state); // 0x00C302D0 (:545)
bool lua_load_dynamic_script(const char* filename, lua_State* root_state, mempool_base* temp_mempool); // 0x00C31800 (:567)
bool lua_load_dynamic_script(const char* filename, unsigned char* data_buffer, unsigned int data_size, lua_State* root_state, mempool_base* _temp_pool); // 0x00C318D0 (:594)
void lua_unload_dynamic_script(const char* filename, lua_State* root_state); // 0x00C30330 (:621)
void lua_disable_nil_assignments(lua_State* lua_state); // 0x00C30380 (:637)
void lua_lock_global_variables(lua_State* lua_state); // 0x00C303B0 (:647)
bool lua_load_script(const char* filename, lua_State* lua_state, mempool_base* temp_mempool); // 0x00C303F0 (:662)
bool lua_load_script(const char* filename, unsigned char* data_buffer, unsigned int data_size, lua_State* lua_state, mempool_base* temp_mempool); // 0x00C30410 (:677)
void lua_do_frame(); // 0x00C319B0 (:694)
lua_State* lua_create_state(bool is_default, mempool_base* temp_pool, void (*error_cb)(lua_thread*)); // 0x00C32700 (:727)
void lua_destroy_state(lua_State* lua_state); // 0x00C31A20 (:760)
void lua_add_include_script(const char* fname); // 0x00C30440 (:786)
void* lua_realloc(void* addy, unsigned int bytes); // 0x00C30460 (:818)
void lua_free(void* addy); // 0x00C30470 (:836)
int lua_total_mem(); // 0x00C30480 (:852)
int lua_free_mem(); // 0x00C30490 (:861)
void lua_debug_printf_begin(); // 0x00C304A0 (:907)
void lua_debug_printf(const char*, ...); // 0x00C304B0 (:908)
void lua_debug_printf_end(); // 0x00C304C0 (:909)
bool lua_root_state_is_paused(lua_State* state); // 0x00C304D0 (:913)
void lua_pause_root_state(lua_State* state, bool pause); // 0x00C30500 (:925)
