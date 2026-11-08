// Original: volition/rfg/code/ai/ai_defense_coordination.h
// Implementation: volition/rfg/code/ai/ai_defense_coordination.cpp
//
// Defense coordination: NPCs defending an area (restricted areas, anchored guards) request assignments against
// targets, and the coordinator arbitrates which defender threatens, beats down or attacks a target and in which
// threaten stage it is. ai_system_do_frame advances it.
#pragma once


class ai_defense_defender;
class ai_defense_target;
class human;
class npc;
class object_restricted_area;

enum ai_defense_assignments
{
    AI_DEFENSE_ASSIGNMENT_NONE = -1,
    AI_DEFENSE_ASSIGNMENT_GUARD = 0,
    AI_DEFENSE_ASSIGNMENT_THREATEN = 1,
    AI_DEFENSE_ASSIGNMENT_ACCOMPANY = 2,
    NUM_AI_DEFENSE_ASSIGNMENTS = 3,
};

enum ai_defense_threaten_stages
{
    AI_DEFENSE_THREATEN_NONE = -1,
    AI_DEFENSE_THREATEN_FIRST_WARNING = 0,
    AI_DEFENSE_THREATEN_WARNING_SHOT = 1,
    AI_DEFENSE_THREATEN_DASH_AND_BASH = 2,
    AI_DEFENSE_THREATEN_KILL = 3,
    NUM_AI_DEFENSE_THREATEN_STAGES = 4,
};

void arbitrate_defense_target_request(ai_defense_defender* defender_info, const human* target); // 0x00673A70 (:80)
bool npc_is_anchored(const npc* np); // 0x006B4C10 (:345)
ai_defense_target* get_target_info(unsigned int def_handle, const human* target); // 0x00673B30 (:371)
object_restricted_area* ai_defense_is_human_in_restricted_area(const human* hp, bool include_warning_areas); // 0x005FB6D0 (:400)
bool ai_defense_is_anchored(const npc* np, unsigned int def_handle); // 0x006B4C80 (:465)
unsigned int ai_defense_request_assignment(const npc* np, const human* target, unsigned int prev_handle); // 0x006B4CD0 (:487)
ai_defense_assignments ai_defense_get_assignment(unsigned int def_handle); // 0x0064D010 (:532)
ai_defense_threaten_stages ai_defense_get_threaten_stage(unsigned int def_handle, const human* target); // 0x00673BA0 (:580)
bool ai_defense_should_wait_to_threaten(unsigned int def_handle, const human* target); // 0x00673BC0 (:600)
bool ai_defense_should_play_beatdown_line(unsigned int def_handle, const human* target); // 0x00673BF0 (:619)
void ai_defense_report_played_beatdown_line(unsigned int def_handle, const human* target); // 0x00673C20 (:628)
npc* ai_defense_get_threatening_npc(unsigned int def_handle); // 0x0064D0C0 (:644)
bool ai_defense_report_threaten_start(unsigned int def_handle); // 0x0064D160 (:684)
void ai_defense_report_threaten_done(unsigned int def_handle); // 0x0064D1F0 (:725)
void ai_defense_report_threaten_failed(unsigned int def_handle); // 0x0064D290 (:759)
void ai_defense_report_no_path(unsigned int def_handle); // 0x0064D320 (:788)
void ai_defense_coordination_level_init_pre(); // 0x006B4D60 (:812)
void ai_defense_coordination_do_frame(); // 0x006B4D70 (:817)
