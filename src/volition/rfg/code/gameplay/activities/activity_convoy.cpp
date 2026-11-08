// Original: volition/rfg/code/gameplay/activities/activity_convoy.cpp
//
// gameflow.md GF4: convoys. A convoy drives from its end point; the player captures the target vehicle and brings it to a
// safehouse, or destroys every vehicle. object_convoy and its end points landed with NPC.md NP1.

#include "volition/rfg/code/gameplay/activities/activity_convoy.h"
#include "volition/rfg/code/gameplay/activities/activity_convoy_internal.h"

#include "keen/base/systemtimer.hpp"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_rewards.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager_internal.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/hud/minimap_icon_info.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_convoy.h"
#include "volition/rfg/code/objects/object_convoy_end_point.h"
#include "volition/rfg/code/objects/object_safehouse.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/turret_info.h"
#include "volition/rfg/code/scripting/dialogue_manager.h"
#include "volition/rfg/code/scripting/objective_highlight.h"
#include "volition/rfg/code/scripting/public_address.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/rand.h"

#include <float.h>
#include <limits.h>
#include <math.h>

activity_convoy_data Convoy_activity_data; // 0x022451E0

static const gfm_orders_flags DEFAULT_ORDERS_FLAGS(true, false); // 0x02270D39, as in gfm_orders_manager.cpp
static int Convoy_victory_wait_ms = 1000; // 0x01651324
static int Convoy_victory_radio_wait_ms = 2000; // 0x01651328
static unsigned int target_vehicle_no_velocity; // 0x02225AE8

static const int MAX_CONVOY_DIFFICULTY_LEVEL = 16; // 0x013115E0
static const float MIN_CONVOY_SPAWN_DIST_SQRD = 22500.0f; // Magic number: 150 metres
static const float CONVOY_ESCAPE_DIST_SQRD = 122500.0f; // Magic number: 350 metres
static const int NUM_CONVOY_SQUADS = 12; // the size of squad_array

// 0x012DAF10
static const char* const Persona_names[NUM_CONVOY_RADIO_LINES] = {
    "OFFCAM", "OFFCAM",
    "PCM01", "ACTIVE", "ACTIVE",
    "PCM01", "ACTIVE", "ACTIVE",
    "ACTIVE", "ACTIVE", "ACTIVE", "ACTIVE",
    "OFFCAM", "OFFCAM", "OFFCAM", "OFFCAM", "OFFCAM", "OFFCAM", "OFFCAM", "OFFCAM",
};

// 0x00780740 (:66)
static bool all_convoys_completed()
{
    for (int i = 0; i < World.get_num_objects<object_convoy_end_point>(); i++)
    {
        if (!World.get_object<object_convoy_end_point>(i)->get_completed_flag())
        {
            return false;
        }
    }
    return true;
}

// 0x00757650 (:82)
static int get_convoy_level(const object_convoy_end_point* end_point)
{
    return end_point->convoy_def != nullptr ? end_point->convoy_def->difficulty_level : 0;
}

// 0x00757670 (:94). rfg.exe: LTCG, convoy_def in ESI. The hours may wrap past midnight; equal hours mean always.
static bool convoy_allowed_to_spawn_tod(const convoy_definition* convoy_def)
{
    const int hour = game_clock_get_time_hour();
    if (convoy_def == nullptr)
    {
        return false;
    }
    const int start = convoy_def->tod_spawn_start_time;
    const int end = convoy_def->tod_spawn_end_time;
    if (start == end)
    {
        return true;
    }
    if (start < end)
    {
        if (hour < start)
        {
            return false;
        }
    }
    else if (hour >= start)
    {
        return true;
    }
    return hour < end;
}

// 0x00780790 (:119). Original: after the last mission, the unfinished convoy of the lowest level, if far enough.
static bool collect_convoy_objects_lowest(farray<object const*, 32>& spawn_objects)
{
    const object_convoy_end_point* lowest = nullptr;
    int lowest_level = INT_MAX;
    for (int i = 0; i < World.get_num_objects<object_convoy_end_point>(); i++)
    {
        const object_convoy_end_point* node = World.get_object<object_convoy_end_point>(i);
        if (!node->get_completed_flag() && get_convoy_level(node) < lowest_level)
        {
            lowest = node;
            lowest_level = get_convoy_level(node);
        }
    }
    const player* pp = lowest != nullptr ? player_get_local_player() : nullptr;
    if (pp == nullptr)
    {
        return false;
    }
    if (convoy_allowed_to_spawn_tod(lowest->convoy_def) && vec_dist_squared(pp->pos, lowest->pos) > MIN_CONVOY_SPAWN_DIST_SQRD)
    {
        spawn_objects.add(lowest);
    }
    return spawn_objects.size() > 0;
}

// 0x007808C0 (:155). Original: the second add has no conditions, so every node goes in, and the good ones twice.
static bool collect_convoy_objects_random(farray<object const*, 32>& spawn_objects)
{
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return false;
    }
    spawn_objects.clear();
    for (int i = 0; i < World.get_num_objects<object_convoy_end_point>(); i++)
    {
        const object_convoy_end_point* node = World.get_object<object_convoy_end_point>(i);
        if (convoy_allowed_to_spawn_tod(node->convoy_def) && vec_dist_squared(pp->pos, node->pos) > MIN_CONVOY_SPAWN_DIST_SQRD)
        {
            spawn_objects.add(node);
        }
        spawn_objects.add(node);
    }
    return spawn_objects.size() > 0;
}

// 0x0076A500 (:196)
static bool vehicle_group_have_no_turrets(const vehicle_spawn_group_info* veh_spawn_group)
{
    for (int i = 0; i < veh_spawn_group->v_info_list.size(); i++)
    {
        const vehicle_info* info = veh_spawn_group->v_info_list[i];
        for (int j = 0; j < info->num_turret_mounts; j++)
        {
            const turret_info* turret = info->turret_mounts[j].attached_turret_info;
            if (turret != nullptr && turret->w_info != nullptr && turret->w_info->max_rounds != 0)
            {
                return false;
            }
        }
    }
    return true;
}

// 0x007576B0 (:222)
static unsigned int get_radio_flag_position(convoy_radio_lines convoy_line)
{
    return (unsigned int)convoy_line < NUM_CONVOY_RADIO_LINES ? 1u << convoy_line : 0;
}

// 0x00780990 (:236). Original: no null check of the player or of a node's definition.
const object_convoy_end_point* find_nearest_convoy_end(int desired_level)
{
    const player* pp = player_get_local_player();
    const object_convoy_end_point* nearest = nullptr;
    float nearest_dist = FLT_MAX;
    for (int i = 0; i < World.get_num_objects<object_convoy_end_point>(); i++)
    {
        const object_convoy_end_point* node = World.get_object<object_convoy_end_point>(i);
        if (node->convoy_def->difficulty_level != desired_level)
        {
            continue;
        }
        const float dist = vec_dist_squared(pp->pos, node->pos);
        if (dist < nearest_dist)
        {
            nearest = node;
            nearest_dist = dist;
        }
    }
    return nearest;
}

// 0x007691D0 (.h:18)
convoy_vehicle_data::convoy_vehicle_data()
{
    reset();
}

// 0x007546B0 (.h:48)
void convoy_vehicle_data::reset()
{
    squad_handle = 0xFFFFFFFF;
    vehicle_handle = 0xFFFFFFFF;
    role = CONVOY_SQUAD_ROLE_UNDEFINED;
    *reinterpret_cast<unsigned int*>(&convoy_veh_flags) = 0;
}

// 0x007546D0 (.h:56)
bool convoy_vehicle_data::in_use() const
{
    return squad_handle != 0xFFFFFFFF;
}

// 0x007546E0 (.h:69)
void convoy_member_flags::reset()
{
    *reinterpret_cast<unsigned int*>(this) = 0;
}

// 0x0077EC20 (.h:80). Original: the rest stays unset until level_init calls reset.
activity_convoy_data::activity_convoy_data()
    : activity_base_modal(ACTIVITY_MODE_CONVOY, true)
{
}

// 0x00769210
activity_convoy_data::~activity_convoy_data()
{
}

