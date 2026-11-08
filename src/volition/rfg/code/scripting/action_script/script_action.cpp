// Original: volition/rfg/code/scripting/action_script/script_action.cpp
//
// Script actions: a callback with its parameter nodes and a delay. Each script keeps its actions in the shared pool
// (categories: 0 waiting to run, 1 done and kept for the script's next repeat).

#include "volition/rfg/code/scripting/action_script/script_action.h"

#include "keen/base/trace.hpp"
#include "volition/rfg/code/gamewide/game_trace.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/rfg/code/scripting/action_script/action_script_internal.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/util/dictionary.h"
#include "volition/vlib/code/util/fastxml.h"

#include <string.h>

object_pool<script_action,6144,1> Script_actions(&script_action::init); // 0x036D1860

// 0x00B37D30 (:33)
bool script_action::save(cfile* file)
{
    return cf_write_string(dictionary<script_action_definition>::find(definition), file) && cf_write_int32(delay, file) && cf_write_int32(variable_idx, file) &&
           script_params_save(file, params);
}

// 0x00B37D90 (:61)
void script_action::load(cfile* file)
{
    char name[1024];
    cf_read_string(name, sizeof(name), file);
    definition = dictionary<script_action_definition>::find(name);
    delay = cf_read_int32(file);
    variable_idx = cf_read_int32(file);
    script_params_load(params, file);
}

// 0x00B1FAD0 (:79)
void script_action::init()
{
    definition = nullptr;
    delay = 0;
    params = nullptr;
    variable_idx = -1;
}

// 0x00B3CC80 (:96)
script_action* script_action_create(xml_element* elt, int time_delay, const char* variable_name, object_pool_client<script_action,2>& action_client_pool, action_script* script)
{
    script_action_definition* definition = dictionary<script_action_definition>::find(elt->text);
    if (definition == nullptr)
    {
        GAME_TRACE("script", "unknown action %s: its script will not run", elt->text);
        keen::debug::nativePrintArgs("Script action not found in dictionary");
        return nullptr;
    }
    script_action* action = action_client_pool.alloc(0);
    if (action == nullptr)
    {
        return nullptr;
    }
    action->definition = definition;
    action->delay = time_delay;
    action->variable_idx = variable_name == nullptr ? -1 : get_variable_index(variable_name);
    int count = 0;
    for (xml_element* child = elt->elements; child != nullptr; child = child->next)
    {
        count++;
    }
    const unsigned int size = count * 4 + 4;
    script_node** dest = (script_node**)Script_mempool.alloc(size, 4);
    memset(dest, 0, size);
    action->params = dest;
    if (script_arg_list_parse(dest, definition->args, elt, script))
    {
        return action;
    }
    action_client_pool.free(action);
    return nullptr;
}

// 0x00B37E10 (:147)
void script_action_free(script_action* action, object_pool_client<script_action,2>& client_pool)
{
    for (int i = 0; action->params[i] != nullptr; i++)
    {
        action->params[i]->release();
        action->params[i] = nullptr;
    }
    action->params = nullptr;
    client_pool.free(action);
}

// 0x00B267C0 (:168)
void script_action_init_pool(object_pool_client<script_action,2>& client_pool)
{
    client_pool.init(&Script_actions);
}

// 0x00B37EE0 (:175)
bool script_action_run(script_action* action, int time_since, object_pool_client<script_action,2>& client_pool, bool release_after_execution)
{
    if (time_since < action->delay)
    {
        return false;
    }
    Action_return_value.clear();
    action->definition->callback(action->params);
    if (action->variable_idx != -1)
    {
        Script_variable_values[action->variable_idx].set((unsigned int)Action_return_value);
    }
    if (release_after_execution)
    {
        script_action_free(action, client_pool);
        return true;
    }
    client_pool.move(action, 1);
    return true;
}

// 0x00B3CD50 (:217)
void script_action_clear_pool()
{
    Script_actions.clear();
}
