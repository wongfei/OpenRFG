// Original: volition/rfg/code/scripting/action_script/script_action.h
// Implementation: volition/rfg/code/scripting/action_script/script_action.cpp
//
// Script actions: the actions a script runs (from a pool per script), their definitions (name, arguments, callback) and
// categories. Placement of the category enum inferred.
#pragma once

#include "volition/rfg/code/scripting/action_script/script_types.h"
#include "volition/vlib/code/memory/object_pool.h"

class action_script;
struct cfile;
struct xml_element;

enum SCRIPT_ACTION_CATEGORY
{
    SAC_RUNNING = 0,
    SAC_COMPLETED = 1,
    NUM_SCRIPT_ACTION__CATEGORIES = 2,
};

// size 0x14
struct script_action_definition
{
    void (*callback)(script_node**); // 0x00
    script_arg_list* args; // 0x04
    const char* desc; // 0x08
    const char* group; // 0x0C
    script_node_type return_type; // 0x10
};

// size 0x10
struct script_action
{
    bool save(cfile* file); // 0x00B37D30 (:33)
    void load(cfile* file); // 0x00B37D90 (:61)
    void init(); // 0x00B1FAD0 (:79)

    const script_action_definition* definition; // 0x00
    int delay; // 0x04
    int variable_idx; // 0x08
    script_node** params; // 0x0C
};

script_action* script_action_create(xml_element* elt, int time_delay, const char* variable_name, object_pool_client<script_action,2>& action_client_pool, action_script* script); // 0x00B3CC80 (:96)
void script_action_free(script_action* action, object_pool_client<script_action,2>& client_pool); // 0x00B37E10 (:147)
void script_action_init_pool(object_pool_client<script_action,2>& client_pool); // 0x00B267C0 (:168)
bool script_action_run(script_action* action, int time_since, object_pool_client<script_action,2>& client_pool, bool release_after_execution); // 0x00B37EE0 (:175)
void script_action_clear_pool(); // 0x00B3CD50 (:217)
