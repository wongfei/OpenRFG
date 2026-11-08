// Original: volition/rfg/code/gameplay/activities/activity_raid.h
// Implementation: volition/rfg/code/gameplay/activities/activity_raid.cpp
//
// Raids: EDF attacks on guerrilla positions and guerrilla attacks on EDF targets; starting a raid at a raid node, the
// demolition targets and area, raid flags on humans and squads, straggler tracking, team totals, and save/load. The
// explosion code asks activity_raid_is_building_demo_charge so raid charges do not alert the AI.
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_raid_internal.h"
#include "volition/rfg/code/objects/humans/human_teams.h"

struct cfile;
struct explosion_info;
class human;
class npc;
class object;
class object_raid_node;
class object_squad;
class vector;

bool is_in_cylindrical_region(const vector& pos, const vector& center, const float radius_sqrd); // 0x00753EB0 (.h:146)
void init_straggler_tracking_check(); // 0x0076EBF0 (:245)
void render_straggler_tracking_debug(const npc* np, straggler_tracking_debug_status status, raid_straggler_info* straggler_info); // 0x007591C0 (:256)
void do_straggler_tracking(npc* np); // 0x00783630 (:313)
bool has_raid_flag(const human* hp); // 0x007591E0 (:387)
bool has_raid_flag(const object_squad* squad); // 0x00759200 (:391)
bool activity_raid_start_raid_local(activity_raid_data& local_raid_system, const object_raid_node* raid_node); // 0x00799830 (:3793)
bool activity_raid_start_raid(const object_raid_node* raid_node); // 0x007998F0 (:3825)
int activity_raid_get_time_until_demolition(); // 0x007596D0 (:3840)
bool activity_raid_is_in_demolition_area(const vector& pos); // 0x00784D40 (:3855)
bool activity_raid_is_in_raid_region(const vector& pos); // 0x00759760 (:3873)
bool activity_raid_is_demolition_target(unsigned int handle); // 0x0076FC30 (:3891)
object* activity_raid_get_raid_object(); // 0x007597E0 (:3919)
float activity_raid_get_demolition_radius(); // 0x00759870 (:3939)
bool activity_raid_get_exit_point(activity_raid_data& local_raid_data, const vector& pos, vector& exit_point); // 0x0076FCF0 (:3957)
bool activity_raid_get_exit_point(const vector& pos, vector& exit_point); // 0x0076FDC0 (:4005)
bool activity_raid_is_building_demo_charge(const vector& pos, const explosion_info* e_info); // 0x007598C0 (:4020)
unsigned int activity_raid_get_current_node(); // 0x00759950 (:4038)
float activity_raid_victory_percentage(); // 0x00759980 (:4052)
void activity_raid_get_team_totals(int& num_friendly, int& num_enemy); // 0x00759A00 (:4079)
bool activity_raid_nodes_are_relatives(unsigned int raid_handle_1, unsigned int raid_handle_2); // 0x0076FE10 (:4114)
void activity_raid_set_regions(unsigned int raid_node_handle, bool attacking_team, unsigned int withdraw_handle, unsigned int standard_handle, unsigned int attack_handle); // 0x0076FE80 (:4147)
void activity_raid_register_target(unsigned int raid_node, unsigned int target_handle); // 0x00784E60 (:4185)
bool activity_raid_has_raid_orders(const npc* np); // 0x00759AC0 (:4220)
void activity_raid_exit(); // 0x007B2AB0 (:4260)
void activity_raid_replay(); // 0x00759B10 (:4282)
void activity_raid_process_player_death(); // 0x00759BA0 (:4303)
void activity_squad_destroy_notify_raid(const object_squad* squad); // 0x007B2B30 (:4317)
void activity_raid_system_save(cfile* save_file); // 0x0076FF90 (:4356)
void activity_raid_system_load(cfile* load_file); // 0x00784F00 (:4376)
bool activity_raid_human_has_raid_flags(const human* hp); // 0x00759C10 (:4403)
bool activity_raid_team_is_defending(human_teams team); // 0x0076FFF0 (:4410)
bool activity_raid_team_is_attacking(human_teams team); // 0x00770050 (:4423)
bool activity_raid_npc_should_prepare(const npc* np); // 0x007700B0 (:4436)
object* activity_raid_get_target(); // 0x00770120 (:4468)
vector activity_raid_get_target_pos(); // 0x00770180 (:4489)