// 0x007546F0 (.h:132)
int activity_convoy_data::get_activity_level() const
{
    return activity_level;
}

// 0x007691F0 (.h:161)
int activity_convoy_data::get_num_spawn_vehicles() const
{
    return convoy_def != nullptr ? convoy_def->get_num_vehicles() : 0;
}

// 0x00754700 (.h:170)
bool activity_convoy_data::get_player_in_target_vehicle() const
{
    return convoy_activity_flags.player_in_target_vehicle != 0;
}

// 0x00754710 (.h:255)
const convoy_definition* activity_convoy_data::get_convoy_definition() const
{
    return convoy_def;
}

// 0x007576D0 (:270). Original: each line plays once per convoy.
void activity_convoy_data::play_radio_line(convoy_radio_lines convoy_line, convoy_radio_lines to_follow_line)
{
    const unsigned int flag = get_radio_flag_position(convoy_line); // rfg.exe inlines it
    if ((radio_line_flags & flag) != 0 || (unsigned int)convoy_line >= NUM_CONVOY_RADIO_LINES)
    {
        return;
    }
    const voice_line_handle handle = convoy_def->radio_lines[convoy_line];
    if (handle == INVALID_VOICE_LINE_HANDLE)
    {
        return;
    }
    const human_voice_persona* persona = human_voice_persona_from_name(Persona_names[convoy_line]);
    if (to_follow_line != CONVOY_RADIO_LINE_NONE && convoy_def->radio_lines[to_follow_line] != INVALID_VOICE_LINE_HANDLE)
    {
        dialogue_manager_add_line(handle, persona, convoy_def->radio_lines[to_follow_line], 500, INVALID_PA_HANDLE, nullptr); // Magic number: half a second after
    }
    else
    {
        const char* icon = nullptr;
        if (convoy_line == CONVOY_RL_START_F || convoy_line == CONVOY_RL_START_M)
        {
            icon = get_minimap_bitmap_icon_name(convoy_def->type == CONVOY_TYPE_CAPTURE ? 0xF : 0xE); // Magic numbers: the convoy map elements
        }
        dialogue_manager_start_dialogue(handle, persona, DIALOGUE_PRIORITY_RADIO, INVALID_PA_HANDLE, icon);
    }
    radio_line_flags |= flag;
}

// 0x0076A570 (:304)
void activity_convoy_data::reset()
{
    my_convoy_handle = 0xFFFFFFFF;
    cached_vehicle_handle = 0xFFFFFFFF;
    cached_vehicle_health = 100.0f;
    convoy_end_handle = 0xFFFFFFFF;
    victory_timer.invalidate();
    squad_spawn_timeout.invalidate();
    belief_prop_timer.invalidate();
    orders_update_period.set(0);
    radio_line_flags = 0;
    despawning_squad_list.clear();
    *reinterpret_cast<unsigned int*>(&convoy_activity_flags) = 0;
    for (int i = 0; i < NUM_CONVOY_SQUADS; i++)
    {
        squad_array[i].reset();
    }
    for (int i = 0; i < 4; i++)
    {
        member_flag_status[i].reset();
    }
}

// 0x007577D0 (:328)
void activity_convoy_data::system_init()
{
    responses_are_female = true;
}

// 0x0076A600 (:336)
void activity_convoy_data::level_init()
{
    activity_base_modal::level_init(); // rfg.exe inlines it
    my_convoy_handle = 0xFFFFFFFF;
    activity_level = 1;
    reset();
}

// 0x007B59A0 (:351)
bool activity_convoy_data::activity_start()
{
    set_active_flags(); // rfg.exe inlines it
    release_attack_squads();
    play_radio_line(responses_are_female ? CONVOY_RL_START_F : CONVOY_RL_START_M, CONVOY_RADIO_LINE_NONE);
    if (!activity_base_modal::activity_start())
    {
        return false;
    }
    ui_tool_tips_add_tip("Convoy", 2000, nullptr); // Magic number: two seconds in
    return true;
}

// 0x00796D10 (:370)
void activity_convoy_data::activity_end()
{
    activity_base_modal::activity_end(); // rfg.exe inlines it
    World.destroy_object(my_convoy_handle, false);
    reset();
}

// 0x00796D40 (:382)
void activity_convoy_data::activity_kill()
{
    activity_base_modal::activity_kill(); // rfg.exe inlines it
    World.destroy_object(my_convoy_handle, false);
    if (convoy_def != nullptr)
    {
        dialogue_manager_cancel(convoy_def->radio_lines[responses_are_female ? 0 : 1], true);
    }
    Cartographer.remove_player_marker(true);
    reset();
}

// 0x0076A660 (:403)
void activity_convoy_data::reinstate(activity_disable_reason disable_reason)
{
    activity_base::reinstate(disable_reason); // rfg.exe inlines it
    reset();
}

// 0x0076A6A0 (:413). Original: the target's crew gets out when the vehicle stands still for two seconds or is damaged.
void activity_convoy_data::process_target_driver_flags(const vehicle* target_vehicle)
{
    if (target_vehicle == nullptr || convoy_def == nullptr || convoy_def->type != CONVOY_TYPE_CAPTURE)
    {
        return;
    }
    for (int i = 0; i < NUM_CONVOY_SQUADS; i++)
    {
        const convoy_vehicle_data& current = squad_array[i];
        if (current.squad_handle == 0xFFFFFFFF || current.role != CONVOY_SQUAD_ROLE_TARGET)
        {
            continue;
        }
        object_squad* squad = World.object_from_handle<object_squad>(current.squad_handle);
        if (squad == nullptr)
        {
            continue;
        }
        bool stopped = false;
        float speed = 0.0f;
        if (vehicle_get_speed(target_vehicle, speed) && speed == 0.0f)
        {
            if (target_vehicle_no_velocity == 0)
            {
                target_vehicle_no_velocity = keen::SystemTimer::getCurrentMilliseconds();
            }
            if (keen::SystemTimer::getCurrentMilliseconds() > target_vehicle_no_velocity + 2000) // Magic number: two seconds
            {
                stopped = true;
            }
        }
        else if (target_vehicle_no_velocity != 0)
        {
            target_vehicle_no_velocity = 0;
        }
        if (target_vehicle->get_vehicle_damage() >= 0.3f || stopped) // Magic number: 30% damage
        {
            for (int j = 0; j < squad->member_array.size(); j++)
            {
                npc* np = squad->member_array[j] != 0xFFFFFFFF ? World.object_from_handle<npc>(squad->member_array[j]) : nullptr;
                if (np != nullptr)
                {
                    np->hflags.disallow_vehicle_exit = 0;
                    np->hflags.limited_vehicle_exit = 0;
                    np->hflags.driverless_exit_only = 0;
                }
            }
        }
        else
        {
            // Deviation: rfg.exe ORs these into an uninitialized word, so convoy_vehicle_exit is whatever the stack held.
            convoy_member_flags flags;
            flags.reset();
            flags.limited_vehicle_exit = 1;
            flags.disallow_vehicle_exit = 1;
            set_member_flags(flags, squad->hflags);
        }
    }
}

// 0x00796DA0 (:490). Original: no null check of the player.
void activity_convoy_data::activity_do_frame_active_capture()
{
    update_cached_vehicle_handle();
    update_target_vehicle_driver();
    update_player_in_target_vehicle();
    const player* pp = player_get_local_player();
    vehicle* target_vehicle = cached_vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(cached_vehicle_handle) : nullptr;
    if (target_vehicle != nullptr && vec_dist_squared(pp->pos, target_vehicle->pos) > CONVOY_ESCAPE_DIST_SQRD)
    {
        gamelog_set_activity_failed_reason(GLCR_ESCAPED);
        capture_convoy_escaped();
        return;
    }
    process_target_driver_flags(target_vehicle);
    if (convoy_target_destroyed())
    {
        gamelog_set_activity_failed_reason(GLCR_VEHICLE_DESTROYED);
        capture_convoy_destroyed();
        if (Cartographer.has_mission_marker())
        {
            Cartographer.remove_player_marker(true);
        }
        return;
    }
    if (convoy_activity_flags.player_in_target_vehicle && safehouse_contains_player())
    {
        target_vehicle_delivered();
        return;
    }
    if (!convoy_activity_flags.player_in_target_vehicle && target_vehicle != nullptr)
    {
        objective_highlight_add(target_vehicle->get_handle(), HIGHLIGHT_PURPOSE_NEUTRAL, HIGHLIGHT_STYLE_DEFAULT);
    }
}

