// Original: volition/rfg/code/gameplay/activities/activity_raid.cpp
//
// The raid common to attack and defend. rfg.exe's calls through vtbl+0xDC to +0xE8 are the virtuals is_inside_region,
// is_defending_team, is_attacking_team and clear_all_flags.

#include "volition/rfg/code/gameplay/activities/activity_raid.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/code_effects/explosion.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_raid_attack_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_raid_defend_internal.h"
#include "volition/rfg/code/gameplay/activities/base_activity_data.h"
#include "volition/rfg/code/gameplay/activities/gfm_reinforcement_coordinator.h"
#include "volition/rfg/code/gameplay/activities/raid_common.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/gameflow/gameflow_manager.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gameplay/squad_spawn.h"
#include "volition/rfg/code/hud/hud_ui_activity_complete.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_bounding_box.h"
#include "volition/rfg/code/objects/object_dummy.h"
#include "volition/rfg/code/objects/object_raid_node.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/rfg/code/objects/object_spawn_region.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/scripting/action_script/script_timer.h"
#include "volition/rfg/code/scripting/dialogue_manager.h"
#include "volition/rfg/code/scripting/public_address.h"
#include "volition/rfg/code/spawn/spawn_resource_manager.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/rfg/code/gamewide/tweak_table.h"

#include <string.h>

extern bool Encounter_disabled; // 0x022295FE (morale_effect_manager.cpp)
extern unsigned int Activity_node_start_on_load; // 0x01651244 (activity.cpp)
extern gfm_orders_flags ACTIVITY_ORDERS_FLAGS; // 0x02231B21 (gfm_smash_building_coordinator.cpp)

static farray<raid_straggler_info, 5> Raid_stragglers; // 0x01653B68
static farray<raid_target_pair, 128> All_raid_targets; // 0x01653C40
static unsigned int Raid_flee_timer_handle = 0xFFFFFFFF; // file-static in rfg.exe
static float Raid_voice_distance = 60.0f; // 0x01651368
static tweak_table_item Tweak_table_item_Raid_voice_distance("Raid_voice_distance", &Raid_voice_distance); // 0x0226D384

// 0x01653BE0, the bytes of rfg.exe's table. Each side's spoken and radio lines are the same.
static const raid_voice_info Raid_voice_lines[NUM_RAID_SIDES][NUM_RAID_VOICE_TYPES] = {
    {
        raid_voice_info(VOICE_LINE_RAID_OFFENSE_PLAYER_ARRIVES, VOICE_LINE_RAID_OFFENSE_PLAYER_ARRIVES),
        raid_voice_info(VOICE_LINE_RAID_FALL_BACK, VOICE_LINE_RAID_FALL_BACK),
        raid_voice_info(VOICE_LINE_RAID_PUSH_FORWARD, VOICE_LINE_RAID_PUSH_FORWARD),
        raid_voice_info(VOICE_LINE_RAID_REACT_GUERILLA_REINFORCEMENTS, VOICE_LINE_RAID_REACT_GUERILLA_REINFORCEMENTS),
        raid_voice_info(VOICE_LINE_RAID_REACT_EDF_REINFORCEMENTS, VOICE_LINE_RAID_REACT_EDF_REINFORCEMENTS),
        raid_voice_info(VOICE_LINE_RAID_OFFENSE_WIN, VOICE_LINE_RAID_OFFENSE_WIN),
    },
    {
        raid_voice_info(VOICE_LINE_RAID_DEFENSE_PLAYER_ARRIVES, VOICE_LINE_RAID_DEFENSE_PLAYER_ARRIVES),
        raid_voice_info(VOICE_LINE_RAID_FALL_BACK, VOICE_LINE_RAID_FALL_BACK),
        raid_voice_info(VOICE_LINE_RAID_PUSH_FORWARD, VOICE_LINE_RAID_PUSH_FORWARD),
        raid_voice_info(VOICE_LINE_RAID_REACT_GUERILLA_REINFORCEMENTS, VOICE_LINE_RAID_REACT_GUERILLA_REINFORCEMENTS),
        raid_voice_info(VOICE_LINE_RAID_REACT_EDF_REINFORCEMENTS, VOICE_LINE_RAID_REACT_EDF_REINFORCEMENTS),
        raid_voice_info(VOICE_LINE_RAID_DEFENSE_WIN, VOICE_LINE_RAID_DEFENSE_WIN),
    },
};

// A bounding box as a squad's defend target: rfg.exe inlines it in the set_*_state overloads.
static void raid_orders_defend_box(gfm_orders* orders, const object* box)
{
    orders->set_target_object(box->get_handle());
    if (!box->pos.is_null())
    {
        orders->set_target_position(box->pos);
    }
    orders->set_orders_type(GFM_ORDERS_TYPE_DEFEND);
}

// 0x00768D50 (.h:33)
activity_raid_participant_info::activity_raid_participant_info()
{
    reset();
}

// 0x0077DF10 (.h:85). Original: the object's other members stay unset until level_init calls activity_reset.
activity_raid_data::activity_raid_data(activity_modes raid_mode)
    : activity_base_modal(raid_mode, false)
{
    memset(&npc_flags, 0, sizeof(npc_flags));
    memset(&hflags, 0, sizeof(hflags));
}

// 0x0077E0E0
activity_raid_data::~activity_raid_data()
{
}

// 0x007591A0 (:91)
raid_voice_info::raid_voice_info(voice_lines sl, voice_lines rl)
    : spoken_line(sl), radio_line(rl)
{
}

// 0x007591D0
raid_straggler_info::raid_straggler_info()
{
}

// 0x00759260
demolition_point::demolition_point()
{
}

// 0x0076EE60
demolition_point_eval_info::demolition_point_eval_info()
{
}

// 0x00753EB0 (.h:146)
bool is_in_cylindrical_region(const vector& pos, const vector& center, const float radius_sqrd)
{
    return vec_xz_dist_squared(pos, center) <= radius_sqrd;
}

// 0x00754010 (.h:203)
bool activity_raid_data::building_is_being_demolished() const
{
    return demolish_building_timer.valid();
}

// 0x00754020 (.h:334)
bool activity_raid_data::raid_ready() const
{
    return default_raid_info != nullptr;
}

// A spawn node with the squad it spawned, in do_frame_convert_squads.
struct squad_node_pair
{
    object_squad_spawn_node* node; // 0x00
    object_squad* squad; // 0x04
};

// A live npc from a handle, or null.
static npc* raid_npc_from_handle(unsigned int handle)
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
    return static_cast<npc*>(objp);
}

// A squad from a handle, or null.
static object_squad* raid_squad_from_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    return objp != nullptr && objp->get_obj_type() == OT_SQUAD ? static_cast<object_squad*>(objp) : nullptr;
}

// A squad spawn node from a handle, or null.
static object_squad_spawn_node* raid_squad_node_from_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    return objp != nullptr && objp->get_obj_type() == OT_SQUAD_SPAWN_NODE ? static_cast<object_squad_spawn_node*>(objp) : nullptr;
}

// A bounding box from a handle, or null.
static const object* raid_bbox_from_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    const object* objp = World.object_from_handle(handle);
    return objp != nullptr && objp->get_obj_type() == OT_BOUNDING_BOX ? objp : nullptr;
}

// 0x0076EBF0 (:245). Drops the stragglers that died.
void init_straggler_tracking_check()
{
    for (int i = 0; i < Raid_stragglers.size(); i++)
    {
        const npc* np = raid_npc_from_handle(Raid_stragglers[i].npc_handle);
        if (np == nullptr || np->is_killed())
        {
            Raid_stragglers.remove_unsorted(i);
            i--;
        }
    }
}

// 0x007591C0 (:256)
void render_straggler_tracking_debug(const npc* np, straggler_tracking_debug_status status, raid_straggler_info* straggler_info)
{
}

// 0x00783630 (:313). An enemy that stays out of the raid's region with nothing to fight, and does not move 20 metres in
// 30 seconds, is killed.
void do_straggler_tracking(npc* np)
{
    player* pp = player_get_local_player();
    if (pp == nullptr || !np->hflags.activity_raid_required || human_teams_get_disposition(pp, np) != TEAM_DISPOSITION_HOSTILE)
    {
        return;
    }
    int index = -1;
    for (int i = 0; i < Raid_stragglers.size(); i++)
    {
        if (Raid_stragglers[i].npc_handle == np->get_handle())
        {
            index = i;
            break;
        }
    }
    activity_raid_data* raid = nullptr;
    if (activity_get_mode() == ACTIVITY_MODE_RAID_ATTACK)
    {
        raid = &Offensive_raid;
    }
    else if (activity_get_mode() == ACTIVITY_MODE_RAID_DEFEND)
    {
        raid = &Defensive_raid;
    }
    const bool straggling = raid != nullptr && !raid->is_inside_region(np->pos) && np->get_ai()->perception.belief_bins[AI_BELIEF_ENEMY_POSITION].beliefs == nullptr;
    if (index >= 0)
    {
        if (!straggling)
        {
            Raid_stragglers.remove_unsorted(index);
            return;
        }
        raid_straggler_info& info = Raid_stragglers[index];
        if (vec_dist_squared(np->pos, info.start_pos) > 400.0f) // Magic number: 20 metres
        {
            info.straggle_timer.set(30000); // Magic number: 30 seconds
            info.start_pos = np->pos;
        }
        else if (info.straggle_timer.elapsed())
        {
            human_damage(np, nullptr, -1.0f, DBT_NONE, nullptr, nullptr, true, false);
            Raid_stragglers.remove_unsorted(index);
        }
    }
    else if (straggling && Raid_stragglers.size() < Raid_stragglers.capacity())
    {
        raid_straggler_info info;
        info.npc_handle = np->get_handle();
        info.straggle_timer.set(30000);
        info.start_pos = np->pos;
        Raid_stragglers.add(info);
    }
}

// 0x007591E0 (:387)
bool has_raid_flag(const human* hp)
{
    return hp->hflags.activity_raid_required || hp->hflags.activity_raid_optional;
}

// 0x00759200 (:391)
bool has_raid_flag(const object_squad* squad)
{
    return squad->hflags.activity_raid_required || squad->hflags.activity_raid_optional;
}

// 0x007837F0. Original: the defensive raid's test reads the offensive raid's participants.
static bool should_give_edf_attack_orders()
{
    const activity_raid_data* offensive = &Offensive_raid;
    const activity_raid_data* defensive = &Defensive_raid;
    const activity_modes mode = activity_get_mode();
    const activity_raid_data* raid = mode == ACTIVITY_MODE_RAID_ATTACK ? offensive : (mode == ACTIVITY_MODE_RAID_DEFEND ? defensive : nullptr);
    if (raid == nullptr || !raid->all_squad_members_spawned())
    {
        return false;
    }
    // Magic number: one or two enemies left go after the player.
    const unsigned int hostile = (unsigned int)offensive->participant_info.hostile_humans_alive();
    return hostile - 1u <= 1u;
}

// 0x007AE300
static void give_edf_attack_orders(npc* np)
{
    player* pp = player_get_local_player();
    if (pp == nullptr || human_teams_get_disposition(pp, np) != TEAM_DISPOSITION_HOSTILE)
    {
        return;
    }
    object_squad* squad = np->get_ai()->get_squad();
    if (squad == nullptr)
    {
        return;
    }
    const gfm_orders* orders = squad->get_squad_orders();
    if (orders == nullptr || orders->get_orders_type() == GFM_ORDERS_TYPE_ATTACK)
    {
        return;
    }
    gfm_orders_creation_params params;
    params.team = np->current_team;
    params.object_target = pp->get_handle();
    params.location_target = pp->pos;
    params.orders_type = GFM_ORDERS_TYPE_ATTACK;
    squad->assign_orders(Orders_manager.create_new_orders(params));
}

// 0x00759220. rfg.exe: LTCG, npc_flags in ECX, hflags in EAX.
static void create_default_flags(npc_flags_struct& npc_flags, human_flags& hflags)
{
    memset(&npc_flags, 0, sizeof(npc_flags));
    memset(&hflags, 0, sizeof(hflags));
    hflags.activity_raid_required = 1;
    hflags.activity_raid_optional = 0;
    npc_flags.use_default_team = 1;
    npc_flags.activity_related = 1;
    hflags.activity_engage = 1;
}

// 0x00783850. Original: tests the raid flags after clearing them, so always_show_on_minimap is never cleared.
static void clear_general_raid_flags()
{
    for (int i = 0; i < World.get_num_objects<npc>(); i++)
    {
        npc* np = World.get_object<npc>(i);
        np->hflags.no_damage = 0;
        np->hflags.activity_raid_required = 0;
        np->hflags.activity_raid_optional = 0;
        np->npc_flags.activity_related = 0;
        np->npc_flags.raid_cheer = 0;
        np->npc_flags.raid_watch = 0;
    }
}

// 0x007AE3A0. No enemy alert while an npc still has a target.
void maybe_set_alert_level_green()
{
    for (int i = 0; i < World.get_num_objects<npc>(); i++)
    {
        const npc* np = World.get_object<npc>(i);
        if (np->get_ai() != nullptr && np->get_ai()->perception.belief_bins[AI_BELIEF_ENEMY_POSITION].beliefs != nullptr)
        {
            return;
        }
    }
    public_address_ignore_next_all_clear_message();
    gsm_set_alert_level(ALERT_LEVEL_GREEN); // rfg.exe inlines it
}

