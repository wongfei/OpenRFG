// Original: volition/rfg/code/gameplay/activities/morale_effect_manager.cpp
//
// gameflow.md GF2. Civilians join the guerrillas when the morale is high: during a fight (more the higher the morale),
// through mission draft mode, and spontaneously after events near them.

#include "volition/rfg/code/gameplay/activities/morale_effect_manager.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/percept.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_internal.h"
#include "volition/rfg/code/gameplay/gameflow/gameflow_enums.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/hud/hud_message.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/vlib/code/util/varray.h"

extern activity_base* All_activities[17]; // 0x01651248 (gameplay.cpp)
extern mission_handle Mission_active_index; // 0x0165150C (gameplay.cpp)
extern gfm_alert_levels Current_alert_level; // 0x022278B4 (alert_level_manager.cpp)
extern cartographer Cartographer; // 0x02BB71F8 (minimap.cpp)

// Magic numbers: the fixed slots of All_activities that stop conversions.
static const int ACTIVITY_SLOT_RAID_DEFEND = 8;
static const int ACTIVITY_SLOT_RAID_ATTACK = 9;
// Magic number: 1 second between spontaneous conversions.
static const int SPONTANEOUS_COOLDOWN_MS = 1000;
// Magic number: 15 m squared, how near an earlier convert has to be to help persuade.
static const float CONVERT_PEER_DIST_SQRD = 225.0f;

farray<unsigned int, 32> All_npcs_converted; // 0x01657318
timestamp Spontaneous_conversion_cooldown; // 0x016573A8
bool Mission_draft_mode; // 0x022295FF
static bool Allow_spontaneous_conversion = true; // 0x016510DB
static float Max_guerrilla_conversion_dist = 60.0f; // 0x01651394
static float Max_enemy_dist_for_guerrilla_convert_sqr = 2500.0f; // 0x01651398
static bool Encounter_started; // 0x022295FD
bool Encounter_disabled; // 0x022295FE
static bool Use_sound_propagation_spontaneous_conversion; // 0x02229600, nothing sets it
// How many civilians may fight at once, per tenth of morale.
static const unsigned char Number_of_converts[11] = { 0, 0, 1, 1, 2, 2, 3, 4, 5, 6, 12 }; // 0x0165C924
static tweak_table_item Tweak_table_item_Max_guerrilla_conversion_dist("Max_guerrilla_conversion_dist", &Max_guerrilla_conversion_dist);

// 0x00788C80 (:133)
static bool encounter_active()
{
    if ((int)Current_alert_level >= 0 && ((int)Current_alert_level >= 4 || (int)Current_alert_level > 0))
    {
        return true;
    }
    for (int i = 0; i < All_npcs_converted.size(); i++)
    {
        const unsigned int handle = All_npcs_converted[i];
        const npc* np = handle != 0xFFFFFFFF ? World.object_from_handle<npc>(handle) : nullptr;
        // A convert with an enemy belief keeps the encounter going.
        if (np != nullptr && np->get_ai()->perception.get_belief(AI_BELIEF_ENEMY_POSITION) != nullptr)
        {
            return true;
        }
    }
    const player* pp = player_get_local_player();
    const int num = World.get_num_objects<npc>();
    for (int i = 0; i < num; i++)
    {
        const npc* np = World.get_object<npc>(i);
        if (human_teams_get_disposition(pp, np) == TEAM_DISPOSITION_HOSTILE && np->get_ai()->perception.any_enemy_player_in_los())
        {
            return true;
        }
    }
    return false;
}

// 0x007AF2E0 (:173)
static int get_convert_count()
{
    // The morale percent rounded to tenths (0.005 for the float's error).
    return Number_of_converts[(int)((gfm_get_effective_morale_percent() + 0.005f) * 10.0f)];
}

// 0x007717D0 (:194)
static void release_all_converts()
{
    for (int i = 0; i < All_npcs_converted.size(); i++)
    {
        const unsigned int handle = All_npcs_converted[i];
        npc* np = handle != 0xFFFFFFFF ? World.object_from_handle<npc>(handle) : nullptr;
        if (np != nullptr)
        {
            np->npc_flags.converted_guerrilla = 0;
            np->start_active_despawn(NPC_DESPAWN_TYPE_LOW, nullptr);
        }
    }
}

