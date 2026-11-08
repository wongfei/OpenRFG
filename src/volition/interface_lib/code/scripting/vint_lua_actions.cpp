// Original: volition/interface_lib/code/scripting/vint_lua_actions.cpp
//
// The documents' Lua side (UI.md UI2): one Lua state for all documents (vint_lua_init loads vint_lib.lua and registers
// the vint_* natives); a document's scripts are its "lua_script_file" metadata, run as "<doc>_init" on load and
// "<doc>_cleanup" on destroy, their threads in the document's context. Lua callbacks (a pool of 128) deliver input,
// tween and data events to named Lua functions. The natives read their arguments from the bottom of the stack (at most
// the native's own count) and handles are numbers. Compiled in interface_lib_unity.cpp after vint_data_responder.cpp,
// which holds the Lua state and the callback pool.

#include "volition/interface_lib/code/scripting/vint_lua_actions.h"

#include "volition/interface_lib/code/objects/animation/vint_object_animation.h"
#include "volition/interface_lib/code/objects/animation/vint_object_tween.h"
#include "volition/interface_lib/code/objects/vint_element_base.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/objects/vint_object_types.h"
#include "volition/interface_lib/code/resources/vint_resource_bitmap.h"
#include "volition/interface_lib/code/resources/vint_resource_font.h"
#include "volition/interface_lib/code/vint_data_event.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_data_responder.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_input_internal.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/libmodules/lua/lua_1_21/lua.h"
#include "volition/lua_lib/code/lua_scripts.h"
#include "volition/lua_lib/code/lua_thread.h"
#include "volition/lua_lib/code/macros_v2.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"
#include "library/modules/runtime/base/include/keen/base/trace.hpp"

#include <math.h>

// ---- file-static globals
// an object handle that stops the property natives in a debugger (never set in rfg.exe)
static unsigned int Vint_object_debug_target = 0xFFFFFFFF; // 0x014F594C
static vint_callback_lua* Active_lua_callbacks; // 0x01A48670
static vint_callback_lua* Avail_lua_callbacks; // 0x01A48674

// The sizes the event handlers expect with their data.
static const unsigned int VINT_LUA_TWEEN_EVENT_BYTES = 8;
static const unsigned int VINT_LUA_INPUT_EVENT_BYTES = 0x58;
static const unsigned int VINT_LUA_DATA_EVENT_BYTES = 0x128;
static const unsigned int VINT_LUA_DATA_ITEM_BYTES = 0x430;

// ---- native helpers (inlined in rfg.exe)

// The arguments a native reads: the stack, at most _max of them.
static int vint_lua_num_args(lua_State* _l, int _max)
{
    int n = lua_gettop(_l);
    return n > _max ? _max : n;
}

// A number as a handle (_ftol2: truncated to 64 bits, the low half kept).
static unsigned int vint_lua_to_handle(lua_State* _l, int _index)
{
    return (unsigned int)(long long)lua_tonumber(_l, _index);
}

// A handle pushed as a number.
static int vint_lua_push_handle(lua_State* _l, unsigned int _handle)
{
    lua_pushnumber(_l, (double)_handle);
    return 1;
}

// A Lua callback by function name: the existing one with a new reference, or a new one.
static vint_callback_lua* vint_lua_callback_acquire(const char* _func_name)
{
    if (vint_callback* cb = vint_callback::find_by_name(crc_stri(_func_name, 0)))
    {
        cb->add_ref();
        return (vint_callback_lua*)cb;
    }
    return vint_callback_lua::create_new(_func_name);
}

// ---- argument helpers

// 0x00479E40 (:162)
// A value of the default's type from the arguments: numbers for numeric types (a color is three 0..1 numbers, a
// vector two), names for colors (the color table), callbacks (Lua functions), bitmaps and fonts; an untyped default
// takes an integer as uint (int when negative), any other number as float, or a string.
template <>
vint_variant lua_get_arg<vint_variant>(lua_State* lua_state, int& current_pos, bool is_required, const vint_variant& val_default)
{
    if (!is_required && (current_pos < 1 || lua_type(lua_state, -current_pos) == LUA_TNIL))
    {
        if (current_pos > 0)
        {
            current_pos--;
        }
        return val_default;
    }
    vint_variant value;
    value = val_default;
    int lua_t = current_pos < 1 ? LUA_TNIL : lua_type(lua_state, -current_pos);
    switch (val_default.get_type())
    {
    case VINT_PROP_TYPE_INT:
    case VINT_PROP_TYPE_UINT:
        if (lua_t == LUA_TNUMBER)
        {
            value = lua_get_arg<int>(lua_state, current_pos, is_required, (int)val_default);
        }
        break;
    case VINT_PROP_TYPE_FLOAT:
        if (lua_t == LUA_TNUMBER)
        {
            value = lua_get_arg<float>(lua_state, current_pos, is_required, (float)val_default);
        }
        break;
    case VINT_PROP_TYPE_STRING:
        if (lua_t == LUA_TSTRING || lua_t == LUA_TNUMBER)
        {
            value = lua_get_arg<const char*>(lua_state, current_pos, is_required, (const char*)val_default);
        }
        else if (lua_t == LUA_TNIL)
        {
            value.clear();
            if (is_required || current_pos > 0)
            {
                current_pos--;
            }
        }
        break;
    case VINT_PROP_TYPE_BOOL:
        if (lua_t == LUA_TBOOLEAN)
        {
            value = lua_get_arg<bool>(lua_state, current_pos, is_required, (bool)val_default);
        }
        break;
    case VINT_PROP_TYPE_COLOR:
        if (lua_t == LUA_TNUMBER)
        {
            // x87: each part times 255, truncated
            color d = (color)val_default;
            color c = d;
            c.red = (unsigned char)(int)((double)lua_get_arg<float>(lua_state, current_pos, is_required, (float)(int)d.red) * 255.0);
            c.green = (unsigned char)(int)((double)lua_get_arg<float>(lua_state, current_pos, is_required, (float)(int)d.green) * 255.0);
            c.blue = (unsigned char)(int)((double)lua_get_arg<float>(lua_state, current_pos, is_required, (float)(int)d.blue) * 255.0);
            value = c;
        }
        else if (lua_t == LUA_TSTRING)
        {
            const char* name = lua_get_arg<const char*>(lua_state, current_pos, is_required, "");
            if (const vint_variant_color* c = vint_color_table_find(name, -1))
            {
                value = *c;
            }
        }
        break;
    case VINT_PROP_TYPE_VECTOR2F:
        if (lua_t == LUA_TNUMBER)
        {
            vector2 d = (vector2)val_default;
            vector2 v;
            v.x = lua_get_arg<float>(lua_state, current_pos, is_required, d.x);
            v.y = lua_get_arg<float>(lua_state, current_pos, is_required, d.y);
            value = v;
        }
        break;
    case VINT_PROP_TYPE_CALLBACK:
        if (lua_t == LUA_TSTRING)
        {
            const char* name = lua_get_arg<const char*>(lua_state, current_pos, is_required, "");
            if (vint_callback_lua* cb = vint_lua_callback_acquire(name))
            {
                // the variant takes its own reference
                value = (vint_callback*)cb;
                cb->release();
            }
        }
        else if (lua_t == LUA_TNIL)
        {
            value = (vint_callback*)nullptr;
        }
        else
        {
            keen::debug::nativePrintArgs("vint_set_property() - lua type not supported for callbacks");
        }
        break;
    case VINT_PROP_TYPE_BITMAP:
    {
        vint_resource_bitmap* bitmap = nullptr;
        if (lua_t == LUA_TNUMBER)
        {
            bitmap = vint_resource_bitmap::find(lua_get_arg<unsigned int>(lua_state, current_pos, is_required, 0u));
            if (bitmap == nullptr)
            {
                break;
            }
            bitmap->add_ref();
        }
        else if (lua_t == LUA_TSTRING)
        {
            bitmap = vint_resource_bitmap::load(lua_get_arg<const char*>(lua_state, current_pos, is_required, ""));
        }
        if (bitmap != nullptr)
        {
            // the variant takes its own reference
            value = bitmap;
            bitmap->release();
        }
        break;
    }
    case VINT_PROP_TYPE_FONT:
        if (lua_t == LUA_TNUMBER)
        {
            // a font by its game title handle
            vint_resource_font* font = vint_resource_font::find_by_title_handle(lua_get_arg<unsigned int>(lua_state, current_pos, is_required, 0u));
            if (font != nullptr)
            {
                // Original: the extra reference is kept (a leak)
                font->add_ref();
            }
            value = font;
        }
        else if (lua_t == LUA_TSTRING)
        {
            value = lua_get_arg<const char*>(lua_state, current_pos, is_required, "");
        }
        break;
    case VINT_PROP_TYPE_UINT64:
        if (lua_t == LUA_TNUMBER)
        {
            value = lua_get_arg<uint64_t>(lua_state, current_pos, is_required, (uint64_t)val_default);
        }
        break;
    case VINT_PROP_TYPE_ENUM:
        if (lua_t == LUA_TNUMBER)
        {
            value = lua_get_arg<int>(lua_state, current_pos, is_required, 0x7FFFFFFF);
        }
        else if (lua_t == LUA_TSTRING)
        {
            value = lua_get_arg<const char*>(lua_state, current_pos, is_required, "");
        }
        break;
    default:
        if (lua_t == LUA_TNUMBER)
        {
            double d = lua_get_arg<double>(lua_state, current_pos, is_required, 0.0);
            if (floor(d) == d)
            {
                if (d >= 0.0)
                {
                    // x87: truncated to 64 bits, the low half kept
                    value = (unsigned int)(long long)d;
                }
                else
                {
                    value = (int)d;
                }
            }
            else if (value.get_type() == VINT_PROP_TYPE_NONE || value.get_type() == VINT_PROP_TYPE_FLOAT)
            {
                value = (float)d;
            }
        }
        else if (lua_t == LUA_TSTRING)
        {
            value.set_type(VINT_PROP_TYPE_STRING);
            value = lua_get_arg<const char*>(lua_state, current_pos, is_required, (const char*)nullptr);
        }
        break;
    }
    return value;
}

