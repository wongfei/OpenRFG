// Original: volition/rfg/code/scripting/action_script/action_script.h
// Implementation: volition/rfg/code/scripting/action_script/action_script.cpp
//
// The action script runtime: loading, running and stopping scripts; script_do_frame runs once per gameplay frame.
#pragma once

#include "volition/rfg/code/scripting/action_script/script_action.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/time/timestamp.h"

enum ACTION_SCRIPT_CATEGORY;
class action_script;
struct cfile;
struct script_node;
struct xml_element;

void unused_dcf_load_script(); // 0x00B406A0 (:121)
action_script* action_script_alloc(ACTION_SCRIPT_CATEGORY category); // 0x00B313D0 (:205)
bool script_activate_managed_group(int handle, bool no_save); // 0x00B3F820 (:248)
void script_terminate_managed_group(int handle, bool remove_scripts); // 0x00B3BB10 (:320)
void script_system_init(); // 0x00B3FA30 (:541)
void script_system_level_init_pre(); // 0x00B1FA10 (:551)
void script_system_level_shutdown(); // 0x00B3FAA0 (:561)
bool script_parse_actions(action_script* script, xml_element* root, int delay); // 0x00B3FEC0 (:615)
bool script_parse_delayed_actions(action_script* script, xml_element* root, int parent_delay); // 0x00B3FF60 (:647)
action_script* script_parse(xml_element* script, int group_handle); // 0x00B40150 (:680)
int script_load(const char* scriptx_file); // 0x00B40310 (:780)
void script_reset(action_script* script); // 0x00B1FA20 (:834)
void script_evaluate_repeatable_condition(action_script* script); // 0x00B1FA30 (:844)
void script_run_script(); // 0x00B3C140 (:865)
void script_run(); // 0x00B3C3D0 (:921)
void script_do_frame(); // 0x00B3C470 (:982)
bool script_is_active(action_script* script); // 0x00B313E0 (:1094)
int get_variable_index(const char* var_name); // 0x00B31420 (:1106)
void reset_variable_index(); // 0x00B1FA70 (:1115)
bool script_system_save_managed(cfile* file, bool managed); // 0x00B3C5A0 (:1120)
bool script_system_save(cfile* file); // 0x00B3C760 (:1206)
void script_system_load(cfile* file); // 0x00B3C8D0 (:1280)

// size 0x48
class action_script
{
public:
    action_script(); // 0x00B3F7D0 (.h:28)
    bool is_managed() const; // 0x00B1F5C0 (.h:65)
    bool save(cfile* file); // 0x00B3BD40 (:374)
    void load(cfile* file); // 0x00B3BEB0 (:444)
    void init(); // 0x00B3C110 (:518)

    script_node* condition; // 0x00
    object_pool_client<script_action,2> action; // 0x08
    int num_actions_run; // 0x30
    timestamp stamp; // 0x34
    int num_ready; // 0x38
    int repeat; // 0x3C
    int group_handle; // 0x40
};