// 0x00798CE0 (:536). Takes turns among the layer's regions that allow this team.
const object_spawn_region* activity_raid_data::find_spawn_region(human_teams team, bool vehicle_spawn)
{
    farray<object_spawn_region*, 32> regions;
    collect_layer_objects<object_spawn_region>(regions);
    for (int i = 0; i < regions.size(); )
    {
        if (regions[i]->team_allowed_to_spawn(team, vehicle_spawn))
        {
            i++;
        }
        else
        {
            regions.remove_unsorted(i);
        }
    }
    if (regions.size() < 1)
    {
        return nullptr;
    }
    spawn_region_idx = (spawn_region_idx + 1) % regions.size();
    return regions[spawn_region_idx];
}

// 0x0076ECB0 (:569). The nearest allied squad node, or else the nearest bounding box.
unsigned int activity_raid_data::find_defend_object(human_teams team, const vector& spawn_pos)
{
    const object* best_box = nullptr;
    float best_box_dist = 3.4028235e+38f;
    const object* best_node = nullptr;
    float best_node_dist = 3.4028235e+38f;
    for (int i = 0; i < layer_objects.size(); i++)
    {
        const object* box = raid_bbox_from_handle(layer_objects[i]);
        if (box != nullptr)
        {
            vector bmin;
            vector bmax;
            box->get_local_bbox(bmin, bmax);
            vector local_pos = spawn_pos;
            vec_change_to_local_coords(local_pos, box->pos, box->orient);
            const float dist = bbox_find_distance_squared(bmin, bmax, local_pos);
            if (dist < best_box_dist)
            {
                best_box = box;
                best_box_dist = dist;
            }
            continue;
        }
        const object_squad_spawn_node* node = raid_squad_node_from_handle(layer_objects[i]);
        if (node != nullptr && human_teams_get_disposition(team, node->get_team()) == TEAM_DISPOSITION_ALLIED)
        {
            const float dist = vec_dist_squared(spawn_pos, node->pos);
            if (dist < best_node_dist)
            {
                best_node = node;
                best_node_dist = dist;
            }
        }
    }
    if (best_node != nullptr)
    {
        return best_node->get_handle();
    }
    if (best_box != nullptr)
    {
        return best_box->get_handle();
    }
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    return 0xFFFFFFFF;
}

// 0x007838C0. rfg.exe: LTCG, raid_objects in ECX. The demolition dummies, nearest first; each goes off one delay later.
static void build_demolition_points(const vector& center, const farray<object const*, 512>& raid_objects, farray<demolition_point, 16>& demolition_points, int demolition_delay_ms)
{
    farray<demolition_point_eval_info, 16> candidates;
    demolition_points.clear();
    for (int i = 0; i < raid_objects.size(); i++)
    {
        const object* objp = raid_objects[i];
        if (objp == nullptr || objp->get_obj_type() != OT_DUMMY || static_cast<const object_dummy*>(objp)->get_dummy_type() != DUMMY_OBJECT_TYPE_DEMOLITION_POINT)
        {
            continue;
        }
        demolition_point_eval_info info;
        info.demo_point.pos = objp->pos;
        info.demo_point.orient = objp->orient;
        info.dist_sqrd = vec_dist_squared(objp->pos, center);
        candidates.add(info);
        if (candidates.size() >= candidates.capacity())
        {
            break;
        }
    }
    while (candidates.size() > 0)
    {
        int nearest = 0;
        for (int i = 1; i < candidates.size(); i++)
        {
            if (candidates[i].dist_sqrd < candidates[nearest].dist_sqrd)
            {
                nearest = i;
            }
        }
        demolition_points.add(candidates[nearest].demo_point);
        candidates.remove(nearest);
    }
    for (int i = 0; i < demolition_points.size(); i++)
    {
        demolition_points[i].time = i * demolition_delay_ms;
    }
}

// 0x007592B0 (:738)
void activity_raid_participant_info::reset()
{
    for (int i = 0; i < 4; i++)
    {
        humans_alive[i] = 0;
        humans_dead[i] = 0;
    }
}

// 0x007592D0 (:744). Magic number: the four teams from HUMAN_TEAM_GUERILLA.
int activity_raid_participant_info::hostile_humans_alive() const
{
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return 0;
    }
    int count = 0;
    for (int team = HUMAN_TEAM_GUERILLA; team < 4; team++)
    {
        if (human_teams_are_enemies(pp, (human_teams)team))
        {
            count += humans_alive[team];
        }
    }
    return count;
}

// 0x00759310 (:764)
int activity_raid_participant_info::alied_humans_alive() const
{
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return 0;
    }
    int count = 0;
    for (int team = HUMAN_TEAM_GUERILLA; team < 4; team++)
    {
        if (human_teams_are_allies(pp, (human_teams)team))
        {
            count += humans_alive[team];
        }
    }
    return count;
}

// 0x00759270 (:698). rfg.exe inlines these four in spawn_raid_objects.
static void spawn_raid_object_vehicle(object_vehicle_spawn_node* veh_spawn_node)
{
    veh_spawn_node->spawn_child_object(0xFFFFFFFF);
}

// 0x00759280 (:706)
static void spawn_raid_object_npc(object_npc_spawn_node* npc_spawn_node)
{
    npc_spawn_node->spawn_child_object();
}

// 0x00759290 (:714)
static void spawn_raid_object_turret(object_turret_spawn_node* tur_spawn_node)
{
    tur_spawn_node->spawn_child_object(0xFFFFFFFF);
}

// 0x007592A0 (:722)
static void spawn_raid_object_dummy(object_dummy* dummy_objp)
{
    dummy_objp->create_dummy_object();
}

// 0x00798DB0 (:791)
void activity_raid_data::spawn_raid_objects(const object_raid_node* raid_node)
{
    farray<object_npc_spawn_node*, 64> npc_nodes;
    farray<object_vehicle_spawn_node*, 64> vehicle_nodes;
    farray<object_turret_spawn_node*, 64> turret_nodes;
    farray<object_dummy*, 256> dummies;
    collect_layer_objects<object_npc_spawn_node>(npc_nodes);
    collect_layer_objects<object_vehicle_spawn_node>(vehicle_nodes);
    collect_layer_objects<object_turret_spawn_node>(turret_nodes);
    collect_layer_objects<object_dummy>(dummies);
    for (int i = 0; i < npc_nodes.size(); i++)
    {
        spawn_raid_object_npc(npc_nodes[i]);
    }
    for (int i = 0; i < vehicle_nodes.size(); i++)
    {
        spawn_raid_object_vehicle(vehicle_nodes[i]);
    }
    for (int i = 0; i < turret_nodes.size(); i++)
    {
        spawn_raid_object_turret(turret_nodes[i]);
    }
    for (int i = 0; i < dummies.size(); i++)
    {
        spawn_raid_object_dummy(dummies[i]);
    }
}

// 0x00783A70 (:824)
const player* activity_raid_data::player_in_raid_region()
{
    if (default_raid_info == nullptr)
    {
        return nullptr;
    }
    for (int i = 0; i < World.get_num_objects<player>(); i++)
    {
        const player* pp = World.get_object<player>(i);
        if (is_inside_region(pp->pos))
        {
            return pp;
        }
    }
    return nullptr;
}

// 0x00759350 (:842)
const activity_raid_participant_info* activity_raid_data::get_participant_info() const
{
    return is_active() && raid_flags.raid_started ? &participant_info : nullptr;
}

// 0x00759380 (:856)
void activity_raid_data::system_init()
{
    Raid_flee_timer_handle = 0xFFFFFFFF;
}

// Stops the timer that sends the player back into the raid's region.
static void delete_raid_flee_timer()
{
    if (Raid_flee_timer_handle != 0xFFFFFFFF)
    {
        script_timer_delete(Raid_flee_timer_handle);
        Raid_flee_timer_handle = 0xFFFFFFFF;
    }
}

// 0x0076EE70 (:864)
void activity_raid_data::level_init()
{
    activity_base_modal::level_init(); // rfg.exe inlines it
    All_raid_targets.clear();
    delete_raid_flee_timer();
}

// 0x007A94F0 (:879)
void activity_raid_data::activity_reset()
{
    activity_base_modal::activity_reset();
    for (int i = 0; i < reinforcement_squad_list.size(); i++)
    {
        gfm_orders* orders = Orders_manager.get_orders_from_handle(reinforcement_squad_list[i].orders_handle);
        if (orders != nullptr)
        {
            Orders_manager.destroy_orders(orders);
        }
        reinforcement_squad_list[i].reset();
    }
    reinforcement_squad_list.clear();
    spawn_node_list.clear();
    for (int i = 0; i < 4; i++)
    {
        enemy_contacts[i] = 0xFFFFFFFF;
    }
    external_dest_list.clear();
    exit_cell_list.clear();
    raid_duration.invalidate();
    raid_termination_period.invalidate();
    victory_polling_timer.invalidate();
    spawn_polling_timer.set(0);
    accelerated_spawn_timeout.set(0);
    npc_polling_timer.set(0);
    belief_timer.set(0);
    attacker_orders_evaluation.set(0);
    defender_orders_evaluation.set(0);
    raid_celebration_duration.set(0);
    demolish_building_timer.invalidate();
    conversion_delay.set(0);
    raid_center.zero();
    raid_object_orient.make_identity();
    orders_location.zero();
    memset(&raid_flags, 0, sizeof(raid_flags));
    voice_flags = 0;
    radio_flags = 0;
    spawn_region_idx = 0;
    create_default_flags(npc_flags, hflags); // rfg.exe inlines it
    raid_handle = 0xFFFFFFFF;
    default_raid_info = nullptr;
    for (int i = 0; i < 5; i++)
    {
        attacker_bounding_boxes[i] = 0xFFFFFFFF;
        defender_bounding_boxes[i] = 0xFFFFFFFF;
    }
    stat_tracking_period.set(0);
    participant_info.reset();
    attacker_value = 0.0f;
    defender_value = 0.0f;
    defenders_alive = 0;
    attackers_alive = 0;
    attacker_state = RAID_TEAM_STATE_WAIT;
    defender_state = RAID_TEAM_STATE_WAIT;
    player_is_marauder = false;
    delete_raid_flee_timer();
}

// 0x00759390 (:973)
void activity_raid_data::declare_winner(human_teams winning_team)
{
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    if (human_teams_get_disposition(pp, winning_team) == TEAM_DISPOSITION_ALLIED)
    {
        record_victory();
        activity_end();
    }
    else
    {
        record_loss();
        if (hud_ui_activity_complete_start(raid_flags.offensive_raid ? ACTIVITY_MODE_RAID_ATTACK : ACTIVITY_MODE_RAID_DEFEND, false))
        {
            raid_flags.splash_screen_active = 1;
        }
        else
        {
            activity_end();
        }
    }
    raid_complete_event data;
    data.raid_handle = raid_handle;
    data.winning_team = winning_team;
    gem_post_event(data);
}

// 0x007AE4A0 (:1013)
void activity_raid_data::clean_raid()
{
    convert_to_ambient();
    for (int i = 0; i < reinforcement_squad_list.size(); i++)
    {
        gfm_orders* orders = Orders_manager.get_orders_from_handle(reinforcement_squad_list[i].orders_handle);
        if (orders != nullptr)
        {
            Orders_manager.destroy_orders(orders);
        }
        object_squad* squad = raid_squad_from_handle(reinforcement_squad_list[i].squad_handle);
        if (squad != nullptr)
        {
            squad->request_despawn_all_members(NPC_DESPAWN_TYPE_MEDUIM, nullptr, true);
        }
    }
    for (int i = 0; i < spawn_node_list.size(); i++)
    {
        const object_squad_spawn_node* node = raid_squad_node_from_handle(spawn_node_list[i]);
        object_squad* squad = node != nullptr ? raid_squad_from_handle(node->get_child_squad_handle()) : nullptr;
        if (squad != nullptr)
        {
            gfm_orders* orders = Orders_manager.get_orders_from_handle(squad->get_orders_handle());
            if (orders != nullptr)
            {
                Orders_manager.destroy_orders(orders);
            }
            squad->request_despawn_all_members(NPC_DESPAWN_TYPE_MEDUIM, nullptr, true);
        }
    }
    clear_all_flags();
    clear_general_raid_flags();
    const player* pp = player_get_local_player();
    if (pp != nullptr && !pp->is_killed())
    {
        if (get_interface_state() == ACTIVITY_INTERFACE_STATE_LOSS)
        {
            if (gsm_get_alert_level() == ALERT_LEVEL_GREEN)
            {
                gsm_set_alert_level(ALERT_LEVEL_YELLOW); // rfg.exe inlines it
            }
        }
        else if (raid_flags.raid_complete)
        {
            maybe_set_alert_level_green();
        }
    }
    Encounter_disabled = true;
    delete_raid_flee_timer();
}

// 0x007AE720 (:1079)
void activity_raid_data::activity_end()
{
    activity_base_modal::activity_end(); // rfg.exe inlines it
    clean_raid();
}

// 0x00798F40 (:1087). Original: ends a second time through activity_end, which finds the state already shut down.
void activity_raid_data::activity_kill()
{
    activity_base_modal::activity_kill(); // rfg.exe inlines it
    if (default_raid_info != nullptr)
    {
        dialogue_manager_cancel(default_raid_info->start_radio_line, true);
    }
    activity_end();
}

// 0x00759440 (:1102)
bool activity_raid_data::is_inside_region(const vector& point) const
{
    return vec_xz_dist_squared(point, raid_center) <= default_raid_info->radius_sqrd;
}

// 0x00759490 (:1110)
bool activity_raid_data::lock_alert_level() const
{
    return is_active();
}

// 0x007594A0 (:1122)
bool activity_raid_data::on_alert(const npc* np) const
{
    return has_raid_flag(np) || is_active();
}

