// Original: volition/rfg/code/ai/ai_scripting.cpp
//
// The AI's scripting interface (NPC.md NP1): priority targets, which vehicles NPCs may use, and scripted animations and
// behaviours on a character or on the nearest member of a team.

#include "volition/rfg/code/ai/ai_scripting.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_voice/voice_line_unlocking.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_action_node.h"
#include "volition/rfg/code/objects/object_dummy.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/spawn/ambient_prop_info.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/util/string_comp.h"

#include <float.h>

// rfg.exe reaches object_dummy.cpp from place_barricades, not ported yet, and nothing else here pulls it out of the library,
// so without this its class never registers and zones cannot create dummies.
#pragma comment(linker, "/include:?get_classname_hash@object_dummy@@SAIXZ")

// 0x012C82E8, indexed by ai_scripting_behaviors.
static const char* const Ai_scripting_behavior_names[NUM_AI_SCRIPTING_BEHAVIORS] =
{
    "none",
    "trapped in building",
    "trapped in jail",
    "marauder lookout",
    "plant bomb",
    "old coot ramble",
    "tech response",
    "look at object",
    "turn and animate",
    "follow wounded",
    "activity intro",
};

// rfg.exe tests animation - ANIMATION_ACTION_IDLE_A < 0x295 unsigned, so everything before IDLE_A is rejected too.
static const unsigned int NUM_SCRIPTED_ANIMATIONS = 0x295;
static const float RANK_NPC_MAX_DIST_SQ = 1600.0f; // 40 m from the local player
static const float TECH_RESPONSE_NEAR_PLAYER_DIST_SQ = 100.0f; // 10 m: a tech this close is left where it is
static const int TECH_RESPONSE_VOICE_COOLDOWN_MS = 10000;
static const unsigned int TECH_RESPONSE_ANIM_FLAGS = 0x40; // rfg.exe's constant, meaning not recovered

static bool scripted_animation_is_valid(animation_action animation)
{
    return (unsigned int)animation - (unsigned int)ANIMATION_ACTION_IDLE_A < NUM_SCRIPTED_ANIMATIONS;
}

// rfg.exe computes to_player - 3: AI_SB_PLAY_ANIMATION (-3) or AI_SB_PLAY_ANIMATION_TO_PLAYER (-2).
static ai_scripting_behaviors play_animation_behavior(bool to_player)
{
    return to_player ? AI_SB_PLAY_ANIMATION_TO_PLAYER : AI_SB_PLAY_ANIMATION;
}

// rfg.exe compares unsigned (CMP ECX,0xa / JA at 0x0064DB44), so the negative play animation behaviours fail here.
static bool scripting_behavior_is_valid(ai_scripting_behaviors behavior)
{
    return (unsigned int)behavior < (unsigned int)NUM_AI_SCRIPTING_BEHAVIORS;
}

static npc* scripting_npc_from_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_NPC)
    {
        return nullptr;
    }
    return (npc*)objp;
}

// Sets the behaviour and asks for a new plan unless one is already asked for.
static void set_custom_behavior_and_replan(ai_system* ai, ai_scripting_behaviors behavior, unsigned int target_obj_handle, int special_value, int action_id)
{
    ai->custom_behavior = behavior;
    ai->custom_behavior_target = target_obj_handle;
    ai->custom_behavior_value = special_value;
    ai->custom_action_id = action_id;
    ai->planner.request_replan();
}

// 0x005DD6F0 (:67)
npc_ranking_info::npc_ranking_info()
    : np(nullptr)
    , dist_sq(0.0f)
{
}

// 0x005DD710 (:68)
npc_ranking_info::npc_ranking_info(npc* np_, float dist_sq_)
    : np(np_)
    , dist_sq(dist_sq_)
{
}

// 0x0064D9B0
npc_ranker::npc_ranker()
{
}

// 0x0064DA00
npc_ranker::~npc_ranker()
{
}