// 0x00469CC0 (:390)
// A value pushed: numbers (a color as three 0..1 numbers, a vector two), a boolean or a string; anything else nil.
template <>
int lua_add_return<vint_variant>(const vint_variant& value, lua_State* lua_state)
{
    switch (value.get_type())
    {
    case VINT_PROP_TYPE_INT:
    case VINT_PROP_TYPE_UINT:
        lua_pushnumber(lua_state, (double)value);
        return 1;
    case VINT_PROP_TYPE_FLOAT:
        lua_pushnumber(lua_state, (double)(float)value);
        return 1;
    case VINT_PROP_TYPE_STRING:
        lua_pushstring(lua_state, (const char*)value);
        return 1;
    case VINT_PROP_TYPE_BOOL:
        lua_pushboolean(lua_state, (bool)value);
        return 1;
    case VINT_PROP_TYPE_COLOR:
    {
        color c = (color)value;
        lua_pushnumber(lua_state, (double)(float)c.red / 255.0);
        lua_pushnumber(lua_state, (double)(float)c.green / 255.0);
        lua_pushnumber(lua_state, (double)(float)c.blue / 255.0);
        return 3;
    }
    case VINT_PROP_TYPE_VECTOR2F:
    {
        vector2 v = (vector2)value;
        lua_pushnumber(lua_state, (double)v.x);
        lua_pushnumber(lua_state, (double)v.y);
        return 2;
    }
    default:
        lua_pushnil(lua_state);
        return 1;
    }
}

// ---- vint_callback_lua

// 0x00461C80 (:73)
vint_callback_lua::vint_callback_lua()
{
}

// 0x00461C90 (:74)
vint_callback_lua::~vint_callback_lua()
{
}

// 0x00461CC0 (:436)
void vint_callback_lua::system_init()
{
    // Original: the lists are not reset first
    for (int i = 0; i < 128; ++i)
    {
        vint_dlist_add(Avail_lua_callbacks, &Lua_callbacks[i], &vint_callback_lua::factory_next, &vint_callback_lua::factory_prev);
    }
}

// 0x00461DA0 (:447)
vint_callback_lua* vint_callback_lua::create_new(const char* _lua_func_name)
{
    vint_callback_lua* cb = Avail_lua_callbacks;
    if (cb == nullptr)
    {
        return nullptr;
    }
    if (!cb->init(_lua_func_name))
    {
        return nullptr;
    }
    vint_dlist_remove(Avail_lua_callbacks, cb, &vint_callback_lua::factory_next, &vint_callback_lua::factory_prev);
    vint_dlist_add(Active_lua_callbacks, cb, &vint_callback_lua::factory_next, &vint_callback_lua::factory_prev);
    return cb;
}

// 0x00461E30 (:463)
void vint_callback_lua::return_to_factory()
{
    vint_dlist_remove(Active_lua_callbacks, this, &vint_callback_lua::factory_next, &vint_callback_lua::factory_prev);
    vint_dlist_add(Avail_lua_callbacks, this, &vint_callback_lua::factory_next, &vint_callback_lua::factory_prev);
}

// 0x00469EB0 (:469)
bool vint_callback_lua::init(const char* _lua_func_name)
{
    if (_lua_func_name == nullptr || !vint_callback::init(_lua_func_name))
    {
        return false;
    }
    strncpy_safe(m_lua_func_name, _lua_func_name, sizeof(m_lua_func_name));
    return true;
}

// 0x00461EA0 (:483)
// LTCG: arguments not mapped (only called through vint_callback_lua::trigger).
static bool lua_tween_event_handler(vint_document* _doc, vint_event_type _event, const char* _lua_func, void* _data, unsigned int _bytes)
{
    // (tween handle, event property name)
    if (_event != VINT_EVENT_TWEEN || _doc == nullptr || _lua_func == nullptr || _data == nullptr || _bytes != VINT_LUA_TWEEN_EVENT_BYTES)
    {
        return false;
    }
    vint_object_base* tween = ((vint_object_base* const*)_data)[0];
    const char* event_name = ((const char* const*)_data)[1];
    if (tween == nullptr || event_name == nullptr)
    {
        return false;
    }
    lua_thread* thread = lua_new_thread(_lua_func, Vint_lua_state, false, false);
    if (thread == nullptr)
    {
        return false;
    }
    thread->push_number((double)tween->get_handle());
    thread->push_string(event_name);
    thread->context = _doc->get_handle();
    lua_execute_thread_immediate(thread);
    return true;
}

// 0x00469EF0 (:505)
static bool lua_input_event_handler(vint_document* _doc, vint_event_type _event, const char* _lua_func, void* _data, unsigned int _bytes)
{
    // an event: (nil, event name, acceleration scale); raw: (input name, value)
    if (_event != VINT_EVENT_INPUT || _doc == nullptr || _lua_func == nullptr || _data == nullptr || _bytes != VINT_LUA_INPUT_EVENT_BYTES)
    {
        return false;
    }
    const vint_input_subscription* sub = (const vint_input_subscription*)_data;
    if (sub->get_type() == VINT_INPUT_SUBSCRIPTION_UNKNOWN)
    {
        return false;
    }
    const char* name = sub->get_name();
    if (name == nullptr)
    {
        return false;
    }
    lua_thread* thread = lua_new_thread(_lua_func, Vint_lua_state, false, false);
    if (thread == nullptr)
    {
        return false;
    }
    float value;
    if (sub->get_type() == VINT_INPUT_SUBSCRIPTION_EVENT)
    {
        thread->push_nil();
        thread->push_string(name);
        value = sub->get_acceleration_scale();
    }
    else if (sub->get_type() == VINT_INPUT_SUBSCRIPTION_RAW)
    {
        thread->push_string(name);
        value = sub->get_last_reported_time();
    }
    else
    {
        // Original: the thread is left unrun
        keen::debug::nativePrintArgs("unsupported input subscription type");
        return false;
    }
    thread->push_number(value);
    thread->context = _doc->get_handle();
    lua_execute_thread_immediate(thread);
    return true;
}

