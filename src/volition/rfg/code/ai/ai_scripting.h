// Original: volition/rfg/code/ai/ai_scripting.h
// Implementation: volition/rfg/code/ai/ai_scripting.cpp
//
// The scripting interface of the AI: priority targets, vehicles NPCs may drive or man, playing animations and
// triggering scripted behaviours on characters or the nearest team members (ranked by npc_ranker).
#pragma once

#include "volition/rfg/code/ai/ai_types.h"
#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_enums.h"
#include "volition/vlib/code/util/varray.h"

class human;
class npc;

// size 0x8
struct npc_ranking_info
{
    npc_ranking_info(npc* np_, float dist_sq_); // 0x005DD710 (:68)
    npc_ranking_info(); // 0x005DD6F0 (:67)

    npc* np; // 0x00
    float dist_sq; // 0x04
};

// size 0x38
struct npc_ranker
{
    bool add(npc* np, float dist_sq); // 0x0064D710 (:75)
    npc_ranker(); // 0x0064D9B0
    ~npc_ranker(); // 0x0064DA00

    farray<npc_ranking_info,5> rankings; // 0x00
};

bool ai_scripting_set_priority_target(unsigned int target_handle, bool is_priority); // 0x005FE660 (:190)
bool ai_scripting_is_priority_target(const human* hp); // 0x0064DA10 (:226)
bool ai_scripting_set_vehicle_enabled_for_npc_drivers(unsigned int vehicle_handle, bool allow_driving); // 0x005FE6F0 (:259)
bool ai_scripting_set_vehicle_turrets_enabled_for_npcs(unsigned int vehicle_handle, bool allow_turret); // 0x005FE740 (:278)
bool ai_scripting_set_only_ride_with_player(unsigned int npc_handle, bool only_player_vehicle); // 0x005FE7E0 (:308)
bool ai_scripting_play_animation_on_character(npc* np, animation_action animation, bool to_player); // 0x0064DA90 (:331)
bool ai_scripting_play_animation_on_character(unsigned int npc_handle, animation_action animation, bool to_player); // 0x0064DAD0 (:339)
npc* ai_scripting_play_animation_on_nearest_team_member(human_teams team, animation_action animation, bool to_player); // 0x00673D70 (:354)
ai_scripting_behaviors ai_scripting_behavior_from_name(const char* name); // 0x005DD730 (:371)
bool ai_scripting_trigger_behavior_on_character(npc* np, ai_scripting_behaviors behavior, unsigned int target_obj_handle, int special_value, int action_id); // 0x0064DB40 (:391)
bool ai_scripting_trigger_behavior_on_character(unsigned int npc_handle, ai_scripting_behaviors behavior, unsigned int target_obj_handle, int special_value, int action_id); // 0x0064DB90 (:398)
npc* ai_scripting_trigger_behavior_on_nearest_team_member(human_teams team, ai_scripting_behaviors behavior, unsigned int target_obj_handle); // 0x00673DB0 (:416)
bool ai_scripting_trigger_behavior_on_character(npc* np, const char* name, unsigned int target_obj_handle); // 0x0064DC00 (:425)
bool ai_scripting_trigger_behavior_on_character(unsigned int npc_handle, const char* name, unsigned int target_obj_handle); // 0x0064DC50 (:436)
npc* ai_scripting_trigger_behavior_on_nearest_team_member(human_teams team, const char* name, unsigned int target_obj_handle); // 0x00673DE0 (:447)
void ai_scripting_trigget_tech_response(voice_lines response_line); // 0x0064DC80 (:459)