// 0x007577E0 (:532)
bool activity_convoy_data::any_convoy_squad_alive() const
{
    for (int i = 0; i < NUM_CONVOY_SQUADS; i++)
    {
        if (squad_array[i].squad_handle != 0xFFFFFFFF && !squad_array[i].convoy_veh_flags.squad_killed)
        {
            return true;
        }
    }
    return false;
}

// 0x007A0E00 (:552). Original: no null check of the player.
void activity_convoy_data::activity_do_frame_active_destroy()
{
    const player* pp = player_get_local_player();
    int vehicles_left = 0;
    for (int i = 0; i < NUM_CONVOY_SQUADS; i++)
    {
        const convoy_vehicle_data& current = squad_array[i];
        if (current.squad_handle == 0xFFFFFFFF || current.convoy_veh_flags.vehicle_killed)
        {
            continue;
        }
        vehicles_left++;
        const vehicle* vp = current.vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(current.vehicle_handle) : nullptr;
        if (vp == nullptr)
        {
            if (current.convoy_veh_flags.squad_killed)
            {
                gamelog_set_activity_failed_reason(GLCR_ESCAPED);
                destruction_convoy_escaped(false); // rfg.exe inlines it
                return;
            }
        }
        else if (vec_dist_squared(vp->pos, pp->pos) > CONVOY_ESCAPE_DIST_SQRD)
        {
            gamelog_set_activity_failed_reason(GLCR_ESCAPED);
            destruction_convoy_escaped(any_convoy_squad_alive()); // rfg.exe inlines both
            return;
        }
    }
    if (vehicles_left == 0)
    {
        destruction_convoy_destroyed();
    }
}

// 0x00780A50 (:607)
bool activity_convoy_data::activity_begin_start_up()
{
    if (!activity_base_modal::activity_begin_start_up())
    {
        return false;
    }
    object_convoy* convoy = get_convoy(); // rfg.exe inlines it
    if (convoy != nullptr)
    {
        convoy->set_attacked_flag(); // rfg.exe inlines it
    }
    return true;
}

// 0x0076A840 (:625)
bool activity_convoy_data::activity_clean_up_completed() const
{
    return activity_base_modal::activity_clean_up_completed() && despawning_squad_list.size() < 1; // rfg.exe inlines the base
}

// 0x007AD020 (:635). Original: an npc still despawning who has the player as an enemy raises a red alert.
void activity_convoy_data::activity_do_frame_clean_up()
{
    activity_base::activity_do_frame_clean_up(); // rfg.exe inlines it
    if (despawning_squad_list.size() <= 0)
    {
        return;
    }
    for (int i = 0; i < despawning_squad_list.size();)
    {
        const object_squad* squad = despawning_squad_list[i] != 0xFFFFFFFF ? World.object_from_handle<object_squad>(despawning_squad_list[i]) : nullptr;
        if (squad == nullptr || squad->is_killed() || squad->member_array.size() < 1)
        {
            despawning_squad_list.remove_unsorted(i);
        }
        else
        {
            i++;
        }
    }
    for (int i = 0; i < despawning_squad_list.size(); i++)
    {
        const object_squad* squad = despawning_squad_list[i] != 0xFFFFFFFF ? World.object_from_handle<object_squad>(despawning_squad_list[i]) : nullptr;
        if (squad == nullptr)
        {
            continue;
        }
        for (int j = 0; j < squad->member_array.size(); j++)
        {
            npc* np = squad->member_array[j] != 0xFFFFFFFF ? World.object_from_handle<npc>(squad->member_array[j]) : nullptr;
            if (np != nullptr && np->get_ai()->perception.has_player_enemy_belief())
            {
                gsm_set_alert_value(Alert_level_ranges[ALERT_LEVEL_RED].min_value);
                despawning_squad_list.clear();
                return;
            }
        }
    }
}

// 0x007B1B40 (:686)
void activity_convoy_data::activity_do_frame_active()
{
    update_squad_status();
    update_squad_vehicles();
    update_npc_beliefs();
    update_squad_orders();
    if (convoy_def != nullptr && convoy_def->type == CONVOY_TYPE_CAPTURE)
    {
        activity_do_frame_active_capture();
    }
    else if (convoy_def != nullptr && convoy_def->type == CONVOY_TYPE_DESTROY)
    {
        activity_do_frame_active_destroy();
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
        activity_kill();
    }
}

// 0x0076A890 (:707). The set_*_flags functions below inline this whole. Original: a squad with no turrets may always get
// out, but for the capture target.
void activity_convoy_data::update_all_member_flags()
{
    for (int i = 0; i < NUM_CONVOY_SQUADS; i++)
    {
        const convoy_vehicle_data& current = squad_array[i];
        object_squad* squad = current.squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(current.squad_handle) : nullptr;
        if (squad == nullptr || current.role < 0 || current.role >= 4)
        {
            continue;
        }
        convoy_member_flags flags = member_flag_status[current.role];
        if (current.convoy_veh_flags.no_vehicle_turret)
        {
            const convoy_type type = get_convoy_type();
            if (type == CONVOY_TYPE_CAPTURE)
            {
                if (current.role != 0) // Magic number: rfg.exe tests the role register against zero
                {
                    flags.limited_vehicle_exit = 0;
                    flags.disallow_vehicle_exit = 0;
                }
            }
            else if (type == CONVOY_TYPE_DESTROY)
            {
                flags.limited_vehicle_exit = 0;
                flags.disallow_vehicle_exit = 0;
            }
            else
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
                activity_kill();
            }
        }
        set_member_flags(flags, squad->hflags);
        set_member_flags(flags, squad->vflags);
        vehicle* vp = squad->squad_vehicle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(squad->squad_vehicle) : nullptr;
        if (vp != nullptr)
        {
            set_member_flags(flags, vp->m_flags);
        }
        if (squad->is_killed() || squad->simulated())
        {
            continue;
        }
        for (int j = 0; j < squad->member_array.size(); j++)
        {
            npc* np = squad->member_array[j] != 0xFFFFFFFF ? World.object_from_handle<npc>(squad->member_array[j]) : nullptr;
            if (np != nullptr)
            {
                set_member_flags(flags, np->hflags);
            }
        }
    }
}

// 0x00757810 (:774). Original: disable_for_player does not reach the vehicle.
void activity_convoy_data::set_member_flags(const convoy_member_flags& flag_status, vehicle_flags& vflags)
{
    vflags.activity = 1;
    vflags.m_scripted_abandon = flag_status.m_scripted_abandon;
    vflags.player_may_capture = flag_status.player_may_capture;
}

// 0x00757850 (:782)
void activity_convoy_data::set_member_flags(const convoy_member_flags& flag_status, human_flags& hflags)
{
    hflags.convoy_vehicle_exit = flag_status.convoy_vehicle_exit;
    hflags.driverless_exit_only = flag_status.limited_vehicle_exit;
    hflags.limited_vehicle_exit = flag_status.limited_vehicle_exit;
    hflags.disallow_vehicle_exit = flag_status.disallow_vehicle_exit;
}

// 0x00780A90 (:791)
void activity_convoy_data::set_ready_flags()
{
    if (convoy_def != nullptr && convoy_def->type == CONVOY_TYPE_CAPTURE)
    {
        set_ready_flags_capture();
    }
    else if (convoy_def != nullptr && convoy_def->type == CONVOY_TYPE_DESTROY)
    {
        set_ready_flags_destroy();
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
        activity_kill();
    }
}