// 0x007594C0 (:1138)
bool activity_raid_data::is_player_cover_blown() const
{
    return is_active();
}

// 0x0076EED0 (:1145)
void activity_raid_data::render_regions() const
{
    static const color RAID_AREA_ACTIVE_COLOR(50, 200, 50, 150);
    static const color RAID_AREA_INACTIVE_COLOR(128, 128, 128, 150);
    if (default_raid_info == nullptr)
    {
        return;
    }
    const bool active = activity_is_active(ACTIVITY_MODE_RAID_DEFEND) || activity_is_active(ACTIVITY_MODE_RAID_ATTACK);
    Cartographer.add_decal(raid_center, MAP_ELEMENT_CIRCLE_FILLED_OUTLINE, default_raid_info->radius, active ? RAID_AREA_ACTIVE_COLOR : RAID_AREA_INACTIVE_COLOR, active);
}

// 0x007594D0 (:1162)
int activity_raid_data::time_until_demolition()
{
    if (is_active() && demolish_building_timer.valid())
    {
        return demolish_building_timer.elapsed() ? 0 : demolish_building_timer.time_until();
    }
    return -1;
}

// 0x0076EFB0 (:1179)
raid_team_states activity_raid_data::get_squad_state(unsigned int squad_handle, raid_team_states default_state) const
{
    for (int i = 0; i < reinforcement_squad_list.size(); i++)
    {
        if (reinforcement_squad_list[i].squad_handle == squad_handle)
        {
            return reinforcement_squad_list[i].orders_state;
        }
    }
    return default_state;
}

// 0x0076F000 (:1195). Original: looks spawn_node_list's node handles up as squads, which finds none, so the second walk
// adds no icon.
void activity_raid_data::render_icons() const
{
    const player* pp = player_get_local_player();
    if (default_raid_info == nullptr || pp == nullptr)
    {
        return;
    }
    for (int i = 0; i < raid_target_list.size(); i++)
    {
        const object* target = World.object_from_handle(raid_target_list[i]);
        if (target == nullptr || target->is_killed())
        {
            continue;
        }
        const object_raid_node* node = World.object_from_handle<object_raid_node>(raid_handle);
        const color& icon_color = node != nullptr && node->get_completed_flag() ? Completed_icon_color : Default_map_item_color;
        // Magic number: the offensive raid clears the low bit of the blue star, which picks the attack icon.
        const minimap_elements icon = (minimap_elements)((~(unsigned int)raid_flags.offensive_raid & MAP_ELEMENT_STAR_BLUE_ICON) | MAP_ELEMENT_RAID_ICON_ALWAYS_SHOW);
        Cartographer.add_temporary_icon(target->pos, icon, true, target->get_handle(), icon_color, false, get_spawn_object_handle());
    }
    if (get_activity_state() != BASE_ACTIVITY_STATE_RUNNING || !reinforcement_should_render_icon())
    {
        return;
    }
    for (int i = 0; i < reinforcement_squad_list.size(); i++)
    {
        const raid_squad_info& info = reinforcement_squad_list[i];
        if (!info.raid_squad_flags.spawned || !human_teams_are_enemies(pp, squad_definition_get_team(info.squad_def)))
        {
            continue;
        }
        const object_squad* squad = World.object_from_handle<object_squad>(info.squad_handle);
        if (squad == nullptr)
        {
            continue;
        }
        const object* marker = nullptr;
        if (squad->simulated())
        {
            marker = squad;
        }
        else if (squad->in_squad_vehicle())
        {
            marker = squad->get_vehicle();
        }
        if (marker != nullptr)
        {
            Cartographer.add_temporary_icon(marker->pos, MAP_ELEMENT_STAR_RED_ICON, true, 0xFFFFFFFF, Default_map_item_color, false, 0xFFFFFFFF);
        }
    }
    for (int i = 0; i < spawn_node_list.size(); i++)
    {
        const object_squad* squad = World.object_from_handle<object_squad>(spawn_node_list[i]);
        if (squad != nullptr && human_teams_are_enemies(pp, squad->get_squad_team()))
        {
            const object* marker = squad->simulated() ? static_cast<const object*>(squad) : (squad->in_squad_vehicle() ? squad->get_vehicle() : nullptr);
            if (marker != nullptr)
            {
                Cartographer.add_temporary_icon(marker->pos, MAP_ELEMENT_STAR_RED_ICON, true, 0xFFFFFFFF, Default_map_item_color, false, 0xFFFFFFFF);
            }
        }
    }
}

// 0x00783B00 (:1299)
void activity_raid_data::do_frame_free_squads()
{
    for (int i = 0; i < spawn_node_list.size(); i++)
    {
        const object_squad_spawn_node* node = raid_squad_node_from_handle(spawn_node_list[i]);
        object_squad* squad = node != nullptr ? raid_squad_from_handle(node->get_child_squad_handle()) : nullptr;
        if (squad != nullptr && !has_raid_flag(squad))
        {
            set_squad_weapons_flags(squad, squad->hflags.activity_raid_required);
        }
    }
}

// 0x00783B90 (:1319)
bool activity_raid_data::activity_begin_start_up()
{
    return activity_base_modal::activity_begin_start_up();
}

// 0x00783BA0 (:1330). An enemy the player kills before the raid starts starts it.
void activity_raid_data::do_frame_ready_npc_participants()
{
    if (!npc_polling_timer.elapsed())
    {
        return;
    }
    npc_polling_timer.set(500); // Magic number: twice a second
    const player* pp = player_get_local_player();
    bool player_struck_first = false;
    for (int i = 0; i < World.get_num_objects<npc>(); i++)
    {
        npc* np = World.get_object<npc>(i);
        if (has_raid_flag(np) || activity_raid_is_in_raid_region(np->pos))
        {
            np->hflags.no_damage = 1;
            if (human_teams_get_disposition(pp, np) == TEAM_DISPOSITION_HOSTILE && np->is_killed() && np->combat.last_killed_by_handle == pp->get_handle())
            {
                player_struck_first = true;
            }
        }
        else
        {
            np->hflags.no_damage = 0;
        }
    }
    if (player_struck_first)
    {
        hflags.activity_engage = 1;
        if (!activity_begin_start_up())
        {
            activity_kill();
        }
    }
}

// 0x007AE740 (:1375)
void activity_raid_data::do_frame_npc_participants()
{
    if (!npc_polling_timer.elapsed())
    {
        return;
    }
    npc_polling_timer.set(500);
    const bool attack_orders = should_give_edf_attack_orders();
    init_straggler_tracking_check();
    for (int i = 0; i < World.get_num_objects<npc>(); i++)
    {
        npc* np = World.get_object<npc>(i);
        if (has_raid_flag(np) && !np->npc_flags.entered_raid)
        {
            np->npc_flags.entered_raid = is_inside_region(np->pos);
        }
        do_straggler_tracking(np);
        if (has_raid_flag(np) || (!human_team_is_non_combatant(np) && is_inside_region(np->pos)))
        {
            if (!raid_flags.raid_complete)
            {
                human_set_combat_ready(np, true, 5000); // Magic number: five more seconds
            }
            if (attack_orders)
            {
                give_edf_attack_orders(np);
            }
        }
    }
}

// 0x00783CC0 (:1413)
void activity_raid_data::activity_do_frame_ready()
{
    activity_base::activity_do_frame_ready();
    do_frame_free_squads();
    do_frame_ready_npc_participants();
    if (default_raid_info != nullptr)
    {
        do_frame_player_wait();
    }
}

// 0x00783CF0 (:1425)
activity_load_state activity_raid_data::activity_do_frame_loading()
{
    const activity_load_state load_state = activity_base::activity_do_frame_loading();
    if (load_state != ACTIVITY_LOAD_STATE_COMPLETE)
    {
        return load_state;
    }
    const object* node = get_spawn_object_handle() != 0xFFFFFFFF ? World.object_from_handle(get_spawn_object_handle()) : nullptr;
    return node != nullptr && node->get_obj_type() == OT_RAID_NODE ? ACTIVITY_LOAD_STATE_COMPLETE : ACTIVITY_LOAD_STATE_FAILED;
}

// 0x0076F250 (:1447)
bool activity_raid_data::is_inside_team_orders(const vector& pos, human_teams team) const
{
    unsigned int handle;
    if (is_attacking_team(team))
    {
        if ((unsigned int)attacker_state > 4)
        {
            return true;
        }
        handle = attacker_bounding_boxes[attacker_state];
    }
    else
    {
        if (!is_defending_team(team) || (unsigned int)defender_state > 4)
        {
            return true;
        }
        handle = defender_bounding_boxes[defender_state];
    }
    const object* box = raid_bbox_from_handle(handle);
    if (box == nullptr)
    {
        return true;
    }
    vector bmin;
    vector bmax;
    box->get_local_bbox(bmin, bmax);
    return ix_point_in_box_oriented(pos, box->pos, box->orient, bmin, bmax);
}

// 0x00783D30 (:1477)
void activity_raid_data::do_frame_convert_squads()
{
    if (!conversion_delay.elapsed())
    {
        return;
    }
    conversion_delay.set_rand(1000, 3000); // Magic number: one to three seconds
    farray<squad_node_pair, 32> convert;
    for (int i = 0; i < spawn_node_list.size(); i++)
    {
        squad_node_pair pair;
        pair.node = raid_squad_node_from_handle(spawn_node_list[i]);
        if (pair.node == nullptr)
        {
            continue;
        }
        pair.squad = raid_squad_from_handle(pair.node->get_child_squad_handle());
        if (pair.squad == nullptr || pair.squad->is_hands_off_raid_squad())
        {
            continue;
        }
        const gfm_orders* orders = pair.squad->get_squad_orders();
        const object* target = orders != nullptr ? orders->get_target_object() : nullptr;
        if (target != nullptr && !is_inside_team_orders(target->pos, pair.squad->get_squad_team()))
        {
            convert.add(pair);
        }
    }
    for (int i = 0; i < convert.size(); i++)
    {
        object_squad* squad = convert[i].squad;
        spawn_node_list.remove_matching(convert[i].node->get_handle());
        accept_new_squad(squad);
    }
}

// 0x007B82E0 (:1541)
void activity_raid_data::activity_do_frame_active()
{
    if (raid_flags.splash_screen_active || !is_active() || !raid_flags.raid_started)
    {
        return;
    }
    if (!raid_duration.valid() && !raid_termination_period.valid())
    {
        return;
    }
    do_frame_player_active();
    if (!is_active())
    {
        return;
    }
    do_frame_free_squads();
    do_frame_convert_squads();
    do_frame_orders();
    do_frame_npc_participants();
    do_frame_stat_tracking();
    do_frame_spawning();
    check_npc_beliefs();
    if (demolish_building_timer.valid())
    {
        do_frame_demolish_building();
    }
    else
    {
        do_frame_referee();
    }
}

// 0x00759510 (:1580)
bool activity_raid_data::is_defending_team(human_teams team) const
{
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    return false;
}

// 0x00759530 (:1586)
bool activity_raid_data::is_defending_team(const human* hp) const
{
    return hp != nullptr && is_defending_team(hp->current_team);
}

// 0x00759560 (:1592)
bool activity_raid_data::is_attacking_team(human_teams team) const
{
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    return false;
}

// 0x00759580 (:1598)
bool activity_raid_data::is_attacking_team(const human* hp) const
{
    return hp != nullptr && is_attacking_team(hp->current_team);
}

// 0x007A1DD0 (:1610)
void activity_raid_data::play_raid_line(human_teams speaker_team, raid_voice_types voice_type)
{
    if ((unsigned int)voice_type >= NUM_RAID_VOICE_TYPES || player_is_marauder)
    {
        return;
    }
    const int side = is_defending_team(speaker_team) ? 1 : 0;
    if (side == 0 && !is_attacking_team(speaker_team))
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    const raid_voice_info& lines = Raid_voice_lines[side][voice_type];
    const float near_dist = Raid_voice_distance * Raid_voice_distance;
    farray<npc*, 100> npcs;
    collect_all_raid_npcs(npcs);
    npc* speaker = nullptr;
    float speaker_dist = 3.4028235e+38f;
    bool use_radio = true;
    for (int i = 0; i < npcs.size(); i++)
    {
        npc* np = npcs[i];
        if (np->is_killed() || !human_teams_are_allies(np, speaker_team))
        {
            continue;
        }
        float dist = vec_dist_squared(np->pos, pp->pos);
        bool radio;
        if (dist <= near_dist)
        {
            radio = false;
            if (!np->voice->has_line(lines.spoken_line))
            {
                if (!np->voice->has_line(lines.radio_line))
                {
                    continue;
                }
                // Original: a nearby npc with only the radio line counts as being at the far edge.
                if (dist - near_dist < 0.0f)
                {
                    dist = near_dist;
                }
                radio = true;
            }
        }
        else
        {
            radio = true;
            if (!np->voice->has_line(lines.radio_line))
            {
                continue;
            }
        }
        if (dist < speaker_dist)
        {
            speaker_dist = dist;
            speaker = np;
            use_radio = radio;
        }
    }
    if (speaker == nullptr)
    {
        return;
    }
    const voice_lines line = use_radio ? lines.radio_line : lines.spoken_line;
    if (line == VOICE_LINE_NONE)
    {
        return;
    }
    const voice_line_handle handle = voice_line_handle_from_line(line);
    if (handle == INVALID_VOICE_LINE_HANDLE)
    {
        return;
    }
    dialogue_manager_start_dialogue(handle, speaker, DIALOGUE_PRIORITY_HIGH);
    if (line == VOICE_LINE_RAID_PUSH_FORWARD)
    {
        play_raid_line_response(speaker, handle, VOICE_LINE_RAID_PUSH_FORWARD_RESPONSE);
    }
    else if (line == VOICE_LINE_RAID_FALL_BACK)
    {
        play_raid_line_response(speaker, handle, VOICE_LINE_RAID_FALL_BACK_RESPONSE);
    }
}