// 0x00461F50 (:548)
static bool lua_data_event_handler(vint_document* _doc, vint_event_type _event, const char* _lua_func, void* _data, unsigned int _bytes)
{
    // (event handle, "update")
    if (_doc == nullptr || _lua_func == nullptr || _data == nullptr || _bytes != VINT_LUA_DATA_EVENT_BYTES)
    {
        return false;
    }
    lua_thread* thread = lua_new_thread(_lua_func, Vint_lua_state, false, false);
    if (thread == nullptr)
    {
        return false;
    }
    thread->push_number((double)((const vint_data_event*)_data)->get_handle());
    if (_event != VINT_EVENT_DATA_EVENT_UPDATE)
    {
        // Original: the thread is left unrun
        keen::debug::nativePrintArgs("lua_data_event_handler() - bad event type");
        return false;
    }
    thread->push_string("update");
    thread->context = _doc->get_handle();
    lua_execute_thread_immediate(thread);
    return true;
}

// 0x00462000 (:579)
static bool lua_data_item_handler(vint_document* _doc, vint_event_type _event, const char* _lua_func, void* _data, unsigned int _bytes)
{
    // (item handle, "update" / "insert" / "remove")
    if (_doc == nullptr || _lua_func == nullptr || _data == nullptr || _bytes != VINT_LUA_DATA_ITEM_BYTES)
    {
        return false;
    }
    lua_thread* thread = lua_new_thread(_lua_func, Vint_lua_state, false, false);
    if (thread == nullptr)
    {
        return false;
    }
    thread->push_number((double)((const vint_data_item*)_data)->get_handle());
    const char* what;
    switch (_event)
    {
    case VINT_EVENT_DATA_ITEM_UPDATE:
    case VINT_EVENT_DATA_GROUP_UPDATE:
        what = "update";
        break;
    case VINT_EVENT_DATA_GROUP_INSERT:
        what = "insert";
        break;
    case VINT_EVENT_DATA_GROUP_REMOVE:
        what = "remove";
        break;
    default:
        keen::debug::nativePrintArgs("lua_data_item_handler() - bad event type");
        return false;
    }
    thread->push_string(what);
    thread->context = _doc->get_handle();
    lua_execute_thread_immediate(thread);
    return true;
}

// 0x00469FE0 (:619)
bool vint_callback_lua::trigger(unsigned int _doc_handle, vint_event_type _type, void* _data, unsigned int _bytes)
{
    vint_document* doc = vint_document::find(_doc_handle);
    if (doc == nullptr)
    {
        return false;
    }
    switch (_type)
    {
    case VINT_EVENT_INPUT:
        return lua_input_event_handler(doc, _type, m_lua_func_name, _data, _bytes);
    case VINT_EVENT_TWEEN:
        return lua_tween_event_handler(doc, _type, m_lua_func_name, _data, _bytes);
    case VINT_EVENT_DATA_ITEM_UPDATE:
    case VINT_EVENT_DATA_GROUP_UPDATE:
    case VINT_EVENT_DATA_GROUP_INSERT:
    case VINT_EVENT_DATA_GROUP_REMOVE:
        return lua_data_item_handler(doc, _type, m_lua_func_name, _data, _bytes);
    case VINT_EVENT_DATA_EVENT_UPDATE:
        return lua_data_event_handler(doc, _type, m_lua_func_name, _data, _bytes);
    default:
        return false;
    }
}

// 0x0046A080 (:661)
vint_document* get_doc_from_context()
{
    // the running thread's context is its document's handle
    lua_thread* thread = lua_get_current_thread();
    if (thread == nullptr)
    {
        return nullptr;
    }
    return vint_document::find(thread->context);
}

// ---- natives

// 0x0046A0C0 (:670)
static int vint_debug_sleep(lua_State* lua_state)
{
    // (seconds): busy-waits
    int n = vint_lua_num_args(lua_state, 1);
    double seconds = lua_tonumber(lua_state, -n);
    int start = timer_get(TIMER_MILLISECONDS);
    // Original bug kept: the wait is start minus the milliseconds negated, so it never waits
    while (timer_get(TIMER_MILLISECONDS) < start - (int)((double)(float)seconds * -1000.0))
    {
    }
    return 0;
}

// The event a group subscription names.
static bool vint_lua_group_event(const char* _name, vint_event_type& _event)
{
    if (v_stricmp(_name, "update") == 0)
    {
        _event = VINT_EVENT_DATA_GROUP_UPDATE;
    }
    else if (v_stricmp(_name, "remove") == 0)
    {
        _event = VINT_EVENT_DATA_GROUP_REMOVE;
    }
    else if (v_stricmp(_name, "insert") == 0)
    {
        _event = VINT_EVENT_DATA_GROUP_INSERT;
    }
    else
    {
        return false;
    }
    return true;
}

// 0x0046A130 (:680)
static int vint_datagroup_add_subscription(lua_State* lua_state)
{
    // (group handle or name, "update" / "insert" / "remove", Lua function); by name it waits for the group
    int n = vint_lua_num_args(lua_state, 3);
    int lua_t = n < 1 ? LUA_TNIL : lua_type(lua_state, -n);
    vint_data_group* group;
    const char* group_name = nullptr;
    if (lua_t == LUA_TNUMBER)
    {
        group = vint_data_group::find(vint_lua_to_handle(lua_state, -n));
    }
    else if (lua_t == LUA_TSTRING)
    {
        group_name = lua_tostring(lua_state, -n);
        group = vint_data_group::find(group_name);
    }
    else
    {
        return 0;
    }
    const char* event_name = lua_tostring(lua_state, -(n - 1));
    const char* func_name = lua_tostring(lua_state, -(n - 2));
    vint_document* doc = get_doc_from_context();
    if (doc == nullptr || event_name == nullptr || func_name == nullptr)
    {
        return 0;
    }
    vint_event_type event;
    if (!vint_lua_group_event(event_name, event))
    {
        return 0;
    }
    vint_callback_lua* cb = vint_lua_callback_acquire(func_name);
    if (cb == nullptr)
    {
        VLIB_CRITICAL_FAILURE();
        return 0;
    }
    if (group != nullptr)
    {
        group->add_subscription(cb->get_handle(), event, doc->get_handle());
    }
    else if (group_name != nullptr)
    {
        vint_data_group::add_subscription(crc_stri(group_name, 0), event, cb->get_handle(), doc->get_handle());
    }
    // the subscription holds its own reference
    cb->release();
    return 0;
}

// 0x00472BA0 (:770)
static int vint_datagroup_insert_item(lua_State* lua_state)
{
    // (group handle): a new unnamed item's handle (0 when none is free)
    int n = vint_lua_num_args(lua_state, 1);
    vint_data_group* group = vint_data_group::find(vint_lua_to_handle(lua_state, -n));
    if (group == nullptr)
    {
        return 0;
    }
    vint_data_item* item = group->add_data_item(nullptr);
    return vint_lua_push_handle(lua_state, item != nullptr ? item->get_handle() : 0);
}

