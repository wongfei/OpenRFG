// Original: volition/rfg/code/scripting/action_script/action_script.cpp
//
// The action script runtime. Pool categories of a script: 0 waiting for its condition, 1 running its delayed actions,
// 2 the stored copy of a managed group's script (activating a group copies them into category 0 by saving and loading
// them), 3 a script that failed to parse (rfg.exe never frees those).

#include "volition/rfg/code/scripting/action_script/action_script.h"

#include "volition/rfg/code/gamewide/game_trace.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/interface/ui_document_hud_missions.h"
#include "volition/rfg/code/level/world.h"
#include "keen/base/trace.hpp"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/rfg/code/scripting/action_script/action_script_internal.h"
#include "volition/rfg/code/scripting/action_script/script_definitions.h"
#include "volition/rfg/code/scripting/managed_script_group.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/util/dictionary.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/util/string_comp.h"

#include <string.h>

// Magic numbers: the pool's categories, see the top of the file.
static const int SCRIPT_WAITING = 0;
static const int SCRIPT_RUNNING = 1;
static const int SCRIPT_STORED = 2;
static const int SCRIPT_FAILED = 3;

static data_hash_table<unsigned int,managed_script_group_data,&hash_uint,32,unsigned char> Loaded_managed_script_groups(false); // 0x016B0D88
static action_script* Cur_script_running; // 0x03611244
static action_script* Cur_script_eval; // 0x0361127C
object_pool<action_script,2048,4> Action_scripts(&action_script::init); // 0x03698830
static str_data_hash_table_cached<int,128,unsigned char> Variable_table; // 0x036F3910

// The game trace (game_trace.h; not in rfg.exe). rfg.exe keeps no script names, so the trace keeps them by pool index:
// from the scriptx at parsing, passed on to a managed group's running copies. Scripts restored from a save have none
// (the save has no names): those show as ? with their group.
static const char* Script_trace_names[2048];

static const char* script_trace_name(const action_script* script)
{
    const char* name = Script_trace_names[Action_scripts.get_index(script)];
    return name != nullptr ? name : "?";
}

static const char* script_trace_group(const action_script* script)
{
    const char* name = script->group_handle != -1 ? managed_script_get_name(script->group_handle) : nullptr;
    return name != nullptr ? name : "-";
}

static int script_trace_args(char* dest, int size, script_node** params);

// Writes a parameter of an action. Only constants and variables are read: reading a function node would run it, which
// could change the game's random numbers or timers, so a function shows as its name and parameters.
static int script_trace_arg(char* dest, int size, script_node* node)
{
    if (node->class_type == SNC_VARIABLE)
    {
        const script_variable_value& value = Script_variable_values[((script_variable*)node)->idx];
        return snprintf(dest, size, "var%d=0x%X(%s)", ((script_variable*)node)->idx, value.handle, value.valid ? game_trace_object_name(value.handle) : "unset");
    }
    if (node->class_type == SNC_FUNCTION)
    {
        const script_function* function = (const script_function*)node;
        int used = snprintf(dest, size, "%s(", dictionary<script_function_definition>::find(function->definition));
        if (used < 0 || used >= size)
        {
            return size;
        }
        used += script_trace_args(dest + used, size - used, function->params);
        if (used < size)
        {
            used += snprintf(dest + used, size - used, ")");
        }
        return used;
    }
    if (node->class_type != SNC_ATOM)
    {
        return snprintf(dest, size, "?");
    }
    const script_variant& value = ((script_atom*)node)->value;
    switch (node->type)
    {
    case SN_NUMBER:
        return snprintf(dest, size, "%g", value.number);
    case SN_STRING:
        return snprintf(dest, size, "\"%s\"", value.string != nullptr ? value.string : "");
    case SN_BOOL:
        return snprintf(dest, size, "%s", (bool)*node ? "true" : "false");
    case SN_OBJECT:
        return snprintf(dest, size, "0x%X(%s)", value.obj, game_trace_object_name(value.obj));
    case SN_OBJECT_LIST:
        return snprintf(dest, size, "list");
    case SN_POINT:
        return snprintf(dest, size, "point(0x%X(%s),%d)", value.point.ref_handle, game_trace_object_name(value.point.ref_handle), value.point.index);
    default:
        return snprintf(dest, size, "%d", (int)value.handle);
    }
}