// 0x0076AAD0 (:803). Original: before the fight the crews stay in, and only the target can be taken.
void activity_convoy_data::set_ready_flags_capture()
{
    for (int i = 0; i < 4; i++)
    {
        member_flag_status[i].reset();
        member_flag_status[i].disallow_vehicle_exit = 1;
    }
    member_flag_status[0].player_may_capture = 1; // Magic number: role 0, the target
    update_all_member_flags(); // rfg.exe inlines it
}

// 0x0076AD40 (:819)
void activity_convoy_data::set_ready_flags_destroy()
{
    for (int i = 0; i < 4; i++)
    {
        member_flag_status[i].reset();
        member_flag_status[i].disable_for_player = 1;
        member_flag_status[i].disallow_vehicle_exit = 1;
    }
    update_all_member_flags(); // rfg.exe inlines it
}

// 0x00780AE0 (:834)
void activity_convoy_data::set_active_flags()
{
    if (convoy_def != nullptr && convoy_def->type == CONVOY_TYPE_CAPTURE)
    {
        set_active_flags_capture();
    }
    else if (convoy_def != nullptr && convoy_def->type == CONVOY_TYPE_DESTROY)
    {
        set_active_flags_destroy();
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
        activity_kill();
    }
}

// 0x0076AFB0 (:846)
void activity_convoy_data::set_active_flags_capture()
{
    for (int i = 0; i < 4; i++)
    {
        member_flag_status[i].reset();
        member_flag_status[i].limited_vehicle_exit = 1;
        member_flag_status[i].convoy_vehicle_exit = 1;
    }
    // Read from the asm (0x0076AFEC): the target's entry is cleared again, so its crew stays limited by nothing.
    member_flag_status[0].reset();
    member_flag_status[0].player_may_capture = 1;
    member_flag_status[0].m_scripted_abandon = 1;
    update_all_member_flags(); // rfg.exe inlines it
}

// 0x0076B230 (:864)
void activity_convoy_data::set_active_flags_destroy()
{
    for (int i = 0; i < 4; i++)
    {
        member_flag_status[i].reset();
        member_flag_status[i].limited_vehicle_exit = 1;
        member_flag_status[i].disable_for_player = 1;
        member_flag_status[i].convoy_vehicle_exit = 1;
    }
    update_all_member_flags(); // rfg.exe inlines it
}

// 0x00780B30 (:880). Original: with no one left driving, every squad breaks off and attacks.
void activity_convoy_data::update_target_vehicle_driver()
{
    const vehicle* vp = cached_vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(cached_vehicle_handle) : nullptr;
    if (vp == nullptr || !vp->streamed())
    {
        return;
    }
    const object_squad* squad = get_target_squad();
    if (squad == nullptr || squad->member_array.size() == 0)
    {
        return;
    }
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        const human* hp = squad->member_array[i] != 0xFFFFFFFF ? World.object_from_handle<npc>(squad->member_array[i]) : nullptr;
        if (hp != nullptr && !hp->is_killed() && hp->vehicle_handle == vp->get_handle())
        {
            const vehicle_seat_index seat = vehicle_passenger_get_seat(vp, hp);
            if (seat == VSI_DRIVER || seat == VSI_INVALID)
            {
                return;
            }
        }
    }
    convert_all_squads(); // rfg.exe inlines it
    if (!is_active())
    {
        activity_begin_start_up();
    }
}

// 0x0076B4A0 (:944)
object_squad* activity_convoy_data::get_target_squad() const
{
    if (convoy_def == nullptr || convoy_def->type != CONVOY_TYPE_CAPTURE)
    {
        return nullptr;
    }
    for (int i = 0; i < NUM_CONVOY_SQUADS; i++)
    {
        if (squad_array[i].convoy_veh_flags.capture_vehicle)
        {
            return squad_array[i].squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(squad_array[i].squad_handle) : nullptr;
        }
    }
    return nullptr;
}

// 0x0076B4F0 (:964). Original: a hit within twice a vehicle's radius starts the fight; rfg_mover_handle goes unused.
void activity_convoy_data::register_player_hit(const vector& hit_location, unsigned int rfg_mover_handle)
{
    if (convoy_activity_flags.start_next_frame)
    {
        return;
    }
    for (int i = 0; i < NUM_CONVOY_SQUADS; i++)
    {
        const object_squad* squad = squad_array[i].squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(squad_array[i].squad_handle) : nullptr;
        if (squad == nullptr || squad->squad_vehicle == 0xFFFFFFFF)
        {
            continue;
        }
        const vehicle* vp = World.object_from_handle<vehicle>(squad->squad_vehicle);
        if (vp == nullptr)
        {
            continue;
        }
        const float radius = vp->info->spawn_size * 2.0f; // Magic number: twice the vehicle's size
        if (vec_dist_squared(vp->pos, hit_location) < radius * radius)
        {
            convoy_activity_flags.start_next_frame = 1;
        }
    }
}

// 0x0076B5D0 (:1003)
void activity_convoy_data::update_cached_vehicle_handle()
{
    if (cached_vehicle_handle != 0xFFFFFFFF && World.object_from_handle<vehicle>(cached_vehicle_handle) != nullptr)
    {
        return;
    }
    const object_squad* squad = get_target_squad();
    if (squad != nullptr && squad->squad_vehicle != 0xFFFFFFFF)
    {
        const vehicle* vp = World.object_from_handle<vehicle>(squad->squad_vehicle);
        if (vp != nullptr)
        {
            cached_vehicle_handle = vp->get_handle();
        }
    }
}

// 0x0076B630 (:1027)
bool activity_convoy_data::convoy_was_attacked(const object_convoy* my_convoy)
{
    if (my_convoy == nullptr)
    {
        return false;
    }
    if (my_convoy->attacked())
    {
        return true;
    }
    for (int i = 0; i < NUM_CONVOY_SQUADS; i++)
    {
        if (squad_array[i].squad_handle == 0xFFFFFFFF)
        {
            continue;
        }
        if (squad_array[i].convoy_veh_flags.squad_killed)
        {
            return true;
        }
        const object_squad* squad = World.object_from_handle<object_squad>(squad_array[i].squad_handle);
        if (squad == nullptr)
        {
            continue;
        }
        for (int j = 0; j < squad->member_array.size(); j++)
        {
            npc* np = World.object_from_handle<npc>(squad->member_array[j]);
            if (np != nullptr && np->get_ai()->perception.has_player_enemy_belief())
            {
                return true;
            }
        }
    }
    return false;
}

// 0x00780C50 (:1078). Original: of the three progress lines, only the halfway one plays.
void activity_convoy_data::activity_do_frame_ready()
{
    activity_base::activity_do_frame_ready();
    object_convoy* convoy = get_convoy(); // rfg.exe inlines it
    if (convoy == nullptr)
    {
        activity_kill();
        return;
    }
    update_squad_status();
    update_squad_vehicles();
    update_cached_vehicle_handle();
    update_target_vehicle_driver();
    update_player_in_target_vehicle();
    if (convoy_was_attacked(convoy) || convoy_activity_flags.start_next_frame)
    {
        play_radio_line(responses_are_female ? CONVOY_RL_ESCAPING_VEHICLE_F : CONVOY_RL_ESCAPING_VEHICLE_M, CONVOY_RADIO_LINE_NONE);
        activity_begin_start_up();
    }
    const int visited = convoy->get_next_visited_index();
    if (distance_25_message_node_index <= visited)
    {
        distance_25_message_node_index = INT_MAX;
    }
    else if (distance_50_message_node_index <= visited)
    {
        play_radio_line(responses_are_female ? CONVOY_RL_DISTANCE_50_F : CONVOY_RL_DISTANCE_50_M, CONVOY_RADIO_LINE_NONE);
        distance_50_message_node_index = INT_MAX;
    }
    else if (distance_75_message_node_index <= visited)
    {
        distance_75_message_node_index = INT_MAX;
    }
}

// 0x00780D40 (:1118). Original: no null check of a definition.
const object_convoy_end_point* activity_convoy_data::pick_start_position()
{
    for (int i = 0; i < World.get_num_objects<object_convoy_end_point>(); i++)
    {
        const object_convoy_end_point* node = World.get_object<object_convoy_end_point>(i);
        if (node->convoy_def->difficulty_level == activity_level)
        {
            return node;
        }
    }
    return nullptr;
}