// 0x00798F80 (:1716)
void activity_raid_data::play_raid_line_response(const npc* call_np, voice_line_handle call_line, voice_lines response_line)
{
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    farray<npc*, 100> npcs;
    collect_all_raid_npcs(npcs);
    npc* speaker = nullptr;
    float speaker_dist = 3.4028235e+38f;
    for (int i = 0; i < npcs.size(); i++)
    {
        npc* np = npcs[i];
        if (np->is_killed() || np == call_np || human_teams_get_disposition(np, call_np) != TEAM_DISPOSITION_ALLIED)
        {
            continue;
        }
        const float dist = vec_dist_squared(np->pos, pp->pos);
        if (dist <= Raid_voice_distance * Raid_voice_distance && np->voice->has_line(response_line) && dist < speaker_dist)
        {
            speaker = np;
            speaker_dist = dist;
        }
    }
    if (speaker != nullptr)
    {
        const voice_line_handle handle = voice_line_handle_from_line(response_line);
        if (handle != INVALID_VOICE_LINE_HANDLE)
        {
            dialogue_manager_add_line(handle, speaker, call_line, 200); // Magic number: 200 ms after the call
        }
    }
}

// 0x0076F320 (:1774)
bool activity_raid_data::human_is_involved_in_raid(const human* hp) const
{
    if (default_raid_info == nullptr)
    {
        return false;
    }
    if (is_inside_region(hp->pos))
    {
        return true;
    }
    return hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_NPC && hp->hflags.activity_raid_required;
}

// 0x007595B0 (:1796)
bool activity_raid_data::npc_is_involved_in_raid(const npc* np) const
{
    return np->hflags.activity_raid_required;
}

// 0x007595D0 (:1807). Original: reads a player that may be null.
void activity_raid_data::do_frame_player_wait()
{
    player* pp = player_get_local_player();
    const bool inside = is_inside_region(pp->pos);
    pp->hflags.activity_engage = inside;
    if (inside)
    {
        if (!activity_begin_start_up())
        {
            activity_kill();
        }
        return;
    }
    // The GPS pointing at this raid keeps it waiting.
    const bool gps_target = Cartographer.get_gps_target_activity_handle() == get_spawn_object_handle();
    if (gps_target && raid_termination_period.time_until() < 5000)
    {
        raid_termination_period.set(5000); // Magic number: five seconds
    }
    if (raid_termination_period.elapsed() && !gps_target)
    {
        activity_kill();
    }
}

// 0x00783F10 (:1848)
void activity_raid_data::do_frame_player_active()
{
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    if (player_in_raid_region() != nullptr)
    {
        delete_raid_flee_timer();
        return;
    }
    const int hostile = participant_info.hostile_humans_alive();
    if (human_is_ragdolled_or_getting_up(pp) || (hostile < 1 && all_required_spawns_spawned()))
    {
        return;
    }
    if (Raid_flee_timer_handle == 0xFFFFFFFF)
    {
        Raid_flee_timer_handle = script_timer_create(10000); // Magic number: ten seconds to return
        if (Raid_flee_timer_handle != 0xFFFFFFFF)
        {
            script_timer_display(Raid_flee_timer_handle, TP_DEFAULT);
        }
        if (participant_info.alied_humans_alive() > 1)
        {
            human_voice_persona* persona = human_voice_find_persona_with_line(VOICE_LINE_RAID_PLAYER_LEAVES, pp->current_team, true);
            if (persona != nullptr)
            {
                dialogue_manager_start_dialogue(voice_line_handle_from_line(VOICE_LINE_RAID_PLAYER_LEAVES), persona, DIALOGUE_PRIORITY_RADIO, INVALID_PA_HANDLE, nullptr);
            }
        }
    }
    else if (script_timer_get_time(Raid_flee_timer_handle) == 0)
    {
        delete_raid_flee_timer();
        gamelog_set_activity_failed_reason(GLCR_OUT_OF_RANGE);
        declare_winner(HUMAN_TEAM_NONE);
    }
}

// 0x00784040 (:1913)
void activity_raid_data::set_attacker_state(raid_squad_info& squad_info)
{
    if ((unsigned int)attacker_state > 4)
    {
        return;
    }
    squad_info.orders_state = attacker_state;
    gfm_orders* orders = Orders_manager.get_orders_from_handle(squad_info.orders_handle);
    const object* box = orders != nullptr ? raid_bbox_from_handle(attacker_bounding_boxes[attacker_state]) : nullptr;
    if (box != nullptr)
    {
        raid_orders_defend_box(orders, box);
    }
}

// 0x007A2070 (:1938)
void activity_raid_data::play_state_transition_line(human_teams team, raid_team_states new_attacker_state, bool team_is_attacker)
{
    if (raid_flags.raid_complete)
    {
        return;
    }
    if (new_attacker_state == RAID_TEAM_STATE_WITHDRAW)
    {
        play_raid_line(team, team_is_attacker ? RAID_VOICE_PUSH_FORWARD : RAID_VOICE_FALL_BACK);
    }
    else if (new_attacker_state == RAID_TEAM_STATE_ATTACK)
    {
        play_raid_line(team, team_is_attacker ? RAID_VOICE_FALL_BACK : RAID_VOICE_PUSH_FORWARD);
    }
}

// 0x007A20E0 (:1961)
void activity_raid_data::set_attacker_state(raid_team_states new_attacker_state)
{
    if ((unsigned int)(new_attacker_state - RAID_TEAM_STATE_WITHDRAW) >= 4)
    {
        return;
    }
    const raid_team_states old_state = attacker_state;
    attacker_state = new_attacker_state;
    attacker_orders_evaluation.set(0);
    raid_state_change_event data;
    data.raid_handle = raid_handle;
    data.new_state = new_attacker_state;
    for (int team = 0; team < 4; team++)
    {
        if (is_attacking_team((human_teams)team))
        {
            data.team = (human_teams)team;
            gem_post_event(data);
        }
    }
    if (old_state == RAID_TEAM_STATE_WAIT)
    {
        return;
    }
    for (int i = 0; i < reinforcement_squad_list.size(); i++)
    {
        if (reinforcement_squad_list[i].raid_squad_flags.attacking_team)
        {
            play_state_transition_line(squad_definition_get_team(reinforcement_squad_list[i].squad_def), new_attacker_state, true); // rfg.exe inlines it
            return;
        }
    }
}

// 0x007840F0 (:2001)
void activity_raid_data::set_defender_state(raid_squad_info& squad_info)
{
    if ((unsigned int)defender_state > 4)
    {
        return;
    }
    squad_info.orders_state = defender_state;
    gfm_orders* orders = Orders_manager.get_orders_from_handle(squad_info.orders_handle);
    const object* box = orders != nullptr ? raid_bbox_from_handle(defender_bounding_boxes[defender_state]) : nullptr;
    if (box != nullptr)
    {
        raid_orders_defend_box(orders, box);
    }
}

// 0x007A21D0 (:2028)
void activity_raid_data::set_defender_state(raid_team_states new_defender_state)
{
    if ((unsigned int)(new_defender_state - RAID_TEAM_STATE_WITHDRAW) >= 4)
    {
        return;
    }
    const raid_team_states old_state = defender_state;
    defender_state = new_defender_state;
    defender_orders_evaluation.set(0);
    raid_state_change_event data;
    data.raid_handle = raid_handle;
    data.new_state = new_defender_state;
    for (int team = 0; team < 4; team++)
    {
        if (is_defending_team((human_teams)team))
        {
            data.team = (human_teams)team;
            gem_post_event(data);
        }
    }
    if (old_state == RAID_TEAM_STATE_WAIT)
    {
        return;
    }
    for (int i = 0; i < reinforcement_squad_list.size(); i++)
    {
        if (reinforcement_squad_list[i].raid_squad_flags.defending_team)
        {
            play_state_transition_line(squad_definition_get_team(reinforcement_squad_list[i].squad_def), new_defender_state, false); // rfg.exe inlines it
            return;
        }
    }
}

// 0x007A22C0 (:2071)
void activity_raid_data::do_frame_stat_tracking()
{
    if (raid_flags.raid_complete || !stat_tracking_period.elapsed())
    {
        return;
    }
    stat_tracking_period.set_rand(250, 750); // Magic number: a quarter to three quarters of a second
    for (int i = 0; i < 4; i++)
    {
        participant_info.humans_alive[i] = 0;
    }
    for (int i = 0; i < World.get_num_objects<human>(); i++)
    {
        human* hp = World.get_object<human>(i);
        const bool required_npc = hp->get_sub_type() == OT_SUB_HUMAN_NPC && hp->hflags.activity_raid_required;
        if (default_raid_info == nullptr || (!is_inside_region(hp->pos) && !required_npc))
        {
            continue;
        }
        const int team = (int)hp->current_team;
        // Original: one human outside the four teams ends the count for this round.
        if (team < 0 || team > 3)
        {
            return;
        }
        if (hp->is_killed())
        {
            if (!hp->hflags.raid_death_ack)
            {
                participant_info.humans_dead[team]++;
                hp->hflags.raid_death_ack = 1;
            }
        }
        else
        {
            participant_info.humans_alive[team]++;
        }
    }
    for (int i = 0; i < World.get_num_objects<object_squad>(); i++)
    {
        const object_squad* squad = World.get_object<object_squad>(i);
        if (squad->simulated() && squad->hflags.activity_raid_required)
        {
            const human_teams team = squad->get_squad_team();
            if ((unsigned int)team < NUM_HUMAN_TEAMS)
            {
                participant_info.humans_alive[team] += squad->get_max_member_count();
            }
        }
    }
    attackers_alive = 0;
    defenders_alive = 0;
    for (int team = 0; team < 4; team++)
    {
        if (is_defending_team((human_teams)team))
        {
            defenders_alive += participant_info.humans_alive[team];
        }
        else if (is_attacking_team((human_teams)team))
        {
            attackers_alive += participant_info.humans_alive[team];
        }
    }
    const int total = attackers_alive + defenders_alive;
    defender_value = (float)defenders_alive / (float)total;
    attacker_value = (float)attackers_alive / (float)total;
    if (!raid_duration.valid())
    {
        return;
    }
    // Original: both sides compare the number of defenders with their limits.
    const raid_team_states new_attacker_state = defenders_alive < default_raid_info->attacker_min ? RAID_TEAM_STATE_WITHDRAW
        : (default_raid_info->attacker_max < defenders_alive ? RAID_TEAM_STATE_ATTACK : RAID_TEAM_STATE_STANDARD);
    const raid_team_states new_defender_state = defenders_alive < default_raid_info->defender_min ? RAID_TEAM_STATE_WITHDRAW
        : (default_raid_info->defender_max < defenders_alive ? RAID_TEAM_STATE_ATTACK : RAID_TEAM_STATE_STANDARD);
    if (new_attacker_state != attacker_state)
    {
        set_attacker_state(new_attacker_state);
    }
    if (new_defender_state != defender_state)
    {
        set_defender_state(new_defender_state);
    }
}

// 0x0076F370 (:2179). The nearest enemy on the raid's list, or the player when inside the region and nearer.
const human* activity_raid_data::get_enemy_human(const npc* np, const farray<npc*, 100>& npc_list)
{
    const human* best = nullptr;
    float best_dist = 3.4028235e+38f;
    const player* pp = player_get_local_player();
    if (human_teams_get_disposition(np, pp) == TEAM_DISPOSITION_HOSTILE && is_inside_region(pp->pos))
    {
        best = pp;
        best_dist = vec_dist_squared(np->pos, pp->pos);
    }
    for (int i = 0; i < npc_list.size(); i++)
    {
        const npc* other = npc_list[i];
        if (human_teams_get_disposition(other, np) == TEAM_DISPOSITION_HOSTILE)
        {
            const float dist = vec_dist_squared(other->pos, np->pos);
            if (dist < best_dist)
            {
                best = other;
                best_dist = dist;
            }
        }
    }
    return best;
}

// 0x007990D0 (:2216). An npc with nothing to look at is told about the nearest enemy.
void activity_raid_data::check_npc_beliefs()
{
    if (!belief_timer.elapsed())
    {
        return;
    }
    belief_timer.set_rand(1000, 2000); // Magic number: one to two seconds
    farray<npc*, 100> npcs;
    collect_all_raid_npcs(npcs);
    for (int i = 0; i < npcs.size(); i++)
    {
        npc* np = npcs[i];
        ai_system* ai = np->get_ai();
        if (!np->is_killed() && ai->perception.belief_bins[0].beliefs == nullptr)
        {
            const human* enemy = get_enemy_human(np, npcs);
            if (enemy != nullptr && !enemy->is_killed())
            {
                ai->perception.force_search(enemy, enemy->pos);
            }
        }
    }
}

// 0x0076F440 (:2257)
int activity_raid_data::find_shortest_edf_spawn_period(int time_since_start_ms) const
{
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return 0x7FFFFFFF;
    }
    int shortest = 0x7FFFFFFF;
    for (int i = 0; i < reinforcement_squad_list.size(); i++)
    {
        const raid_squad_info& info = reinforcement_squad_list[i];
        if (human_teams_are_enemies(pp, squad_definition_get_team(info.squad_def)) && !info.raid_squad_flags.spawned)
        {
            const int period = info.spawn_delay_ms - time_since_start_ms;
            if (period < shortest)
            {
                shortest = period;
            }
        }
    }
    return shortest;
}

