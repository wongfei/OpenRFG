// Original: volition/rfg/code/gameplay/activities/activity_raid_attack.cpp
//
// The offensive raid: a guerrilla squad waits at the nearest activity spawn for the player, whose arrival (a talk with
// the squad, activity_raid_attack_start) starts the raid.

#include "volition/rfg/code/gameplay/activities/activity_raid_attack.h"

#include "volition/rfg/code/ai/ai_scripting.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_house_arrest_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_raid.h"
#include "volition/rfg/code/gameplay/activities/activity_raid_attack_internal.h"
#include "volition/rfg/code/gameplay/activities/raid_common.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gameplay/squad_spawn.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/hud/minimap_icon_info.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_action_node.h"
#include "volition/rfg/code/objects/object_activity_spawn.h"
#include "volition/rfg/code/objects/object_raid_node.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/scripting/dialogue_manager.h"
#include "volition/rfg/code/scripting/objective_highlight.h"
#include "volition/vlib/code/math/rand.h"

#include <string.h>

extern mission_handle Mission_active_index; // 0x0165150C (gameplay.cpp)
extern activity_base* All_activities[NUM_OF_ACTIVITY_MODES]; // 0x01651248 (activity.cpp)

offensive_raid_data Offensive_raid; // 0x0226EDC8

static unsigned int Last_activiated_raid = 0xFFFFFFFF; // 0x0165136C

// 0x0077E1A0 (.h:29). Original: the object's other members stay unset until level_init calls do_raid_clean_up.
offensive_raid_data::offensive_raid_data()
    : activity_raid_data(ACTIVITY_MODE_RAID_ATTACK)
{
}

// 0x0077E1C0
offensive_raid_data::~offensive_raid_data()
{
}

// rfg.exe inlines it in can_start_raid, should_stop_raid, wait_do_frame_inactive and activity_raid_attack_level_init_post.
// Magic number: the alert level must stay below 2.
static bool raid_can_run(const offensive_raid_data& raid)
{
    if (Mission_active_index != INVALID_MISSION_HANDLE || raid.is_disabled())
    {
        return false;
    }
    const int level = (int)gsm_get_alert_level();
    if (level >= 0 && level >= 2)
    {
        return false;
    }
    return activity_none() || activity_is_active(raid.mode);
}

// 0x007701B0 (:69)
const object_action_node* find_raid_action_node(const object_activity_spawn* node)
{
    if (node == nullptr)
    {
        return nullptr;
    }
    const object* first = node->child_ptr;
    for (const object* child = first; child != nullptr; )
    {
        if (child->get_obj_type() == OT_ACTION_NODE)
        {
            return static_cast<const object_action_node*>(child);
        }
        child = child->child_next;
        if (child == first)
        {
            break;
        }
    }
    return nullptr;
}

// 0x00784FC0 (:87). rfg.exe: LTCG, best_spawn_node in EDI.
bool find_nearest_spawn_object(raid_node_selection_info& best_spawn_node)
{
    best_spawn_node.node_objp = nullptr;
    best_spawn_node.raid_node = nullptr;
    best_spawn_node.node = nullptr;
    best_spawn_node.dist_sqrd = 3.4028235e+38f;
    const player* pp = player_get_local_player();
    for (int i = 0; i < World.get_num_objects<object_activity_spawn>(); i++)
    {
        const object_activity_spawn* spawn = World.get_object<object_activity_spawn>(i);
        if (!spawn->obj_flags.streamed)
        {
            continue;
        }
        const object* node = spawn->get_activity_node();
        if (node == nullptr || node->get_obj_type() != OT_RAID_NODE || !static_cast<const object_raid_node*>(node)->can_spawn())
        {
            continue;
        }
        const float dist = vec_dist_squared(pp->pos, spawn->pos);
        if (dist < best_spawn_node.dist_sqrd)
        {
            best_spawn_node.node_objp = node;
            best_spawn_node.raid_node = static_cast<const object_raid_node*>(node);
            best_spawn_node.node = spawn;
            best_spawn_node.dist_sqrd = dist;
        }
    }
    return best_spawn_node.node != nullptr;
}

