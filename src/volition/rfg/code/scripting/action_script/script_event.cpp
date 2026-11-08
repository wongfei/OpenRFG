// Original: volition/rfg/code/scripting/action_script/script_event.cpp
//
// Script events: a boolean node that turns true when the global event it registered for is posted and its definition's
// callback accepts it.

#include "volition/rfg/code/scripting/action_script/script_event.h"

#include "keen/base/trace.hpp"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/rfg/code/scripting/action_script/action_script_internal.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/util/dictionary.h"
#include "volition/vlib/code/util/fastxml.h"

#include <string.h>

object_pool<script_event,4096,1> Script_events(&script_event::init); // 0x0366F3B0

// 0x00B25440
script_event::script_event()
{
}

// 0x00B25550
script_event::~script_event()
{
}

// 0x00B25450 (:60)
script_event::operator bool()
{
    return state != 0;
}

// 0x00B25460 (:64)
script_event::operator float()
{
    return (float)state;
}

// 0x00B387C0 (:90)
void script_event::release()
{
    gem_unregister_for_event(registration_handle);
    registration_handle = nullptr;
    for (int i = 0; params[i] != nullptr; i++)
    {
        params[i]->release();
        params[i] = nullptr;
    }
    params = nullptr;
    Script_events.free(this);
}

// 0x00B38830 (:114)
void script_event::reset()
{
    for (int i = 0; params[i] != nullptr; i++)
    {
        params[i]->reset();
    }
    state = 0;
    gem_unregister_for_event(registration_handle);
    notify_info info;
    info.type = definition->type;
    info.callback = script_event_evaluate;
    info.data = this;
    registration_handle = gem_register_for_event(info);
}

// 0x00B38940 (:144)
bool script_event::save(cfile* file)
{
    return cf_write_uint32(class_type, file) && cf_write_uint32(type, file) && cf_write_string(dictionary<script_event_definition>::find(definition), file) &&
           script_params_save(file, params) && cf_write_int32(state, file);
}

// 0x00B389B0 (:174)
void script_event::load(cfile* file)
{
    class_type = (script_node_classes)cf_read_uint32(file);
    type = (script_node_type)cf_read_uint32(file);
    char name[1024];
    cf_read_string(name, sizeof(name), file);
    definition = dictionary<script_event_definition>::find(name);
    notify_info info;
    info.type = definition->type;
    info.callback = script_event_evaluate;
    info.data = this;
    registration_handle = gem_register_for_event(info);
    script_params_load(params, file);
    script = Cur_script_loading;
    state = cf_read_int32(file);
}

// 0x00B2D0F0 (:204)
void script_event::init()
{
    type = SN_VOID;
    class_type = SNC_EVENT;
    definition = nullptr;
    params = nullptr;
    script = nullptr;
    state = 0;
    registration_handle = nullptr;
}

// 0x00B3D5E0 (:219)
void script_event_clear_pool()
{
    Script_events.clear();
}

// 0x00B329D0 (:234)
script_event* script_event_alloc()
{
    return Script_events.alloc(0);
}

// 0x00B38A60 (:244)
script_event* script_event_create(xml_element* elt, action_script* script)
{
    script_event_definition* definition = dictionary<script_event_definition>::find(elt->text);
    if (definition == nullptr)
    {
        keen::debug::nativePrintArgs("Script event not found in dictionary");
    }
    script_event* event = Script_events.alloc(0);
    if (event == nullptr)
    {
        return nullptr;
    }
    event->script = script;
    event->definition = definition;
    event->class_type = SNC_EVENT;
    event->type = SN_BOOL;
    event->state = 0;
    int count = 0;
    for (xml_element* child = elt->elements; child != nullptr; child = child->next)
    {
        count++;
    }
    const unsigned int size = count * 4 + 4;
    script_node** dest = (script_node**)Script_mempool.alloc(size, 4);
    memset(dest, 0, size);
    event->params = dest;
    if (script_arg_list_parse(dest, definition->args, elt, script))
    {
        notify_info info;
        info.type = definition->type;
        info.callback = script_event_evaluate;
        info.data = event;
        event->registration_handle = gem_register_for_event(info);
        return event;
    }
    Script_events.free(event);
    return nullptr;
}

// 0x00B329E0 (:302)
event_result script_event_evaluate(const event_data& edata, notify_info* info)
{
    script_event* event = (script_event*)info->data;
    if (event == nullptr || !script_is_active(event->script))
    {
        return EVENT_IGNORED;
    }
    const event_result result = event->definition->callback(&edata, event->params);
    if (result == EVENT_HANDLED || result == EVENT_OK)
    {
        event->state++;
    }
    if (result == EVENT_HANDLED)
    {
        event->registration_handle = nullptr;
    }
    else if (result != EVENT_OK)
    {
        return result;
    }
    script_event_evaluate_repeatable_condition(event);
    return result;
}

// 0x00B22E20 (:336)
void script_event_evaluate_repeatable_condition(script_event* s_event)
{
    script_evaluate_repeatable_condition(s_event->script);
}