// 0x007B6400 (:2295)
void activity_raid_data::do_frame_spawning()
{
    if (raid_flags.raid_complete)
    {
        return;
    }
    const int elapsed_ms = raid_duration.valid() ? raid_duration.time_since() : 0;
    bool spawned_any = false;
    for (int i = 0; i < reinforcement_squad_list.size(); i++)
    {
        raid_squad_info& info = reinforcement_squad_list[i];
        if (info.raid_squad_flags.spawned || (!info.raid_squad_flags.inital_spawn && info.spawn_delay_ms >= elapsed_ms))
        {
            continue;
        }
        spawn_raid_squad(info);
        spawned_any = true;
        if (elapsed_ms > 0)
        {
            const player* pp = player_get_local_player();
            if (pp == nullptr)
            {
                return;
            }
            const human_team_dispositions disposition = human_teams_get_disposition(pp, squad_definition_get_team(info.squad_def));
            if (disposition == TEAM_DISPOSITION_HOSTILE)
            {
                play_raid_line(pp->current_team, RAID_VOICE_EDF_REINFORCEMENTS);
            }
            else if (disposition == TEAM_DISPOSITION_ALLIED)
            {
                play_raid_line(pp->current_team, RAID_VOICE_GUERRILLA_REINFORCEMENTS);
            }
        }
    }
    // With no enemy left to fight, the next wave comes at once.
    if (elapsed_ms > 1000 && !spawned_any && accelerated_spawn_timeout.elapsed() && participant_info.hostile_humans_alive() < 1)
    {
        const int period = find_shortest_edf_spawn_period(elapsed_ms);
        if (period > 0)
        {
            for (int i = 0; i < reinforcement_squad_list.size(); i++)
            {
                if (!reinforcement_squad_list[i].raid_squad_flags.spawned)
                {
                    reinforcement_squad_list[i].spawn_delay_ms -= period;
                }
            }
            accelerated_spawn_timeout.set(2000); // Magic number: two seconds
        }
    }
}

// 0x0076F4D0 (:2383)
bool activity_raid_data::all_squad_members_spawned() const
{
    for (int i = 0; i < reinforcement_squad_list.size(); i++)
    {
        const raid_squad_info& info = reinforcement_squad_list[i];
        if (info.raid_squad_flags.required && info.raid_squad_flags.spawned)
        {
            const object_squad* squad = raid_squad_from_handle(info.squad_handle);
            if (squad != nullptr && !is_inside_region(squad->pos) && squad->simulated())
            {
                return false;
            }
        }
    }
    return true;
}

// 0x0076F550 (:2409)
bool activity_raid_data::should_demolish_target_building() const
{
    if (default_raid_info->explosive_info == nullptr)
    {
        return false;
    }
    return demolition_points.size() > 0 && !raid_flags.skip_demolition && participant_info.alied_humans_alive() > 1;
}

// 0x007991A0 (:2424)
void activity_raid_data::set_weapons_flag(bool damage_flag, bool grenade_flag, bool turret_flag)
{
    npc_flags.activity_related = 1;
    hflags.no_damage = damage_flag;
    hflags.activity_engage = 1;
    npc_flags.disable_grenades = grenade_flag;
    npc_flags.no_turret_attacks = turret_flag;
    farray<object_squad*, 100> squads;
    collect_all_raid_squads(squads);
    for (int i = 0; i < squads.size(); i++)
    {
        squads[i]->hflags.no_damage = damage_flag;
        squads[i]->npc_flags.disable_grenades = grenade_flag;
        squads[i]->npc_flags.no_turret_attacks = turret_flag;
    }
    farray<npc*, 100> npcs;
    collect_all_raid_npcs(npcs);
    for (int i = 0; i < npcs.size(); i++)
    {
        npcs[i]->hflags.always_show_on_minimap = 1;
        npcs[i]->hflags.no_damage = damage_flag;
        npcs[i]->npc_flags.disable_grenades = grenade_flag;
        npcs[i]->npc_flags.no_turret_attacks = turret_flag;
    }
    if (!damage_flag)
    {
        for (int i = 0; i < World.get_num_objects<npc>(); i++)
        {
            World.get_object<npc>(i)->hflags.no_damage = 0;
        }
    }
}

// 0x007841A0 (:2478)
void activity_raid_data::convert_to_ambient()
{
    for (int i = 0; i < World.get_num_objects<npc>(); i++)
    {
        npc* np = World.get_object<npc>(i);
        if (has_raid_flag(np))
        {
            np->npc_flags.ambient_spawn = 1;
        }
    }
}

// 0x00799380 (:2491)
void activity_raid_data::clear_all_flags()
{
    farray<object_squad*, 100> squads;
    collect_all_raid_squads(squads);
    for (int i = 0; i < squads.size(); i++)
    {
        object_squad* squad = squads[i];
        squad->hflags.no_damage = 0;
        squad->hflags.activity_raid_optional = 0;
        squad->hflags.activity_raid_required = 0;
        squad->hflags.activity_engage = 0;
        squad->npc_flags.activity_related = 0;
        squad->npc_flags.disable_grenades = 0;
        squad->npc_flags.no_turret_attacks = 0;
    }
    farray<npc*, 100> npcs;
    collect_all_raid_npcs(npcs);
    for (int i = 0; i < npcs.size(); i++)
    {
        npc* np = npcs[i];
        np->hflags.no_damage = 0;
        np->hflags.activity_raid_optional = 0;
        np->hflags.activity_raid_required = 0;
        np->hflags.activity_engage = 0;
        np->npc_flags.activity_related = 0;
        np->npc_flags.disable_grenades = 0;
        np->npc_flags.no_turret_attacks = 0;
    }
}

// 0x0076F5A0 (:2529). Marauders are never optional.
void activity_raid_data::set_squad_weapons_flags(object_squad* squad, bool required_squad)
{
    const human_teams team = squad->get_squad_team();
    const bool optional = team != HUMAN_TEAM_MARAUDER && !required_squad;
    squad->hflags = hflags;
    squad->hflags.activity_raid_required = required_squad;
    squad->hflags.activity_raid_optional = optional;
    squad->npc_flags = npc_flags;
    squad->prevent_reassignment();
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        npc* np = raid_npc_from_handle(squad->member_array[i]);
        if (np != nullptr)
        {
            np->npc_flags.disable_grenades = npc_flags.disable_grenades;
            np->npc_flags.activity_related = 1;
            np->npc_flags.no_turret_attacks = npc_flags.no_turret_attacks;
            np->hflags.activity_raid_required = required_squad;
            np->hflags.activity_raid_optional = optional;
            np->hflags.activity_engage = 1;
            np->hflags.no_damage = hflags.no_damage;
        }
    }
}

// 0x00784200 (:2564)
void activity_raid_data::collect_all_raid_squads(farray<object_squad*, 100>& squad_list)
{
    squad_list.clear();
    for (int i = 0; i < World.get_num_objects<object_squad>(); i++)
    {
        object_squad* squad = World.get_object<object_squad>(i);
        if (has_raid_flag(squad))
        {
            if (squad_list.size() >= squad_list.capacity())
            {
                return;
            }
            squad_list.add(squad);
        }
    }
}

// 0x007842A0 (:2581)
void activity_raid_data::collect_all_raid_npcs(farray<npc*, 100>& npc_list)
{
    npc_list.clear();
    for (int i = 0; i < World.get_num_objects<npc>(); i++)
    {
        npc* np = World.get_object<npc>(i);
        if (has_raid_flag(np))
        {
            if (npc_list.size() >= npc_list.capacity())
            {
                return;
            }
            npc_list.add(np);
        }
    }
}

// 0x00799480 (:2598)
void activity_raid_data::do_frame_orders()
{
    if (attacker_orders_evaluation.elapsed())
    {
        do_frame_orders_attacker();
        attacker_orders_evaluation.set(5000); // Magic number: every five seconds
    }
    if (defender_orders_evaluation.elapsed())
    {
        do_frame_orders_defender();
        defender_orders_evaluation.set(5000);
    }
}

// The squad on that side still on old orders that is furthest from the player: only one changes at a time.
// rfg.exe builds a list of orders_evaluation and inlines this in do_frame_orders_attacker and _defender.
static raid_squad_info* raid_find_furthest_stale_squad(farray<raid_squad_info, 32>& squads, bool attacking, raid_team_states state)
{
    const player* pp = player_get_local_player();
    raid_squad_info* best = nullptr;
    float best_dist = 0.0f;
    for (int i = 0; i < squads.size(); i++)
    {
        raid_squad_info& info = squads[i];
        const bool on_side = attacking ? info.raid_squad_flags.attacking_team : info.raid_squad_flags.defending_team;
        if (!on_side || info.orders_state == state)
        {
            continue;
        }
        const object_squad* squad = raid_squad_from_handle(info.squad_handle);
        if (squad != nullptr)
        {
            const float dist = vec_dist_squared(pp->pos, squad->pos);
            if (best == nullptr || best_dist < dist)
            {
                best = &info;
                best_dist = dist;
            }
        }
    }
    return best;
}

// 0x00784340 (:2615)
void activity_raid_data::do_frame_orders_attacker()
{
    raid_squad_info* info = raid_find_furthest_stale_squad(reinforcement_squad_list, true, attacker_state);
    if (info != nullptr)
    {
        set_attacker_state(*info);
    }
}

// 0x00784490 (:2659)
void activity_raid_data::do_frame_orders_defender()
{
    raid_squad_info* info = raid_find_furthest_stale_squad(reinforcement_squad_list, false, defender_state);
    if (info != nullptr)
    {
        set_defender_state(*info);
    }
}

// 0x007994E0 (:2704). Original: dereferences a null player once the points are gone.
void activity_raid_data::do_frame_demolish_building()
{
    if (default_raid_info->explosive_info == nullptr)
    {
        return;
    }
    const int allies = participant_info.alied_humans_alive();
    if (!demolish_building_timer.elapsed() && allies >= 2)
    {
        return;
    }
    queue_raid_celebration();
    const int since = demolish_building_timer.time_since();
    const player* pp = player_get_local_player();
    if (pp == nullptr || allies < 2)
    {
        demolition_points.clear();
    }
    else
    {
        for (int i = 0; i < demolition_points.size(); i++)
        {
            if (demolition_points[i].time <= since)
            {
                explosion_create(default_raid_info->explosive_info, nullptr, nullptr, demolition_points[i].pos, demolition_points[i].orient, nullptr, nullptr, false);
                demolition_points.remove_unsorted(i);
                raid_celebration_duration.set(2000); // Magic number: two seconds after the last blast
                break;
            }
        }
    }
    if (demolition_points.size() < 1 && raid_celebration_duration.elapsed())
    {
        declare_winner(pp->current_team);
    }
}

// 0x007845E0 (:2757)
bool activity_raid_data::start_demolish_building()
{
    const player* pp = player_get_local_player();
    if (pp == nullptr || demolition_points.size() < 1)
    {
        return false;
    }
    demolish_building_timer.set(10000); // Magic number: ten seconds to get clear
    const voice_line_handle line = default_raid_info->demolition_radio_line;
    if (line != INVALID_VOICE_LINE_HANDLE)
    {
        human_voice_persona* persona = human_voice_find_persona_with_line(line, pp->current_team, true);
        if (persona != nullptr)
        {
            dialogue_manager_start_dialogue(line, persona, DIALOGUE_PRIORITY_HIGH, INVALID_PA_HANDLE, nullptr);
        }
    }
    farray<object const*, 16> targets;
    for (int i = 0; i < raid_target_list.size(); i++)
    {
        const object* target = World.object_from_handle(raid_target_list[i]);
        if (target != nullptr && targets.size() < targets.capacity())
        {
            targets.add(target);
        }
    }
    for (int i = 0; i < World.get_num_objects<npc>(); i++)
    {
        npc* np = World.get_object<npc>(i);
        if (!np->is_killed() && is_in_cylindrical_region(np->pos, raid_center, default_raid_info->safe_distance_sqrd))
        {
            for (int t = 0; t < targets.size(); t++)
            {
                np->get_ai()->perception.flag_unsafe_building(targets[t]);
            }
        }
    }
    return true;
}

// 0x0076F740 (:2810). No building left standing: the attackers win.
void activity_raid_data::refree_check_building()
{
    if (raid_flags.raid_complete)
    {
        return;
    }
    for (int i = 0; i < raid_target_list.size(); i++)
    {
        const object* target = raid_target_list[i] != 0xFFFFFFFF ? World.object_from_handle(raid_target_list[i]) : nullptr;
        if (target != nullptr && target->get_obj_type() == OT_MOVER && target->get_sub_type() == OT_SUB_MOVER_RFG && !target->is_killed())
        {
            return;
        }
    }
    gamelog_set_activity_failed_reason(GLCR_BUILDING_DESTROYED);
    raid_flags.attacker_victory = 1;
    raid_flags.skip_demolition = 1;
}

// 0x0076F7C0 (:2842). The decompile drops a block here: the player's side wins when the other side has nobody left.
void activity_raid_data::refree_check_humans()
{
    if (raid_flags.raid_complete || !all_squad_members_spawned())
    {
        return;
    }
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    int defenders = 0;
    int attackers = 0;
    for (int team = 0; team < 4; team++)
    {
        if (is_defending_team((human_teams)team))
        {
            defenders += participant_info.humans_alive[team];
        }
        else if (is_attacking_team((human_teams)team))
        {
            attackers += participant_info.humans_alive[team];
        }
    }
    if (is_attacking_team(pp))
    {
        raid_flags.attacker_victory = raid_flags.attacker_victory || defenders < 1;
        return;
    }
    raid_flags.defender_victory = raid_flags.defender_victory || attackers < 1;
}

