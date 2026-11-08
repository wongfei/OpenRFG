// Original: volition/rfg/code/scripting/action_script/script_event.h
// Implementation: volition/rfg/code/scripting/action_script/script_event.cpp
//
// Script events: events a script waits for (a script node), evaluated when a global event is posted; their definitions
// map event names to event types.
#pragma once

#include "volition/rfg/code/scripting/action_script/global_event.h"
#include "volition/rfg/code/scripting/action_script/script_types.h"

class action_script;
struct cfile;
struct xml_element;

// size 0x10
struct script_event_definition
{
    event_result (*callback)(const event_data*, script_node**); // 0x00
    event_type type; // 0x04
    script_arg_list* args; // 0x08
    const char* desc; // 0x0C
};

// size 0x20
struct script_event : public script_node
{
    virtual void release(); // vtbl+0x04 0x00B387C0 (:90)
    virtual void reset(); // vtbl+0x08 0x00B38830 (:114)
    virtual bool save(cfile* file); // vtbl+0x0C 0x00B38940 (:144)
    virtual void load(cfile* file); // vtbl+0x10 0x00B389B0 (:174)
    virtual void init(); // vtbl+0x14 0x00B2D0F0 (:204)
    virtual operator bool(); // vtbl+0x1C 0x00B25450 (:60)
    virtual operator float(); // vtbl+0x20 0x00B25460 (:64)
    script_event(); // 0x00B25440
    virtual ~script_event(); // vtbl+0x00 0x00B25550

    const script_event_definition* definition; // 0x0C
    script_node** params; // 0x10
    action_script* script; // 0x14
    int state; // 0x18
    notify_info* registration_handle; // 0x1C
};

void script_event_clear_pool(); // 0x00B3D5E0 (:219)
script_event* script_event_alloc(); // 0x00B329D0 (:234)
script_event* script_event_create(xml_element* elt, action_script* script); // 0x00B38A60 (:244)
event_result script_event_evaluate(const event_data& edata, notify_info* info); // 0x00B329E0 (:302)
void script_event_evaluate_repeatable_condition(script_event* s_event); // 0x00B22E20 (:336)