// 0x00472C80 (:791)
static int vint_datagroup_remove_item(lua_State* lua_state)
{
    // (group name, item handle)
    int n = vint_lua_num_args(lua_state, 2);
    const char* name = lua_tostring(lua_state, -n);
    unsigned int handle = vint_lua_to_handle(lua_state, -(n - 1));
    if (vint_data_group* group = vint_data_group::find(name))
    {
        group->remove_data_item(handle);
    }
    return 0;
}

// 0x0046A340 (:805)
static int vint_object_add_child(lua_State* lua_state)
{
    // (parent handle, child handle), both in the script's document
    int n = vint_lua_num_args(lua_state, 2);
    unsigned int parent_handle = vint_lua_to_handle(lua_state, -n);
    unsigned int child_handle = vint_lua_to_handle(lua_state, -(n - 1));
    vint_document* doc = get_doc_from_context();
    if (doc == nullptr)
    {
        return 0;
    }
    vint_object_base* parent = vint_object_base::find_object(parent_handle, doc);
    vint_object_base* child = vint_object_base::find_object(child_handle, doc);
    if (parent != nullptr && child != nullptr)
    {
        child->remove_from_parent();
        parent->add_child(child);
    }
    return 0;
}

// 0x0046A400 (:826)
static int vint_object_clone(lua_State* lua_state)
{
    // (handle, parent handle optional): the clone's handle (under the given parent, else the original's), or 0
    int n = vint_lua_num_args(lua_state, 2);
    unsigned int handle = vint_lua_to_handle(lua_state, -n);
    unsigned int parent_handle = 0;
    if (n - 1 >= 1 && lua_type(lua_state, -(n - 1)) != LUA_TNIL)
    {
        parent_handle = vint_lua_to_handle(lua_state, -(n - 1));
    }
    vint_document* doc = get_doc_from_context();
    // Original: the original is not checked against the script's document
    vint_object_base* obj = doc != nullptr ? vint_object_base::find_object(handle, nullptr) : nullptr;
    if (obj == nullptr)
    {
        return vint_lua_push_handle(lua_state, 0);
    }
    vint_object_base* parent = parent_handle != 0 && parent_handle != 0xFFFFFFFF ? vint_object_base::find_object(parent_handle, doc) : nullptr;
    if (parent == nullptr)
    {
        parent = obj->parent_object;
    }
    vint_object_base* clone = obj->clone(true, parent);
    return vint_lua_push_handle(lua_state, clone != nullptr ? clone->get_handle() : 0);
}

// 0x0046A510 (:862)
static int vint_object_clone_rename(lua_State* lua_state)
{
    // (handle, new name): a parentless clone's handle, or 0
    int n = vint_lua_num_args(lua_state, 2);
    unsigned int handle = vint_lua_to_handle(lua_state, -n);
    const char* name = lua_tostring(lua_state, -(n - 1));
    vint_document* doc = get_doc_from_context();
    vint_object_base* obj = name != nullptr && doc != nullptr ? vint_object_base::find_object(handle, nullptr) : nullptr;
    if (obj != nullptr)
    {
        if (vint_object_base* clone = obj->clone(true, nullptr))
        {
            clone->set_name(name);
            unsigned int clone_handle = clone->get_handle();
            return lua_add_return<unsigned int>(clone_handle, lua_state);
        }
    }
    return vint_lua_push_handle(lua_state, 0);
}

// 0x0047FF50 (:899)
static int vint_anim_clone_and_retarget(lua_State* lua_state)
{
    // (animation handle, new target handle): a parentless clone whose tweens target the same-named objects under the
    // new target; the clone's handle, or 0
    int n = vint_lua_num_args(lua_state, 2);
    unsigned int anim_handle = vint_lua_to_handle(lua_state, -n);
    unsigned int target_handle = vint_lua_to_handle(lua_state, -(n - 1));
    vint_document* doc = get_doc_from_context();
    vint_object_base* anim = doc != nullptr ? vint_object_base::find_object(anim_handle, nullptr) : nullptr;
    vint_object_base* target = anim != nullptr ? vint_object_base::find_object(target_handle, nullptr) : nullptr;
    vint_object_base* clone = target != nullptr ? anim->clone(true, nullptr) : nullptr;
    if (clone == nullptr)
    {
        return vint_lua_push_handle(lua_state, 0);
    }
    for (vint_object_base* child = clone->first_child; child != nullptr; child = child->next_sibling)
    {
        if (child->get_type() != vint_object_tween::Object_type)
        {
            continue;
        }
        if (vint_object_base* found = target->find_child(((vint_object_tween*)child)->get_target_name_crc()))
        {
            vint_variant value;
            value = found->get_handle();
            child->set_property_value("target_handle", value, 0);
        }
    }
    return vint_lua_push_handle(lua_state, clone->get_handle());
}

// 0x0046A600 (:948)
static int vint_object_create(lua_State* lua_state)
{
    // (name, type name, parent handle): the new object's handle (nothing when it fails)
    int n = vint_lua_num_args(lua_state, 3);
    const char* name = lua_tostring(lua_state, -n);
    const char* type_name = lua_tostring(lua_state, -(n - 1));
    unsigned int parent_handle = vint_lua_to_handle(lua_state, -(n - 2));
    if (type_name == nullptr || name == nullptr)
    {
        return 0;
    }
    vint_document* doc = get_doc_from_context();
    vint_object_base* parent = doc != nullptr ? vint_object_base::find_object(parent_handle, doc) : nullptr;
    if (parent == nullptr)
    {
        return 0;
    }
    vint_object_base* obj = vint_object_base::create_new(parent, name, type_name);
    if (obj == nullptr)
    {
        return 0;
    }
    unsigned int handle = obj->get_handle();
    return lua_add_return<unsigned int>(handle, lua_state);
}

// 0x0046A6D0 (:979)
static int vint_dataitem_add_subscription(lua_State* lua_state)
{
    // (item handle or name, "update", Lua function); by name it waits for the item
    int n = vint_lua_num_args(lua_state, 3);
    int lua_t = n < 1 ? LUA_TNIL : lua_type(lua_state, -n);
    unsigned int handle = 0;
    const char* item_name = nullptr;
    if (lua_t == LUA_TNUMBER)
    {
        handle = vint_lua_to_handle(lua_state, -n);
    }
    else if (lua_t == LUA_TSTRING)
    {
        item_name = lua_tostring(lua_state, -n);
    }
    else
    {
        return 0;
    }
    // Original: the event name is required but not compared
    const char* event_name = lua_tostring(lua_state, -(n - 1));
    const char* func_name = lua_tostring(lua_state, -(n - 2));
    vint_document* doc = get_doc_from_context();
    if (doc == nullptr || event_name == nullptr || func_name == nullptr)
    {
        return 0;
    }
    vint_data_item* item = vint_data_item::find(handle);
    if (item == nullptr && item_name == nullptr)
    {
        return 0;
    }
    vint_callback_lua* cb = vint_lua_callback_acquire(func_name);
    if (cb == nullptr)
    {
        return 0;
    }
    if (item == nullptr)
    {
        vint_data_item::add_subscription(crc_stri(item_name, 0), VINT_EVENT_DATA_ITEM_UPDATE, cb->get_handle(), doc->get_handle());
    }
    else
    {
        item->add_subscription(cb->get_handle(), VINT_EVENT_DATA_ITEM_UPDATE, doc->get_handle());
    }
    cb->release();
    return 0;
}

// 0x0046A840 (:1036)
static int vint_dataitem_find(lua_State* lua_state)
{
    // (name): the item's handle, or 0
    int n = vint_lua_num_args(lua_state, 1);
    const char* name = lua_tostring(lua_state, -n);
    vint_data_item* item = vint_data_item::find(name);
    return vint_lua_push_handle(lua_state, item != nullptr ? item->get_handle() : 0);
}