// Writes parameters, separated by commas (params: null terminated, or none).
static int script_trace_args(char* dest, int size, script_node** params)
{
    int used = 0;
    dest[0] = '\0';
    for (int i = 0; params != nullptr && params[i] != nullptr && used < size - 1; i++)
    {
        if (i > 0)
        {
            used += snprintf(dest + used, size - used, ", ");
        }
        const int written = used < size - 1 ? script_trace_arg(dest + used, size - used, params[i]) : -1;
        if (written < 0 || written >= size - used)
        {
            return size - 1;
        }
        used += written;
    }
    return used;
}

static void script_trace_action(const action_script* script, const script_action* action)
{
    char args[640];
    script_trace_args(args, sizeof(args), action->params);
    GAME_TRACE("action", "%s: %s(%s) delay=%d", script_trace_name(script), dictionary<script_action_definition>::find(action->definition), args, action->delay);
}

// 0x00B406A0 (:121)
void unused_dcf_load_script()
{
    if (Console_command)
    {
        console_get_arg(2, 0);
        char filename[1024];
        cf_add_ext(filename, sizeof(filename), Console_arg, ".scriptx", false);
        const int count = script_load(filename);
        if (count == 0)
        {
            console_printf("'%s' contains no scripts\n", filename);
        }
        else
        {
            console_printf("Successfully loaded %d scripts\n", count);
        }
    }
}

// 0x00B31380 (:149)
static bool script_group_loaded(int handle)
{
    return Loaded_managed_script_groups.exists((unsigned int)handle);
}

// 0x00B379F0 (:155) LTCG: handle in ESI, flags on the stack
static void script_group_add(int handle, unsigned char flags)
{
    if (!script_group_loaded(handle))
    {
        managed_script_group_data data;
        data.flags = flags;
        data.mark = Script_mempool.mark();
        Loaded_managed_script_groups.insert_key_data((unsigned int)handle, data);
    }
}

// 0x00B37A80 (:167)
static void script_group_remove(int handle)
{
    managed_script_group_data* data = Loaded_managed_script_groups.get_ptr((unsigned int)handle);
    if (data == nullptr)
    {
        return;
    }
    const unsigned int mark = data->mark;
    // The group loaded next above this one inherits its mark; with none above, the script memory is given back.
    managed_script_group_data* above = nullptr;
    for (managed_script_group_data* other = Loaded_managed_script_groups.get_first(); other != nullptr; other = Loaded_managed_script_groups.get_next(other))
    {
        // Original bug kept: the comparison with the best one so far uses <= mark, not the best's own mark.
        if (other != data && mark <= other->mark && (above == nullptr || above->mark <= mark))
        {
            above = other;
        }
    }
    if (above != nullptr)
    {
        above->mark = mark;
    }
    else
    {
        Script_mempool.restore_to_mark(mark);
    }
    Loaded_managed_script_groups.remove((unsigned int)handle);
}

// 0x00B313D0 (:205)
action_script* action_script_alloc(ACTION_SCRIPT_CATEGORY category)
{
    return Action_scripts.alloc(category);
}

// 0x00B3BA20 (:217)
static action_script* script_free_script(action_script* script)
{
    if (Cur_script_eval == script)
    {
        Cur_script_eval = Action_scripts.get_next(Cur_script_eval);
    }
    if (Cur_script_running == script)
    {
        Cur_script_running = Action_scripts.get_next(Cur_script_running);
    }
    script_action* next;
    for (script_action* action = script->action.get_first(SAC_RUNNING); action != nullptr; action = next)
    {
        next = script->action.get_next(action);
        script_action_free(action, script->action);
    }
    Script_trace_names[Action_scripts.get_index(script)] = nullptr;
    script->action.clear();
    script->condition->release();
    script->condition = nullptr;
    return Action_scripts.free(script);
}

