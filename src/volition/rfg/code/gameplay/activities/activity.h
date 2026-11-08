// Original: volition/rfg/code/gameplay/activities/activity.h
// Implementation: volition/rfg/code/gameplay/activities/activity.cpp
//
// The activity system: activity_system_do_frame advances the registered activities; activity_is_active / _is_ready
// query them by mode.
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_enum.h"
#include "volition/rfg/code/scripting/action_script/global_event.h"

class activity_base;
enum activity_disable_reason;
enum activity_end_ready_reasons;
struct cfile;
class district;
struct event_data;
struct notify_info;
class npc;
class object;
class object_activity_spawn;
class object_squad;
class player_start;
class vector;

activity_base* activity_get_activity_data(int index); // 0x00755080 (:287)
bool should_render_icons(base_activity_states state); // 0x007550B0 (:291)
bool should_render_regions(base_activity_states state); // 0x007550D0 (:297)
bool is_complete_state(activity_interface_states interface_state); // 0x007550F0 (:321)
bool activity_draw_hud(int x_pos, int y_pos); // 0x00755EC0 (:3134)
bool activity_pursuit_npc(const npc* np); // 0x00755F30 (:3159)
bool activity_is_player_cover_blown(); // 0x00755F50 (:3165)
bool activity_none(); // 0x007699C0 (:3191)
bool activity_running_or_shutting_down(activity_modes mode); // 0x00755FD0 (:3212)
bool activity_is_active(activity_modes mode); // 0x00756010 (:3225)
unsigned int activity_get_active_node_handle(); // 0x00756040 (:3237)
bool activity_is_ready(activity_modes mode); // 0x00756080 (:3255)
bool activity_is_loading(activity_modes mode); // 0x007560B0 (:3267)
const char* activity_subtitle(activity_modes mode); // 0x007560F0 (:3279)
const char* activity_title(activity_modes mode); // 0x00756120 (:3291)
activity_interface_states activity_get_interface_mode(activity_modes mode); // 0x00756150 (:3304)
int activity_interface_mode_duration(activity_modes mode); // 0x00756180 (:3316)
const char* activity_get_name(activity_modes mode); // 0x007561C0 (:3328)
bool activity_get_ui_text(char* value, int& time_til_end, int buffer_size, activity_modes mode); // 0x00756200 (:3358)
activity_modes activity_get_mode_from_name(const char* activity_name); // 0x00756250 (:3373)
activity_modes activity_get_mode(); // 0x007562A0 (:3392)
const char* activity_get_current_instance_name(); // 0x007562B0 (:3398)
bool activity_lock_alert_level(); // 0x00756370 (:3429)
bool activity_on_alert(const npc* np); // 0x007563D0 (:3445)
void activity_system_do_frame(bool game_paused); // 0x007ACBB0 (:3468)
bool activity_is_modal(activity_modes mode); // 0x00756450 (:3543)
event_result activity_report_recruitment(const event_data& in_event, notify_info* info); // 0x00756480 (:3556)
event_result activity_report_patrol_reached(const event_data& in_event, notify_info* info); // 0x007564F0 (:3578)
event_result activity_report_action_node(const event_data& in_event, notify_info* info); // 0x00769A20 (:3597)
void activity_suspend_activities_special(activity_disable_reason disable_reason); // 0x00756550 (:3644)
void activity_suspend_activities(activity_disable_reason disable_reason); // 0x007565D0 (:3673)
void activity_suspend_diversions(activity_disable_reason disable_reason); // 0x00756640 (:3694)
void activity_reinstate_activities(activity_disable_reason disable_reason); // 0x007566C0 (:3723)
void activity_reinstate_diversions(activity_disable_reason disable_reason); // 0x00756700 (:3736)
void activity_end_ready_activity(activity_end_ready_reasons reason); // 0x00756740 (:3750)
void activity_enable_all_after_mission(); // 0x007567C0 (:3783)
void activity_disable_mayhem_for_tutorial(); // 0x007567F0 (:3792)
void activity_kill_all_except(activity_modes mode); // 0x00756820 (:3797)
bool activity_is_diversion(activity_modes mode); // 0x00756870 (:3812)
void activity_system_init(); // 0x007B5550 (:3831)
void activity_system_shutdown(); // 0x007568B0 (:3892)
void activity_system_level_init_pre(); // 0x00769AB0 (:3907)
void activity_system_level_init_post(); // 0x007B72B0 (:3953)
bool activity_allow_guerrilla_reinforcements(); // 0x007568D0 (:3980)
void activity_system_level_shutdown(); // 0x00756930 (:4001)
bool activity_outside_defensive_perimeter(const npc* np, const vector& pos, bool& result, const float* buffer_dist); // 0x007569B0 (:4037)
void activity_squad_destroy_notify(const object_squad* squad); // 0x007B55B0 (:4057)
void activity_system_save(cfile* save_file); // 0x00769B80 (:4065)
void activity_system_load(cfile* save_file); // 0x00769BE0 (:4096)
void activity_release_squad(unsigned int spawn_node_handle); // 0x00769C30 (:4113)
bool activity_disable_ambient_ped_spawn(); // 0x00756A10 (:4148)
unsigned int activity_get_last_spawned(); // 0x00756A50 (:4171)
bool activity_disable_ambient_veh_spawn(); // 0x00756AD0 (:4211)
base_activity_states activity_state_from_name(const char* name); // 0x00756B10 (:4237)
player_start* find_parented_player_start(object* objp); // 0x00769CF0 (:4261)
object_activity_spawn* find_activity_spawn_node(object* activity_obj); // 0x0077F550 (:4279)
player_start* activity_get_player_start_from_spawn(object* activity_obj); // 0x0077F5C0 (:4295)
object* activity_get_player_start(unsigned int activity_handle); // 0x0077F690 (:4330)
void activity_warp_and_start(object* activity_node, bool force_reload); // 0x0077F770 (:4426)
void activity_force_start_generic(object* activity_node, bool reload_on_fail); // 0x007A8710 (:4457)
const district* activity_int_get_district_from_node(const object* objp); // 0x00796180 (:4627)
unsigned int activity_int_get_district_handle_from_node(const object* objp); // 0x007961E0 (:4648)
bool activity_set_node_start_on_load(unsigned int handle); // 0x00756B50 (:4660)
unsigned int activity_get_node_start_on_load(); // 0x00756B60 (:4667)
void activity_auto_save(); // 0x00756B70 (:4673)
void activity_kill_gps_message(); // 0x00756B80 (:4678)