// 0x00780DA0 (:1134). Original: player_pos and the district are ignored. A convoy's level goes up from the finished ones
// and the liberated districts; the last end point in the list decides.
bool activity_convoy_data::collect_spawn_objects(farray<object const*, 32>& spawn_objects, const vector& player_pos, unsigned int district_handlep) const
{
    spawn_objects.clear();
    const bool all_completed = all_convoys_completed();
    if (mission_final_mission_complete())
    {
        return all_completed ? collect_convoy_objects_random(spawn_objects) : collect_convoy_objects_lowest(spawn_objects);
    }
    if (all_completed)
    {
        spawn_objects.clear();
        return false;
    }
    // rfg.exe writes activity_level from a const method.
    activity_convoy_data* self = const_cast<activity_convoy_data*>(this);
    self->activity_level = 1;
    for (int i = 0; i < World.get_num_objects<object_convoy_end_point>(); i++)
    {
        const object_convoy_end_point* node = World.get_object<object_convoy_end_point>(i);
        const unsigned int home = node->get_home_district_handle();
        const district* dp = home != 0xFFFFFFFF ? World.object_from_handle<district>(home) : nullptr;
        if (dp != nullptr && (node->get_completed_flag() || dp->is_liberated()))
        {
            self->activity_level = get_convoy_level(node) + 1;
        }
    }
    const player* pp = player_get_local_player();
    for (int i = 0; i < World.get_num_objects<object_convoy_end_point>(); i++)
    {
        object_convoy_end_point* node = World.get_object<object_convoy_end_point>(i);
        // Original: no null check of the definition before its level, nor of the player.
        if (!node->get_completed_flag() && !node->is_suspended_or_disabled() && activity_level == node->convoy_def->difficulty_level && convoy_allowed_to_spawn_tod(node->convoy_def) && vec_dist_squared(pp->pos, node->pos) > MIN_CONVOY_SPAWN_DIST_SQRD)
        {
            spawn_objects.add(node);
        }
    }
    return spawn_objects.size() > 0;
}

// 0x007B5A40 (:1203). Original: a missing end point skips the completed flag and the rewards.
void activity_convoy_data::record_victory()
{
    ui_tool_tips_add_tip("Guerrilla_Action_Complete", 4000, nullptr); // Magic number: four seconds in
    object_convoy_end_point* end_point = convoy_end_handle != 0xFFFFFFFF ? World.object_from_handle<object_convoy_end_point>(convoy_end_handle) : nullptr;
    if (end_point != nullptr)
    {
        end_point->set_completed_flag(true);
        if (convoy_def != nullptr)
        {
            const unsigned int home = end_point->get_home_district_handle();
            district* dp = home != 0xFFFFFFFF ? World.object_from_handle<district>(home) : nullptr;
            apply_standard_rewards(convoy_def->standard_rewards, dp);
        }
    }
    activity_base_modal::record_victory();
}

// 0x00796EA0 (:1233)
bool activity_convoy_data::activity_initialize(const object* spawn_objp, unsigned int new_district_handle)
{
    if (spawn_objp == nullptr || spawn_objp->get_obj_type() != OT_CONVOY_END || !activity_base_modal::activity_initialize(spawn_objp, new_district_handle))
    {
        return false;
    }
    reset();
    convoy_end_handle = spawn_objp->get_handle();
    responses_are_female = vrand() % 100 < 51; // Magic number: rfg.exe compares with 0x33
    const bool spawned = spawn_convoy(static_cast<const object_convoy_end_point*>(spawn_objp));
    set_ready_flags();
    return spawned;
}

// 0x0076B6E0 (:1262)
void activity_convoy_data::find_message_points(const object_convoy* convoy)
{
    const int route = convoy->convoy_route.size();
    distance_50_message_node_index = route / 2;
    distance_25_message_node_index = route / 4;
    distance_75_message_node_index = route / 2 + route / 4;
}

// 0x00780FA0 (:1274). Original: asks the global instance, which is the only one.
bool activity_convoy_data::spawn_convoy(const object_convoy_end_point* start_point)
{
    if (Convoy_activity_data.is_disabled() || start_point == nullptr)
    {
        return false;
    }
    convoy_def = start_point->convoy_def;
    if (convoy_def == nullptr)
    {
        return false;
    }
    object_convoy* convoy = convoy_create(start_point, convoy_def, nullptr);
    if (convoy == nullptr)
    {
        return false;
    }
    squad_spawn_timeout.set(2000); // Magic number: two seconds
    belief_prop_timer.set(0);
    my_convoy_handle = convoy->get_handle();
    build_squad_list(convoy);
    play_radio_line(responses_are_female ? CONVOY_RL_START_F : CONVOY_RL_START_M, CONVOY_RADIO_LINE_NONE);
    find_message_points(convoy);
    return true;
}

// 0x0076B720 (:1317)
void activity_convoy_data::build_squad_list(const object_convoy* convoy)
{
    if (convoy == nullptr)
    {
        return;
    }
    for (int i = 0; i < NUM_CONVOY_SQUADS; i++)
    {
        convoy_vehicle_data& current = squad_array[i];
        current.reset();
        const convoy_member_info* info = convoy->get_convoy_member_info(i);
        if (info == nullptr || info->squad_handle == 0xFFFFFFFF)
        {
            continue;
        }
        const object_squad* squad = World.object_from_handle<object_squad>(info->squad_handle);
        if (squad == nullptr)
        {
            continue;
        }
        current.squad_handle = info->squad_handle;
        current.role = info->squad_info != nullptr ? info->squad_info->role : CONVOY_SQUAD_ROLE_UNDEFINED;
        if (get_convoy_type() == CONVOY_TYPE_CAPTURE)
        {
            current.convoy_veh_flags.capture_vehicle = current.role == CONVOY_SQUAD_ROLE_TARGET;
        }
        current.convoy_veh_flags.no_vehicle_turret = vehicle_group_have_no_turrets(squad->veh_spawn_group);
    }
}

// 0x0076B7F0 (:1373). Original: every so often, an npc with an empty belief bin 1 (+0x230 of its ai) learns about the
// player. The bin's meaning is inferred from this use.
void activity_convoy_data::update_npc_beliefs()
{
    if (!belief_prop_timer.elapsed())
    {
        return;
    }
    belief_prop_timer.set_rand(1000, 3000); // Magic numbers: one to three seconds
    const player* pp = player_get_local_player();
    for (int i = 0; i < NUM_CONVOY_SQUADS; i++)
    {
        const object_squad* squad = squad_array[i].squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(squad_array[i].squad_handle) : nullptr;
        if (squad == nullptr)
        {
            continue;
        }
        for (int j = 0; j < squad->member_array.size(); j++)
        {
            npc* np = squad->member_array[j] != 0xFFFFFFFF ? World.object_from_handle<npc>(squad->member_array[j]) : nullptr;
            if (np != nullptr && np->get_ai()->perception.belief_bins[1].beliefs == nullptr)
            {
                np->get_ai()->perception.alert_enemy(nullptr, pp);
            }
        }
    }
}

// 0x007B1BB0 (:1420)
void activity_convoy_data::release_attack_squads()
{
    object_convoy* convoy = get_convoy(); // rfg.exe inlines it
    if (convoy == nullptr)
    {
        return;
    }
    for (int i = 0; i < NUM_CONVOY_SQUADS; i++)
    {
        if (squad_array[i].squad_handle != 0xFFFFFFFF && squad_array[i].role == CONVOY_SQUAD_ROLE_BREAK_AND_ATTACK)
        {
            convoy->leave_convoy_formation(squad_array[i].squad_handle);
            update_squad_orders_attack(squad_array[i]);
        }
    }
}