// 0x00B3F820 (:248)
bool script_activate_managed_group(int handle, bool no_save)
{
    if (script_group_loaded(handle))
    {
        return false;
    }
    GAME_TRACE("script", "managed group %s starts", managed_script_get_name(handle));
    script_group_add(handle, no_save ? 1 : 0);
    // Magic number: 64 KB of temporary memory for the group's scripts as a saved stream.
    const unsigned int mark = Common_mempool.mark();
    unsigned char* buffer = (unsigned char*)Common_mempool.alloc(0x10000, 0);
    if (buffer == nullptr)
    {
        return false;
    }
    cfile* file = cf_open(buffer, 0x10000, "wb", PLATFORM_PC);
    if (file == nullptr)
    {
        return false;
    }
    int count = 0;
    for (action_script* script = Action_scripts.get_first(SCRIPT_STORED); script != nullptr; script = Action_scripts.get_next(script))
    {
        if (handle == script->group_handle)
        {
            script->save(file);
            count++;
        }
    }
    cf_close(file);
    file = cf_open(buffer, 0x10000, "rb", PLATFORM_PC);
    if (file == nullptr)
    {
        return false;
    }
    action_script* trace_stored = Action_scripts.get_first(SCRIPT_STORED);
    while (count > 0)
    {
        count--;
        action_script* script = Action_scripts.alloc(SCRIPT_WAITING);
        if (script == nullptr)
        {
            break;
        }
        script->load(file);
        // The copies load in the order they were saved, so the stored scripts of the group walk along for the names.
        while (trace_stored != nullptr && trace_stored->group_handle != handle)
        {
            trace_stored = Action_scripts.get_next(trace_stored);
        }
        if (trace_stored != nullptr)
        {
            Script_trace_names[Action_scripts.get_index(script)] = Script_trace_names[Action_scripts.get_index(trace_stored)];
            trace_stored = Action_scripts.get_next(trace_stored);
        }
    }
    cf_close(file);
    Common_mempool.restore_to_mark(mark);
    return true;
}

static void script_terminate_category(int category, int handle)
{
    action_script* next;
    for (action_script* script = Action_scripts.get_first(category); script != nullptr; script = next)
    {
        next = Action_scripts.get_next(script);
        if (script->group_handle == handle)
        {
            script_free_script(script);
        }
    }
}

// 0x00B3BB10 (:320)
void script_terminate_managed_group(int handle, bool remove_scripts)
{
    if (script_group_loaded(handle))
    {
        GAME_TRACE("script", "managed group %s stops", managed_script_get_name(handle));
        script_terminate_category(SCRIPT_WAITING, handle);
        script_terminate_category(SCRIPT_RUNNING, handle);
        if (remove_scripts)
        {
            script_terminate_category(SCRIPT_STORED, handle);
        }
        script_group_remove(handle);
    }
}

static bool script_actions_save(action_script* script, int category, cfile* file)
{
    if (!cf_write_uint32((unsigned int)script->action.get_count(category), file))
    {
        return false;
    }
    for (script_action* action = script->action.get_first(category); action != nullptr; action = script->action.get_next(action))
    {
        if (!action->save(file))
        {
            return false;
        }
    }
    return true;
}

// 0x00B3BD40 (:374)
bool action_script::save(cfile* file)
{
    if (!condition->save(file) || !cf_write_int32(num_actions_run, file))
    {
        return false;
    }
    if (!cf_write_int32(stamp.valid() ? stamp.time_since() : 0, file) || !cf_write_int32(num_ready, file) || !cf_write_int32(repeat, file))
    {
        return false;
    }
    const char* name = managed_script_get_name(group_handle);
    if (!cf_write_string(name != nullptr ? name : "", file))
    {
        return false;
    }
    return script_actions_save(this, SAC_RUNNING, file) && script_actions_save(this, SAC_COMPLETED, file);
}

// 0x00B3BEB0 (:444)
void action_script::load(cfile* file)
{
    Cur_script_loading = this;
    script_node_load(condition, file);
    num_actions_run = cf_read_int32(file);
    stamp.set(-cf_read_int32(file));
    num_ready = cf_read_int32(file);
    repeat = cf_read_int32(file);
    group_handle = -1;
    char group_name[128];
    if (cf_read_string(group_name, sizeof(group_name), file))
    {
        if (group_name[0] != '\0')
        {
            int found;
            group_handle = Managed_script_table->get(group_name, found) ? found : -1;
        }
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "action_script.cpp", 444);
    }
    action.init(&Script_actions);
    const unsigned int running = cf_read_uint32(file);
    for (unsigned int i = 0; i < running; i++)
    {
        script_action* entry = action.alloc(SAC_RUNNING);
        if (entry == nullptr)
        {
            break;
        }
        entry->load(file);
    }
    const unsigned int completed = cf_read_uint32(file);
    for (unsigned int i = 0; i < completed; i++)
    {
        script_action* entry = action.alloc(SAC_COMPLETED);
        if (entry == nullptr)
        {
            return;
        }
        // The same as script_action::load except that the parameter array is built here (inlined differently in rfg.exe).
        entry->load(file);
    }
}