// 0x0047A540 (:1052)
static int vint_dataitem_set(lua_State* lua_state)
{
    // (item handle, values...): each element in turn from the arguments, in its own type (nil or missing keeps it)
    int n = vint_lua_num_args(lua_state, 97);
    if (n < 1)
    {
        keen::debug::nativePrintArgs("Please specify a data item handle");
        return 0;
    }
    if (lua_type(lua_state, -n) != LUA_TNUMBER)
    {
        return 0;
    }
    vint_data_item* item = vint_data_item::find(vint_lua_to_handle(lua_state, -n));
    if (item == nullptr)
    {
        return 0;
    }
    int pos = n - 1;
    vint_variant elements[32];
    item->get(elements, 32);
    int num = item->get_num_elements();
    // lua_get_arg<vint_variant> inlined in rfg.exe
    for (int i = 0; i < num; ++i)
    {
        elements[i] = lua_get_arg<vint_variant>(lua_state, pos, false, elements[i]);
    }
    for (int i = 0; i < num && i < 32; ++i)
    {
        item->set_element(i, elements[i]);
    }
    return 0;
}

// 0x0047AF80 (:1082)
static int vint_dataitem_get(lua_State* lua_state)
{
    // (item handle): the elements as returns
    int n = vint_lua_num_args(lua_state, 1);
    vint_data_item* item = vint_data_item::find(vint_lua_to_handle(lua_state, -n));
    if (item == nullptr)
    {
        return 0;
    }
    vint_variant elements[32];
    item->get(elements, 32);
    int returns = 0;
    for (int i = 0; i < item->get_num_elements(); ++i)
    {
        returns += lua_add_return<vint_variant>(elements[i], lua_state);
    }
    return returns;
}

// 0x0047B0A0 (:1102)
static int vint_internal_dataresponder_request(lua_State* lua_state)
{
    // (responder name, Lua function, max records, parameters...): the parameters as booleans, floats or strings
    int n = vint_lua_num_args(lua_state, 19);
    vint_variant parameters[16];
    const char* name = lua_tostring(lua_state, -n);
    vint_data_responder* responder = name != nullptr ? vint_data_responder::find(name) : nullptr;
    if (responder == nullptr)
    {
        keen::debug::nativePrintArgs("vint_dataresponder_request : Please specify a valid data responder name");
        return 0;
    }
    if (n - 1 < 1 || lua_type(lua_state, -(n - 1)) != LUA_TSTRING)
    {
        keen::debug::nativePrintArgs("vint_dataresponder_request : Please specify a valid lua callback name");
        return 0;
    }
    const char* func_name = lua_tostring(lua_state, -(n - 1));
    if (n - 2 < 1 || lua_type(lua_state, -(n - 2)) != LUA_TNUMBER)
    {
        keen::debug::nativePrintArgs("vint_dataresponder_request : Please specify a number of max records to return");
        return 0;
    }
    int max_records = (int)(long long)lua_tonumber(lua_state, -(n - 2));
    int num_parameters = n - 3;
    // Original: more than 16 parameters overrun the array
    int pos = num_parameters;
    for (int i = 0; i < num_parameters; ++i)
    {
        vint_variant& p = parameters[i];
        int lua_t = pos < 1 ? LUA_TNIL : lua_type(lua_state, -pos);
        if (lua_t == LUA_TBOOLEAN)
        {
            p = lua_toboolean(lua_state, -pos) != 0;
            --pos;
        }
        else if (lua_t == LUA_TNUMBER)
        {
            p = (float)lua_tonumber(lua_state, -pos);
            --pos;
        }
        else if (lua_t == LUA_TSTRING)
        {
            p = lua_tostring(lua_state, -pos);
            --pos;
        }
        else
        {
            p.clear();
        }
    }
    responder->set_parent_context(lua_get_current_thread()->context);
    responder->retrieve_data(func_name, max_records, parameters, num_parameters);
    return 0;
}

// 0x0046A8E0 (:1157)
static int vint_dataresponder_finished(lua_State* lua_state)
{
    // (responder name): whether its submission is complete
    int n = vint_lua_num_args(lua_state, 1);
    const char* name = lua_tostring(lua_state, -n);
    vint_data_responder* responder = name != nullptr ? vint_data_responder::find(name) : nullptr;
    if (responder == nullptr)
    {
        return 0;
    }
    lua_pushboolean(lua_state, responder->is_submission_complete());
    return 1;
}

// 0x0046A960 (:1166)
static int vint_document_find(lua_State* lua_state)
{
    // (name, else the script's own document): the handle, or 0
    int n = vint_lua_num_args(lua_state, 1);
    vint_document* doc;
    if (n > 0 && lua_type(lua_state, -n) != LUA_TNIL && lua_tostring(lua_state, -n) != nullptr)
    {
        doc = vint_document::find(lua_tostring(lua_state, -n));
    }
    else
    {
        doc = get_doc_from_context();
    }
    return vint_lua_push_handle(lua_state, doc != nullptr ? doc->get_handle() : 0);
}

// 0x00482500 (:1184)
static int vint_document_load(lua_State* lua_state)
{
    int n = vint_lua_num_args(lua_state, 1);
    vint_document::load(lua_tostring(lua_state, -n), Vint_temp_mempool);
    return 0;
}

// 0x0046AA50 (:1190)
static int vint_dataevent_add_subscription(lua_State* lua_state)
{
    // (event handle or name, "update", Lua function); by name it waits for the event
    int n = vint_lua_num_args(lua_state, 3);
    int lua_t = n < 1 ? LUA_TNIL : lua_type(lua_state, -n);
    vint_data_event* event = nullptr;
    const char* event_object_name = nullptr;
    if (lua_t == LUA_TNUMBER)
    {
        unsigned int handle = vint_lua_to_handle(lua_state, -n);
        event = handle != 0 ? vint_data_event::find(handle) : nullptr;
    }
    else if (lua_t == LUA_TSTRING)
    {
        event_object_name = lua_tostring(lua_state, -n);
        event = vint_data_event::find(event_object_name);
    }
    else
    {
        return 0;
    }
    const char* event_name = lua_tostring(lua_state, -(n - 1));
    const char* func_name = lua_tostring(lua_state, -(n - 2));
    vint_document* doc = get_doc_from_context();
    if (doc == nullptr || event_name == nullptr || func_name == nullptr || v_stricmp(event_name, "update") != 0)
    {
        return 0;
    }
    vint_callback_lua* cb = vint_lua_callback_acquire(func_name);
    if (cb == nullptr)
    {
        return 0;
    }
    if (event != nullptr)
    {
        event->add_subscription(cb->get_handle(), VINT_EVENT_DATA_EVENT_UPDATE, doc->get_handle());
    }
    else if (event_object_name != nullptr)
    {
        vint_data_event::add_subscription(crc_stri(event_object_name, 0), VINT_EVENT_DATA_EVENT_UPDATE, cb->get_handle(), doc->get_handle());
    }
    cb->release();
    return 0;
}

// 0x0046AC20 (:1275)
static int vint_dataevent_find(lua_State* lua_state)
{
    int n = vint_lua_num_args(lua_state, 1);
    const char* name = lua_tostring(lua_state, -n);
    vint_data_event* event = vint_data_event::find(name);
    return vint_lua_push_handle(lua_state, event != nullptr ? event->get_handle() : 0);
}

// 0x0047B370 (:1291)
static int vint_dataevent_set(lua_State* lua_state)
{
    // (event handle, values...): as vint_dataitem_set, for the 8 elements
    int n = vint_lua_num_args(lua_state, 25);
    if (n < 1)
    {
        keen::debug::nativePrintArgs("Please specify a data event handle");
        return 0;
    }
    if (lua_type(lua_state, -n) != LUA_TNUMBER)
    {
        return 0;
    }
    vint_data_event* event = vint_data_event::find(vint_lua_to_handle(lua_state, -n));
    if (event == nullptr)
    {
        return 0;
    }
    int pos = n - 1;
    vint_variant elements[8];
    event->get(elements, 8);
    int num = event->get_num_elements();
    for (int i = 0; i < num; ++i)
    {
        elements[i] = lua_get_arg<vint_variant>(lua_state, pos, false, elements[i]);
    }
    for (int i = 0; i < num && i < 32; ++i)
    {
        event->set_element(i, elements[i]);
    }
    return 0;
}