// 0x0076F8B0 (:2882)
bool activity_raid_data::all_required_spawns_spawned() const
{
    for (int i = 0; i < reinforcement_squad_list.size(); i++)
    {
        if (reinforcement_squad_list[i].raid_squad_flags.required && !reinforcement_squad_list[i].raid_squad_flags.spawned)
        {
            return false;
        }
    }
    return true;
}

// 0x007A2580 (:2901)
void activity_raid_data::do_frame_referee()
{
    if (!victory_polling_timer.elapsed())
    {
        return;
    }
    victory_polling_timer.set_rand(250, 750);
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    refree_check_building();
    if (all_required_spawns_spawned())
    {
        refree_check_humans();
    }
    const bool player_defends = is_defending_team(pp->current_team);
    const bool player_won = player_defends ? raid_flags.defender_victory : raid_flags.attacker_victory;
    const bool player_lost = player_defends ? raid_flags.attacker_victory : raid_flags.defender_victory;
    raid_flags.raid_complete = player_lost || player_won;
    if (!raid_flags.raid_complete)
    {
        return;
    }
    if (player_won)
    {
        play_raid_line(pp->current_team, RAID_VOICE_WIN);
        if (should_demolish_target_building() && start_demolish_building())
        {
            queue_raid_watch();
            return;
        }
        // Original: says the winning line twice on this path.
        play_raid_line(pp->current_team, RAID_VOICE_WIN);
        declare_winner(pp->current_team);
    }
    else if (player_lost)
    {
        play_raid_line(HUMAN_TEAM_EDF, RAID_VOICE_WIN);
        declare_winner(HUMAN_TEAM_EDF);
        raid_flags.raid_complete = 1;
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
}

// 0x00759690 (:2979)
vector activity_raid_data::pick_point_in_box(const bbox& building_bbox, const vector& size)
{
    vector point;
    point.y = building_bbox.min.y;
    point.x = fl_rand() * size.x + building_bbox.min.x;
    point.z = fl_rand() * size.z + building_bbox.min.z;
    return point;
}

// 0x0076F8F0 (:2999)
void activity_raid_data::shuffle_destination_list()
{
    if (external_dest_list.size() < 2)
    {
        return;
    }
    for (int i = 0; i < external_dest_list.size(); i++)
    {
        const int other = int_rand_range(0, external_dest_list.size() - 1);
        const vector saved = external_dest_list[i];
        external_dest_list[i] = external_dest_list[other];
        external_dest_list[other] = saved;
    }
}

// 0x0076F970 (:3017). Magic number: at least 15 metres from another squad's destination.
bool activity_raid_data::is_far_from_other_orders(const vector& pos)
{
    for (int i = 0; i < reinforcement_squad_list.size(); i++)
    {
        const vector& other = reinforcement_squad_list[i].orders_destination_loc;
        if (!other.is_null() && vec_xz_dist_squared(pos, other) < 225.0f)
        {
            return false;
        }
    }
    return true;
}

// 0x0076F9D0 (:3041)
int activity_raid_data::pick_far_destination_location_index()
{
    for (int i = 0; i < external_dest_list.size(); i++)
    {
        if (is_far_from_other_orders(external_dest_list[i]))
        {
            return i;
        }
    }
    return -1;
}

// 0x0076FA10 (:3056)
vector activity_raid_data::pick_destination_location()
{
    if (external_dest_list.size() < 1)
    {
        return orders_location;
    }
    int index = external_dest_list.size() == 1 ? 0 : pick_far_destination_location_index();
    if (index < 0)
    {
        index = 0;
    }
    const vector picked = external_dest_list[index];
    external_dest_list.remove_unsorted(index);
    return picked;
}

// 0x007B2870 (:3089)
bool activity_raid_data::spawn_raid_squad(raid_squad_info& squad_info)
{
    if (squad_info.raid_squad_flags.spawned)
    {
        return true;
    }
    squad_spawn_parameters params;
    params.orders.orders_type = GFM_ORDERS_TYPE_DEFEND;
    object* spawn_objp = World.object_from_handle(squad_info.spawn_object);
    if (spawn_objp == nullptr)
    {
        return true;
    }
    squad_info.raid_squad_flags.spawned = true;
    params.squad.squad_pos = spawn_objp->pos;
    if (is_active())
    {
        const human_teams team = squad_definition_get_team(squad_info.squad_def);
        unsigned int box_handle;
        if (is_defending_team(team))
        {
            if ((unsigned int)defender_state > 4)
            {
                return true;
            }
            box_handle = defender_bounding_boxes[defender_state];
        }
        else
        {
            if (!is_attacking_team(team))
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                return false;
            }
            if ((unsigned int)attacker_state > 4)
            {
                return true;
            }
            box_handle = attacker_bounding_boxes[attacker_state];
        }
        const object* box = World.object_from_handle(box_handle);
        if (box == nullptr)
        {
            return false;
        }
        squad_info.orders_destination_loc = box->pos;
        params.orders.object_target = box->get_handle();
    }
    else
    {
        squad_info.orders_destination_loc = spawn_objp->pos;
        params.orders.object_target = spawn_objp->get_handle();
    }
    params.orders.location_target = squad_info.orders_destination_loc;
    params.squad.squad_def = squad_info.squad_def;
    params.squad.squad_flags.disallow_reassignment = 1;
    params.squad.veh_spawn_group = squad_info.veh_spawn_group;
    params.orders.ofp = ACTIVITY_ORDERS_FLAGS;
    params.squad.spawn_node_handle = spawn_objp->get_handle();
    params.squad.osrp = spawn_objp->get_obj_type() == OT_SPAWN_REGION ? static_cast<object_spawn_region*>(spawn_objp) : nullptr;
    params.spawn_squad();
    if (params.spawned_squad != nullptr && params.created_orders != nullptr)
    {
        squad_info.orders_handle = params.created_orders->get_orders_handle();
        squad_info.raid_squad_flags.spawned = true;
        squad_info.squad_handle = params.spawned_squad->get_handle();
        squad_info.orders_creation_time.set(0);
        set_squad_weapons_flags(params.spawned_squad, squad_info.raid_squad_flags.required);
        return true;
    }
    params.destroy_squad();
    if (params.created_orders != nullptr)
    {
        Orders_manager.destroy_orders(params.created_orders);
    }
    return true;
}

// 0x007995F0 (:3177)
void activity_raid_data::supliment_squad_list(const farray<object const*, 512>& raid_objects)
{
    for (int i = 0; i < raid_objects.size(); i++)
    {
        const object* objp = raid_objects[i];
        if (objp == nullptr || objp->get_obj_type() != OT_SQUAD_SPAWN_NODE)
        {
            continue;
        }
        const object_squad_spawn_node* node = static_cast<const object_squad_spawn_node*>(objp);
        if (node->get_spawn_prob() > 0.0f && node->get_parent() == 0xFFFFFFFF && !node->is_hands_off_raid_squad())
        {
            add_squad_to_list(node);
        }
        else if (spawn_node_list.size() < spawn_node_list.capacity())
        {
            spawn_node_list.add(node->get_handle());
        }
        else
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        }
    }
}

// 0x00784780 (:3212). Original: an attacker gets the defenders' first box and a defender the attackers' first.
bool activity_raid_data::accept_new_squad(object_squad* squad)
{
    if (squad == nullptr || squad->is_hands_off_raid_squad())
    {
        return false;
    }
    raid_squad_info info;
    info.squad_def = squad->definition;
    info.veh_spawn_group = squad->veh_spawn_group;
    const human_teams team = squad->get_squad_team();
    info.raid_squad_flags.attacking_team = is_attacking_team(team);
    info.raid_squad_flags.defending_team = is_defending_team(team);
    if (!info.raid_squad_flags.attacking_team && !info.raid_squad_flags.defending_team)
    {
        return false;
    }
    info.defended_object = info.raid_squad_flags.attacking_team ? defender_bounding_boxes[0] : attacker_bounding_boxes[0];
    info.raid_squad_flags.spawned = true;
    info.squad_handle = squad->get_handle();
    info.orders_handle = squad->get_orders_handle();
    info.orders_creation_time.set(0);
    info.orders_destination_loc = squad->pos;
    reinforcement_squad_list.add(info);
    return true;
}

// 0x007848B0 (:3248)
void activity_raid_data::add_squad_to_list(const object_squad_spawn_node* squad_node)
{
    if (reinforcement_squad_list.size() >= reinforcement_squad_list.capacity() || squad_node->is_hands_off_raid_squad())
    {
        return;
    }
    raid_squad_info info;
    info.squad_def = squad_node->get_info();
    info.veh_spawn_group = squad_node->get_veh_info();
    info.spawn_object = squad_node->get_handle();
    const human_teams team = squad_definition_get_team(info.squad_def);
    if (is_attacking_team(team))
    {
        info.raid_squad_flags.attacking_team = true;
    }
    else if (is_defending_team(team))
    {
        info.raid_squad_flags.defending_team = true;
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
    info.defended_object = squad_node->get_handle();
    info.raid_squad_flags.inital_spawn = true;
    info.raid_squad_flags.guard_spawn_object = true;
    info.raid_squad_flags.required = true;
    reinforcement_squad_list.add(info);
}

// 0x007996F0 (:3280)
void activity_raid_data::add_squad_to_list(const common_raid_squad_info& squad_info)
{
    if (reinforcement_squad_list.size() >= reinforcement_squad_list.capacity())
    {
        return;
    }
    const human_teams team = squad_definition_get_team(squad_info.squad_def);
    if ((unsigned int)team >= 4)
    {
        return;
    }
    raid_squad_info info;
    info.squad_def = squad_info.squad_def;
    info.veh_spawn_group = squad_info.veh_spawn_group;
    const object_spawn_region* region = find_spawn_region(team, info.veh_spawn_group != nullptr);
    if (region == nullptr)
    {
        return;
    }
    info.spawn_object = region->get_handle();
    if (is_attacking_team(team))
    {
        info.raid_squad_flags.attacking_team = true;
    }
    else if (is_defending_team(team))
    {
        info.raid_squad_flags.defending_team = true;
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
    info.defended_object = find_defend_object(team, region->pos);
    if (info.defended_object != 0xFFFFFFFF)
    {
        info.spawn_delay_ms = squad_info.spawn_delay_ms;
        info.raid_squad_flags.required = squad_info.squad_flags.required;
        reinforcement_squad_list.add(info);
    }
}

// 0x007B6580 (:3326)
void activity_raid_data::record_victory()
{
    ui_tool_tips_add_tip("Guerrilla_Action_Complete", 2000, nullptr); // Magic number: two seconds in
    if (default_raid_info != nullptr)
    {
        apply_standard_rewards(default_raid_info->standard_rewards, nullptr);
    }
    object_raid_node* node = raid_handle != 0xFFFFFFFF ? World.object_from_handle<object_raid_node>(raid_handle) : nullptr;
    if (node != nullptr)
    {
        // The topmost raid node above this one.
        for (object* parent = World.object_from_handle(node->get_parent()); parent != nullptr && parent->get_obj_type() == OT_RAID_NODE; parent = World.object_from_handle(parent->get_parent()))
        {
            node = static_cast<object_raid_node*>(parent);
        }
        farray<object_raid_node*, 64> nodes;
        collect_layer_objects<object_raid_node>(nodes);
        if (nodes.size() < nodes.capacity())
        {
            nodes.add(node);
        }
        else
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        }
        for (int i = 0; i < nodes.size(); i++)
        {
            nodes[i]->set_completed_flag();
        }
    }
    activity_base_modal::record_victory();
}

// 0x007849C0 (:3373)
void activity_raid_data::collect_raid_objects(const object* spawn_objp, farray<object const*, 512>& raid_objects)
{
    raid_objects.clear();
    raid_objects.add(spawn_objp);
    for (int i = 0; i < layer_objects.size(); i++)
    {
        const object* objp = World.object_from_handle(layer_objects[i]);
        if (objp != nullptr)
        {
            if (raid_objects.size() < raid_objects.capacity())
            {
                raid_objects.add(objp);
            }
            else
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            }
        }
    }
}

// 0x0076FAE0 (:3393). The largest box of the layer.
const object_bounding_box* activity_raid_data::find_default_bbox() const
{
    const object_bounding_box* best = nullptr;
    float best_volume = 0.0f;
    for (int i = 0; i < layer_objects.size(); i++)
    {
        const object* box = raid_bbox_from_handle(layer_objects[i]);
        if (box != nullptr)
        {
            vector bmin;
            vector bmax;
            box->get_local_bbox(bmin, bmax);
            const float volume = (bmax.y - bmin.y) * (bmax.x - bmin.x) * (bmax.z - bmin.z);
            if (best_volume < volume)
            {
                best = static_cast<const object_bounding_box*>(box);
                best_volume = volume;
            }
        }
    }
    return best;
}