// 0x00B3C110 (:518)
void action_script::init()
{
    condition = nullptr;
    num_actions_run = 0;
    num_ready = 0;
    repeat = 0;
    group_handle = -1;
    action.clear();
    stamp.invalidate();
}

// 0x00B3F7D0 (.h:28)
action_script::action_script()
{
    stamp.invalidate();
    condition = nullptr;
    num_actions_run = 0;
    num_ready = 0;
    repeat = 0;
    group_handle = -1;
    action.clear();
    stamp.invalidate();
}

// 0x00B1F5C0 (.h:65)
bool action_script::is_managed() const
{
    return group_handle != -1;
}

// 0x00B3FA30 (:541)
void script_system_init()
{
    managed_script_system_init();
    Cur_script_eval = nullptr;
    Cur_script_running = nullptr;
    script_timer_display_reset();
}

// 0x00B1FA10 (:551)
void script_system_level_init_pre()
{
    Cur_script_eval = nullptr;
    Cur_script_running = nullptr;
}

// 0x00B3FAA0 (:561)
void script_system_level_shutdown()
{
    for (int category = SCRIPT_WAITING; category <= SCRIPT_STORED; category++)
    {
        action_script* next;
        for (action_script* script = Action_scripts.get_first(category); script != nullptr; script = next)
        {
            next = Action_scripts.get_next(script);
            script_free_script(script);
        }
    }
    script_atoms_clear_pool();
    Action_scripts.clear();
    memset(Script_trace_names, 0, sizeof(Script_trace_names));
    script_event_clear_pool();
    script_function_clear_pool();
    script_action_clear_pool();
    script_variables_clear();
    script_timer_hide(script_timer_get_display_handle());
    Script_timers.clear();
    Loaded_managed_script_groups.clear();
    Script_mempool.clear();
}

// 0x00B3FEC0 (:615)
bool script_parse_actions(action_script* script, xml_element* root, int delay)
{
    for (xml_element* element = root->elements; element != nullptr; element = element->next)
    {
        if (v_stricmp(element->name, Script_node_tags[SN_ACTION]) == 0)
        {
            if (script_action_create(element, delay, nullptr, script->action, script) == nullptr)
            {
                return false;
            }
        }
        else if (v_stricmp(element->name, Script_node_tags[SN_VARIABLE]) == 0)
        {
            const char* variable = script_alloc_string(xml_get_string(element, nullptr));
            xml_element* action = xml_find(element, Script_node_tags[SN_ACTION]);
            if (action == nullptr || script_action_create(action, delay, variable, script->action, script) == nullptr)
            {
                return false;
            }
        }
    }
    return true;
}

// 0x00B3FF60 (:647)
bool script_parse_delayed_actions(action_script* script, xml_element* root, int parent_delay)
{
    for (xml_element* element = xml_find(root, Script_node_tags[SN_DELAY]); element != nullptr; element = xml_find_next(root, element, Script_node_tags[SN_DELAY]))
    {
        const float minimum = xml_get_float(xml_find(element, Script_node_tags[SN_MIN]), 0.0f);
        const float maximum = xml_get_float(xml_find(element, Script_node_tags[SN_MAX]), 0.0f);
        // Magic number: seconds to milliseconds.
        const int delay = int_rand_range((int)(minimum * 1000.0f), (int)(maximum * 1000.0f)) + parent_delay;
        if (!script_parse_actions(script, element, delay) || !script_parse_delayed_actions(script, element, delay))
        {
            return false;
        }
    }
    return true;
}

