// The pools and tables shared by the action script .cpp files (new header, stub_impl.md S9: rfg.pdb lists them as file
// statics, but the original's whole program optimization placed them and they are used across the files).
#pragma once

#include "volition/rfg/code/scripting/action_script/action_script.h"
#include "volition/rfg/code/scripting/action_script/script_event.h"
#include "volition/rfg/code/scripting/action_script/script_function.h"
#include "volition/rfg/code/scripting/action_script/script_timer.h"
#include "volition/rfg/code/scripting/action_script/script_types.h"
#include "volition/vlib/code/memory/object_pool.h"

extern script_variable_value Action_return_value; // 0x016B2658
extern script_variable_value Script_variable_values[128]; // 0x036F6DC0
extern int Variable_idx;
extern action_script* Cur_script_loading;
extern const char* Script_node_tags[NUM_SN_TYPES];
extern object_pool<action_script,2048,4> Action_scripts; // 0x03698830
extern object_pool<script_action,6144,1> Script_actions; // 0x036D1860
extern object_pool<script_atom,15360,1> Script_atoms; // 0x03611500
extern object_pool<script_event,4096,1> Script_events; // 0x0366F3B0
extern object_pool<script_function,7168,1> Script_functions; // 0x036F71C0
extern object_pool<script_timer,64,3> Script_timers; // 0x036F6500
extern object_pool<script_variable,512,1> Script_variables; // 0x03695BE0

// Script_timer_* display state (script_timer.cpp, also reset by script_system_init and script_system_level_shutdown).
void script_timer_display_reset(); // not in rfg.exe: the inlined assignments of those two