// 0x007B66B0 (:3428)
bool activity_raid_data::activity_initialize(const object* spawn_objp, unsigned int new_district_handle)
{
    if (spawn_objp == nullptr || !activity_base_modal::activity_initialize(spawn_objp, new_district_handle))
    {
        return false;
    }
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return false;
    }
    player_is_marauder = pp->current_team == HUMAN_TEAM_MARAUDER;
    if (spawn_objp->get_obj_type() != OT_RAID_NODE || raid_flags.raid_started)
    {
        return false;
    }
    clear_general_raid_flags();
    farray<object const*, 512> raid_objects;
    collect_raid_objects(spawn_objp, raid_objects);
    raid_target_list.clear();
    for (int i = 0; i < All_raid_targets.size(); i++)
    {
        if (spawn_objp->get_handle() == All_raid_targets[i].raid_handle && raid_target_list.size() < raid_target_list.capacity())
        {
            const rfg_mover* mover = World.object_from_handle<rfg_mover>(All_raid_targets[i].target_handle);
            if (mover != nullptr)
            {
                raid_target_list.add(mover->get_handle());
            }
        }
    }
    if (raid_target_list.size() < 1)
    {
        return false;
    }
    const object_bounding_box* default_box = find_default_bbox();
    if (default_box == nullptr)
    {
        return false;
    }
    for (int i = 0; i < 5; i++)
    {
        attacker_bounding_boxes[i] = default_box->get_handle();
        defender_bounding_boxes[i] = default_box->get_handle();
    }
    raid_flags.raid_started = 1;
    raid_handle = spawn_objp->get_handle();
    default_raid_info = static_cast<const object_raid_node*>(spawn_objp)->get_raid_def();
    raid_center = spawn_objp->pos;
    raid_object_orient = spawn_objp->orient;
    for (int i = 0; i < 4; i++)
    {
        enemy_contacts[i] = 0xFFFFFFFF;
    }
    for (int i = 0; i < default_raid_info->squad_prop_list.size(); i++)
    {
        add_squad_to_list(default_raid_info->squad_prop_list[i]);
    }
    supliment_squad_list(raid_objects);
    if (!gfm_pick_exterior_point(spawn_objp, orders_location))
    {
        orders_location = raid_center;
    }
    raid_build_exit_points(spawn_objp, default_raid_info->safe_distance, activity_raid_get_target_pos(), activity_raid_get_demolition_radius(), exit_cell_list, raid_target_list);
    if (exit_cell_list.size() < 1)
    {
        activity_kill();
        return false;
    }
    external_dest_list.clear();
    raid_build_reinforcement_waypoints(spawn_objp, external_dest_list, raid_center, default_raid_info->radius, raid_target_list);
    if (external_dest_list.size() < 1)
    {
        return false;
    }
    demolition_points.clear();
    build_demolition_points(raid_center, raid_objects, demolition_points, default_raid_info->explosive_delay);
    raid_flags.offensive_raid = !is_defending_team(pp->current_team);
    raid_termination_period.set(default_raid_info->max_startup_delay_ms);
    shuffle_destination_list();
    spawn_raid_objects(static_cast<const object_raid_node*>(spawn_objp));
    do_frame_spawning();
    set_weapons_flag(true, true, true);
    human_voice_persona* persona = human_voice_find_persona_with_line(default_raid_info->start_radio_line, pp->current_team, true);
    if (persona != nullptr)
    {
        dialogue_manager_start_dialogue(default_raid_info->start_radio_line, persona, DIALOGUE_PRIORITY_RADIO, INVALID_PA_HANDLE, get_icon_image_name());
    }
    // Clears the bodies out of the region.
    for (int i = 0; i < World.get_num_objects<npc>(); i++)
    {
        npc* np = World.get_object<npc>(i);
        if (np->is_killed() && activity_raid_is_in_raid_region(np->pos))
        {
            World.destroy_object(np, false);
        }
    }
    return true;
}

// 0x0076FBF0 (:3589)
int activity_raid_data::find_victory_period() const
{
    if (default_raid_info == nullptr)
    {
        return 0;
    }
    int latest = 0;
    for (int i = 0; i < default_raid_info->squad_prop_list.size(); i++)
    {
        const common_raid_squad_info& info = default_raid_info->squad_prop_list[i];
        if (info.squad_flags.required && latest < info.spawn_delay_ms)
        {
            latest = info.spawn_delay_ms;
        }
    }
    return latest + 2000; // Magic number: two seconds after the last required squad
}

// 0x007AE870 (:3616)
bool activity_raid_data::activity_start()
{
    if (!activity_base_modal::activity_start())
    {
        return false;
    }
    const player* pp = player_get_local_player();
    if (pp == nullptr || default_raid_info == nullptr || raid_handle == 0xFFFFFFFF)
    {
        return false;
    }
    const object* node = World.object_from_handle(raid_handle);
    if (node == nullptr || node->get_obj_type() != OT_RAID_NODE)
    {
        return false;
    }
    ui_tool_tips_add_tip(raid_flags.offensive_raid ? "Offensive_Raid" : "Defensive_Raid", 500, nullptr); // Magic number: half a second in
    play_raid_line(pp->current_team, RAID_VOICE_PLAYER_ARRIVES);
    raid_flags.player_entered_region = 1;
    Cartographer.remove_player_marker(false);
    raid_duration.set(0);
    victory_polling_timer.set_rand(250, 750);
    set_weapons_flag(false, default_raid_info->raid_flags.disable_grenades, false);
    // Civilians leave.
    const int count = World.get_num_objects<npc>();
    for (int i = 0; i < count; i++)
    {
        npc* np = World.get_object<npc>(i);
        if (human_team_is_non_combatant(np) && activity_raid_is_in_raid_region(np->pos))
        {
            if (np->get_ai() != nullptr)
            {
                np->get_ai()->pedestrian_info.reset();
            }
            np->start_active_despawn(NPC_DESPAWN_TYPE_LOW, nullptr);
        }
    }
    raid_termination_period.invalidate();
    return true;
}

// 0x00784A60 (:3686). Original: every pass replans the npc the world walk ended on, not the npc just flagged.
void activity_raid_data::queue_raid_watch()
{
    if (raid_flags.watching_started)
    {
        return;
    }
    raid_flags.watching_started = 1;
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    farray<npc*, 100> watchers;
    const float range = default_raid_info->radius + 100.0f; // Magic number: 100 metres past the raid
    const float range_sqrd = range * range;
    npc* last_np = nullptr;
    for (int i = 0; i < World.get_num_objects<npc>(); i++)
    {
        last_np = World.get_object<npc>(i);
        if (has_raid_flag(last_np) || (last_np->npc_flags.ambient_spawn && human_teams_are_friendly(last_np, pp->current_team) && vec_xz_dist_squared(last_np->pos, raid_center) < range_sqrd))
        {
            watchers.add(last_np);
        }
    }
    for (int i = 0; i < watchers.size(); i++)
    {
        watchers[i]->npc_flags.raid_watch = 1;
        last_np->get_ai()->planner.request_replan();
    }
}

// 0x00784BF0 (:3738)
void activity_raid_data::queue_raid_celebration()
{
    if (raid_flags.cheering_started)
    {
        return;
    }
    raid_flags.cheering_started = 1;
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    const float range = default_raid_info->radius + 100.0f; // Magic number: 100 metres past the raid
    for (int i = 0; i < World.get_num_objects<npc>(); i++)
    {
        npc* np = World.get_object<npc>(i);
        if (np->npc_flags.raid_watch)
        {
            np->npc_flags.raid_cheer = 1;
            human_set_combat_ready(np, false, 0);
        }
        else if (np->npc_flags.ambient_spawn && human_teams_are_friendly(np, pp) && vec_xz_dist_squared(np->pos, raid_center) < range * range)
        {
            np->npc_flags.raid_cheer = 1;
            human_set_combat_ready(np, false, 0);
            np->get_ai()->planner.request_replan();
        }
    }
}

// 0x00799830 (:3793)
bool activity_raid_start_raid_local(activity_raid_data& local_raid_system, const object_raid_node* raid_node)
{
    if (raid_node == nullptr || local_raid_system.is_active() || !local_raid_system.can_initialize())
    {
        return false;
    }
    const unsigned int handle = activity_int_get_district_handle_from_node(raid_node);
    district* dp = handle != 0xFFFFFFFF ? World.object_from_handle<district>(handle) : nullptr;
    if (!local_raid_system.start_activity_spawn(raid_node, dp))
    {
        local_raid_system.activity_kill();
        return false;
    }
    return true;
}

// 0x007998F0 (:3825)
bool activity_raid_start_raid(const object_raid_node* raid_node)
{
    if (raid_node == nullptr)
    {
        return false;
    }
    if (raid_node->is_defensive_raid())
    {
        return activity_raid_start_raid_local(Defensive_raid, raid_node);
    }
    return activity_raid_start_raid_local(Offensive_raid, raid_node);
}

// 0x007596D0 (:3840)
int activity_raid_get_time_until_demolition()
{
    activity_raid_data* raid;
    if (Offensive_raid.mode == activity_get_mode())
    {
        raid = &Offensive_raid;
    }
    else if (Defensive_raid.mode == activity_get_mode())
    {
        raid = &Defensive_raid;
    }
    else
    {
        return 0;
    }
    if (raid->is_active() && raid->demolish_building_timer.valid())
    {
        return raid->demolish_building_timer.elapsed() ? 0 : raid->demolish_building_timer.time_until();
    }
    return -1;
}

// 0x00784D40 (:3855)
bool activity_raid_is_in_demolition_area(const vector& pos)
{
    const activity_raid_data* raid = nullptr;
    if (Offensive_raid.default_raid_info != nullptr && Offensive_raid.demolish_building_timer.valid())
    {
        raid = &Offensive_raid;
    }
    else if (Defensive_raid.default_raid_info != nullptr && Defensive_raid.demolish_building_timer.valid())
    {
        raid = &Defensive_raid;
    }
    if (raid == nullptr)
    {
        return false;
    }
    const object* target = activity_raid_get_target();
    const vector target_pos = target != nullptr ? target->pos : ZERO_VECTOR;
    return vec_xz_dist_squared(pos, target_pos) <= raid->default_raid_info->safe_distance_sqrd;
}

// 0x00759760 (:3873)
bool activity_raid_is_in_raid_region(const vector& pos)
{
    if (Offensive_raid.default_raid_info != nullptr && vec_xz_dist_squared(pos, Offensive_raid.raid_center) <= Offensive_raid.default_raid_info->radius_sqrd)
    {
        return true;
    }
    return Defensive_raid.default_raid_info != nullptr && vec_xz_dist_squared(pos, Defensive_raid.raid_center) <= Defensive_raid.default_raid_info->radius_sqrd;
}

// rfg.exe inlines it in activity_raid_is_demolition_target and activity_raid_get_raid_object.
static bool raid_demolition_running(const activity_raid_data& raid)
{
    return (raid.was_initialized() || raid.is_active()) && raid.demolish_building_timer.valid();
}

// 0x0076FC30 (:3891)
bool activity_raid_is_demolition_target(unsigned int handle)
{
    if (raid_demolition_running(Offensive_raid))
    {
        for (int i = 0; i < Offensive_raid.raid_target_list.size(); i++)
        {
            if (handle == Offensive_raid.raid_target_list[i])
            {
                return true;
            }
        }
    }
    if (raid_demolition_running(Defensive_raid))
    {
        for (int i = 0; i < Defensive_raid.raid_target_list.size(); i++)
        {
            if (handle == Defensive_raid.raid_target_list[i])
            {
                return true;
            }
        }
    }
    return false;
}

// 0x007597E0 (:3919)
object* activity_raid_get_raid_object()
{
    if (raid_demolition_running(Offensive_raid))
    {
        return World.object_from_handle(Offensive_raid.raid_handle);
    }
    if (raid_demolition_running(Defensive_raid))
    {
        return World.object_from_handle(Defensive_raid.raid_handle);
    }
    return nullptr;
}

// 0x00759870 (:3939)
float activity_raid_get_demolition_radius()
{
    if ((Offensive_raid.mode == activity_get_mode() || Offensive_raid.get_state() == BASE_ACTIVITY_STATE_READY) && Offensive_raid.default_raid_info != nullptr)
    {
        return Offensive_raid.default_raid_info->safe_distance;
    }
    if ((Defensive_raid.mode == activity_get_mode() || Defensive_raid.get_state() == BASE_ACTIVITY_STATE_READY) && Defensive_raid.default_raid_info != nullptr)
    {
        return Defensive_raid.default_raid_info->safe_distance;
    }
    return 0.0f;
}

// 0x0076FCF0 (:3957). Original: reads the first exit cell without checking the list has one.
bool activity_raid_get_exit_point(activity_raid_data& local_raid_data, const vector& pos, vector& exit_point)
{
    if (local_raid_data.default_raid_info == nullptr)
    {
        return false;
    }
    int best = 0;
    float best_dist = vec_xz_dist_squared(pos, local_raid_data.exit_cell_list[0].center_pos);
    for (int i = 1; i < local_raid_data.exit_cell_list.size(); i++)
    {
        const float dist = vec_xz_dist_squared(pos, local_raid_data.exit_cell_list[i].center_pos);
        if (dist < best_dist)
        {
            best = i;
            best_dist = dist;
        }
    }
    const nav_cell* ncp = nav_cell_from_handle(local_raid_data.exit_cell_list[best].cell_handle);
    if (ncp == nullptr)
    {
        return false;
    }
    nav_cell_get_random_point(exit_point, ncp);
    return exit_point.is_valid();
}

// 0x0076FDC0 (:4005)
bool activity_raid_get_exit_point(const vector& pos, vector& exit_point)
{
    if (Offensive_raid.mode == activity_get_mode())
    {
        return activity_raid_get_exit_point(Offensive_raid, pos, exit_point);
    }
    if (Defensive_raid.mode == activity_get_mode())
    {
        return activity_raid_get_exit_point(Defensive_raid, pos, exit_point);
    }
    return false;
}