// 0x00B40150 (:680)
action_script* script_parse(xml_element* script, int group_handle)
{
    xml_element* condition = xml_find(script, Script_node_tags[SN_CONDITION]);
    if (condition == nullptr)
    {
        return nullptr;
    }
    action_script* parsed = Action_scripts.alloc(group_handle != -1 ? SCRIPT_STORED : SCRIPT_WAITING);
    if (parsed == nullptr)
    {
        keen::debug::nativePrintArgs("Too many action scripts executing at the same time!");
    }
    parsed->group_handle = group_handle;
    if (Game_trace_file != nullptr)
    {
        Script_trace_names[Action_scripts.get_index(parsed)] = script_alloc_string(script->text);
    }
    parsed->action.init(&Script_actions);
    parsed->condition = script_node_parse(condition->elements, parsed);
    if (parsed->condition == nullptr || parsed->condition->type != SN_BOOL)
    {
        keen::debug::nativePrintArgs("Script condition invalid or does not evaluate to a boolean!");
    }
    else
    {
        parsed->num_actions_run = 0;
        if (script_parse_actions(parsed, script, 0) && script_parse_delayed_actions(parsed, script, 0))
        {
            parsed->num_ready = 0;
            parsed->repeat = xml_get_int(xml_find(script, Script_node_tags[SN_REPEATABLE]), 0);
            return parsed;
        }
    }
    GAME_TRACE("script", "%s failed to parse: it will never run", script_trace_name(parsed));
    Action_scripts.move(parsed, SCRIPT_FAILED);
    return nullptr;
}

// 0x00B40230 (:739)
static int script_read(xml_element* elt, int script_group_handle)
{
    int count = 0;
    for (xml_element* element = elt->elements; element != nullptr; element = element->next)
    {
        if (xml_find(element, Script_node_tags[SN_DISABLED]) != nullptr)
        {
            continue;
        }
        if (v_stricmp(element->name, Script_node_tags[SN_SCRIPT]) == 0)
        {
            if (script_parse(element, script_group_handle) != nullptr)
            {
                count++;
            }
        }
        else if (v_stricmp(element->name, Script_node_tags[SN_REF]) == 0 || v_stricmp(element->name, Script_node_tags[SN_GROUP]) == 0)
        {
            count += script_read(element, script_group_handle);
        }
        else if (v_stricmp(element->name, Script_node_tags[SN_MANAGED]) == 0)
        {
            int handle = managed_script_get_handle(element->text);
            if (handle == -1)
            {
                handle = managed_script_create_handle(element->text);
            }
            count += script_read(element, handle);
        }
    }
    return count;
}

// 0x00B40310 (:780)
int script_load(const char* scriptx_file)
{
    mempool_mark_scope scope(&Common_mempool);
    xml_element* root = xml_parse(scriptx_file, &Common_mempool);
    if (root == nullptr)
    {
        return -1;
    }
    GAME_TRACE("script", "load %s", scriptx_file);
    Variable_table.clear();
    if (xml_find(root, "version") == nullptr)
    {
        return -1;
    }
    for (xml_element* variable = xml_find(root, "var"); variable != nullptr; variable = xml_find_next(root, variable, "var"))
    {
        const int index = Variable_idx++;
        if (!Variable_table.exists(variable->text))
        {
            Variable_table.insert_key_data(variable->text, index);
        }
    }
    return script_read(root, -1);
}

// 0x00B1FA20 (:834)
void script_reset(action_script* script)
{
    script->num_actions_run = 0;
}

// 0x00B1FA30 (:844)
void script_evaluate_repeatable_condition(action_script* script)
{
    if ((script->repeat < 0 || script->num_ready < script->repeat) && (bool)*script->condition)
    {
        script->num_ready++;
        if (script->num_ready <= script->repeat || script->repeat < 0)
        {
            script->condition->reset();
        }
    }
}