// 0x0047BE00 (:1321)
static int vint_dataevent_get(lua_State* lua_state)
{
    int n = vint_lua_num_args(lua_state, 1);
    unsigned int handle = vint_lua_to_handle(lua_state, -n);
    vint_data_event* event = handle != 0 ? vint_data_event::find(handle) : nullptr;
    if (event == nullptr)
    {
        return 0;
    }
    vint_variant elements[8];
    event->get(elements, 8);
    int returns = 0;
    for (int i = 0; i < event->get_num_elements(); ++i)
    {
        returns += lua_add_return<vint_variant>(elements[i], lua_state);
    }
    return returns;
}

// The document a native names by handle (a number argument), else the script's own (a bad handle is null).
static vint_document* vint_lua_doc_arg(lua_State* _l, int _n)
{
    if (_n > 0 && lua_type(_l, -_n) != LUA_TNIL)
    {
        unsigned int handle = vint_lua_to_handle(_l, -_n);
        if (handle != 0)
        {
            return vint_document::find(handle);
        }
    }
    return get_doc_from_context();
}

// 0x0046ACC0 (:1341)
static int vint_document_unload(lua_State* lua_state)
{
    // (document handle, else the script's own): destroyed after this frame's processing
    int n = vint_lua_num_args(lua_state, 1);
    if (vint_document* doc = vint_lua_doc_arg(lua_state, n))
    {
        doc->kill_at_end_of_frame();
    }
    return 0;
}

// 0x0046AD70 (:1356)
static int vint_object_destroy(lua_State* lua_state)
{
    // (handle, unused)
    int n = vint_lua_num_args(lua_state, 2);
    unsigned int handle = vint_lua_to_handle(lua_state, -n);
    if (n - 1 >= 1 && lua_type(lua_state, -(n - 1)) != LUA_TNIL)
    {
        lua_tonumber(lua_state, -(n - 1));
    }
    vint_object_base* obj = vint_object_base::find_object(handle, nullptr);
    if (obj == nullptr)
    {
        return 0;
    }
    vint_document* doc = obj->get_parent_document();
    if (doc == nullptr)
    {
        return 0;
    }
    if (!obj->is_type(vint_element_base::Object_type) || doc->get_root_element() != nullptr)
    {
        obj->remove_from_parent();
    }
    obj->destroy();
    return 0;
}

// 0x0046AE10 (:1383)
static int vint_object_find(lua_State* lua_state)
{
    // (name, parent handle optional, document handle optional): the handle, or 0
    int n = vint_lua_num_args(lua_state, 3);
    const char* name = lua_tostring(lua_state, -n);
    int pos = n - 1;
    unsigned int parent_handle = 0;
    if (pos >= 1)
    {
        // A nil parent is parent 0 (0x0046AE55).
        if (lua_type(lua_state, -pos) != LUA_TNIL)
        {
            parent_handle = vint_lua_to_handle(lua_state, -pos);
        }
        pos = n - 2;
    }
    vint_document* doc;
    if (pos >= 1 && lua_type(lua_state, -pos) != LUA_TNIL && vint_lua_to_handle(lua_state, -pos) != 0)
    {
        doc = vint_document::find(vint_lua_to_handle(lua_state, -pos));
        if (doc == nullptr)
        {
            return vint_lua_push_handle(lua_state, 0);
        }
    }
    else
    {
        doc = get_doc_from_context();
    }
    if (doc == nullptr)
    {
        return vint_lua_push_handle(lua_state, 0);
    }
    vint_object_base* obj;
    if (parent_handle == 0)
    {
        if (name == nullptr)
        {
            return vint_lua_push_handle(lua_state, 0);
        }
        obj = vint_object_base::find_object_by_name_crc(crc_stri(name, 0), doc);
    }
    else
    {
        vint_object_base* parent = vint_object_base::find_object(parent_handle, nullptr);
        if (parent == nullptr)
        {
            return vint_lua_push_handle(lua_state, 0);
        }
        obj = parent->find_child(crc_stri(name, 0));
    }
    return vint_lua_push_handle(lua_state, obj != nullptr ? obj->get_handle() : 0);
}

// 0x0047BF60 (:1423)
static int vint_get_property(lua_State* lua_state)
{
    // (handle, property name): the value as returns (a font as its game title handle); nothing when not found
    int n = vint_lua_num_args(lua_state, 2);
    unsigned int handle = vint_lua_to_handle(lua_state, -n);
    const char* name = lua_tostring(lua_state, -(n - 1));
    if (handle == Vint_object_debug_target)
    {
        VLIB_CRITICAL_FAILURE();
    }
    if (name == nullptr || get_doc_from_context() == nullptr)
    {
        return 0;
    }
    vint_object_base* obj = vint_object_base::find_object(handle, nullptr);
    if (obj == nullptr)
    {
        return 0;
    }
    vint_variant value;
    bool found = obj->get_property_value(crc_stri(name, 0), value, 0);
    if (value.get_type() == VINT_PROP_TYPE_FONT)
    {
        vint_resource_font* font = (vint_resource_font*)value;
        unsigned int title = font != nullptr ? font->get_title_handle() : 0;
        value.clear();
        value = title;
    }
    return found ? lua_add_return<vint_variant>(value, lua_state) : 0;
}

// 0x0046AF80 (:1470)
static int vint_get_time_index(lua_State* lua_state)
{
    // (document handle, else the script's own)
    int n = vint_lua_num_args(lua_state, 1);
    vint_document* doc = vint_lua_doc_arg(lua_state, n);
    if (doc == nullptr)
    {
        return 0;
    }
    lua_pushnumber(lua_state, (double)doc->get_time_index());
    return 1;
}

// 0x0046B050 (:1488)
static int vint_set_time_index(lua_State* lua_state)
{
    // (time): the script's own document
    int n = vint_lua_num_args(lua_state, 1);
    double time_index = lua_tonumber(lua_state, -n);
    if (vint_document* doc = get_doc_from_context())
    {
        doc->set_time_index((float)time_index);
    }
    return 0;
}

// 0x00480090 (:1513)
static int vint_set_property(lua_State* lua_state)
{
    // (handle, property name, value(s), index optional): the value read in the property's type (a tween's start or
    // end value in its target property's type)
    int n = vint_lua_num_args(lua_state, 6);
    unsigned int handle = 0;
    if (n > 0 && lua_type(lua_state, -n) == LUA_TNUMBER)
    {
        handle = vint_lua_to_handle(lua_state, -n);
    }
    if (handle == Vint_object_debug_target)
    {
        VLIB_CRITICAL_FAILURE();
    }
    const char* name = lua_tostring(lua_state, -(n - 1));
    int pos = n - 2;
    vint_object_base* obj = vint_object_base::find_object(handle, nullptr);
    if (name == nullptr || obj == nullptr)
    {
        return 0;
    }
    const vint_prop_lookup* lookup = obj->get_property_lookup(name);
    if (lookup == nullptr)
    {
        return 0;
    }
    vint_variant_type type = lookup->type;
    if (type == VINT_PROP_TYPE_VARIABLE && obj->get_type() == vint_object_tween::Object_type && (v_stricmp(name, "end_value") == 0 || v_stricmp(name, "start_value") == 0))
    {
        vint_variant target_value;
        ((vint_object_tween*)obj)->get_target_handle(target_value, 0);
        vint_object_base* target = vint_object_base::find_object((unsigned int)target_value, nullptr);
        vint_variant property;
        obj->get_property_value("target_property", property, 0);
        const char* property_name = property.get_type() == VINT_PROP_TYPE_STRING ? (const char*)property : nullptr;
        if (target != nullptr && property_name != nullptr)
        {
            if (const vint_prop_lookup* target_lookup = target->get_property_lookup(property_name))
            {
                type = target_lookup->type;
            }
        }
    }
    vint_variant value;
    if ((int)type < VINT_PROP_TYPE_ENUM)
    {
        value.set_type(type);
    }
    // lua_get_arg<vint_variant> inlined in rfg.exe
    value = lua_get_arg<vint_variant>(lua_state, pos, false, vint_variant(value));
    int index = 0;
    if (pos > 0 && lua_type(lua_state, -pos) != LUA_TNIL)
    {
        index = (int)(long long)lua_tonumber(lua_state, -pos);
    }
    obj->set_property_value(name, value, index);
    return 0;
}