// 0x007AD180 (:1449). Original: no null check of the player.
void activity_convoy_data::update_squad_orders_attack(convoy_vehicle_data& current_squad)
{
    object_squad* squad = current_squad.squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(current_squad.squad_handle) : nullptr;
    if (squad == nullptr || squad->is_killed())
    {
        return;
    }
    const player* pp = player_get_local_player();
    gfm_orders* orders = squad->get_squad_orders();
    if (orders == nullptr || orders->get_orders_type() != GFM_ORDERS_TYPE_ATTACK)
    {
        gfm_orders_creation_params params;
        params.ofp = DEFAULT_ORDERS_FLAGS;
        params.orders_type = GFM_ORDERS_TYPE_ATTACK;
        params.team = squad->get_squad_team();
        params.location_target = pp->pos;
        params.object_target = pp->get_handle();
        const unsigned int new_orders = Orders_manager.create_new_orders(params); // rfg.exe inlines it
        if (new_orders == 0xFFFFFFFF)
        {
            return;
        }
        orders = Orders_manager.get_orders_from_handle(new_orders);
        if (orders == nullptr)
        {
            return;
        }
        squad->assign_orders(new_orders);
    }
    orders->location_target = pp->pos;
}

// 0x007AD2C0 (:1489)
void activity_convoy_data::update_squad_orders()
{
    if (!orders_update_period.elapsed())
    {
        return;
    }
    orders_update_period.set_rand(1000, 2000); // Magic numbers: one to two seconds
    for (int i = 0; i < NUM_CONVOY_SQUADS; i++)
    {
        if (squad_array[i].squad_handle != 0xFFFFFFFF && squad_array[i].role == CONVOY_SQUAD_ROLE_BREAK_AND_ATTACK)
        {
            update_squad_orders_attack(squad_array[i]);
        }
    }
}

// 0x0076B8C0 (:1515). Original: a squad that dies unsimulated takes its vehicle with it, if the vehicle is gone or dead.
void activity_convoy_data::update_squad_status()
{
    for (int i = 0; i < NUM_CONVOY_SQUADS; i++)
    {
        convoy_vehicle_data& current = squad_array[i];
        if (current.squad_handle == 0xFFFFFFFF || current.convoy_veh_flags.squad_killed)
        {
            continue;
        }
        const object_squad* squad = World.object_from_handle<object_squad>(current.squad_handle);
        if (squad != nullptr && !squad->is_killed())
        {
            current.convoy_veh_flags.simulated_last_frame = squad->simulated();
            continue;
        }
        current.convoy_veh_flags.squad_killed = 1;
        if (!current.convoy_veh_flags.simulated_last_frame)
        {
            const vehicle* vp = current.vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(current.vehicle_handle) : nullptr;
            if (vp == nullptr || vp->is_killed())
            {
                current.convoy_veh_flags.vehicle_killed = 1;
            }
        }
    }
}

// 0x0076B960 (:1554)
void activity_convoy_data::update_squad_vehicles()
{
    for (int i = 0; i < NUM_CONVOY_SQUADS; i++)
    {
        convoy_vehicle_data& current = squad_array[i];
        if (current.squad_handle == 0xFFFFFFFF || current.convoy_veh_flags.vehicle_killed)
        {
            continue;
        }
        const object_squad* squad = World.object_from_handle<object_squad>(current.squad_handle);
        if (squad != nullptr)
        {
            if (squad->simulated())
            {
                if (current.vehicle_handle != 0xFFFFFFFF && World.object_from_handle<vehicle>(current.vehicle_handle) == nullptr)
                {
                    current.vehicle_handle = 0xFFFFFFFF;
                }
                continue;
            }
            if (current.vehicle_handle == 0xFFFFFFFF)
            {
                current.vehicle_handle = squad->squad_vehicle;
            }
        }
        const vehicle* vp = current.vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(current.vehicle_handle) : nullptr;
        if (vp != nullptr && vp->is_killed())
        {
            current.convoy_veh_flags.vehicle_killed = 1;
        }
    }
}

// 0x0076BA10 (:1616). Original: no null check of the player.
void activity_convoy_data::update_player_in_target_vehicle()
{
    if (convoy_def == nullptr || convoy_def->type != CONVOY_TYPE_CAPTURE || cached_vehicle_handle == 0xFFFFFFFF)
    {
        return;
    }
    const vehicle* vp = World.object_from_handle<vehicle>(cached_vehicle_handle);
    if (vp == nullptr)
    {
        return;
    }
    convoy_activity_flags.player_in_target_vehicle = human_in_vehicle_interaction(player_get_local_player(), vp);
    if (!convoy_activity_flags.player_in_target_vehicle)
    {
        return;
    }
    if (!is_active())
    {
        activity_begin_start_up();
    }
    if (!convoy_activity_flags.played_capture_lines)
    {
        convoy_activity_flags.played_capture_lines = 1;
        // rfg.exe inlines the first and names the persona.
        play_radio_line(CONVOY_RL_CAPTURED_VEHICLE, CONVOY_RADIO_LINE_NONE);
        play_radio_line(responses_are_female ? CONVOY_RL_CAPTURED_VEHICLE_RES_F : CONVOY_RL_CAPTURED_VEHICLE_RES_M, CONVOY_RL_CAPTURED_VEHICLE);
    }
}

// 0x00781040 (:1651)
const object* activity_convoy_data::get_target_object() const
{
    const vehicle* vp = cached_vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(cached_vehicle_handle) : nullptr;
    if (vp != nullptr)
    {
        return vp;
    }
    const object_convoy* convoy = get_convoy();
    return convoy != nullptr ? convoy->get_primary_target_squad() : nullptr;
}

// 0x00781090 (:1677)
void activity_convoy_data::render_icons() const
{
    const object_convoy_end_point* end_point = convoy_end_handle != 0xFFFFFFFF ? World.object_from_handle<object_convoy_end_point>(convoy_end_handle) : nullptr;
    const color& icon_color = end_point != nullptr && end_point->get_completed_flag() ? Completed_icon_color : Default_map_item_color;
    if (convoy_def == nullptr || convoy_def->type != CONVOY_TYPE_CAPTURE)
    {
        for (int i = 0; i < NUM_CONVOY_SQUADS; i++)
        {
            const convoy_vehicle_data& current = squad_array[i];
            if (current.squad_handle == 0xFFFFFFFF || current.convoy_veh_flags.vehicle_killed)
            {
                continue;
            }
            const object* target = current.vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(current.vehicle_handle) : nullptr;
            if (target == nullptr)
            {
                target = World.object_from_handle<object_squad>(current.squad_handle);
            }
            if (target != nullptr)
            {
                Cartographer.add_temporary_icon(target->pos, MAP_ELEMENT_CONVOY_ICON, true, target->get_handle(), icon_color, false, get_spawn_object_handle());
            }
        }
        return;
    }
    if (!convoy_activity_flags.player_in_target_vehicle)
    {
        const object* target = get_target_object();
        if (target != nullptr)
        {
            Cartographer.add_temporary_icon(target->pos, MAP_ELEMENT_CONVOY_CAPTURE_ICON, true, target->get_handle(), icon_color, false, get_spawn_object_handle());
        }
        return;
    }
    // Driving the prize: mark the nearest safehouse. Original: no null check of the player.
    const object_safehouse* safehouse = safehouse_find_nearest_safehouse(player_get_local_player()->pos, false, false);
    if (safehouse != nullptr && (!Cartographer.has_player_marker() || !convoy_activity_flags.added_safehouse_marker) && !Cartographer.has_mission_marker())
    {
        Cartographer.add_player_marker(safehouse->pos, true, 0xFFFFFFFF, false, false);
        // rfg.exe writes this flag from a const method.
        const_cast<activity_convoy_data*>(this)->convoy_activity_flags.added_safehouse_marker = 1;
    }
}

// 0x007578A0 (:1753)
bool activity_convoy_data::render_hud(int x_pos, int y_pos)
{
    return false;
}

// 0x007578B0 (:1761)
bool activity_convoy_data::vehicle_in_convoy(const vehicle* vp)
{
    if (vp != nullptr)
    {
        for (int i = 0; i < NUM_CONVOY_SQUADS; i++)
        {
            if (squad_array[i].vehicle_handle == vp->get_handle())
            {
                return true;
            }
        }
    }
    return false;
}