// 0x00B3C140 (:865)
void script_run_script()
{
    const int time_since = Cur_script_running->stamp.time_since();
    if (Cur_script_running->group_handle != -1)
    {
        World.begin_mission_object_creation();
    }
    // An action may free scripts, which moves Cur_script_running; the list walked stays the one first read.
    action_script* script = Cur_script_running;
    script_action* next;
    for (script_action* action = script->action.get_first(SAC_RUNNING); action != nullptr; action = next)
    {
        next = script->action.get_next(action);
        const int repeat = script->repeat;
        if (action->delay <= time_since)
        {
            if (Game_trace_file != nullptr)
            {
                script_trace_action(script, action);
            }
            Action_return_value.clear();
            action->definition->callback(action->params);
            if (action->variable_idx != -1)
            {
                GAME_TRACE("action", "  var%d = 0x%X(%s)", action->variable_idx, Action_return_value.handle, game_trace_object_name(Action_return_value.handle));
                Script_variable_values[action->variable_idx].set(Action_return_value.handle);
            }
            if (repeat == 0)
            {
                script_action_free(action, script->action);
            }
            else
            {
                script->action.move(action, SAC_COMPLETED);
            }
            script = Cur_script_running;
            script->num_actions_run++;
        }
    }
    if (Cur_script_running->group_handle != -1)
    {
        World.end_mission_object_creation();
    }
    script = Cur_script_running;
    if (script->action.get_count(SAC_RUNNING) != 0)
    {
        Cur_script_running = Action_scripts.get_next(script);
        return;
    }
    if (script->repeat == 0)
    {
        Cur_script_running = script_free_script(script);
        return;
    }
    if (script->repeat > 0)
    {
        script->repeat--;
    }
    script_action* next_done;
    for (script_action* action = script->action.get_first(SAC_COMPLETED); action != nullptr; action = next_done)
    {
        next_done = script->action.get_next(action);
        script->action.move(action, SAC_RUNNING);
    }
    Cur_script_running = Action_scripts.move(script, SCRIPT_WAITING);
}

// 0x00B3C3D0 (:921)
void script_run()
{
    for (int count = Action_scripts.get_count(SCRIPT_RUNNING); count > 0; count--)
    {
        if (Cur_script_running == nullptr)
        {
            Cur_script_running = Action_scripts.get_first(SCRIPT_RUNNING);
        }
        action_script* script = Cur_script_running;
        if ((script->repeat < 0 || script->num_ready < script->repeat) && (bool)*script->condition)
        {
            script = Cur_script_running;
            script->num_ready++;
            if (script->num_ready != script->repeat)
            {
                script->condition->reset();
            }
        }
        script_run_script();
    }
}

// 0x00B3C470 (:982)
void script_do_frame()
{
    if (World.sl_flags.performing_save_game || World.sl_flags.performing_save_state || World.sl_flags.saving_state_data || (mission_is_active() && !mission_is_ready()))
    {
        return;
    }
    script_cache_vip_npcs();
    script_timers_do_frame();
    for (int count = Action_scripts.get_count(SCRIPT_WAITING); count > 0; count--)
    {
        if (Cur_script_eval == nullptr)
        {
            Cur_script_eval = Action_scripts.get_first(SCRIPT_WAITING);
        }
        action_script* script = Cur_script_eval;
        action_script* next = Action_scripts.get_next(script);
        if (script->repeat != 0 || script->num_ready == 0)
        {
            if ((bool)*script->condition)
            {
                script = Cur_script_eval;
                script->num_ready++;
                if (script->repeat != 0)
                {
                    script->condition->reset();
                    script = Cur_script_eval;
                }
            }
            script = Cur_script_eval;
        }
        if (script->num_ready > 0)
        {
            GAME_TRACE("script", "%s fires: group=%s repeat=%d", script_trace_name(script), script_trace_group(script), script->repeat);
            script->stamp.set(0);
            script->num_ready--;
            Action_scripts.move(script, SCRIPT_RUNNING);
            action_script* saved_running = Cur_script_running;
            Cur_script_running = Cur_script_eval;
            script_run_script();
            Cur_script_running = saved_running;
        }
        Cur_script_eval = next;
    }
    script_run();
}

// 0x00B313E0 (:1094)
bool script_is_active(action_script* script)
{
    const int index = Action_scripts.get_index(script);
    return !((unsigned int)index < Action_scripts.get_capacity() && Action_scripts.get_category(index) == SCRIPT_STORED);
}

// 0x00B31420 (:1106)
int get_variable_index(const char* var_name)
{
    int index;
    return Variable_table.get(var_name, index) ? index : -1;
}

// 0x00B1FA70 (:1115)
void reset_variable_index()
{
    Variable_idx = 0;
}