// 0x0047EFC0 (:1589)
static int vint_set_property_typed(lua_State* lua_state)
{
    // (type name, handle, property name, value(s)): the value read in the named type
    int n = vint_lua_num_args(lua_state, 6);
    const char* type_name = lua_tostring(lua_state, -n);
    vint_variant_type type = type_name != nullptr ? vint_variant::get_type_from_name(type_name) : VINT_PROP_TYPE_NONE;
    if (type == VINT_PROP_TYPE_NONE || n - 1 < 1 || lua_type(lua_state, -(n - 1)) != LUA_TNUMBER)
    {
        return 0;
    }
    unsigned int handle = vint_lua_to_handle(lua_state, -(n - 1));
    const char* name = lua_tostring(lua_state, -(n - 2));
    int pos = n - 3;
    vint_document* doc = name != nullptr ? get_doc_from_context() : nullptr;
    vint_object_base* obj = doc != nullptr ? vint_object_base::find_object(handle, doc) : nullptr;
    if (obj == nullptr)
    {
        return 0;
    }
    const vint_object_type_info* info = vint_object_types_get_type_info(obj->get_type());
    if (info == nullptr)
    {
        return 0;
    }
    vint_variant value;
    value.set_type(type);
    value = lua_get_arg<vint_variant>(lua_state, pos, false, vint_variant(value));
    info->prop_mgr->set_prop_value(obj, name, &value, 0);
    return 0;
}

// 0x0046B0C0 (:1828)
static int vint_get_avg_processing_time(lua_State* lua_state)
{
    // compiled out: no result
    lua_gettop(lua_state);
    return 0;
}

// An animation of the script's document (rfg.exe inlines the checks).
static vint_object_base* vint_lua_anim_arg(lua_State* _l, int _index)
{
    unsigned int handle = vint_lua_to_handle(_l, _index);
    vint_document* doc = get_doc_from_context();
    vint_object_base* anim = doc != nullptr ? vint_object_base::find_object(handle, doc) : nullptr;
    return anim != nullptr && anim->get_type() == vint_object_animation::Object_type ? anim : nullptr;
}

// 0x0047C0B0 (:1833)
static int vint_get_anim_duration(lua_State* lua_state)
{
    // (animation handle): the latest child tween end
    int n = vint_lua_num_args(lua_state, 1);
    vint_object_base* anim = vint_lua_anim_arg(lua_state, -n);
    // Original bug kept: the tween's duration is fetched into the float result read as a variant (its first word is
    // the variant's type: a 0.0 result becomes type FLOAT, bits 3), not into the variant added, so the end time is the
    // start time alone and a zero result comes out as the denormal 4.2e-45. The slot is variant-sized here so the
    // write past the float stays inside it.
    union
    {
        float duration;
        unsigned char bytes[sizeof(vint_variant)];
        uint64_t align;
    } result = {};
    if (anim != nullptr)
    {
        for (vint_object_base* child = anim->first_child; child != nullptr; child = child->next_sibling)
        {
            if (child->get_type() != vint_object_tween::Object_type)
            {
                continue;
            }
            vint_object_tween* tween = (vint_object_tween*)child;
            vint_variant start_time;
            vint_variant tween_duration;
            tween->get_start_time(start_time, 0);
            tween->get_duration(*(vint_variant*)result.bytes, 0);
            float end = (float)((double)(float)start_time + (double)(float)tween_duration);
            if (result.duration < end)
            {
                result.duration = end;
            }
        }
    }
    lua_pushnumber(lua_state, (double)result.duration);
    return 1;
}

// 0x0047C240 (:1872)
static int vint_set_child_tween_state(lua_State* lua_state)
{
    // (animation handle, state)
    int n = vint_lua_num_args(lua_state, 2);
    vint_object_base* anim = vint_lua_anim_arg(lua_state, -n);
    unsigned int state = vint_lua_to_handle(lua_state, -(n - 1));
    if (anim == nullptr || state >= NUM_VINT_TWEEN_STATES)
    {
        return 0;
    }
    for (vint_object_base* child = anim->first_child; child != nullptr; child = child->next_sibling)
    {
        if (child->get_type() == vint_object_tween::Object_type)
        {
            vint_variant value;
            value = (int)state;
            ((vint_object_tween*)child)->set_state(value, 0);
        }
    }
    return 0;
}

// 0x0047C340 (:1907)
static int vint_set_child_tween_reverse(lua_State* lua_state)
{
    // (animation handle, reverse)
    int n = vint_lua_num_args(lua_state, 2);
    vint_object_base* anim = vint_lua_anim_arg(lua_state, -n);
    bool reverse = lua_toboolean(lua_state, -(n - 1)) != 0;
    if (anim == nullptr)
    {
        return 0;
    }
    for (vint_object_base* child = anim->first_child; child != nullptr; child = child->next_sibling)
    {
        if (child->get_type() == vint_object_tween::Object_type)
        {
            vint_variant value;
            value = reverse;
            ((vint_object_tween*)child)->set_reverse(value, 0);
        }
    }
    return 0;
}

// 0x00475810 (:1935)
static int vint_reset_child_tween_object(lua_State* lua_state)
{
    // (animation handle): every child tween's target back to its start value
    int n = vint_lua_num_args(lua_state, 1);
    vint_object_base* anim = vint_lua_anim_arg(lua_state, -n);
    if (anim == nullptr)
    {
        return 0;
    }
    for (vint_object_base* child = anim->first_child; child != nullptr; child = child->next_sibling)
    {
        if (child->get_type() == vint_object_tween::Object_type)
        {
            ((vint_object_tween*)child)->reset_tween_object();
        }
    }
    return 0;
}

// 0x0046B0D0 (:1959)
static int vint_get_doc_name(lua_State* lua_state)
{
    lua_gettop(lua_state);
    if (vint_document* doc = get_doc_from_context())
    {
        lua_pushstring(lua_state, doc->get_name());
        return 1;
    }
    lua_pushstring(lua_state, "null");
    return 1;
}

// 0x0046B130 (:1970)
static int vint_hack_is_std_res(lua_State* lua_state)
{
    // compiled out: no result
    lua_gettop(lua_state);
    return 0;
}

// 0x0047C440 (:1976)
static int vint_get_global_anchor(lua_State* lua_state)
{
    // (element handle): its anchor in screen space (x, y); nothing for animations and tweens
    int n = vint_lua_num_args(lua_state, 1);
    unsigned int handle = vint_lua_to_handle(lua_state, -n);
    vint_document* doc = get_doc_from_context();
    vint_object_base* obj = doc != nullptr ? vint_object_base::find_object(handle, doc) : nullptr;
    if (obj == nullptr || obj->get_type() == vint_object_animation::Object_type || obj->get_type() == vint_object_tween::Object_type)
    {
        return 0;
    }
    // rfg.exe calls the protected three-argument form (the transforms unused); the public one wraps it
    vector2 anchor;
    ((vint_element_base*)obj)->get_global_anchor(anchor);
    lua_pushnumber(lua_state, (double)anchor.x);
    lua_pushnumber(lua_state, (double)anchor.y);
    return 2;
}

