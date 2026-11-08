// Original: volition/rfg/code/scripting/action_script/script_function.cpp
//
// script_function, a call node: every read of its value calls the definition's callback.

#include "volition/rfg/code/scripting/action_script/script_function.h"

#include "keen/base/trace.hpp"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/rfg/code/scripting/action_script/action_script_internal.h"
#include "volition/rfg/code/scripting/action_script/script_definitions.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/util/dictionary.h"
#include "volition/vlib/code/util/fastxml.h"

#include <string.h>

object_pool<script_function,7168,1> Script_functions(&script_function::init); // 0x036F71C0

// 0x00B25470
script_function::script_function()
{
}

// 0x00B25560
script_function::~script_function()
{
}

// 0x00B25480 (:57)
script_function::operator const char*()
{
    definition->callback(value, custom_value, params);
    return value.string;
}

// 0x00B254A0 (:58)
script_function::operator bool()
{
    definition->callback(value, custom_value, params);
    return value.number != 0.0f;
}

// 0x00B254E0 (:59)
script_function::operator float()
{
    definition->callback(value, custom_value, params);
    return value.number;
}

// 0x00B25500 (:60)
script_function::operator int()
{
    definition->callback(value, custom_value, params);
    return (int)value.number;
}

// 0x00B25520 (:61)
script_function::operator unsigned int()
{
    definition->callback(value, custom_value, params);
    return value.obj;
}

// 0x00B2D120 (:84)
void script_function::init()
{
    type = SN_VOID;
    class_type = SNC_FUNCTION;
    definition = nullptr;
    params = nullptr;
    memset(&value, 0, sizeof(value));
    memset(&custom_value, 0, sizeof(custom_value));
}

// 0x00B38B50 (:102)
void script_function::release()
{
    for (int i = 0; params[i] != nullptr; i++)
    {
        params[i]->release();
    }
    params = nullptr;
    Script_functions.free(this);
}

// 0x00B22E70 (:121)
void script_function::reset()
{
    for (int i = 0; params[i] != nullptr; i++)
    {
        params[i]->reset();
    }
    memset(&custom_value, 0, sizeof(custom_value));
}

// 0x00B38C30 (:138)
bool script_function::save(cfile* file)
{
    return cf_write_uint32(class_type, file) && cf_write_uint32(type, file) && cf_write_string(dictionary<script_function_definition>::find(definition), file) &&
           script_params_save(file, params) && script_variant_save(file, custom_value, type);
}

// 0x00B38CB0 (:169)
void script_function::load(cfile* file)
{
    class_type = (script_node_classes)cf_read_uint32(file);
    type = (script_node_type)cf_read_uint32(file);
    char name[1024];
    cf_read_string(name, sizeof(name), file);
    definition = dictionary<script_function_definition>::find(name);
    script_params_load(params, file);
    script_variant_load(file, custom_value, type);
}

// 0x00B32AA0 (:198)
script_function* script_function_alloc()
{
    return Script_functions.alloc(0);
}

// 0x00B38D40 (:207)
script_function* script_function_create(xml_element* elt, action_script* script)
{
    script_function_definition* definition = dictionary<script_function_definition>::find(elt->text);
    if (definition == nullptr)
    {
        keen::debug::nativePrintArgs("Script function not found in dictionary");
    }
    script_function* function = Script_functions.alloc(0);
    if (function == nullptr)
    {
        return nullptr;
    }
    function->definition = definition;
    function->type = definition->result;
    int count = 0;
    for (xml_element* child = elt->elements; child != nullptr; child = child->next)
    {
        count++;
    }
    const unsigned int size = count * 4 + 4;
    script_node** dest = (script_node**)Script_mempool.alloc(size, 4);
    memset(dest, 0, size);
    function->params = dest;
    if (script_arg_list_parse(dest, definition->args, elt, script))
    {
        return function;
    }
    Script_functions.free(function);
    return nullptr;
}

// 0x00B3D620 (:254)
void script_function_clear_pool()
{
    Script_functions.clear();
}