static bool script_save_category(cfile* file, int category, bool managed)
{
    const unsigned int count_pos = cf_tell(file);
    unsigned int count = 0;
    if (!cf_write_uint32(0, file))
    {
        return false;
    }
    for (action_script* script = Action_scripts.get_first(category); script != nullptr; script = Action_scripts.get_next(script))
    {
        const unsigned int group = (unsigned int)script->group_handle;
        if (managed != (group != 0xFFFFFFFF))
        {
            continue;
        }
        if (group != 0xFFFFFFFF)
        {
            managed_script_group_data* data = Loaded_managed_script_groups.get_ptr(group);
            if (data == nullptr)
            {
                return false;
            }
            // Flag 1: no_save.
            if ((data->flags & 1) != 0)
            {
                continue;
            }
        }
        if (!script->save(file))
        {
            return false;
        }
        count++;
    }
    const unsigned int end_pos = cf_tell(file);
    return cf_seek(file, count_pos, CF_SEEK_SET) && cf_write_uint32(count, file) && cf_seek(file, end_pos, CF_SEEK_SET);
}

// 0x00B3C5A0 (:1120)
bool script_system_save_managed(cfile* file, bool managed)
{
    return script_save_category(file, SCRIPT_WAITING, managed) && script_save_category(file, SCRIPT_RUNNING, managed);
}

// 0x00B3C760 (:1206)
bool script_system_save(cfile* file)
{
    if (!cf_write_uint32((unsigned int)Action_scripts.get_count(SCRIPT_STORED), file))
    {
        return false;
    }
    for (action_script* script = Action_scripts.get_first(SCRIPT_STORED); script != nullptr; script = Action_scripts.get_next(script))
    {
        if (!script->save(file))
        {
            return false;
        }
    }
    const unsigned int count_pos = cf_tell(file);
    unsigned int count = 0;
    if (!cf_write_uint32(0, file))
    {
        return false;
    }
    for (managed_script_group_data* data = Loaded_managed_script_groups.get_first(); data != nullptr; data = Loaded_managed_script_groups.get_next(data))
    {
        if ((data->flags & 1) == 0)
        {
            unsigned int key;
            Loaded_managed_script_groups.get_key(data, key);
            if (!cf_write_uint32(key, file) || !cf_write_uint8(data->flags, file))
            {
                return false;
            }
            count++;
        }
    }
    const unsigned int end_pos = cf_tell(file);
    if (!cf_seek(file, count_pos, CF_SEEK_SET))
    {
        return false;
    }
    cf_write_uint32(count, file);
    if (!cf_seek(file, end_pos, CF_SEEK_SET) || !script_variables_save(file))
    {
        return false;
    }
    script_timers_save(file);
    return script_system_save_managed(file, false) && script_system_save_managed(file, true);
}

static bool script_load_category(cfile* file, int category, unsigned int group_mark, bool set_mark)
{
    const unsigned int count = cf_read_uint32(file);
    for (unsigned int i = 0; i < count; i++)
    {
        action_script* script = Action_scripts.alloc(category);
        if (script == nullptr)
        {
            return false;
        }
        script->load(file);
        if (set_mark)
        {
            managed_script_group_data* data = Loaded_managed_script_groups.get_ptr((unsigned int)script->group_handle);
            if (data == nullptr)
            {
                return false;
            }
            data->mark = group_mark;
        }
    }
    return true;
}

// 0x00B3C8D0 (:1280)
void script_system_load(cfile* file)
{
    script_load_category(file, SCRIPT_STORED, 0, false);
    for (unsigned int groups = cf_read_uint32(file); groups != 0; groups--)
    {
        const int handle = (int)cf_read_uint32(file);
        const unsigned char flags = cf_read_uint8(file);
        script_group_add(handle, flags);
    }
    script_variables_load(file);
    script_timers_load(file);
    script_load_category(file, SCRIPT_WAITING, 0, false);
    script_load_category(file, SCRIPT_RUNNING, 0, false);
    const unsigned int mark = Script_mempool.mark();
    if (script_load_category(file, SCRIPT_WAITING, mark, true))
    {
        script_load_category(file, SCRIPT_RUNNING, mark, true);
    }
}
