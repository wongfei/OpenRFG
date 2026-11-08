// Original: volition/interface_lib/code/vint_data_responder.cpp
//
// Data responders (UI.md UI2): a script asks the game for records through a responder (retrieve_data names the Lua
// function to answer with and calls the game's fulfillment function); the game fills the responder's 16 elements and
// submits each record, which calls the Lua function with the elements as arguments. One responder in the pool.
// Compiled in interface_lib_unity.cpp; the Lua state and the Lua callback pool shared with vint_lua_actions.cpp are
// placed here (their dynamic initializers come first in the unity TU).

#include "volition/interface_lib/code/vint_data_responder.h"

#include "volition/interface_lib/code/internal/vint_dlist.h"
#include "volition/interface_lib/code/scripting/vint_lua_actions.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/lua_lib/code/lua_thread.h"
#include "volition/libmodules/lua/lua_1_21/lstate.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/util/color.h"
#include "library/modules/runtime/base/include/keen/base/trace.hpp"

#include <string.h>

// ---- globals
vint_data_responder* Data_responders_free; // 0x01A486B0
vint_data_responder* Data_responders_used; // 0x01A486B4

// ---- file-static globals
static lua_State* Vint_lua_state; // 0x01A4866C
static vint_callback_lua Lua_callbacks[128]; // 0x01A52C28
static unsigned int Data_responder_count; // 0x01A486B8
static vint_data_responder Data_responders[1]; // 0x01A55028

// 0x00479DE0 (.h:37)
vint_data_responder::vint_data_responder()
{
    m_handle = 0;
    m_fulfillment_func = nullptr;
    m_num_elements = 0;
}

// 0x00479E10 (.h:38)
vint_data_responder::~vint_data_responder()
{
    for (int i = 15; i >= 0; --i)
    {
        m_elements[i].clear();
    }
}

// 0x00461C10 (.h:41)
unsigned int vint_data_responder::get_handle() const
{
    return m_handle;
}

// 0x00461C20 (.h:50)
bool vint_data_responder::is_submission_complete()
{
    return m_submission_complete;
}

// 0x00461C30 (.h:52)
void vint_data_responder::set_parent_context(unsigned int context)
{
    m_parent_context = context;
}

// 0x004642B0 (:79)
void vint_data_responder::init(const char* _name)
{
    m_num_elements = 0;
    m_name_crc = crc_stri(_name, 0);
    ++Data_responder_count;
    m_handle = (unsigned int)(this - Data_responders) + (Data_responder_count << 16);
}

// 0x00476010 (:98)
void vint_data_responder::destroy()
{
    for (int i = 0; i < 16; ++i)
    {
        vint_variant& e = m_elements[i];
        if (e.get_type() == VINT_PROP_TYPE_STRING && (const char*)e != nullptr && Memory_callbacks != nullptr && Memory_callbacks->free != nullptr)
        {
            Memory_callbacks->free((void*)(const char*)e);
        }
        e.clear();
    }
    m_name_crc = 0;
    m_handle = 0;
    vint_dlist_remove(Data_responders_used, this, &vint_data_responder::factory_next, &vint_data_responder::factory_prev);
    vint_dlist_add(Data_responders_free, this, &vint_data_responder::factory_next, &vint_data_responder::factory_prev);
    // Original: the count drops on destroy (so a handle can repeat)
    --Data_responder_count;
}

// 0x00464310 (:127)
void vint_data_responder::retrieve_data(const char* _response_func_name, int _max_num_records, vint_variant* _parameters, int _num_parameters)
{
    // complete unless the game's fulfillment says the records come later
    m_submission_complete = true;
    if (_response_func_name == nullptr || m_fulfillment_func == nullptr)
    {
        return;
    }
    // Original: an unbounded copy into the 64-byte name
    strcpy(m_response_lua_func_name, _response_func_name);
    m_submission_complete = m_fulfillment_func(_max_num_records, _parameters, _num_parameters);
}

// 0x0046BA50 (:137)
void vint_data_responder::submit_record()
{
    // the elements as the Lua function's arguments (a color as three 0..1 numbers)
    lua_thread* thread = lua_new_thread(m_response_lua_func_name, Vint_lua_state, false, false);
    if (thread == nullptr)
    {
        return;
    }
    thread->context = m_parent_context;
    for (unsigned int i = 0; i < m_num_elements; ++i)
    {
        vint_variant& e = m_elements[i];
        switch (e.get_type())
        {
        case VINT_PROP_TYPE_INT:
        case VINT_PROP_TYPE_UINT:
        case VINT_PROP_TYPE_FLOAT:
            thread->push_number((double)e);
            break;
        case VINT_PROP_TYPE_STRING:
            thread->push_string((const char*)e);
            break;
        case VINT_PROP_TYPE_BOOL:
            thread->push_boolean((bool)e);
            break;
        case VINT_PROP_TYPE_COLOR:
        {
            color c = (color)e;
            thread->push_number((float)((double)c.red / 255.0));
            thread->push_number((float)((double)c.green / 255.0));
            thread->push_number((float)((double)c.blue / 255.0));
            break;
        }
        default:
            thread->push_nil();
            break;
        }
    }
    lua_execute_thread_immediate(thread);
}