// 0x0046B140 (:2005)
static int vint_mod(lua_State* lua_state)
{
    // (a, b): a mod b as integers (0 for b = 0)
    int n = vint_lua_num_args(lua_state, 2);
    int a = (int)(long long)lua_tonumber(lua_state, -n);
    int b = (int)(long long)lua_tonumber(lua_state, -(n - 1));
    lua_pushnumber(lua_state, b == 0 ? 0.0 : (double)(a % b));
    return 1;
}

// ---- Lua state

// 0x004620F0 (:2025)
lua_State* vint_lua_get_state()
{
    return Vint_lua_state;
}

// 0x00482540 (:2030)
void vint_lua_actions_register_custom_functions(lua_State* lua_state)
{
    static const struct
    {
        const char* name;
        lua_CFunction func;
    } natives[38] = {
        {"vint_debug_sleep", vint_debug_sleep},
        {"vint_datagroup_add_subscription", vint_datagroup_add_subscription},
        {"vint_datagroup_insert_item", vint_datagroup_insert_item},
        {"vint_datagroup_remove_item", vint_datagroup_remove_item},
        {"vint_dataitem_add_subscription", vint_dataitem_add_subscription},
        {"vint_dataitem_find", vint_dataitem_find},
        {"vint_dataitem_set", vint_dataitem_set},
        {"vint_dataitem_get", vint_dataitem_get},
        {"vint_dataevent_add_subscription", vint_dataevent_add_subscription},
        {"vint_dataevent_find", vint_dataevent_find},
        {"vint_dataevent_set", vint_dataevent_set},
        {"vint_dataevent_get", vint_dataevent_get},
        {"vint_document_find", vint_document_find},
        {"vint_document_load", vint_document_load},
        {"vint_document_unload", vint_document_unload},
        {"vint_get_property", vint_get_property},
        {"vint_get_time_index", vint_get_time_index},
        {"vint_internal_dataresponder_request", vint_internal_dataresponder_request},
        {"vint_dataresponder_finished", vint_dataresponder_finished},
        {"vint_object_add_child", vint_object_add_child},
        {"vint_object_clone", vint_object_clone},
        {"vint_object_clone_rename", vint_object_clone_rename},
        {"vint_anim_clone_and_retarget", vint_anim_clone_and_retarget},
        {"vint_object_create", vint_object_create},
        {"vint_object_destroy", vint_object_destroy},
        {"vint_object_find", vint_object_find},
        {"vint_set_property", vint_set_property},
        {"vint_set_property_typed", vint_set_property_typed},
        {"vint_set_time_index", vint_set_time_index},
        {"vint_hack_is_std_res", vint_hack_is_std_res},
        {"vint_get_global_anchor", vint_get_global_anchor},
        {"vint_mod", vint_mod},
        {"vint_get_avg_processing_time", vint_get_avg_processing_time},
        {"vint_get_anim_duration", vint_get_anim_duration},
        {"vint_set_child_tween_state", vint_set_child_tween_state},
        {"vint_set_child_tween_reverse", vint_set_child_tween_reverse},
        {"vint_reset_child_tween_object", vint_reset_child_tween_object},
        {"vint_get_doc_name", vint_get_doc_name},
    };
    // as globals
    for (int i = 0; i < 38; ++i)
    {
        lua_pushstring(lua_state, natives[i].name);
        lua_pushcclosure(lua_state, natives[i].func, 0);
        lua_settable(lua_state, LUA_GLOBALSINDEX);
    }
}

// 0x00472CE0 (:2090)
void vint_lua_document_init(vint_document* _doc, mempool_base* _temp_pool)
{
    // the document's script files, then "<doc>_init" in its context
    if (_temp_pool == nullptr || _doc == nullptr)
    {
        return;
    }
    int index = 0;
    for (const char* filename = _doc->find_metadata("lua_script_file", &index); filename != nullptr; filename = _doc->find_metadata("lua_script_file", &index))
    {
        lua_load_dynamic_script(filename, Vint_lua_state, _temp_pool);
        ++index;
    }
    char func_name[128];
    _snprintf_safe(func_name, sizeof(func_name), "%s_init", _doc->get_name());
    lua_thread* thread = lua_new_thread(func_name, Vint_lua_state, false, false);
    if (thread != nullptr)
    {
        thread->context = _doc->get_handle();
        lua_execute_thread_immediate(thread);
    }
}

// 0x00472DC0 (:2123)
void vint_lua_document_init(vint_document* _doc, unsigned char* data_buffer, unsigned int data_size, mempool_base* _temp_pool)
{
    // the script from memory, under the first "lua_script_file" name
    if (_doc == nullptr || data_buffer == nullptr || data_size == 0)
    {
        return;
    }
    int index = 0;
    const char* filename = _doc->find_metadata("lua_script_file", &index);
    // Original: a "<doc>.lua" name is formatted and not used
    char lua_name[128];
    _snprintf_safe(lua_name, sizeof(lua_name), "%s.lua", _doc->get_name());
    lua_load_dynamic_script(filename, data_buffer, data_size, Vint_lua_state, _temp_pool);
    char func_name[128];
    _snprintf_safe(func_name, sizeof(func_name), "%s_init", _doc->get_name());
    lua_thread* thread = lua_new_thread(func_name, Vint_lua_state, false, false);
    if (thread != nullptr)
    {
        thread->context = _doc->get_handle();
        lua_execute_thread_immediate(thread);
    }
}

// 0x00472EF0 (:2145)
void vint_lua_document_destroy(vint_document* _doc)
{
    // "<doc>_cleanup", then the document's threads stop and its scripts unload
    if (_doc == nullptr)
    {
        return;
    }
    char func_name[64];
    _snprintf_safe(func_name, sizeof(func_name), "%s_cleanup", _doc->get_name());
    lua_thread* thread = lua_new_thread(func_name, Vint_lua_state, false, false);
    if (thread != nullptr)
    {
        thread->context = _doc->get_handle();
        lua_execute_thread_immediate(thread);
    }
    lua_clear_threads_in_context(_doc->get_handle());
    int index = 0;
    for (const char* filename = _doc->find_metadata("lua_script_file", &index); filename != nullptr; filename = _doc->find_metadata("lua_script_file", &index))
    {
        lua_unload_dynamic_script(filename, Vint_lua_state);
        ++index;
    }
}

// 0x004828A0 (:2174)
void vint_lua_init(void (*_register_funcs_callback)(lua_State*), mempool_base* _temp_pool)
{
    // the state, the natives (vint's and the game's), vint_lib.lua
    if (_temp_pool == nullptr)
    {
        return;
    }
    Vint_lua_state = lua_create_state(false, _temp_pool, nullptr);
    if (Vint_lua_state == nullptr)
    {
        keen::debug::nativePrintArgs("Unable to create lua state object");
    }
    vint_lua_actions_register_custom_functions(Vint_lua_state);
    vint_callback_lua::system_init();
    if (_register_funcs_callback != nullptr)
    {
        _register_funcs_callback(Vint_lua_state);
    }
    if (!lua_load_script("vint_lib.lua", Vint_lua_state, _temp_pool))
    {
        keen::debug::nativePrintArgs("Unable to open custom lua library");
    }
    lua_disable_nil_assignments(Vint_lua_state);
    // the data responders (vint_data_responder::system_init inlined)
    vint_data_responder::system_init();
}

// 0x00462100 (:2205)
void vint_lua_garbage_collect()
{
    lua_do_garbage_collection(Vint_lua_state);
}