// 0x0076BB10 (:1780). Original: a vehicle not spawned yet counts while its simulated squad should have one.
int activity_convoy_data::get_num_vehicles() const
{
    int count = 0;
    for (int i = 0; i < NUM_CONVOY_SQUADS; i++)
    {
        const convoy_vehicle_data& current = squad_array[i];
        if (current.squad_handle == 0xFFFFFFFF || current.convoy_veh_flags.vehicle_killed)
        {
            continue;
        }
        const vehicle* vp = current.vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(current.vehicle_handle) : nullptr;
        if (vp != nullptr)
        {
            if (!vp->is_killed())
            {
                count++;
            }
            continue;
        }
        const object_squad* squad = World.object_from_handle<object_squad>(current.squad_handle);
        if (squad != nullptr && squad->simulated() && squad->should_spawn_with_vehicle())
        {
            count++;
        }
    }
    return count;
}

// 0x00781280 (:1822). Original: no null check of the player.
float activity_convoy_data::get_target_distance() const
{
    const player* pp = player_get_local_player();
    if (convoy_def != nullptr && convoy_def->type == CONVOY_TYPE_CAPTURE)
    {
        if (convoy_activity_flags.player_in_target_vehicle)
        {
            return 0.0f;
        }
        const object* target = get_target_object();
        return target != nullptr ? vec_dist(pp->pos, target->pos) : 0.0f;
    }
    if (convoy_def != nullptr && convoy_def->type == CONVOY_TYPE_DESTROY)
    {
        float nearest = FLT_MAX;
        for (int i = 0; i < NUM_CONVOY_SQUADS; i++)
        {
            const convoy_vehicle_data& current = squad_array[i];
            if (current.squad_handle == 0xFFFFFFFF || current.convoy_veh_flags.vehicle_killed)
            {
                continue;
            }
            const object* target = current.vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(current.vehicle_handle) : nullptr;
            if (target == nullptr)
            {
                target = World.object_from_handle<object_squad>(current.squad_handle);
            }
            const float dist = target != nullptr ? vec_dist_squared(pp->pos, target->pos) : FLT_MAX;
            if (dist < nearest)
            {
                nearest = dist;
            }
        }
        return nearest >= FLT_MAX ? 0.0f : sqrtf(nearest);
    }
    keen::debug::nativePrintArgs("Convoy type not found");
    return 0.0f;
}

// 0x007578E0 (:1897)
convoy_type activity_convoy_data::get_convoy_type() const
{
    return convoy_def != nullptr ? convoy_def->type : CONVOY_TYPE_UNDEFINED;
}

// 0x00757900 (:1907)
void activity_convoy_data::convert_all_squads()
{
    for (int i = 0; i < NUM_CONVOY_SQUADS; i++)
    {
        if (squad_array[i].squad_handle != 0xFFFFFFFF)
        {
            squad_array[i].role = CONVOY_SQUAD_ROLE_BREAK_AND_ATTACK;
        }
    }
}

// 0x00757930 (:1922)
void activity_convoy_data::capture_target_left_convoy()
{
    convert_all_squads(); // rfg.exe inlines it
    if (!is_active())
    {
        activity_begin_start_up();
    }
}

// 0x00757970 (:1936)
void activity_convoy_data::squad_left_convoy(unsigned int squad_handle)
{
    if (squad_handle == 0xFFFFFFFF)
    {
        return;
    }
    for (int i = 0; i < NUM_CONVOY_SQUADS; i++)
    {
        if (squad_array[i].squad_handle == squad_handle)
        {
            squad_array[i].convoy_veh_flags.left_convoy = 1;
            if (squad_array[i].convoy_veh_flags.capture_vehicle)
            {
                capture_target_left_convoy();
            }
        }
    }
}

// 0x0076BBA0 (:1955)
float activity_convoy_data::get_target_health()
{
    if (convoy_def == nullptr || convoy_def->type != CONVOY_TYPE_CAPTURE)
    {
        return 0.0f;
    }
    const vehicle* vp = cached_vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(cached_vehicle_handle) : nullptr;
    if (vp != nullptr)
    {
        cached_vehicle_health = (1.0f - vp->get_vehicle_damage()) * 100.0f;
    }
    return cached_vehicle_health;
}

// 0x0076BC00 (:1977)
void activity_convoy_data::target_vehicle_delivered()
{
    if (convoy_def == nullptr || convoy_def->type != CONVOY_TYPE_CAPTURE)
    {
        return;
    }
    World.change_tech_level((float)convoy_def->tech_level_reward);
    // Clamped to 1 to 16.
    Convoy_activity_data.activity_level++;
    if (Convoy_activity_data.activity_level < 1)
    {
        Convoy_activity_data.activity_level = 1;
    }
    else if (Convoy_activity_data.activity_level > MAX_CONVOY_DIFFICULTY_LEVEL)
    {
        Convoy_activity_data.activity_level = MAX_CONVOY_DIFFICULTY_LEVEL;
    }
    play_radio_line(responses_are_female ? CONVOY_RL_DELIVERED_VEHICLE_F : CONVOY_RL_DELIVERED_VEHICLE_M, CONVOY_RADIO_LINE_NONE);
    vi_exit_data exit_data;
    exit_data.initialize(); // rfg.exe clears the flags byte
    vehicle_interaction_request_exit(player_get_local_player(), &exit_data);
    vehicle* vp = cached_vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(cached_vehicle_handle) : nullptr;
    if (vp != nullptr)
    {
        vp->m_flags.disable_for_npc_drivers = 1;
        vp->m_flags.disable_for_player = 1;
        vehicle_set_controls_when_abandoned(vp);
        vehicle_reposition_on_ground(vp);
    }
    record_victory();
    activity_end();
}

// 0x007579C0 (:2014)
void activity_convoy_data::destruction_convoy_escaped(bool play_radio)
{
    if (play_radio)
    {
        play_radio_line(responses_are_female ? CONVOY_RL_FAIL_F : CONVOY_RL_FAIL_M, CONVOY_RADIO_LINE_NONE);
    }
    record_loss();
    activity_end();
}

// 0x00796F10 (:2027). Original: waits a second, records the win, waits two more, says the lines and ends.
void activity_convoy_data::destruction_convoy_destroyed()
{
    if (!victory_timer.valid())
    {
        victory_timer.set(Convoy_victory_wait_ms);
        return;
    }
    if (!victory_timer.elapsed())
    {
        return;
    }
    if (!get_victor_record_flag())
    {
        record_victory();
        victory_timer.set(Convoy_victory_radio_wait_ms);
        return;
    }
    // rfg.exe inlines the first and names the persona.
    play_radio_line(CONVOY_RL_DESTROYED_VEHICLE, CONVOY_RADIO_LINE_NONE);
    play_radio_line(responses_are_female ? CONVOY_RL_DESTROYED_VEHICLE_RES_F : CONVOY_RL_DESTROYED_VEHICLE_RES_M, CONVOY_RL_DESTROYED_VEHICLE);
    activity_convoy_data::activity_end(); // rfg.exe inlines it on the global instance
}

// 0x007AD320 (:2058). Original: the target's crew defends the abandoned vehicle while it streams out; other squads go.
bool activity_convoy_data::convert_despawning_squad(const convoy_vehicle_data& squad_info, object_squad* squad)
{
    if (squad == nullptr)
    {
        return false;
    }
    if (squad_info.convoy_veh_flags.left_convoy || squad_info.role != CONVOY_SQUAD_ROLE_TARGET)
    {
        squad->request_despawn_all_members(NPC_DESPAWN_TYPE_LOW, nullptr, true);
        return true;
    }
    vehicle* vp = squad->squad_vehicle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(squad->squad_vehicle) : nullptr;
    if (vp == nullptr)
    {
        return false;
    }
    vp->set_persistent(false);
    vp->destroy_on_stream();
    gfm_orders_creation_params params;
    params.orders_type = GFM_ORDERS_TYPE_DEFEND;
    params.team = squad->get_squad_team();
    params.location_target = vp->pos;
    params.object_target = vp->get_handle();
    const unsigned int new_orders = Orders_manager.create_new_orders(params);
    if (new_orders == 0xFFFFFFFF)
    {
        return false;
    }
    squad->assign_base_orders(new_orders);
    despawning_squad_list.add(squad->get_handle());
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        npc* np = World.object_from_handle<npc>(squad->member_array[i]);
        if (np != nullptr)
        {
            np->hflags.disallow_vehicle_exit = 0;
            vehicle_interaction_request_exit(np, nullptr);
        }
    }
    vp->m_flags.disable_for_npc_drivers = 1;
    vp->m_flags.disable_for_player = 1;
    return true;
}