// 0x007AE9D0 (:152). Magic numbers: the morale thirds.
const squad_definition* pick_raid_squad_def()
{
    static const squad_definition* HIGH_MORALE_SQUAD_DEF = squad_definition_from_name("guerilla_squad_3");
    static const squad_definition* MEDIUM_MORALE_SQUAD_DEF = squad_definition_from_name("guerilla_squad_2");
    static const squad_definition* LOW_MORALE_SQUAD_DEF = squad_definition_from_name("guerilla_squad_1");
    static const squad_definition* MARAUDER_SQUAD_DEF = squad_definition_from_name("marauder_squad_1");
    if (player_get_local_player()->current_team == HUMAN_TEAM_MARAUDER)
    {
        return MARAUDER_SQUAD_DEF;
    }
    const float morale = gfm_get_effective_morale_percent();
    if (morale >= 0.66f)
    {
        return HIGH_MORALE_SQUAD_DEF;
    }
    return morale < 0.33f ? LOW_MORALE_SQUAD_DEF : MEDIUM_MORALE_SQUAD_DEF;
}

// 0x00799930 (:187)
bool offensive_raid_data::start_new_raid()
{
    if (!offensive_raid_flags.wait_for_player || new_raid_handle == 0xFFFFFFFF)
    {
        return false;
    }
    const object* spawn_objp = World.object_from_handle(new_raid_handle);
    if (spawn_objp == nullptr || spawn_objp->get_obj_type() != OT_RAID_NODE)
    {
        return false;
    }
    district* dp = district_get_from_pos(spawn_objp->pos);
    if (dp == nullptr)
    {
        return false;
    }
    activity_kill_all_except(mode);
    return start_activity_spawn(spawn_objp, dp);
}

// 0x007701E0 (:209)
bool offensive_raid_data::should_stop_raid() const
{
    return !raid_can_run(*this);
}

// 0x00770240 (:236). Magic number: within ten metres.
npc* offensive_raid_data::get_intro_npc(const object_squad* squad) const
{
    if (squad == nullptr)
    {
        return nullptr;
    }
    npc* best = nullptr;
    float best_dist = 3.4028235e+38f;
    const player* pp = player_get_local_player();
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        npc* np = World.object_from_handle<npc>(squad->member_array[i]);
        if (np != nullptr && !np->is_killed())
        {
            const float dist = vec_dist_squared(pp->pos, np->pos);
            if (dist < best_dist)
            {
                best = np;
                best_dist = dist;
            }
        }
    }
    return best_dist <= 100.0f ? best : nullptr;
}

// 0x007850D0 (:280). False ends the wait: the squad is gone, it fights the player, or a nearer spawn took over.
bool offensive_raid_data::wait_do_frame_active()
{
    if (base_raid_def == nullptr || should_stop_raid())
    {
        return false;
    }
    raid_node_selection_info best;
    if (!find_nearest_spawn_object(best) || raid_spawn_node != best.node->get_handle() || start_up_squad_handle == 0xFFFFFFFF)
    {
        return false;
    }
    const object* objp = World.object_from_handle(start_up_squad_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_SQUAD)
    {
        return false;
    }
    const object_squad* squad = static_cast<const object_squad*>(objp);
    const player* pp = player_get_local_player();
    if (!offensive_raid_flags.played_intro_line && pp->vehicle_handle == 0xFFFFFFFF)
    {
        npc* intro = get_intro_npc(squad);
        if (intro != nullptr)
        {
            ai_scripting_trigger_behavior_on_character(intro, AI_SB_ACTIVITY_INTRO, pp->get_handle(), base_raid_def->intro_line, base_raid_def->intro_anim_action);
            offensive_raid_flags.played_intro_line = 1;
        }
    }
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        const npc* np = World.object_from_handle<npc>(squad->member_array[i]);
        if (np != nullptr && human_combat_get_target(np) == pp)
        {
            return false;
        }
    }
    return true;
}

// 0x00770310 (:342)
bool offensive_raid_data::can_start_raid() const
{
    return raid_can_run(*this);
}