// 0x00771830 (:210)
static void reset_encounter_data()
{
    All_npcs_converted.clear();
}

// 0x00788D70 (:217)
static npc* find_nearest_enemy_of_player()
{
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return nullptr;
    }
    float best_dist = Max_enemy_dist_for_guerrilla_convert_sqr;
    npc* best = nullptr;
    const int num = World.get_num_objects<npc>();
    for (int i = 0; i < num; i++)
    {
        npc* np = World.get_object<npc>(i);
        if (np->is_killed() || human_teams_get_disposition(pp, np) != TEAM_DISPOSITION_HOSTILE)
        {
            continue;
        }
        const float dist = vec_dist_squared(np->pos, pp->pos);
        if (dist < best_dist && np->get_ai()->perception.get_belief(AI_BELIEF_ENEMY_POSITION, static_cast<const object*>(pp)) != nullptr)
        {
            best_dist = dist;
            best = np;
        }
    }
    return best;
}

// 0x007B3B50 (:251)
static npc* find_nearest_convert_npc()
{
    float best_dist = Max_guerrilla_conversion_dist * Max_guerrilla_conversion_dist;
    npc* best = nullptr;
    const player* pp = player_get_local_player();
    const int num = World.get_num_objects<npc>();
    for (int i = 0; i < num; i++)
    {
        npc* np = World.get_object<npc>(i);
        if (!morale_npc_can_be_converted(np))
        {
            continue;
        }
        const float dist = vec_dist_squared(pp->pos, np->pos);
        if (dist <= best_dist)
        {
            best = np;
            best_dist = dist;
        }
    }
    return best;
}

// 0x007B3C00 (:284)
static void convert_to_guerrilla(int npc_count)
{
    const npc* enemy = find_nearest_enemy_of_player();
    if (enemy == nullptr)
    {
        return;
    }
    int converted = 0;
    for (int i = 0; i < npc_count; i++)
    {
        npc* np = find_nearest_convert_npc();
        if (np == nullptr)
        {
            break;
        }
        morale_convert_single_npc(np, enemy);
        converted++;
    }
    if (Mission_draft_mode || converted < 1)
    {
        return;
    }
    if (converted == 1)
    {
        hud_message_add_minor(localization_get_display_string_req("HM_GUERRILLA_ASSIST", nullptr), -1.0f, false);
    }
    else
    {
        wchar_t count[10];
        _snprintf_safe(count, 10, L"%d", converted);
        wchar_t text[64];
        _snprintf_safe(text, 64, localization_get_display_string_req("HM_GUERRILLAS_ASSIST", nullptr), count);
        hud_message_add_minor(text, -1.0f, false);
    }
    if (!ui_tool_tips_tip_is_queued("Guerrilla_Action_Complete"))
    {
        ui_tool_tips_add_tip("Guerrilla_Joined", 2000, nullptr);
    }
}

// 0x0075B5F0 (:328)
static bool release_convert(const npc* np)
{
    if (np == nullptr || np->is_killed())
    {
        return true;
    }
    const float dist = Max_guerrilla_conversion_dist * 2.0f;
    return dist * dist < vec_dist_squared(np->pos, player_get_local_player()->pos);
}

// 0x00771840 (:347)
static void release_converts()
{
    int i = 0;
    while (i < All_npcs_converted.size())
    {
        const unsigned int handle = All_npcs_converted[i];
        npc* np = handle != 0xFFFFFFFF ? World.object_from_handle<npc>(handle) : nullptr;
        if (np != nullptr && !release_convert(np))
        {
            i++;
            continue;
        }
        if (np != nullptr)
        {
            np->npc_flags.converted_guerrilla = 0;
            np->start_active_despawn(NPC_DESPAWN_TYPE_LOW, nullptr);
        }
        All_npcs_converted.remove_unsorted(i);
    }
}