// 0x00464370 (:183)
bool vint_data_responder::system_init()
{
    Data_responders_free = nullptr;
    vint_dlist_add(Data_responders_free, &Data_responders[0], &vint_data_responder::factory_next, &vint_data_responder::factory_prev);
    Data_responders_used = nullptr;
    return true;
}

// 0x004643A0 (:206)
vint_data_responder* vint_data_responder::create_new(const char* _name)
{
    if (_name == nullptr)
    {
        return nullptr;
    }
    vint_data_responder* responder = Data_responders_free;
    if (responder != nullptr)
    {
        vint_dlist_remove(Data_responders_free, responder, &vint_data_responder::factory_next, &vint_data_responder::factory_prev);
        vint_dlist_add(Data_responders_used, responder, &vint_data_responder::factory_next, &vint_data_responder::factory_prev);
        responder->init(_name);
    }
    return responder;
}

// 0x00464440 (:234)
vint_data_responder* vint_data_responder::find_by_name_crc(unsigned int _crc)
{
    if (_crc == 0)
    {
        return nullptr;
    }
    vint_data_responder* responder = Data_responders_used;
    while (responder != nullptr)
    {
        if (responder->m_name_crc == _crc)
        {
            return responder;
        }
        responder = responder->factory_next;
        if (responder == Data_responders_used)
        {
            break;
        }
    }
    return nullptr;
}

// 0x00464470 (:260)
vint_data_responder* vint_data_responder::find(const char* _name)
{
    if (_name == nullptr)
    {
        return nullptr;
    }
    return find_by_name_crc(crc_stri(_name, 0));
}

// 0x004644B0 (:268)
vint_data_responder* vint_data_responder::find(unsigned int _handle)
{
    if (_handle == 0 || (_handle & 0xFFFF) != 0)
    {
        return nullptr;
    }
    return Data_responders[0].m_handle == _handle ? &Data_responders[0] : nullptr;
}

// 0x0046BBF0 (:296)
void vint_data_responder::set_template(vint_variant_type* _element_types, int _num_elements)
{
    for (int i = 0; i < _num_elements; ++i)
    {
        // Original bug kept: the count is tested against 127 for the 16 elements
        if (_num_elements > 127)
        {
            break;
        }
        if (m_elements[i].get_type() == VINT_PROP_TYPE_NONE)
        {
            m_elements[i].set_type(_element_types[i]);
        }
        else if (m_elements[i].get_type() != _element_types[i])
        {
            keen::debug::nativePrintArgs("vint_data_responder::set_template() - Attempt to change element type after it's been established.");
        }
    }
    m_num_elements = (unsigned int)_num_elements;
}

// 0x004760E0 (:310)
void vint_data_responder::set_element(int _index, const vint_variant& _e)
{
    // strings are copied; an unchanged value is left alone
    if (_index >= 16)
    {
        return;
    }
    vint_variant& element = m_elements[_index];
    if (_e == element)
    {
        return;
    }
    if (_e.get_type() == VINT_PROP_TYPE_STRING)
    {
        const char* string = (const char*)_e;
        if (string == nullptr)
        {
            // Original: the old string is not freed
            element = (const char*)nullptr;
        }
        else
        {
            element.set_type(VINT_PROP_TYPE_STRING);
            const char* old = element.get_type() == VINT_PROP_TYPE_STRING ? (const char*)element : nullptr;
            if (Memory_callbacks != nullptr && Memory_callbacks->realloc != nullptr)
            {
                if (char* copy = (char*)Memory_callbacks->realloc((void*)old, (unsigned int)strlen(string) + 1))
                {
                    strcpy(copy, string);
                    element = (const char*)copy;
                }
            }
        }
    }
    else
    {
        element = _e;
    }
    if ((int)m_num_elements < _index + 1)
    {
        m_num_elements = (unsigned int)(_index + 1);
    }
}

// 0x004761D0 (:341)
void vint_data_responder::get_elements(vint_variant* _elements, unsigned int _num_elements)
{
    // Original: not bounded by the 16 elements
    if (_elements == nullptr)
    {
        return;
    }
    for (unsigned int i = 0; i < _num_elements; ++i)
    {
        _elements[i] = m_elements[i];
    }
}