// 0x007B6AC0 (:362). The waiting squad now follows the player and stays for the raid.
bool offensive_raid_data::activity_initialize(const object* spawn_objp, unsigned int new_district_handle)
{
    gfm_orders_creation_params params;
    if (!activity_raid_data::activity_initialize(spawn_objp, new_district_handle))
    {
        return false;
    }
    const player* pp = player_get_local_player();
    params.location_target = pp->pos;
    params.object_target = pp->get_handle();
    params.orders_type = GFM_ORDERS_TYPE_DEFEND;
    params.team = HUMAN_TEAM_GUERILLA;
    const unsigned int orders_handle = Orders_manager.create_new_orders(params); // rfg.exe inlines it
    object* objp = start_up_squad_handle != 0xFFFFFFFF ? World.object_from_handle(start_up_squad_handle) : nullptr;
    if (objp != nullptr && objp->get_obj_type() == OT_SQUAD)
    {
        object_squad* squad = static_cast<object_squad*>(objp);
        squad->assign_orders(orders_handle);
        squad->npc_flags.ambient_spawn = 0;
        for (int i = 0; i < squad->member_array.size(); i++)
        {
            npc* np = World.object_from_handle<npc>(squad->member_array[i]);
            if (np != nullptr)
            {
                np->npc_flags.ambient_spawn = 0;
            }
        }
        squad->squad_flags.special_despawn_rules = 1;
    }
    World.destroy_object(start_effect_handle, false);
    offensive_raid_flags.wait_for_player = 0;
    change_interface_state(ACTIVITY_INTERFACE_STATE_RUNNING); // rfg.exe inlines it
    return true;
}

// 0x00770370 (:418). Original: the colour comes from raid_handle, the icon's node from new_raid_handle.
void offensive_raid_data::render_icons() const
{
    if (get_state() != BASE_ACTIVITY_STATE_READY)
    {
        activity_raid_data::render_icons();
        return;
    }
    const object* node = new_raid_handle != 0xFFFFFFFF ? World.object_from_handle(new_raid_handle) : nullptr;
    if (node == nullptr || node->get_obj_type() != OT_RAID_NODE)
    {
        return;
    }
    const object_raid_node* raid = raid_handle != 0xFFFFFFFF ? World.object_from_handle<object_raid_node>(raid_handle) : nullptr;
    const color* icon_color = raid != nullptr && raid->get_completed_flag() ? &Completed_icon_color : &Default_map_item_color;
    Cartographer.add_temporary_icon(node->pos, MAP_ELEMENT_RAID_ICON_ALWAYS_SHOW, true, node->get_handle(), *icon_color, false, node->get_handle());
    if (!offensive_raid_flags.gps_triggered)
    {
        Cartographer.add_player_marker(node->pos, false, node->get_handle(), false, false);
        const_cast<offensive_raid_data*>(this)->offensive_raid_flags.gps_triggered = 1;
    }
}

// 0x007999A0 (:459)
void offensive_raid_data::clear_all_flags()
{
    activity_raid_data::clear_all_flags();
    farray<object_squad*, 100> squads;
    farray<npc*, 100> npcs;
    collect_all_raid_squads(squads);
    for (int i = 0; i < squads.size(); i++)
    {
        squads[i]->hflags.raid_introduction_lines = 0;
    }
    collect_all_raid_npcs(npcs);
    for (int i = 0; i < npcs.size(); i++)
    {
        npcs[i]->hflags.raid_introduction_lines = 0;
    }
}

// 0x007B6C40 (:486). Magic number: a new raid's squad appears only more than 80 metres from the player.
void offensive_raid_data::wait_do_frame_inactive()
{
    raid_node_selection_info best;
    if (!raid_can_run(*this) || !find_nearest_spawn_object(best))
    {
        return;
    }
    if (vec_dist_squared(player_get_local_player()->pos, best.node->pos) < 6400.0f)
    {
        Last_activiated_raid = best.node->get_handle();
        return;
    }
    spawn_new_raid_squad(best);
}