// 0x007B3D20 (:368)
static void do_frame_convert()
{
    if (!Multiplayer)
    {
        activity_base* attack = All_activities[ACTIVITY_SLOT_RAID_ATTACK];
        if (attack != nullptr && attack->is_active())
        {
            return;
        }
        activity_base* defend = All_activities[ACTIVITY_SLOT_RAID_DEFEND];
        if (defend != nullptr && defend->is_active())
        {
            return;
        }
    }
    if (Mission_draft_mode)
    {
        release_converts();
    }
    else if (player_get_local_player()->vehicle_handle != 0xFFFFFFFF)
    {
        return;
    }
    const int wanted = get_convert_count() - All_npcs_converted.size();
    if (wanted > 0)
    {
        convert_to_guerrilla(wanted);
    }
}

// 0x00771920 (:409)
void morale_effect_level_init_pre()
{
    reset_encounter_data();
    Mission_draft_mode = false;
    Spontaneous_conversion_cooldown.set_rand(SPONTANEOUS_COOLDOWN_MS, SPONTANEOUS_COOLDOWN_MS);
}

// 0x007B3DC0 (:420)
void morale_effect_do_frame()
{
    if (Multiplayer)
    {
        return;
    }
    if (encounter_active())
    {
        Encounter_started = true;
        Spontaneous_conversion_cooldown.invalidate();
        if (!Encounter_disabled)
        {
            do_frame_convert();
        }
        return;
    }
    Encounter_disabled = false;
    if (Encounter_started)
    {
        Encounter_started = false;
        release_all_converts();
        reset_encounter_data();
    }
    if (!Spontaneous_conversion_cooldown.valid())
    {
        Spontaneous_conversion_cooldown.set_rand(SPONTANEOUS_COOLDOWN_MS, SPONTANEOUS_COOLDOWN_MS);
    }
}

// 0x0075B660 (:467)
void morale_effect_disable_encounter()
{
    Encounter_disabled = true;
}

// 0x00771950 (:473)
void morale_start_draft_mode()
{
    if (Mission_active_index != INVALID_MISSION_HANDLE)
    {
        Mission_draft_mode = true;
    }
}

// 0x0075B670 (:481)
void morale_end_draft_mode()
{
    Mission_draft_mode = false;
}

// 0x007AF310 (:487)
bool morale_npc_can_be_converted(const npc* np)
{
    if (get_convert_count() - All_npcs_converted.size() < 1)
    {
        return false;
    }
    if (np->hflags.do_not_convert_to_guerrilla != 0 || np->is_killed() || np->npc_flags.tied_up != 0 || np->current_team != HUMAN_TEAM_CIVILIAN || np->h_info->default_team != HUMAN_TEAM_GUERILLA || np->get_ai() == nullptr || np->get_ai()->is_despawning() || np->npc_flags.high_priority_despawn != 0 || np->npc_flags.activity_related != 0 || np->npc_flags.mission_related != 0)
    {
        return false;
    }
    return np->vehicle_handle == 0xFFFFFFFF || vehicle_passenger_in_or_entering_seat(np->vehicle_handle, VSI_DRIVER) == np;
}

// 0x00771970 (:539)
static bool collect_conversion_candidates_cb(object* hit_objp, unsigned int shape_key, const void* user_data)
{
    (void)shape_key;
    (void)user_data;
    if (hit_objp == nullptr || hit_objp->get_obj_type() != OT_HUMAN || hit_objp->get_sub_type() != 2)
    {
        return false;
    }
    const npc* np = static_cast<const npc*>(hit_objp);
    return np->current_team == HUMAN_TEAM_CIVILIAN && np->h_info->default_team == HUMAN_TEAM_GUERILLA;
}