// 0x007AD460 (:2120)
void activity_convoy_data::convoy_at_destination()
{
    play_radio_line(responses_are_female ? CONVOY_RL_FAIL_F : CONVOY_RL_FAIL_M, CONVOY_RADIO_LINE_NONE);
    for (int i = 0; i < NUM_CONVOY_SQUADS; i++)
    {
        convoy_vehicle_data& current = squad_array[i];
        object_squad* squad = current.squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(current.squad_handle) : nullptr;
        if (squad != nullptr)
        {
            squad->set_persistent(false);
            if (squad->simulated())
            {
                World.destroy_object(squad, false);
            }
            else if (!convert_despawning_squad(current, squad))
            {
                squad->request_despawn_all_members(NPC_DESPAWN_TYPE_MEDUIM, nullptr, true);
            }
        }
        current.reset();
    }
    Orders_manager.destroy_all_orders_of_type(GFM_ORDERS_TYPE_CONVOY);
    activity_kill();
}

// 0x00757A00 (:2160)
void activity_convoy_data::capture_convoy_escaped()
{
    if (any_convoy_squad_alive()) // rfg.exe inlines it
    {
        play_radio_line(responses_are_female ? CONVOY_RL_FAIL_F : CONVOY_RL_FAIL_M, CONVOY_RADIO_LINE_NONE);
    }
    record_loss();
    activity_end();
}

// 0x00757A60 (:2177). Original: no null check of the player.
void activity_convoy_data::capture_convoy_destroyed()
{
    if (!player_get_local_player()->is_killed())
    {
        // rfg.exe inlines the first and names the persona.
        play_radio_line(CONVOY_RL_DESTROYED_VEHICLE, CONVOY_RADIO_LINE_NONE);
        play_radio_line(responses_are_female ? CONVOY_RL_DESTROYED_VEHICLE_RES_F : CONVOY_RL_DESTROYED_VEHICLE_RES_M, CONVOY_RL_DESTROYED_VEHICLE);
    }
    record_loss();
    activity_end();
}

// 0x0076BCF0 (:2194)
bool activity_convoy_data::convoy_target_destroyed() const
{
    const vehicle* cached = cached_vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(cached_vehicle_handle) : nullptr;
    if (cached != nullptr)
    {
        return cached->is_killed();
    }
    for (int i = 0; i < NUM_CONVOY_SQUADS; i++)
    {
        if (!squad_array[i].convoy_veh_flags.capture_vehicle)
        {
            continue;
        }
        const object_squad* squad = squad_array[i].squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(squad_array[i].squad_handle) : nullptr;
        if (squad != nullptr)
        {
            if (squad->simulated())
            {
                return false;
            }
            const vehicle* vp = squad_array[i].vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(squad_array[i].vehicle_handle) : nullptr;
            if (vp != nullptr && !vp->is_killed())
            {
                return false;
            }
        }
        return true;
    }
    return false;
}

// 0x00757AF0 (:2237)
const char* activity_convoy_data::get_title() const
{
    return "ACT_CONVOY";
}

// 0x00757B00 (:2251)
const char* activity_convoy_data::get_subtitle() const
{
    if (convoy_def != nullptr)
    {
        if (convoy_def->type == CONVOY_TYPE_CAPTURE)
        {
            return convoy_activity_flags.player_in_target_vehicle ? "ACT_CONVOY_CAPTURE_RETURN" : "ACT_CONVOY_CAPTURE";
        }
        if (convoy_def->type == CONVOY_TYPE_DESTROY)
        {
            return "ACT_CONVOY_DESTROY";
        }
    }
    keen::debug::nativePrintArgs("Convoy does not have a valid definition");
    return "";
}

// 0x00757B50 (:2277)
const char* activity_convoy_data::get_icon_image_name() const
{
    return get_minimap_bitmap_icon_name(convoy_def != nullptr && convoy_def->type == CONVOY_TYPE_CAPTURE ? 0xF : 0xE); // Magic numbers: the convoy map elements
}

// 0x00757B80 (:2300)
bool activity_convoy_data::on_alert(const npc* np) const
{
    return is_active();
}

// 0x0076BDA0 (:2309)
object_convoy* activity_convoy_data::get_convoy() const
{
    return my_convoy_handle != 0xFFFFFFFF ? World.object_from_handle<object_convoy>(my_convoy_handle) : nullptr;
}

// 0x007AD520 (:2319)
void activity_convoy_at_destination(const object_convoy* convoy)
{
    if (convoy == Convoy_activity_data.get_convoy())
    {
        Convoy_activity_data.convoy_at_destination();
    }
}

// 0x0076BDD0 (:2327)
object_convoy* get_activity_convoy_object()
{
    return Convoy_activity_data.get_convoy();
}

// 0x00757B90 (:2332)
convoy_type activity_convoy_get_type()
{
    return Convoy_activity_data.get_convoy_type();
}

// 0x00781430 (:2337)
float activity_convoy_get_nearest_target_dist()
{
    return Convoy_activity_data.get_target_distance();
}

// 0x0076BE00 (:2342)
float activity_convoy_get_nearest_target_health()
{
    return Convoy_activity_data.get_target_health(); // rfg.exe inlines it
}

// 0x0076BE70 (:2347)
int activity_convoy_get_total_num_start_vehicles()
{
    return Convoy_activity_data.get_num_spawn_vehicles(); // rfg.exe inlines it
}

// 0x0076BE90 (:2352)
int activity_convoy_get_num_current_vehicles()
{
    return Convoy_activity_data.get_num_vehicles();
}

// 0x00757BB0 (:2357)
int activity_convoy_get_level()
{
    return Convoy_activity_data.get_activity_level();
}

// 0x007A0F10 (:2363)
bool activity_convoy_start(const object_convoy_end_point* end_point)
{
    if (Convoy_activity_data.was_initialized() || Convoy_activity_data.is_active())
    {
        return false;
    }
    const unsigned int handle = activity_int_get_district_handle_from_node(end_point);
    const district* dp = handle != 0xFFFFFFFF ? World.object_from_handle<district>(handle) : nullptr;
    if (!Convoy_activity_data.activity_base::start_activity_spawn(end_point, dp))
    {
        Convoy_activity_data.activity_convoy_data::activity_kill();
        return false;
    }
    return true;
}

// 0x00757BC0 (:2387)
bool activity_convoy_vehicle_destroy(const vehicle* vp)
{
    // Deviation from rfg.exe: activity_get_mode, where it reads activity.cpp's private static.
    if (Convoy_activity_data.mode != activity_get_mode() || Convoy_activity_data.get_convoy_type() != CONVOY_TYPE_DESTROY || vp->is_killed())
    {
        return false;
    }
    return Convoy_activity_data.vehicle_in_convoy(vp);
}

// 0x00757C10 (:2404)
bool activity_convoy_vehicle_in_convoy(const vehicle* vp)
{
    return Convoy_activity_data.vehicle_in_convoy(vp); // rfg.exe inlines it
}

// 0x00757C40 (:2409)
bool activity_convoy_player_in_vehicle()
{
    return Convoy_activity_data.get_player_in_target_vehicle();
}

// 0x00757C50 (:2415)
void activity_convoy_squad_left_convoy(unsigned int squad_handle)
{
    Convoy_activity_data.squad_left_convoy(squad_handle);
}

// 0x0076BEA0 (:2421)
void activity_convoy_record_player_hit(const vector& hit_location, unsigned int rfg_mover_handle)
{
    if (Convoy_activity_data.get_state() == BASE_ACTIVITY_STATE_READY)
    {
        Convoy_activity_data.register_player_hit(hit_location, rfg_mover_handle);
    }
}