// 0x007B2BC0 (:514)
void offensive_raid_data::spawn_new_raid_squad(const raid_node_selection_info& spawn_node_info)
{
    if (spawn_node_info.node == nullptr || spawn_node_info.raid_node == nullptr)
    {
        return;
    }
    Last_activiated_raid = spawn_node_info.node->get_handle();
    raid_spawn_node = Last_activiated_raid;
    new_raid_handle = spawn_node_info.raid_node->get_handle();
    base_raid_def = spawn_node_info.raid_node->get_raid_def();
    if (base_raid_def == nullptr)
    {
        return;
    }
    squad_spawn_parameters params;
    const object_activity_spawn* spawn = spawn_node_info.node;
    const object_action_node* action_node = find_raid_action_node(spawn); // rfg.exe inlines it
    if (action_node != nullptr)
    {
        params.orders.orders_type = GFM_ORDERS_TYPE_AMBIENT_ACTION;
        params.orders.location_target = action_node->pos;
        params.orders.object_target = action_node->get_handle();
    }
    else
    {
        params.orders.orders_type = GFM_ORDERS_TYPE_WAIT;
        params.orders.location_target = spawn->pos;
        params.orders.object_target = spawn->get_handle();
    }
    params.squad.hflags.raid_introduction_lines = 1;
    params.squad.hflags.activity_raid_required = 1;
    params.squad.hflags.do_not_play_ambient_or_greet_lines = 1;
    params.squad.hflags.disallow_vehicle_drive = 1;
    params.squad.npc_flags.disallow_idle_despawn = 1;
    params.squad.squad_pos = spawn->pos;
    params.squad.squad_orient = spawn->orient;
    params.squad.squad_def = pick_raid_squad_def();
    if (params.squad.squad_def == nullptr)
    {
        return;
    }
    params.spawn_squad();
    if (params.spawned_squad == nullptr || params.created_orders == nullptr)
    {
        params.destroy_squad();
        params.destroy_orders();
        return;
    }
    start_up_squad_handle = params.spawned_squad->get_handle();
    World.destroy_object(start_effect_handle, false);
    start_up_pos = spawn->pos;
    start_effect_handle = objective_highlight_activity_marker_add(spawn, ZERO_VECTOR);
    offensive_raid_flags.wait_for_player = 1;
    offensive_raid_flags.played_vehicle_entry = 0;
}

// 0x007851F0 (:581)
void offensive_raid_data::level_init()
{
    activity_raid_data::level_init();
    do_raid_clean_up();
}

// 0x00799A80 (:591)
void offensive_raid_data::activity_do_frame_ready()
{
    activity_base::activity_do_frame_ready();
    do_frame_free_squads();
    do_frame_ready_npc_participants();
    if (default_raid_info != nullptr)
    {
        do_frame_player_wait();
    }
    if (base_raid_def == nullptr)
    {
        return;
    }
    object* objp = start_up_squad_handle != 0xFFFFFFFF ? World.object_from_handle(start_up_squad_handle) : nullptr;
    if (objp != nullptr && objp->get_obj_type() == OT_SQUAD)
    {
        object_squad* squad = static_cast<object_squad*>(objp);
        const player* pp = player_get_local_player();
        for (int i = 0; i < squad->member_array.size(); i++)
        {
            const npc* np = World.object_from_handle<npc>(squad->member_array[i]);
            if (np == nullptr)
            {
                continue;
            }
            if (human_combat_get_target(np) == pp)
            {
                squad->request_despawn_all_members(NPC_DESPAWN_TYPE_LOW, nullptr, true);
                start_up_squad_handle = 0xFFFFFFFF;
            }
            else
            {
                Cartographer.add_temporary_icon(np->pos, MAP_ELEMENT_STAR_BLUE_ICON, false, np->get_handle(), Default_map_item_color, false, 0xFFFFFFFF);
            }
        }
    }
    if (default_raid_info != nullptr)
    {
        if (get_state_duration() <= default_raid_info->max_startup_delay_ms)
        {
            play_vehicle_lines();
        }
        else
        {
            activity_kill();
        }
    }
}

