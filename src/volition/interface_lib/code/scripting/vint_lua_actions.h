// Original: volition/interface_lib/code/scripting/vint_lua_actions.h
// Implementation: volition/interface_lib/code/scripting/vint_lua_actions.cpp
//
// The vint natives of the documents' Lua scripts (vint_set_property, vint_object_find, subscriptions, tweens, ...),
// Lua callbacks (a Lua function called on an event) and the document's Lua state set-up (vint_lua_init).
#pragma once

#include "volition/interface_lib/code/vint_callback.h"

struct lua_State;
class mempool_base;
class vint_document;

// size 0x48
class vint_callback_lua : public vint_callback
{
public:
    vint_callback_lua(); // 0x00461C80 (:73)
    virtual ~vint_callback_lua(); // vtbl+0x00 0x00461C90 (:74)
    virtual bool trigger(unsigned int _doc_handle, vint_event_type _type, void* _data, unsigned int _bytes); // vtbl+0x04 0x00469FE0 (:619)
    static vint_callback_lua* create_new(const char* _lua_func_name); // 0x00461DA0 (:447)
    static void system_init(); // 0x00461CC0 (:436)
protected:
    virtual bool init(const char* _lua_func_name); // vtbl+0x08 0x00469EB0 (:469)
    virtual void return_to_factory(); // vtbl+0x10 0x00461E30 (:463)

    vint_callback_lua* factory_next; // 0x20
    vint_callback_lua* factory_prev; // 0x24
private:
    char m_lua_func_name[32]; // 0x28
};

// template instance: vint_variant lua_get_arg<vint_variant>(lua_State* lua_state, int& current_pos, bool is_required, const vint_variant& val_default); // 0x00479E40 (:162)
// template instance: int lua_add_return<vint_variant>(const vint_variant& value, lua_State* lua_state); // 0x00469CC0 (:390)
vint_document* get_doc_from_context(); // 0x0046A080 (:661)
lua_State* vint_lua_get_state(); // 0x004620F0 (:2025)
void vint_lua_actions_register_custom_functions(lua_State* lua_state); // 0x00482540 (:2030)
void vint_lua_document_init(vint_document* _doc, mempool_base* _temp_pool); // 0x00472CE0 (:2090)
void vint_lua_document_init(vint_document* _doc, unsigned char* data_buffer, unsigned int data_size, mempool_base* _temp_pool); // 0x00472DC0 (:2123)
void vint_lua_document_destroy(vint_document* _doc); // 0x00472EF0 (:2145)
void vint_lua_init(void (*_register_funcs_callback)(lua_State*), mempool_base* _temp_pool); // 0x004828A0 (:2174)
void vint_lua_garbage_collect(); // 0x00462100 (:2205)