// 0x0064D710 (:75): keeps the five closest, nearest first. A tie goes in front of the NPC already there.
bool npc_ranker::add(npc* np, float dist_sq)
{
    const npc_ranking_info info(np, dist_sq);
    const int count = rankings.size();
    if (count == 0)
    {
        rankings.add(info);
        return true;
    }
    if (rankings[count - 1].dist_sq < dist_sq)
    {
        if (count < rankings.capacity())
        {
            rankings.add(info);
            return true;
        }
        return false;
    }
    int slot = 0;
    while (slot < count - 1 && dist_sq > rankings[slot].dist_sq)
    {
        slot++;
    }
    if (count < rankings.capacity())
    {
        rankings.add(info); // only grows the list, the loop below moves everything into place
    }
    for (int i = rankings.size() - 1; i > slot; i--)
    {
        rankings[i] = rankings[i - 1];
    }
    rankings[slot] = info;
    return true;
}

// 0x0064D8D0 (:131)
static void find_and_rank_npcs(npc_ranker& ranker, human_teams team, bool skip_scripted_npcs)
{
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    const int count = World.get_num_objects<npc>();
    for (int i = 0; i < count; i++)
    {
        npc* np = World.get_object<npc>(i);
        const ai_system* ai = np->get_ai();
        if (human_is_dead(np) || human_is_ragdolled(np) || np->current_team != team)
        {
            continue;
        }
        if (skip_scripted_npcs && ai->custom_behavior != AI_SB_NONE)
        {
            continue;
        }
        const float dist_sq = vec_dist_squared(pp->pos, np->pos);
        if (dist_sq <= RANK_NPC_MAX_DIST_SQ)
        {
            ranker.add(np, dist_sq);
        }
    }
}

// 0x00673CB0 (:158)
// LTCG: team in ECX, then behavior, target_obj_handle and special_value on the stack (the callers' pushes); the PDB's pos
// parameter is gone, the ranking measures from the local player. Asks for no replan, unlike the character overloads.
static npc* trigger_behavior_on_nearest_team_member(human_teams team, ai_scripting_behaviors behavior, unsigned int target_obj_handle, int special_value)
{
    npc_ranker ranker;
    find_and_rank_npcs(ranker, team, false);
    if (ranker.rankings.size() <= 0)
    {
        return nullptr;
    }
    npc* np = ranker.rankings[0].np;
    ai_system* ai = np->get_ai();
    ai->custom_behavior = behavior;
    ai->custom_behavior_target = target_obj_handle;
    ai->custom_behavior_value = special_value;
    ai->custom_action_id = -1;
    return np;
}

// 0x005FE660 (:190)
bool ai_scripting_set_priority_target(unsigned int target_handle, bool is_priority)
{
    object* objp = World.object_from_handle(target_handle);
    if (objp == nullptr)
    {
        return false;
    }
    switch (objp->get_obj_type())
    {
    case OT_HUMAN:
        ((human*)objp)->hflags.high_priority_target = is_priority;
        return true;
    case OT_VEHICLE:
        ((vehicle*)objp)->m_flags.high_priority_target = is_priority;
        return true;
    case OT_SQUAD:
        ((object_squad*)objp)->set_high_priority(is_priority);
        return true;
    default:
        // Original: Int3 critical function here.
        return false;
    }
}

// 0x0064DA10 (:226)
bool ai_scripting_is_priority_target(const human* hp)
{
    if (hp->hflags.high_priority_target)
    {
        return true;
    }
    const vehicle* vp = human_in_vehicle(hp);
    if (vp != nullptr && vp->m_flags.high_priority_target)
    {
        return true;
    }
    if (hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_NPC)
    {
        const unsigned int squad_handle = ((const npc*)hp)->get_ai()->get_squad_handle();
        if (squad_handle != 0xFFFFFFFF)
        {
            const object* squad = World.object_from_handle(squad_handle);
            if (squad != nullptr && squad->get_obj_type() == OT_SQUAD && ((const object_squad*)squad)->is_high_priority())
            {
                return true;
            }
        }
    }
    return false;
}

// 0x005FE6F0 (:259)
bool ai_scripting_set_vehicle_enabled_for_npc_drivers(unsigned int vehicle_handle, bool allow_driving)
{
    vehicle* vp = vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(vehicle_handle) : nullptr;
    if (vp == nullptr)
    {
        return false;
    }
    vp->m_flags.disable_for_npc_drivers = !allow_driving;
    return true;
}