// 0x007598C0 (:4020)
bool activity_raid_is_building_demo_charge(const vector& pos, const explosion_info* e_info)
{
    if (Offensive_raid.default_raid_info != nullptr && e_info == Offensive_raid.default_raid_info->explosive_info && vec_xz_dist_squared(pos, Offensive_raid.raid_center) <= Offensive_raid.default_raid_info->safe_distance_sqrd)
    {
        return true;
    }
    return Defensive_raid.default_raid_info != nullptr && e_info == Defensive_raid.default_raid_info->explosive_info && vec_xz_dist_squared(pos, Defensive_raid.raid_center) <= Defensive_raid.default_raid_info->safe_distance_sqrd;
}

// 0x00759950 (:4038)
unsigned int activity_raid_get_current_node()
{
    if (Offensive_raid.mode == activity_get_mode())
    {
        return Offensive_raid.raid_handle;
    }
    if (Defensive_raid.mode == activity_get_mode())
    {
        return Defensive_raid.raid_handle;
    }
    return 0xFFFFFFFF;
}

// 0x00759980 (:4052)
float activity_raid_victory_percentage()
{
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return 0.0f;
    }
    const activity_raid_data* raid;
    if (Offensive_raid.mode == activity_get_mode())
    {
        raid = &Offensive_raid;
    }
    else if (Defensive_raid.mode == activity_get_mode())
    {
        raid = &Defensive_raid;
    }
    else
    {
        return -1.0f;
    }
    return human_teams_get_disposition(pp, raid->default_raid_info->defending_team) == TEAM_DISPOSITION_ALLIED ? raid->defender_value : raid->attacker_value;
}

// rfg.exe inlines it twice in activity_raid_get_team_totals.
static void raid_add_team_totals(const player* pp, const activity_raid_participant_info& info, int& num_friendly, int& num_enemy)
{
    for (int team = 0; team < 4; team++)
    {
        if (human_teams_are_enemies(pp, (human_teams)team))
        {
            num_enemy += info.humans_alive[team];
        }
        else if (human_teams_are_allies(pp, (human_teams)team))
        {
            num_friendly += info.humans_alive[team];
        }
    }
}

// 0x00759A00 (:4079)
void activity_raid_get_team_totals(int& num_friendly, int& num_enemy)
{
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    num_enemy = 0;
    num_friendly = 0;
    if (Offensive_raid.mode == activity_get_mode())
    {
        raid_add_team_totals(pp, Offensive_raid.participant_info, num_friendly, num_enemy);
    }
    if (Defensive_raid.mode == activity_get_mode())
    {
        raid_add_team_totals(pp, Defensive_raid.participant_info, num_friendly, num_enemy);
    }
}

// 0x0076FE10 (:4114)
bool activity_raid_nodes_are_relatives(unsigned int raid_handle_1, unsigned int raid_handle_2)
{
    if (raid_handle_1 == 0xFFFFFFFF || raid_handle_2 == 0xFFFFFFFF)
    {
        return false;
    }
    const object* node_1 = World.object_from_handle(raid_handle_1);
    if (node_1 == nullptr || node_1->get_obj_type() != OT_RAID_NODE)
    {
        return false;
    }
    const object* node_2 = World.object_from_handle(raid_handle_2);
    if (node_2 == nullptr || node_2->get_obj_type() != OT_RAID_NODE)
    {
        return false;
    }
    return raid_handle_1 == raid_handle_2 || raid_handle_1 == node_2->get_parent() || raid_handle_2 == node_1->get_parent();
}

// rfg.exe inlines it twice in activity_raid_set_regions. Index 0 is RAID_TEAM_STATE_WAIT's box.
static void raid_set_regions(activity_raid_data& raid, unsigned int raid_node_handle, bool attacking_team, unsigned int withdraw_handle, unsigned int standard_handle, unsigned int attack_handle)
{
    if (!activity_raid_nodes_are_relatives(raid.raid_handle, raid_node_handle) || World.object_from_handle(withdraw_handle) == nullptr || World.object_from_handle(standard_handle) == nullptr || World.object_from_handle(attack_handle) == nullptr)
    {
        return;
    }
    unsigned int* boxes = attacking_team ? raid.attacker_bounding_boxes : raid.defender_bounding_boxes;
    boxes[RAID_TEAM_STATE_WITHDRAW] = withdraw_handle;
    boxes[RAID_TEAM_STATE_STANDARD] = standard_handle;
    boxes[RAID_TEAM_STATE_ATTACK] = attack_handle;
}

// 0x0076FE80 (:4147). Original: the offensive side returns early when a box is missing, so the defensive side is
// skipped too; both sides see the same handles, so the outcome is the same.
void activity_raid_set_regions(unsigned int raid_node_handle, bool attacking_team, unsigned int withdraw_handle, unsigned int standard_handle, unsigned int attack_handle)
{
    raid_set_regions(Offensive_raid, raid_node_handle, attacking_team, withdraw_handle, standard_handle, attack_handle);
    raid_set_regions(Defensive_raid, raid_node_handle, attacking_team, withdraw_handle, standard_handle, attack_handle);
}

// 0x00784E60 (:4185). A target of a raid is one of its raid node children too.
void activity_raid_register_target(unsigned int raid_node, unsigned int target_handle)
{
    if (raid_node == 0xFFFFFFFF || target_handle == 0xFFFFFFFF)
    {
        return;
    }
    for (int i = 0; i < All_raid_targets.size(); i++)
    {
        if (All_raid_targets[i].raid_handle == raid_node && All_raid_targets[i].target_handle == target_handle)
        {
            return;
        }
    }
    raid_target_pair pair;
    pair.raid_handle = raid_node;
    pair.target_handle = target_handle;
    All_raid_targets.add(pair);
    const object* node = World.object_from_handle(raid_node);
    if (node == nullptr)
    {
        return;
    }
    const object* first = node->child_ptr;
    for (const object* child = first; child != nullptr; )
    {
        if (node->get_obj_type() == child->get_obj_type())
        {
            activity_raid_register_target(child->get_handle(), target_handle);
        }
        child = child->child_next;
        if (child == first)
        {
            break;
        }
    }
}

// 0x00759AC0 (:4220)
bool activity_raid_has_raid_orders(const npc* np)
{
    const gfm_orders* orders = np->get_ai()->get_orders();
    if (orders == nullptr || orders->get_target_object_handle() == 0xFFFFFFFF)
    {
        return false;
    }
    const unsigned int target = orders->get_target_object_handle();
    for (int i = 0; i < 5; i++)
    {
        if (target == Offensive_raid.attacker_bounding_boxes[i] || target == Offensive_raid.defender_bounding_boxes[i] || target == Defensive_raid.attacker_bounding_boxes[i] || target == Defensive_raid.defender_bounding_boxes[i])
        {
            return true;
        }
    }
    return false;
}

// 0x007B2AB0 (:4260)
void activity_raid_exit()
{
    const player* pp = player_get_local_player();
    if (pp != nullptr && !pp->is_killed())
    {
        Defensive_raid.activity_raid_data::activity_end(); // rfg.exe inlines it
        Offensive_raid.offensive_raid_data::activity_end(); // rfg.exe inlines it
        return;
    }
    delete_raid_flee_timer(); // rfg.exe inlines it
}

// 0x00759B10 (:4282)
void activity_raid_replay()
{
    delete_raid_flee_timer(); // rfg.exe inlines it
    if (Defensive_raid.mode == activity_get_mode())
    {
        Activity_node_start_on_load = Defensive_raid.get_spawn_object_handle();
    }
    else if (Offensive_raid.mode == activity_get_mode() || Offensive_raid.get_state() == BASE_ACTIVITY_STATE_READY)
    {
        Activity_node_start_on_load = Offensive_raid.get_spawn_object_handle();
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    World.save_and_reload();
}

// 0x00759BA0 (:4303)
void activity_raid_process_player_death()
{
    if (Defensive_raid.mode == activity_get_mode())
    {
        gamelog_set_activity_failed_reason(GLCR_PLAYER_DIED);
        Defensive_raid.declare_winner(HUMAN_TEAM_EDF);
    }
    else if (Offensive_raid.mode == activity_get_mode() || Offensive_raid.get_state() == BASE_ACTIVITY_STATE_READY)
    {
        gamelog_set_activity_failed_reason(GLCR_PLAYER_DIED);
        Offensive_raid.declare_winner(HUMAN_TEAM_EDF);
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
}

// 0x007B2B30 (:4317). A squad of a raid that is still waiting for the player ends that raid.
void activity_squad_destroy_notify_raid(const object_squad* squad)
{
    if (squad == nullptr)
    {
        return;
    }
    if (Offensive_raid.get_state() == BASE_ACTIVITY_STATE_READY)
    {
        for (int i = 0; i < Offensive_raid.reinforcement_squad_list.size(); i++)
        {
            if (squad->get_handle() == Offensive_raid.reinforcement_squad_list[i].squad_handle)
            {
                Offensive_raid.offensive_raid_data::activity_end();
            }
        }
    }
    if (Defensive_raid.get_state() == BASE_ACTIVITY_STATE_READY)
    {
        for (int i = 0; i < Defensive_raid.reinforcement_squad_list.size(); i++)
        {
            if (squad->get_handle() == Defensive_raid.reinforcement_squad_list[i].squad_handle)
            {
                Defensive_raid.activity_raid_data::activity_end();
            }
        }
    }
}

// 0x0076FF90 (:4356)
void activity_raid_system_save(cfile* save_file)
{
    cf_write_uint32(All_raid_targets.capacity(), save_file);
    cf_write_uint32(All_raid_targets.size(), save_file);
    for (int i = 0; i < All_raid_targets.size(); i++)
    {
        cf_write_uint32(All_raid_targets[i].target_handle, save_file);
        cf_write_uint32(All_raid_targets[i].raid_handle, save_file);
    }
}

// 0x00784F00 (:4376)
void activity_raid_system_load(cfile* load_file)
{
    if ((int)cf_read_uint32(load_file) != All_raid_targets.capacity())
    {
        keen::debug::nativePrintArgs("The number of objects in the file is inconsistent with the number expected. This save file can't be loaded.");
    }
    All_raid_targets.clear();
    const int count = (int)cf_read_uint32(load_file);
    if (count > All_raid_targets.capacity())
    {
        keen::debug::nativePrintArgs("Invalid number of objects in the save game file. This save file is can't be loaded.");
    }
    for (int i = 0; i < count; i++)
    {
        raid_target_pair pair;
        pair.target_handle = cf_read_uint32(load_file);
        pair.raid_handle = cf_read_uint32(load_file);
        if (All_raid_targets.size() < All_raid_targets.capacity())
        {
            All_raid_targets.add(pair);
        }
        else
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        }
    }
}

// 0x00759C10 (:4403)
bool activity_raid_human_has_raid_flags(const human* hp)
{
    return hp != nullptr && (hp->hflags.activity_raid_required || hp->hflags.activity_raid_optional);
}

// 0x0076FFF0 (:4410)
bool activity_raid_team_is_defending(human_teams team)
{
    const player* pp = player_get_local_player();
    if (Defensive_raid.mode == activity_get_mode())
    {
        return pp != nullptr && human_teams_get_disposition(pp, team) == TEAM_DISPOSITION_ALLIED;
    }
    if (Offensive_raid.mode == activity_get_mode())
    {
        return pp != nullptr && human_teams_get_disposition(pp, team) == TEAM_DISPOSITION_HOSTILE;
    }
    return false;
}

// 0x00770050 (:4423)
bool activity_raid_team_is_attacking(human_teams team)
{
    const player* pp = player_get_local_player();
    if (Defensive_raid.mode == activity_get_mode())
    {
        return pp != nullptr && human_teams_get_disposition(pp, team) == TEAM_DISPOSITION_HOSTILE;
    }
    if (Offensive_raid.mode == activity_get_mode())
    {
        return pp != nullptr && human_teams_get_disposition(pp, team) == TEAM_DISPOSITION_ALLIED;
    }
    return false;
}

// 0x007700B0 (:4436). Defenders with nothing to fight yet get ready.
bool activity_raid_npc_should_prepare(const npc* np)
{
    if ((unsigned int)(Defensive_raid.get_state() - BASE_ACTIVITY_STATE_READY) >= 3) // Magic number: ready, starting, active
    {
        return false;
    }
    const player* pp = player_get_local_player();
    if (pp == nullptr || human_teams_get_disposition(pp, np) != TEAM_DISPOSITION_ALLIED || !has_raid_flag(np) || !Defensive_raid.is_defending_team(np->current_team))
    {
        return false;
    }
    return np->get_ai()->perception.belief_bins[AI_BELIEF_ENEMY_POSITION].beliefs == nullptr;
}

// 0x00770120 (:4468). Original: reads the first target without checking the list has one.
object* activity_raid_get_target()
{
    if (Defensive_raid.mode == activity_get_mode() || Defensive_raid.get_state() == BASE_ACTIVITY_STATE_READY)
    {
        object* target = World.object_from_handle(Defensive_raid.raid_target_list[0]);
        if (target != nullptr)
        {
            return target;
        }
    }
    if (Offensive_raid.mode == activity_get_mode() || Offensive_raid.get_state() == BASE_ACTIVITY_STATE_READY)
    {
        return World.object_from_handle(Offensive_raid.raid_target_list[0]);
    }
    return nullptr;
}

// 0x00770180 (:4489)
vector activity_raid_get_target_pos()
{
    const object* target = activity_raid_get_target();
    return target != nullptr ? target->pos : ZERO_VECTOR;
}