// 0x007B3E40 (:566)
int morale_maybe_convert_civilians_spontaneously_around_event(const vector& pos, float radius, const human* enemy, float pct_chance)
{
    farray<npc*, 20> converted;
    const float morale_pct = gfm_get_district_morale(nullptr) / gfm_get_district_morale_max(nullptr);
    // Original quirk kept: nothing in rfg.exe turns on the sound propagation mode, which ends here with nothing done.
    if (Use_sound_propagation_spontaneous_conversion)
    {
        return 0;
    }
    // Magic number: 20 candidates at most.
    unsigned int handles[20];
    const int num_handles = World.collect_objects<npc>(pos, radius, handles, 20, collect_conversion_candidates_cb, nullptr);
    int count = 0;
    for (int i = 0; i < num_handles; i++)
    {
        npc* np = handles[i] != 0xFFFFFFFF ? World.object_from_handle<npc>(handles[i]) : nullptr;
        // Each convert nearby makes it 25 points more likely.
        float chance = morale_pct * pct_chance;
        for (int j = 0; j < converted.size(); j++)
        {
            if (vec_xz_dist_squared(np->pos, converted[j]->pos) <= CONVERT_PEER_DIST_SQRD)
            {
                chance += 0.25f;
            }
        }
        if (!(fl_rand() <= chance))
        {
            continue;
        }
        const object_squad* squad = np->get_ai()->get_squad();
        const bool group = squad != nullptr && squad->member_array.size() > 2;
        // Magic number: a negative chance skips the district's morale roll.
        if (!morale_maybe_convert_civilian_spontaneously(np, enemy, -1.0f))
        {
            continue;
        }
        converted.add(np);
        count++;
        if (group)
        {
            // The whole group follows the first, and the search ends.
            farray<unsigned int, 10> members;
            members.copy(squad->member_array);
            for (int j = 0; j < members.size(); j++)
            {
                npc* member = members[j] != 0xFFFFFFFF ? World.object_from_handle<npc>(members[j]) : nullptr;
                if (member != nullptr && member != np && morale_maybe_convert_civilian_spontaneously(member, enemy, -1.0f))
                {
                    converted.add(member);
                    count++;
                }
            }
            return count;
        }
    }
    return count;
}

// 0x007AF3E0 (:660)
bool morale_maybe_convert_civilian_spontaneously(npc* np, const human* enemy, float pct_chance)
{
    if (!Allow_spontaneous_conversion || Mission_active_index != INVALID_MISSION_HANDLE || !activity_none())
    {
        return false;
    }
    if ((int)Current_alert_level >= 0 && ((int)Current_alert_level >= 4 || (int)Current_alert_level > 0))
    {
        return false;
    }
    if (Spontaneous_conversion_cooldown.valid() && !Spontaneous_conversion_cooldown.elapsed())
    {
        return false;
    }
    if (0.0f <= pct_chance)
    {
        // One roll per npc.
        if (np->npc_flags.attempted_spontaneous_conversion != 0)
        {
            return false;
        }
        np->npc_flags.attempted_spontaneous_conversion = 1;
    }
    if (!human_teams_are_allies(enemy, HUMAN_TEAM_EDF) || !morale_npc_can_be_converted(np))
    {
        return false;
    }
    if (0.0f <= pct_chance)
    {
        const float morale_pct = gfm_get_district_morale(nullptr) / gfm_get_district_morale_max(nullptr);
        // Magic number: nobody joins below half the district's morale.
        if (morale_pct < 0.5f || !fl_rand_chance(morale_pct * pct_chance))
        {
            return false;
        }
    }
    morale_convert_single_npc(np, enemy);
    return true;
}

// 0x00788E60 (:727)
void morale_convert_single_npc(npc* np, const human* enemy_to_target)
{
    np->set_team(HUMAN_TEAM_GUERILLA);
    np->hflags.limited_vehicle_exit = 0;
    np->npc_flags.despawn_npc = 0;
    ai_system* ai = np->get_ai();
    np->npc_flags.ambient_spawn = 0;
    np->npc_flags.converted_guerrilla = 1;
    np->npc_flags.ambient_vehicle_spawn = 0;
    if (enemy_to_target != nullptr)
    {
        ai->perception.force_enemy_knowledge(enemy_to_target);
    }
    ai->planner.request_replan();
    if (ai->is_in_player_squad(true) == nullptr)
    {
        ai->leave_squad();
    }
    All_npcs_converted.add_unique(np->get_handle());
    Cartographer.add_converted_guerrilla(np->get_handle());
    npc_maybe_set_male_guerrilla_anim_set(np);
}

// 0x0075B680 (:770)
bool morale_use_sound_propagation_spontaneous_conversion()
{
    return Use_sound_propagation_spontaneous_conversion;
}