// 0x005FE740 (:278)
bool ai_scripting_set_vehicle_turrets_enabled_for_npcs(unsigned int vehicle_handle, bool allow_turret)
{
    vehicle* vp = vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(vehicle_handle) : nullptr;
    if (vp == nullptr)
    {
        return false;
    }
    vp->m_flags.disable_turrets_for_npcs = !allow_turret;
    for (int i = 0; i < vp->m_num_turret_mounts; i++)
    {
        const unsigned int turret_handle = vp->m_turret_mounts[i].attached_turret;
        turret* tp = turret_handle != 0xFFFFFFFF ? World.object_from_handle<turret>(turret_handle) : nullptr;
        if (tp != nullptr)
        {
            tp->turret_flags.locked_turret = !allow_turret;
        }
    }
    return true;
}

// 0x005FE7E0 (:308)
bool ai_scripting_set_only_ride_with_player(unsigned int npc_handle, bool only_player_vehicle)
{
    npc* np = scripting_npc_from_handle(npc_handle);
    if (np == nullptr || human_teams_get_disposition(np, HUMAN_TEAM_GUERILLA) <= TEAM_DISPOSITION_UNFRIENDLY)
    {
        return false;
    }
    np->npc_flags.only_ride_with_player = only_player_vehicle;
    return true;
}

// 0x0064DA90 (:331): no replan, unlike the trigger overloads.
bool ai_scripting_play_animation_on_character(npc* np, animation_action animation, bool to_player)
{
    if (!scripted_animation_is_valid(animation))
    {
        return false;
    }
    ai_system* ai = np->get_ai();
    ai->custom_behavior_value = animation;
    ai->custom_behavior = play_animation_behavior(to_player);
    ai->custom_behavior_target = 0xFFFFFFFF;
    ai->custom_action_id = -1;
    return true;
}

// 0x0064DAD0 (:339)
bool ai_scripting_play_animation_on_character(unsigned int npc_handle, animation_action animation, bool to_player)
{
    npc* np = scripting_npc_from_handle(npc_handle);
    return np != nullptr && ai_scripting_play_animation_on_character(np, animation, to_player);
}

// 0x00673D70 (:354)
npc* ai_scripting_play_animation_on_nearest_team_member(human_teams team, animation_action animation, bool to_player)
{
    if (!scripted_animation_is_valid(animation))
    {
        return nullptr;
    }
    return trigger_behavior_on_nearest_team_member(team, play_animation_behavior(to_player), 0xFFFFFFFF, animation);
}

// 0x005DD730 (:371)
ai_scripting_behaviors ai_scripting_behavior_from_name(const char* name)
{
    for (int i = 0; i < NUM_AI_SCRIPTING_BEHAVIORS; i++)
    {
        if (v_stricmp(Ai_scripting_behavior_names[i], name) == 0)
        {
            return (ai_scripting_behaviors)i;
        }
    }
    return AI_SB_UNDEFINED;
}

// 0x0064DB40 (:391)
bool ai_scripting_trigger_behavior_on_character(npc* np, ai_scripting_behaviors behavior, unsigned int target_obj_handle, int special_value, int action_id)
{
    if (!scripting_behavior_is_valid(behavior))
    {
        return false;
    }
    set_custom_behavior_and_replan(np->get_ai(), behavior, target_obj_handle, special_value, action_id);
    return true;
}

// 0x0064DB90 (:398)
bool ai_scripting_trigger_behavior_on_character(unsigned int npc_handle, ai_scripting_behaviors behavior, unsigned int target_obj_handle, int special_value, int action_id)
{
    npc* np = scripting_npc_from_handle(npc_handle);
    return np != nullptr && ai_scripting_trigger_behavior_on_character(np, behavior, target_obj_handle, special_value, action_id);
}