// 0x00785200 (:640). A member sitting in the player's vehicle.
npc* offensive_raid_data::get_raid_start_npc() const
{
    if (start_up_squad_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    const object* objp = World.object_from_handle(start_up_squad_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_SQUAD)
    {
        return nullptr;
    }
    const player* pp = player_get_local_player();
    if (pp->vehicle_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    const object_squad* squad = static_cast<const object_squad*>(objp);
    farray<npc*, 100> riders;
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        npc* np = World.object_from_handle<npc>(squad->member_array[i]);
        if (np != nullptr && !np->is_killed() && pp->vehicle_handle == np->vehicle_handle && !human_in_vehicle_transition(np, nullptr, VI_INTERACTION_ENTER) && !human_in_vehicle_transition(np, nullptr, VI_INTERACTION_EXIT))
        {
            riders.add(np);
        }
    }
    if (riders.size() == 0)
    {
        return nullptr;
    }
    return riders.size() == 1 ? riders[0] : riders[int_rand_range(0, riders.size() - 1)];
}

// 0x00785360 (:697)
void offensive_raid_data::play_vehicle_lines()
{
    if (base_raid_def == nullptr || !vehicle_line_delay.elapsed())
    {
        return;
    }
    npc* np = get_raid_start_npc();
    if (np == nullptr)
    {
        return;
    }
    const voice_line_handle line = offensive_raid_flags.played_vehicle_entry ? base_raid_def->travel_line : base_raid_def->vehicle_entry_line;
    if (line != INVALID_VOICE_LINE_HANDLE)
    {
        np->play_voice_line(line, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_DIALOGUE_NORMAL, false, false, true, true);
    }
    offensive_raid_flags.played_vehicle_entry = 1;
    vehicle_line_delay.set_rand(4000, 12000); // Magic numbers: four to twelve seconds
}

// A modal activity on its way to the world. rfg.exe inlines it twice in render_inactive_icons.
static bool raid_activity_starting(const activity_base* activity)
{
    return activity != nullptr && (activity->get_state() == BASE_ACTIVITY_STATE_LOADING || activity->get_state() == BASE_ACTIVITY_STATE_READY || activity->get_state() == BASE_ACTIVITY_STATE_STARTING);
}

// 0x007853F0 (:730)
void offensive_raid_data::render_inactive_icons()
{
    if (activity_get_mode() != ACTIVITY_MODE_NONE || raid_activity_starting(this) || raid_activity_starting(&House_arrest_activity))
    {
        return;
    }
    for (int i = 0; i < World.get_num_objects<object_activity_spawn>(); i++)
    {
        const object_activity_spawn* spawn = World.get_object<object_activity_spawn>(i);
        if (spawn->get_activity_type() != OT_RAID_NODE || spawn->get_activity_handle() == 0xFFFFFFFF)
        {
            continue;
        }
        const object_raid_node* node = World.object_from_handle<object_raid_node>(spawn->get_activity_handle());
        if (node == nullptr || !node->can_spawn())
        {
            continue;
        }
        if (Last_activiated_raid == spawn->get_handle() && raid_spawn_node == 0xFFFFFFFF)
        {
            if (spawn->obj_flags.streamed)
            {
                continue;
            }
            Last_activiated_raid = 0xFFFFFFFF;
        }
        const color& icon_color = node->get_completed_flag() ? Completed_icon_color : Default_map_item_color;
        Cartographer.add_temporary_icon(spawn->pos, MAP_ELEMENT_RAID_ICON, false, 0xFFFFFFFF, icon_color, false, spawn->get_handle());
    }
}

// 0x007B6CF0 (:784)
void offensive_raid_data::activity_do_frame_inactive()
{
    render_inactive_icons();
    if (!offensive_raid_flags.wait_for_player)
    {
        wait_do_frame_inactive();
    }
    else if (!wait_do_frame_active())
    {
        do_raid_clean_up();
    }
}

// 0x00759C30 (:803)
bool offensive_raid_data::is_defending_team(human_teams team) const
{
    const player* pp = player_get_local_player();
    return pp != nullptr && human_teams_get_disposition(pp, team) == TEAM_DISPOSITION_HOSTILE;
}

// 0x00759C60 (:815)
bool offensive_raid_data::is_attacking_team(human_teams team) const
{
    const player* pp = player_get_local_player();
    return pp != nullptr && human_teams_get_disposition(pp, team) == TEAM_DISPOSITION_ALLIED;
}

// 0x00770430 (:829)
void offensive_raid_data::do_raid_clean_up()
{
    object* squad_objp = start_up_squad_handle != 0xFFFFFFFF ? World.object_from_handle(start_up_squad_handle) : nullptr;
    if (squad_objp != nullptr && squad_objp->get_obj_type() == OT_SQUAD)
    {
        static_cast<object_squad*>(squad_objp)->request_despawn_all_members(NPC_DESPAWN_TYPE_LOW, nullptr, true);
    }
    object* effect = start_effect_handle != 0xFFFFFFFF ? World.object_from_handle(start_effect_handle) : nullptr;
    if (effect != nullptr && effect->get_obj_type() == OT_EFFECT)
    {
        World.destroy_object(effect, false);
    }
    memset(&offensive_raid_flags, 0, sizeof(offensive_raid_flags));
    start_effect_handle = 0xFFFFFFFF;
    start_up_squad_handle = 0xFFFFFFFF;
    raid_spawn_node = 0xFFFFFFFF;
    new_raid_handle = 0xFFFFFFFF;
    start_up_pos.zero();
    base_raid_def = nullptr;
    vehicle_line_delay.set(0);
}

// 0x007AEAC0 (:859)
void offensive_raid_data::activity_end()
{
    activity_raid_data::activity_end(); // rfg.exe inlines it
    do_raid_clean_up();
}

// 0x00799BC0 (:869)
void offensive_raid_data::activity_kill()
{
    activity_base_modal::activity_kill(); // rfg.exe inlines it
    if (default_raid_info != nullptr)
    {
        dialogue_manager_cancel(default_raid_info->start_radio_line, true);
    }
    activity_end();
}

// 0x00759C90 (:878)
const char* offensive_raid_data::get_title() const
{
    return dlc_get_current_id() == 1 ? "HM_DLC_MARAUDER_RAID" : "ACT_RAID_OFFENSE"; // Magic number: the Demons of the Badlands DLC
}

// 0x00759CB0 (:896)
const char* offensive_raid_data::get_subtitle() const
{
    return get_state() == BASE_ACTIVITY_STATE_READY ? "ACT_RAID_GOTO" : "ACT_RAID_DESTROY";
}

// 0x00759CD0 (:906)
const char* offensive_raid_data::get_icon_image_name() const
{
    return get_minimap_bitmap_icon_name(0x14); // Magic number: the raid's icon
}

// 0x00759CE0 (:918)
bool activity_raid_attack_start_info(vector& start_pos, unsigned int& start_node_handle)
{
    if (!Offensive_raid.offensive_raid_flags.wait_for_player || Offensive_raid.raid_spawn_node == 0xFFFFFFFF)
    {
        return false;
    }
    start_pos = Offensive_raid.start_up_pos;
    start_node_handle = Offensive_raid.raid_spawn_node;
    return true;
}

// 0x00799C00 (:934)
void activity_raid_attack_start()
{
    if (Offensive_raid.get_state() == BASE_ACTIVITY_STATE_WAIT && !Offensive_raid.start_new_raid())
    {
        Offensive_raid.offensive_raid_data::activity_kill(); // rfg.exe inlines it
    }
}

// 0x007B2DC0 (:952)
void activity_raid_attack_level_init_post()
{
    raid_node_selection_info best;
    if (Offensive_raid.can_start_raid() && find_nearest_spawn_object(best))
    {
        Offensive_raid.spawn_new_raid_squad(best);
    }
}

// 0x007704E0 (:970)
npc* activity_raid_attack_get_intro_npc()
{
    if (Offensive_raid.start_up_squad_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    const object* objp = World.object_from_handle(Offensive_raid.start_up_squad_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_SQUAD)
    {
        return nullptr;
    }
    return Offensive_raid.get_intro_npc(static_cast<const object_squad*>(objp));
}
