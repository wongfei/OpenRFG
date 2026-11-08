// Original: volition/rfg/code/scripting/action_script/script_function.h
// Implementation: volition/rfg/code/scripting/action_script/script_function.cpp
//
// script_function, a call node: its definition (name, parameter types, callback), parameter nodes and result.
#pragma once

#include "volition/rfg/code/scripting/action_script/script_types.h"

class action_script;
struct cfile;
struct xml_element;

// size 0x14
struct script_function_definition
{
    void (*callback)(script_variant& dest_result, script_variant& custom_value, script_node** args); // 0x00
    script_node_type result; // 0x04
    script_arg_list* args; // 0x08
    const char* desc; // 0x0C
    const char* group; // 0x10
};

// size 0x24
struct script_function : public script_node
{
    virtual void init(); // vtbl+0x14 0x00B2D120 (:84)
    virtual void release(); // vtbl+0x04 0x00B38B50 (:102)
    virtual void reset(); // vtbl+0x08 0x00B22E70 (:121)
    virtual bool save(cfile* file); // vtbl+0x0C 0x00B38C30 (:138)
    virtual void load(cfile* file); // vtbl+0x10 0x00B38CB0 (:169)
    virtual operator const char*(); // vtbl+0x18
    virtual operator bool(); // vtbl+0x1C 0x00B254A0 (:58)
    virtual operator float(); // vtbl+0x20 0x00B254E0 (:59)
    virtual operator int(); // vtbl+0x24 0x00B25500 (:60)
    virtual operator unsigned int(); // vtbl+0x28 0x00B25520 (:61)
    script_function(); // 0x00B25470
    virtual ~script_function(); // vtbl+0x00 0x00B25560

    const script_function_definition* definition; // 0x0C
    script_node** params; // 0x10
    script_variant value; // 0x14
    script_variant custom_value; // 0x1C
};

script_function* script_function_alloc(); // 0x00B32AA0 (:198)
script_function* script_function_create(xml_element* elt, action_script* script); // 0x00B38D40 (:207)
void script_function_clear_pool(); // 0x00B3D620 (:254)