// 0x00673DB0 (:416)
npc* ai_scripting_trigger_behavior_on_nearest_team_member(human_teams team, ai_scripting_behaviors behavior, unsigned int target_obj_handle)
{
    if (!scripting_behavior_is_valid(behavior))
    {
        return nullptr;
    }
    return trigger_behavior_on_nearest_team_member(team, behavior, target_obj_handle, -1);
}

// 0x0064DC00 (:425)
bool ai_scripting_trigger_behavior_on_character(npc* np, const char* name, unsigned int target_obj_handle)
{
    const ai_scripting_behaviors behavior = ai_scripting_behavior_from_name(name);
    return behavior != AI_SB_UNDEFINED && ai_scripting_trigger_behavior_on_character(np, behavior, target_obj_handle, -1, -1);
}

// 0x0064DC50 (:436)
bool ai_scripting_trigger_behavior_on_character(unsigned int npc_handle, const char* name, unsigned int target_obj_handle)
{
    const ai_scripting_behaviors behavior = ai_scripting_behavior_from_name(name);
    return behavior != AI_SB_UNDEFINED && ai_scripting_trigger_behavior_on_character(npc_handle, behavior, target_obj_handle, -1, -1);
}

// 0x00673DE0 (:447)
npc* ai_scripting_trigger_behavior_on_nearest_team_member(human_teams team, const char* name, unsigned int target_obj_handle)
{
    const ai_scripting_behaviors behavior = ai_scripting_behavior_from_name(name);
    if (behavior == AI_SB_UNDEFINED || !scripting_behavior_is_valid(behavior))
    {
        return nullptr;
    }
    return trigger_behavior_on_nearest_team_member(team, behavior, target_obj_handle, -1);
}

// 0x0064DC80 (:459): moves the first guerrilla tech NPC to the nearest tech response dummy when the player is 10 m or more
// away. Original quirk kept: response_line is only range checked, the line is never played from here.
void ai_scripting_trigget_tech_response(voice_lines response_line)
{
    if ((unsigned int)response_line >= (unsigned int)NUM_VOICE_LINES)
    {
        return;
    }
    npc* tech = nullptr;
    const int num_npcs = World.get_num_objects<npc>();
    for (int i = 0; i < num_npcs; i++)
    {
        npc* np = World.get_object<npc>(i);
        if (np->h_info->flags.guerrilla_tech)
        {
            tech = np;
            break;
        }
    }
    if (tech == nullptr)
    {
        return;
    }
    const vector& player_pos = player_get_local_player()->pos;
    if (vec_dist_squared(player_pos, tech->pos) < TECH_RESPONSE_NEAR_PLAYER_DIST_SQ)
    {
        return;
    }
    const object_dummy* closest = nullptr;
    float closest_dist_sq = FLT_MAX;
    const int num_dummies = World.get_num_objects<object_dummy>();
    for (int i = 0; i < num_dummies; i++)
    {
        const object_dummy* dummy = World.get_object<object_dummy>(i);
        if (dummy->get_dummy_type() == DUMMY_OBJECT_TYPE_TECH_RESPONSE)
        {
            const float dist_sq = vec_dist_squared(player_pos, dummy->pos);
            if (dist_sq < closest_dist_sq)
            {
                closest = dummy;
                closest_dist_sq = dist_sq;
            }
        }
    }
    if (closest == nullptr)
    {
        return;
    }
    amb_prop_detach_props(tech, true);
    object_action_node* node = tech->get_current_action_node(false);
    if (node != nullptr)
    {
        node->emergency_exit(tech);
    }
    tech->get_ai()->planner.lock_action(false);
    tech->update_pos_and_orient(closest->pos, closest->orient, true);
    tech->adjust_child_transforms(false, false);
    const animation_action animation = int_rand_range(0, 1) == 0 ? ANIMATION_ACTION_SAM_STAND_IDLE : ANIMATION_ACTION_SAM_STAND_IDLE_A;
    tech->char_inst->play_action(animation, ANIM_ALIAS_SCRIPTED, 1.0f, TECH_RESPONSE_ANIM_FLAGS);
    voice_line_unlocking_set_global_cooldown(tech, VLUG_SAMANYA, TECH_RESPONSE_VOICE_COOLDOWN_MS);
}
