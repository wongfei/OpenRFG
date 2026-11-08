// Original: volition/rfg/code/gameplay/activities/activity_house_arrest.cpp
//
// gameflow.md GF4: rescuing hostages from EDF houses. While no activity runs, do_frame_start_up keeps an informant squad
// at the nearest activity spawn; talking to the informant or attacking the house starts the activity.

#include "volition/rfg/code/gameplay/activities/activity_house_arrest.h"
#include "volition/rfg/code/gameplay/activities/activity_house_arrest_internal.h"

#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/rfg/code/ai/ai_scripting.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_rewards.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gameplay/squad_spawn.h"
#include "volition/rfg/code/gamewide/custom_dlc_data.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/hud/hud_message.h"
#include "volition/rfg/code/hud/hud_ui_activity_complete.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/hud/minimap_icon_info.h"
#include "volition/rfg/code/interface/ui_document_hud_missions.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/rfg/code/objects/humans/human_voice/voice_line_unlocking.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/object_action_node.h"
#include "volition/rfg/code/objects/object_activity_spawn.h"
#include "volition/rfg/code/objects/object_dummy.h"
#include "volition/rfg/code/objects/object_guard_node.h"
#include "volition/rfg/code/objects/object_house_arrest_node.h"
#include "volition/rfg/code/objects/object_patrol.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/object_restricted_area.h"
#include "volition/rfg/code/objects/object_roadblock_node.h"
#include "volition/rfg/code/objects/object_safehouse.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/rfg/code/objects/object_spawn_region.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/scripting/dialogue_manager.h"
#include "volition/rfg/code/scripting/objective_highlight.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/rfg/code/vehicle/vehicle_interaction_info.h"
#include "volition/vfxlib/code/vfx_effect.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/rand.h"

#include <string.h>

extern unsigned int Activity_node_start_on_load; // 0x01651244 (activity.cpp)

activity_house_arrest House_arrest_activity; // 0x022481E0

float Player_kills_hostage_morale_penalty = 5.0f; // 0x01651334

static float Hostage_death_distance = 100.0f; // 0x0165132C
static float Hostage_hp_multiplier = 2.0f; // 0x01651330
static unsigned int Hostage_effect_info = 0xFFFFFFFF; // 0x01651338
static timestamp Begin_activity_timer; // 0x016532AC, set from attacking the house, read in activity_do_frame_ready
static bool Render_hostage_icons = false; // 0x022295C5

static tweak_table_item Tweak_table_item_Hostage_death_distance("Hostage_death_distance", &Hostage_death_distance);
static tweak_table_item Tweak_table_item_Hostage_hp_multiplier("Hostage_hp_multiplier", &Hostage_hp_multiplier);
static tweak_table_item Tweak_table_item_Player_kills_hostage_morale_penalty("Player_kills_hostage_morale_penalty", &Player_kills_hostage_morale_penalty);
static custom_dlc_bool Custom_DLC_boolRender_hostage_icons("Render_hostage_icons", Render_hostage_icons);

// Magic number: FLT_MAX, the distance of no start point.
static const float NO_ACTIVITY_DIST_SQRD = 3.4028235e+38f;

// Magic numbers: the informant spawns between 80 and 200 metres from the player.
static const float INFORMANT_MIN_DIST_SQRD = 6400.0f;
static const float INFORMANT_MAX_DIST_SQRD = 40000.0f;

// 0x00769150 (.h:43)
house_arrest_start_info::house_arrest_start_info()
{
    reset();
}

// 0x007545C0 (.h:49)
void house_arrest_start_info::reset()
{
    ha_node = nullptr;
    ha_spawn_node = nullptr;
    dist_sqrd = NO_ACTIVITY_DIST_SQRD;
}

// 0x00769170 (.h:59)
house_arrest_squad_info::house_arrest_squad_info()
{
    reset();
}

// 0x007545E0 (.h:65)
void house_arrest_squad_info::reset()
{
    squad_handle = 0xFFFFFFFF;
    orders_handle = 0xFFFFFFFF;
    base_squad_data = nullptr;
}

// 0x00769190 (.h:74)
house_arrest_hostage_info::house_arrest_hostage_info()
{
    reset();
}

// 0x00754600 (.h:87)
void house_arrest_hostage_info::reset()
{
    spawn_node_handle = 0xFFFFFFFF;
    hostage_handle = 0xFFFFFFFF;
    action_node_handle = 0xFFFFFFFF;
    objective_highlight_handle = 0xFFFFFFFF;
    hostage_flags.spawned = false;
    hostage_flags.rescued = false;
    hostage_flags.killed = false;
}

// 0x0077E860 (.h:102). Original: the rest stays unset until level_init calls reset.
activity_house_arrest::activity_house_arrest()
    : activity_base_modal(ACTIVITY_MODE_HOUSE_ARREST, false)
{
    memset(&npc_flags, 0, sizeof(npc_flags));
    memset(&hflags, 0, sizeof(hflags));
}

// 0x0077E960
activity_house_arrest::~activity_house_arrest()
{
}

// 0x00782220 (:166)
static bool near_safehouse(const vector& pos)
{
    for (int i = 0; i < World.get_num_objects<object_safehouse>(); i++)
    {
        if (vec_dist_squared(pos, World.get_object<object_safehouse>(i)->pos) < 1225.0f) // Magic number: 35 metres
        {
            return true;
        }
    }
    return false;
}

// 0x0076D270 (:184)
static bool roadblock_created_npc(const npc* np)
{
    const object_squad* squad = const_cast<npc*>(np)->get_ai()->get_squad();
    if (squad == nullptr || squad->spawn_node_handle == 0xFFFFFFFF)
    {
        return false;
    }
    const object_squad_spawn_node* node = World.object_from_handle<object_squad_spawn_node>(squad->spawn_node_handle);
    return node != nullptr && node->is_roadblock_spawn_node();
}

// 0x00758960 (:206). rfg.exe: LTCG, np in ESI.
static void draw_weapon_no_anim(npc* np)
{
    np->npc_flags.prevent_weapon_draw = 0;
    inventory_item* iip = inv_item_get_best_firearm(np);
    if (iip != nullptr)
    {
        np->desired_equipped_inv_item = iip;
        np->set_equipped_inv_item(iip);
    }
}

// 0x007822A0 (:227)
static const object_spawn_region* find_spawn_region(human_teams team, bool vehicle_spawn)
{
    farray<object_spawn_region*, 32> regions;
    for (int i = 0; i < World.get_num_objects<object_spawn_region>(); i++)
    {
        object_spawn_region* region = World.get_object<object_spawn_region>(i);
        if (region->team_allowed_to_spawn(team, vehicle_spawn))
        {
            regions.add(region);
        }
    }
    if (regions.size() < 1)
    {
        return nullptr;
    }
    return regions.size() == 1 ? regions[0] : regions[int_rand_range(0, regions.size() - 1)];
}

// 0x0076D2B0 (:257). rfg.exe: LTCG, objp in EAX, root_handle in ESI.
// Original bug kept: a handle whose object is gone reads through null at parent. Both callers can pass a null objp too.
static bool is_roadblock_child(unsigned int root_handle, const object* objp)
{
    unsigned int handle = objp->get_parent();
    while (handle != 0xFFFFFFFF && handle != root_handle)
    {
        const object* parent_objp = World.object_from_handle(handle);
        if (parent_objp != nullptr && parent_objp->get_obj_type() == OT_ROADBLOCK_NODE)
        {
            return true;
        }
        handle = parent_objp->get_parent();
    }
    return false;
}

// 0x00798330 (:278). Reads the global, not a this.
static void enable_restricted_areas()
{
    farray<object_restricted_area*, 16> areas;
    House_arrest_activity.collect_layer_objects<object_restricted_area>(areas);
    for (int i = 0; i < areas.size(); i++)
    {
        areas[i]->enable(); // rfg.exe inlines it
    }
}

// 0x00758990 (:300)
static bool is_hostage_spawn(const object_npc_spawn_node* npc_spawn_node)
{
    return npc_spawn_node->is_hostage_node();
}

// 0x0076D2E0 (:306). Puts the hostage's flags back if something cleared them.
static void check_hostage_flags(unsigned int hostage_handle)
{
    npc* np = hostage_handle != 0xFFFFFFFF ? World.object_from_handle<npc>(hostage_handle) : nullptr;
    if (np == nullptr || np->npc_flags.activity_ha)
    {
        return;
    }
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    np->npc_flags.activity_related = 1;
    np->npc_flags.activity_ha = 1;
    np->npc_flags.prevent_weapon_draw = 1;
    np->npc_flags.tied_up = 1;
    np->hflags.activity_ha_hostage = 1;
    np->hflags.considered_armed = 1;
    np->hflags.pushes_other_humans = 1;
}

// 0x0076D350 (:326)
bool is_npc_an_escort(const player* pp, const npc* np)
{
    if (pp->escort_squad_handle == 0xFFFFFFFF)
    {
        return false;
    }
    const object_squad* squad = World.object_from_handle<object_squad>(pp->escort_squad_handle);
    if (squad == nullptr)
    {
        return false;
    }
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        if (squad->member_array[i] == np->get_handle())
        {
            return true;
        }
    }
    return false;
}

// 0x007823B0 (:348)
void pick_spawn_point(const object* spawn_objp, const human_spawn_group_info* spawn_group, vector& spawn_point)
{
    farray<const object_npc_spawn_node*, 64> nodes;
    object* child = spawn_objp->child_ptr;
    while (child != nullptr)
    {
        if (child->get_obj_type() == OT_NPC_SPAWN_NODE && static_cast<const object_npc_spawn_node*>(child)->get_info() == spawn_group)
        {
            nodes.add(static_cast<const object_npc_spawn_node*>(child));
        }
        child = child->child_next;
        if (child == spawn_objp->child_ptr)
        {
            break;
        }
    }
    if (nodes.size() > 0)
    {
        spawn_point = nodes[nodes.size() == 1 ? 0 : int_rand_range(0, nodes.size() - 1)]->pos;
        return;
    }
    spawn_point = spawn_objp->pos;
}

// 0x007589A0 (:378). Original: empty, it does not call the base.
void activity_house_arrest::system_init()
{
}

// 0x007824B0 (:384)
void activity_house_arrest::level_init()
{
    activity_base_modal::level_init(); // rfg.exe inlines it
    reset();
    Hostage_effect_info = effects_get_handle("gp_marker_target_a");
    start_up_squad_handle = 0xFFFFFFFF;
    activity_spawn_node_handle = 0xFFFFFFFF;
    house_arrest_node_handle = 0xFFFFFFFF;
}

// 0x0076D3B0 (:407)
bool activity_house_arrest::house_destreamed() const
{
    for (int i = 0; i < hostage_info_list.size(); i++)
    {
        const house_arrest_hostage_info& info = hostage_info_list[i];
        if (info.hostage_flags.killed)
        {
            continue;
        }
        if (info.action_node_handle != 0xFFFFFFFF && World.object_from_handle<object_action_node>(info.action_node_handle) == nullptr)
        {
            return true;
        }
        if (info.hostage_flags.spawned && info.hostage_handle != 0xFFFFFFFF && World.object_from_handle<npc>(info.hostage_handle) == nullptr)
        {
            return true;
        }
    }
    return false;
}

// 0x007B60D0 (:446)
void house_arrest_level_init_post()
{
    House_arrest_activity.do_frame_start_up(true);
}

// 0x0076D450 (:453). Original: leaves leaving_hostage_message_timer alone.
void activity_house_arrest::reset()
{
    ha_node_handle = 0xFFFFFFFF;
    hud_message_handle = 0xFFFFFFFF;
    hostages_killed_by_player = 0;
    prisoners_rescued_total = 0;
    defender_count = 0;
    house_center.zero();
    base_house_arrest_data = nullptr;
    reinforcement_squad_list.clear();
    squad_node_list.clear();
    hostage_info_list.clear();
    hostage_line_period.set(0);
    response_line_period.invalidate();
    last_hostage_line_loc.zero();
    chat_period.invalidate();
    vehicle_complaint_period.invalidate();
    do_frame_polling_period.set(0);
    memset(&ha_flags, 0, sizeof(ha_flags));
    post_spawn_duration.invalidate();
    defender_count_event.set(0);
    hostage_kill_timer.invalidate();
    memset(&npc_flags, 0, sizeof(npc_flags));
    memset(&hflags, 0, sizeof(hflags));
    npc_flags.activity_ha = 1;
    npc_flags.activity_related = 1;
    Begin_activity_timer.invalidate();
    activity_cooldown_period.set(0);
}

// 0x007AD800 (:495)
bool activity_house_arrest::activity_start()
{
    if (Multiplayer || !activity_base_modal::activity_start())
    {
        return false;
    }
    spawn_all_hostages();
    ui_tool_tips_add_tip("House_Arrest", 2000, nullptr); // Magic number: two seconds
    hostage_kill_timer.set(base_house_arrest_data->hostage_kill_time);
    ha_flags.all_hostages_freed = are_all_hostages_freed();
    leaving_hostage_message_timer.set(0);
    return true;
}

// 0x007AD880 (:525)
void activity_house_arrest::activity_end()
{
    activity_base_modal::activity_end(); // rfg.exe inlines it
    clean_house();
}

// 0x007AD8A0 (:533)
void activity_house_arrest::activity_kill()
{
    activity_base_modal::activity_kill(); // rfg.exe inlines it
    delete_prisoners();
    clean_house();
}

// 0x007589B0 (:545)
const char* activity_house_arrest::get_title() const
{
    return "ACT_HA_NAME";
}

// 0x0076D540 (:558). The player's vehicle must seat the player and every hostage alive.
const char* activity_house_arrest::get_subtitle() const
{
    if (!ha_flags.all_hostages_freed)
    {
        return "ACT_HA_FREE";
    }
    const player* pp = player_get_local_player();
    const vehicle* vp = nullptr;
    if (pp != nullptr && pp->vehicle_handle != 0xFFFFFFFF)
    {
        vp = World.object_from_handle<vehicle>(pp->vehicle_handle);
    }
    int alive = 0;
    for (int i = 0; i < hostage_info_list.size(); i++)
    {
        const unsigned int handle = hostage_info_list[i].hostage_handle;
        const npc* np = handle != 0xFFFFFFFF ? World.object_from_handle<npc>(handle) : nullptr;
        if (np != nullptr && !np->is_killed())
        {
            alive++;
        }
    }
    if (vp != nullptr && vp->info != nullptr && vp->info->vi_info != nullptr && vp->info->vi_info->num_seats < alive + 1)
    {
        return "ACT_HA_LARGER_VEHICLE";
    }
    return "ACT_HA_RESCUE";
}

// 0x007589C0 (:583)
const char* activity_house_arrest::get_icon_image_name() const
{
    return get_minimap_bitmap_icon_name(0x17); // Magic number: the house's icon, MAP_ELEMENT_HOUSE_ICON
}

// 0x0076D620 (:588). When time runs out: a hostage not yet spawned ends the activity; otherwise the living hostages join
// the player's team. Nothing here kills them, despite the timer's name.
void activity_house_arrest::check_hostage_execution_timer()
{
    if (!hostage_kill_timer.valid() || !hostage_kill_timer.elapsed())
    {
        return;
    }
    hostage_kill_timer.invalidate();
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    for (int i = 0; i < hostage_info_list.size(); i++)
    {
        if (!hostage_info_list[i].hostage_flags.spawned)
        {
            process_results();
            return;
        }
    }
    for (int i = 0; i < hostage_info_list.size(); i++)
    {
        npc* np = World.object_from_handle<npc>(hostage_info_list[i].hostage_handle);
        if (np != nullptr && !np->is_killed())
        {
            np->set_team(pp->current_team);
        }
    }
}

// 0x0076D700 (:629). A hostage still tied up does not survive falling, for example when the house comes down.
void activity_house_arrest::check_hostage_falling()
{
    for (int i = 0; i < hostage_info_list.size(); i++)
    {
        const unsigned int handle = hostage_info_list[i].hostage_handle;
        npc* np = handle != 0xFFFFFFFF ? World.object_from_handle<npc>(handle) : nullptr;
        if (np != nullptr && !np->is_killed() && np->npc_flags.tied_up && np->hflags.is_falling)
        {
            human_damage(np, nullptr, np->hit_points, DBT_NONE, nullptr, nullptr, true, false);
        }
    }
}

// 0x00782520 (:646). The hostage's action node, the one in the zone or a new one.
bool activity_house_arrest::load_single_hostage_node(const object_npc_spawn_node* npc_node)
{
    house_arrest_hostage_info info;
    info.spawn_node_handle = npc_node->get_handle();
    object* child = npc_node->child_ptr;
    while (child != nullptr)
    {
        if (child->get_obj_type() == OT_ACTION_NODE)
        {
            info.action_node_handle = child->get_handle();
            hostage_info_list.add(info);
            return true;
        }
        child = child->child_next;
        if (child == npc_node->child_ptr)
        {
            break;
        }
    }
    static const vector ACTION_NODE_OFFSET(1.0f, 1.0f, 1.0f); // 0x02273DA8
    const vector node_pos(npc_node->pos.x + ACTION_NODE_OFFSET.x, npc_node->pos.y + ACTION_NODE_OFFSET.y, npc_node->pos.z + ACTION_NODE_OFFSET.z);
    obj_prop_block block(node_pos, npc_node->orient, nullptr);
    block.set(ACTION_NODE_PROP_ANIM_TYPE, "Player Free Hostage");
    object_action_node* action_node = World.create_object<object_action_node>(block, npc_node->get_handle());
    if (action_node == nullptr)
    {
        return false;
    }
    info.action_node_handle = action_node->get_handle();
    hostage_info_list.add(info);
    return true;
}

// 0x00782750 (:686)
activity_load_state activity_house_arrest::activity_do_frame_loading()
{
    const activity_load_state load_state = activity_base::activity_do_frame_loading();
    if (load_state != ACTIVITY_LOAD_STATE_COMPLETE)
    {
        return load_state;
    }
    const unsigned int handle = get_spawn_object_handle();
    return handle != 0xFFFFFFFF && World.object_from_handle<object_house_arrest_node>(handle) != nullptr ? ACTIVITY_LOAD_STATE_COMPLETE : ACTIVITY_LOAD_STATE_FAILED;
}

// 0x007B60E0 (:710). Original: NEAR_HOUSE_DISTANCE (0x02273E44) is set here and read by nothing.
void activity_house_arrest::activity_do_frame_active()
{
    if (ha_flags.splash_screen_active)
    {
        return;
    }
    do_frame_hostages(true);
    do_frame_squad_management();
    do_frame_edf_count();
    play_chat_line();
    if (!do_frame_polling_period.elapsed())
    {
        return;
    }
    do_frame_polling_period.set(250); // Magic number: four times a second
    hostage_draw_weapon();
    if (all_hostages_dead() || all_hostages_saved())
    {
        process_results();
        return;
    }
    const player* pp = player_get_local_player();
    if (pp->current_team != HUMAN_TEAM_MARAUDER && ha_flags.all_hostages_freed && !ha_flags.safe_house_line_played)
    {
        ha_flags.safe_house_line_played = true;
        bool spoken = false;
        for (int i = 0; i < hostage_info_list.size() && !spoken; i++)
        {
            if (hostage_info_list[i].hostage_flags.killed)
            {
                continue;
            }
            npc* np = World.object_from_handle<npc>(hostage_info_list[i].hostage_handle);
            if (np != nullptr)
            {
                dialogue_manager_start_dialogue(voice_line_handle_from_line(VOICE_LINE_HA_HOSTAGE_FREED), np, DIALOGUE_PRIORITY_HIGH);
                chat_period.set_rand(10000, 20000); // Magic numbers: ten to twenty seconds
                spoken = true;
            }
        }
        // Original: with no one left to say the line, this frame skips the three calls below.
        if (!spoken)
        {
            return;
        }
    }
    do_frame_reinforcements();
    check_hostage_execution_timer();
    check_hostage_falling();
}

// 0x00782790 (:791)
unsigned int house_arrest_get_informant_npc()
{
    return House_arrest_activity.get_informant_npc(); // rfg.exe inlines it
}

// 0x007983A0 (:797). Original: -1 with no start point; the squared distance otherwise.
float house_arrest_get_nearest_activity(unsigned int& as_node_handle, unsigned int& ha_node_handle)
{
    house_arrest_start_info start_info;
    if (!House_arrest_activity.get_nearest_activity(start_info))
    {
        as_node_handle = 0xFFFFFFFF;
        ha_node_handle = 0xFFFFFFFF;
        return -1.0f;
    }
    if (start_info.ha_node == nullptr || start_info.ha_spawn_node == nullptr)
    {
        return NO_ACTIVITY_DIST_SQRD;
    }
    as_node_handle = start_info.ha_spawn_node->get_handle();
    ha_node_handle = start_info.ha_node->get_handle();
    return start_info.dist_sqrd;
}

// 0x0076D7B0 (:817)
unsigned int activity_house_arrest::get_informant_npc()
{
    if (start_up_squad_handle == 0xFFFFFFFF)
    {
        return 0xFFFFFFFF;
    }
    const object_squad* squad = World.object_from_handle<object_squad>(start_up_squad_handle);
    if (squad == nullptr)
    {
        return 0xFFFFFFFF;
    }
    const npc* leader = squad->get_leader();
    return leader != nullptr && !leader->is_killed() ? leader->get_handle() : 0xFFFFFFFF;
}

// 0x0076D800 (:842)
object_action_node* house_arrest_get_action_node(object_activity_spawn* node)
{
    if (node == nullptr)
    {
        return nullptr;
    }
    object* child = node->child_ptr;
    while (child != nullptr)
    {
        if (child->get_obj_type() == OT_ACTION_NODE)
        {
            return static_cast<object_action_node*>(child);
        }
        child = child->child_next;
        if (child == node->child_ptr)
        {
            break;
        }
    }
    return nullptr;
}

// 0x007B20B0 (:859). The informant: a marauder for a marauder player, a guerrilla near a safehouse, else a civilian.
void activity_house_arrest::informant_spawn(const house_arrest_start_info& start_info)
{
    static const squad_definition* const CIVILIAN_SQUAD_DEF = squad_definition_from_name("civilian_squad_1"); // 0x02273E24
    static const squad_definition* const GUERRILLA_SQUAD_DEF = squad_definition_from_name("guerilla_squad_1"); // 0x02273E20
    static const squad_definition* const MARAUDER_SQUAD_DEF = squad_definition_from_name("marauder_squad_1"); // 0x02273E1C
    if (start_info.ha_node == nullptr || start_info.ha_spawn_node == nullptr || CIVILIAN_SQUAD_DEF == nullptr || GUERRILLA_SQUAD_DEF == nullptr || MARAUDER_SQUAD_DEF == nullptr)
    {
        return;
    }
    if (start_up_squad_handle != 0xFFFFFFFF || activity_spawn_node_handle != 0xFFFFFFFF || house_arrest_node_handle != 0xFFFFFFFF)
    {
        return;
    }
    squad_spawn_parameters params;
    object_activity_spawn* node = start_info.ha_spawn_node;
    const object_action_node* action_node = house_arrest_get_action_node(node);
    if (action_node == nullptr)
    {
        params.orders.orders_type = GFM_ORDERS_TYPE_WAIT;
        params.orders.location_target = node->pos;
        params.orders.object_target = node->get_handle();
    }
    else
    {
        params.orders.orders_type = GFM_ORDERS_TYPE_AMBIENT_ACTION;
        params.orders.location_target = action_node->pos;
        params.orders.object_target = action_node->get_handle();
    }
    params.squad.hflags.miner_persona_lines = 1;
    params.squad.hflags.do_not_play_ambient_or_greet_lines = 1;
    params.squad.hflags.do_not_convert_to_guerrilla = 1;
    params.squad.npc_flags.amb_watch_player = 1;
    params.squad.squad_pos = node->pos;
    params.squad.squad_orient = node->orient;
    const player* pp = player_get_local_player();
    if (pp->current_team == HUMAN_TEAM_MARAUDER)
    {
        params.squad.squad_def = MARAUDER_SQUAD_DEF;
    }
    else
    {
        params.squad.squad_def = near_safehouse(node->pos) ? GUERRILLA_SQUAD_DEF : CIVILIAN_SQUAD_DEF;
    }
    params.spawn_squad();
    if (params.spawned_squad != nullptr && params.created_orders != nullptr)
    {
        start_up_squad_handle = params.spawned_squad->get_handle();
        activity_spawn_node_handle = node->get_handle();
        house_arrest_node_handle = start_info.ha_node->get_handle();
        return;
    }
    params.destroy_squad();
    params.destroy_orders();
}

// 0x007827E0 (:935)
bool activity_house_arrest::get_nearest_activity(house_arrest_start_info& start_info)
{
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return false;
    }
    start_info.reset();
    for (int i = 0; i < World.get_num_objects<object_activity_spawn>(); i++)
    {
        object_activity_spawn* spawn = World.get_object<object_activity_spawn>(i);
        if (spawn->get_activity_type() != OT_HOUSE_ARREST_NODE || spawn->get_activity_handle() == 0xFFFFFFFF)
        {
            continue;
        }
        object_house_arrest_node* node = World.object_from_handle<object_house_arrest_node>(spawn->get_activity_handle());
        if (node == nullptr || !node->can_spawn())
        {
            continue;
        }
        const float dist = vec_dist_squared(pp->pos, spawn->pos);
        if (dist < start_info.dist_sqrd)
        {
            start_info.ha_node = node;
            start_info.ha_spawn_node = spawn;
            start_info.dist_sqrd = dist;
        }
    }
    return start_info.ha_node != nullptr;
}

// 0x0076D830 (:985). The icon sits on the informant once the player is within 60 metres of a live squad.
void activity_house_arrest::add_start_up_icon_active()
{
    const player* pp = player_get_local_player();
    if (pp == nullptr || activity_spawn_node_handle == 0xFFFFFFFF)
    {
        return;
    }
    const object_squad* squad = start_up_squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(start_up_squad_handle) : nullptr;
    object_activity_spawn* spawn = World.object_from_handle<object_activity_spawn>(activity_spawn_node_handle);
    if (spawn == nullptr)
    {
        return;
    }
    const unsigned int gps_handle = Cartographer.get_gps_target_activity_handle();
    const float dist = vec_dist_squared(pp->pos, spawn->pos);
    const color& icon_color = spawn->get_completed_flag() ? Completed_icon_color : Default_map_item_color;
    if (squad != nullptr && dist < 3600.0f) // Magic number: 60 metres
    {
        Cartographer.add_temporary_icon(squad->pos, MAP_ELEMENT_HOUSE_ICON, false, 0xFFFFFFFF, icon_color, false, squad->get_handle());
        if (spawn->get_handle() == gps_handle)
        {
            Cartographer.add_player_marker(squad->pos, false, squad->get_handle(), false, false);
        }
        return;
    }
    Cartographer.add_temporary_icon(spawn->pos, MAP_ELEMENT_HOUSE_ICON, false, 0xFFFFFFFF, icon_color, false, spawn->get_handle());
    if (start_up_squad_handle != 0xFFFFFFFF && start_up_squad_handle == gps_handle)
    {
        Cartographer.add_player_marker(spawn->pos, false, spawn->get_handle(), false, false);
    }
}

// 0x00782910 (:1053)
void activity_house_arrest::add_start_up_icons()
{
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    add_start_up_icon_active();
    for (int i = 0; i < World.get_num_objects<object_activity_spawn>(); i++)
    {
        object_activity_spawn* spawn = World.get_object<object_activity_spawn>(i);
        if (spawn->get_activity_type() != OT_HOUSE_ARREST_NODE || spawn->get_handle() == activity_spawn_node_handle || spawn->get_activity_handle() == 0xFFFFFFFF)
        {
            continue;
        }
        const object_house_arrest_node* node = World.object_from_handle<object_house_arrest_node>(spawn->get_activity_handle());
        if (node == nullptr || !node->can_spawn())
        {
            continue;
        }
        if (vec_dist_squared(pp->pos, spawn->pos) >= INFORMANT_MIN_DIST_SQRD)
        {
            Cartographer.add_temporary_icon(spawn->pos, MAP_ELEMENT_HOUSE_ICON, false, 0xFFFFFFFF, spawn->get_completed_flag() ? Completed_icon_color : Default_map_item_color, false, spawn->get_handle());
        }
    }
}

// 0x007B2310 (:1113)
void activity_house_arrest::do_frame_start_up_inactive(bool hard_load, house_arrest_start_info& start_info)
{
    if (start_info.dist_sqrd <= INFORMANT_MAX_DIST_SQRD && (hard_load || start_info.dist_sqrd >= INFORMANT_MIN_DIST_SQRD))
    {
        informant_spawn(start_info);
    }
}

// 0x0076D9A0 (:1132). True while the informant still belongs to the nearest start point.
bool activity_house_arrest::do_frame_start_up_active(house_arrest_start_info& start_info)
{
    if (start_info.ha_node == nullptr || start_info.ha_spawn_node == nullptr || start_up_squad_handle == 0xFFFFFFFF)
    {
        return false;
    }
    object_squad* squad = World.object_from_handle<object_squad>(start_up_squad_handle);
    if (squad == nullptr || start_info.ha_spawn_node->get_handle() != activity_spawn_node_handle || start_info.ha_node->get_handle() != house_arrest_node_handle || start_info.dist_sqrd > INFORMANT_MAX_DIST_SQRD)
    {
        return false;
    }
    if (!squad->simulated())
    {
        const npc* leader = squad->get_leader();
        if (leader != nullptr && !leader->is_killed())
        {
            objective_highlight_add(leader->get_handle(), HIGHLIGHT_PURPOSE_ACTIVITY, HIGHLIGHT_STYLE_ARROW);
        }
    }
    return true;
}

// 0x007589D0 (:1176). objective_highlight_remove on a squad also removes its members', so this clears the leader's.
void activity_house_arrest::spawn_effect_kill()
{
    if (start_up_squad_handle != 0xFFFFFFFF)
    {
        objective_highlight_remove(start_up_squad_handle);
    }
}

// 0x0076DA50 (:1185)
void activity_house_arrest::informant_despawn()
{
    spawn_effect_kill();
    if (start_up_squad_handle != 0xFFFFFFFF)
    {
        object_squad* squad = World.object_from_handle<object_squad>(start_up_squad_handle);
        if (squad != nullptr)
        {
            squad->request_despawn_all_members(NPC_DESPAWN_TYPE_LOW, nullptr, true);
        }
    }
    start_up_squad_handle = 0xFFFFFFFF;
}

// 0x0076DAA0 (:1200)
void activity_house_arrest::release_and_reset_squad()
{
    informant_despawn(); // rfg.exe inlines it
    activity_spawn_node_handle = 0xFFFFFFFF;
    house_arrest_node_handle = 0xFFFFFFFF;
}

// 0x007A1D10 (:3138)
bool house_arrest_can_be_started()
{
    // Deviation from rfg.exe: gsm_get_alert_level, where it reads the manager's private Current_alert_level.
    // The getter's clamp does not change this test.
    return !Multiplayer && activity_none() && House_arrest_activity.get_state() == BASE_ACTIVITY_STATE_WAIT && House_arrest_activity.can_initialize() && (int)gsm_get_alert_level() < ALERT_LEVEL_ORANGE && !hud_ui_activity_interface_active();
}

// 0x007B2350 (:1214). rfg.exe inlines house_arrest_can_be_started and do_frame_start_up_inactive.
void activity_house_arrest::do_frame_start_up(bool hard_load)
{
    house_arrest_start_info start_info;
    if (house_arrest_can_be_started())
    {
        add_start_up_icons();
        if (get_nearest_activity(start_info))
        {
            if (start_up_squad_handle == 0xFFFFFFFF)
            {
                do_frame_start_up_inactive(hard_load, start_info);
                return;
            }
            if (do_frame_start_up_active(start_info))
            {
                return;
            }
        }
    }
    release_and_reset_squad();
}

// 0x007B2450 (:1244)
void activity_house_arrest::activity_do_frame_inactive()
{
    if (!Multiplayer)
    {
        do_frame_start_up(false);
    }
}

// 0x0076DB10 (:1256)
void activity_house_arrest::notify_npc_at_safehouse(npc* np)
{
    player* pp = np->get_ai()->is_in_player_squad(false);
    if (pp != nullptr)
    {
        pp->escort_squad_remove(np, true);
    }
    np->npc_flags.recruitable_escort = 0;
    prisoners_rescued_total++;
    np->npc_flags.activity_ha = 0;
    for (int i = 0; i < hostage_info_list.size(); i++)
    {
        if (hostage_info_list[i].hostage_handle == np->get_handle())
        {
            hostage_info_list[i].hostage_flags.rescued = true;
        }
    }
}

// 0x0076DB90 (:1285)
bool activity_house_arrest::is_ha_npc(const npc* np) const
{
    if (np->npc_flags.activity_ha)
    {
        return true;
    }
    return vec_xz_dist_squared(np->pos, house_center) < 5625.0f && roadblock_created_npc(np); // Magic number: 75 metres
}

// 0x00798430 (:1310). Waits near the house. Starts when no hostage is left to free, when the player is seen fighting
// the house's guards, or when the timer that fight set runs out. Lapses when the player has walked away.
void activity_house_arrest::activity_do_frame_ready()
{
    activity_base::activity_do_frame_ready();
    if (hostage_info_list.size() < 1)
    {
        activity_kill();
        return;
    }
    do_frame_hostages(false);
    do_frame_squad_management();
    do_frame_edf_count();
    const unsigned int gps_handle = Cartographer.get_gps_target_activity_handle();
    // Magic numbers: a minute in this state, 100 metres from the house.
    if (get_state_duration() > 60000 && gps_handle != ha_node_handle && player_find_shortest_player_xz_dist_sqrd(house_center) > 10000.0f)
    {
        activity_kill();
        return;
    }
    if (!do_frame_polling_period.elapsed())
    {
        return;
    }
    do_frame_polling_period.set(250); // Magic number: four times a second
    if (house_destreamed())
    {
        activity_kill();
        return;
    }
    int num_total;
    int num_killed;
    count_prisoners(num_total, num_killed);
    if (num_total < 1)
    {
        if (num_killed < 1)
        {
            activity_kill();
            return;
        }
        if (!activity_begin_start_up())
        {
            activity_kill();
        }
        ha_flags.does_not_have_interface = true;
        return;
    }
    if (Begin_activity_timer.valid())
    {
        if (Begin_activity_timer.elapsed() && !activity_begin_start_up())
        {
            activity_kill();
        }
        return;
    }
    const player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        // Magic number: three quarters of the leaving distance. Too far from a hostage, nothing starts.
        const float far_dist = (float)((double)Hostage_death_distance * 0.75 * (double)Hostage_death_distance * 0.75);
        for (int i = 0; i < hostage_info_list.size(); i++)
        {
            const unsigned int handle = hostage_info_list[i].hostage_handle;
            const npc* hostage = handle != 0xFFFFFFFF ? World.object_from_handle<npc>(handle) : nullptr;
            if (hostage != nullptr && vec_xz_dist_squared(pp->pos, hostage->pos) > far_dist)
            {
                return;
            }
        }
    }
    for (int i = 0; i < World.get_num_objects<npc>(); i++)
    {
        npc* np = World.get_object<npc>(i);
        if (human_teams_get_disposition(np, HUMAN_TEAM_EDF) != TEAM_DISPOSITION_ALLIED || !is_ha_npc(np))
        {
            continue;
        }
        if (object_is_player(human_combat_get_target(np)) || (np->is_killed() && object_is_player(np->combat.last_killed_by_handle)))
        {
            Begin_activity_timer.set(1000); // Magic number: a second
            return;
        }
    }
}

// 0x0076DBE0 (:1441). A freed hostage arms only while the player cannot see it happen.
void activity_house_arrest::hostage_draw_weapon()
{
    for (int i = 0; i < hostage_info_list.size(); i++)
    {
        const unsigned int handle = hostage_info_list[i].hostage_handle;
        npc* np = handle != 0xFFFFFFFF ? World.object_from_handle<npc>(handle) : nullptr;
        if (np != nullptr && !np->npc_flags.tied_up && np->npc_flags.prevent_weapon_draw && !np->is_killed() && np->is_offscreen() && np->get_offscreen_period() > 3000) // Magic number: three seconds
        {
            draw_weapon_no_anim(np);
        }
    }
}

// 0x007589F0 (:1470)
static bool in_vehicle_callback(const human* hp, void* data)
{
    return hp != nullptr && !hp->is_killed() && human_in_vehicle(hp) != nullptr;
}

// 0x00782A10 (:1476). Original quirks kept: the player is not checked for null, and vehicle_complaint_period is armed
// only once, so after three minutes every chat in a vehicle far from a safehouse complains.
void activity_house_arrest::play_chat_line()
{
    const player* pp = player_get_local_player();
    if (pp->current_team == HUMAN_TEAM_MARAUDER || !chat_period.valid() || !chat_period.elapsed())
    {
        return;
    }
    chat_period.set_rand(10000, 20000); // Magic numbers: ten to twenty seconds
    const vehicle* player_vehicle = human_in_vehicle(pp);
    farray<npc*, 100> candidates;
    for (int i = 0; i < hostage_info_list.size(); i++)
    {
        const unsigned int handle = hostage_info_list[i].hostage_handle;
        npc* np = handle != 0xFFFFFFFF ? World.object_from_handle<npc>(handle) : nullptr;
        if (np != nullptr && !np->is_killed() && np->get_current_voice_line_handle() == INVALID_VOICE_LINE_HANDLE && human_in_vehicle(np) == player_vehicle)
        {
            candidates.add(np);
        }
    }
    if (candidates.size() < 1)
    {
        return;
    }
    npc* np = candidates[candidates.size() == 1 ? 0 : int_rand_range(0, candidates.size() - 1)];
    const object_safehouse* safehouse = safehouse_find_nearest_safehouse(np->pos, false, false);
    float safehouse_dist = 90100.0f; // Magic number: past the complaint distance below
    if (safehouse != nullptr)
    {
        const double dx = (double)(safehouse->pos.x - np->pos.x);
        const double dz = (double)(safehouse->pos.z - np->pos.z);
        safehouse_dist = (float)(dx * dx + 0.0 + dz * dz);
    }
    if (np->vehicle_handle == 0xFFFFFFFF)
    {
        np->play_voice_line(VOICE_LINE_HA_HOSTAGE_CHAT, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        return;
    }
    if (vehicle_complaint_period.elapsed() && safehouse_dist > 90000.0f) // Magic number: 300 metres
    {
        np->play_voice_line(VOICE_LINE_HA_HOSTAGE_VEH_COMPLAINT, in_vehicle_callback, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
    }
    if (!vehicle_complaint_period.valid())
    {
        vehicle_complaint_period.set(180000); // Magic number: three minutes
    }
}

// 0x00782C70 (:1557). The line goes to the nearest of the activity's npcs on that side with nothing else to say.
void activity_house_arrest::play_house_arrest_line(const vector& pos, human_teams team, voice_lines line)
{
    npc* nearest = nullptr;
    float nearest_dist = NO_ACTIVITY_DIST_SQRD;
    for (int i = 0; i < World.get_num_objects<npc>(); i++)
    {
        npc* np = World.get_object<npc>(i);
        if (!np->npc_flags.activity_ha || np->is_killed() || human_teams_get_disposition(np, team) != TEAM_DISPOSITION_ALLIED || np->get_current_voice_line_handle() != INVALID_VOICE_LINE_HANDLE)
        {
            continue;
        }
        const float dist = vec_dist_squared(pos, np->pos);
        if (dist < nearest_dist)
        {
            nearest = np;
            nearest_dist = dist;
        }
    }
    if (nearest != nullptr)
    {
        nearest->play_voice_line(line, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
    }
}

// 0x0076DCA0 (:1584). The player finished freeing a hostage at its action node.
void activity_house_arrest::report_action_node_end(unsigned int action_node_handle)
{
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    for (int i = 0; i < hostage_info_list.size(); i++)
    {
        if (hostage_info_list[i].action_node_handle != action_node_handle)
        {
            continue;
        }
        npc* np = World.object_from_handle<npc>(hostage_info_list[i].hostage_handle);
        if (np != nullptr && !np->is_killed())
        {
            np->npc_flags.freed_from_bonds = 1;
            np->set_team(pp->current_team);
            pp->escort_squad_add(np);
        }
    }
}

// 0x007A1B70 (:1610)
void activity_house_arrest::report_recruitment(npc* np)
{
    if (Multiplayer || !is_npc_an_escort(player_get_local_player(), np))
    {
        return;
    }
    np->npc_flags.freed_from_bonds = 1;
    stop_objective_highlight(np->get_handle());
    objective_highlight_add(np->get_handle(), HIGHLIGHT_PURPOSE_NEUTRAL, HIGHLIGHT_STYLE_ARROW);
    if (!ha_flags.all_hostages_freed)
    {
        ha_flags.all_hostages_freed = are_all_hostages_freed();
    }
    if (ha_flags.hostage_freed)
    {
        return;
    }
    ha_flags.hostage_freed = true;
    post_spawn_duration.set(0);
    if (!is_active() && !activity_begin_start_up())
    {
        activity_kill();
        return;
    }
    if (ha_node_handle != 0xFFFFFFFF)
    {
        const object_house_arrest_node* ha_node = World.object_from_handle<object_house_arrest_node>(ha_node_handle);
        if (ha_node != nullptr)
        {
            do_spawn_post(ha_node);
        }
    }
}

// 0x00758A20 (:1667)
void activity_house_arrest::do_despawn_hostage(house_arrest_hostage_info& node_info)
{
    if (!node_info.hostage_flags.killed)
    {
        node_info.hostage_flags.spawned = false;
        World.destroy_object(node_info.hostage_handle, false);
        node_info.hostage_handle = 0xFFFFFFFF;
    }
}

// 0x0076DD30 (:1679). Hostages are civilians with twice the hit points.
bool activity_house_arrest::do_spawn_hostage(house_arrest_hostage_info& node_info)
{
    node_info.hostage_flags.spawned = true;
    if (node_info.spawn_node_handle == 0xFFFFFFFF)
    {
        return false;
    }
    object_npc_spawn_node* node = World.object_from_handle<object_npc_spawn_node>(node_info.spawn_node_handle);
    if (node == nullptr)
    {
        return false;
    }
    node_info.hostage_handle = node->spawn_child_object();
    npc* np = node_info.hostage_handle != 0xFFFFFFFF ? World.object_from_handle<npc>(node_info.hostage_handle) : nullptr;
    if (np == nullptr)
    {
        return false;
    }
    np->hflags.driverless_exit_only = 1;
    np->hflags.disallow_vehicle_drive = 1;
    np->set_team(HUMAN_TEAM_CIVILIAN);
    np->npc_flags.activity_related = 1;
    np->npc_flags.activity_ha = 1;
    np->npc_flags.prevent_weapon_draw = 1;
    np->npc_flags.tied_up = 1;
    np->hflags.activity_ha_hostage = 1;
    np->hflags.considered_armed = 1;
    np->hflags.pushes_other_humans = 1;
    const int new_max_hit_points = (int)((double)(float)np->max_hit_points * (double)Hostage_hp_multiplier);
    np->max_hit_points = new_max_hit_points;
    np->hit_points = (float)new_max_hit_points;
    return true;
}

// 0x00758A50 (:1726)
bool activity_house_arrest::create_orders_patrol(const object_squad_spawn_node* spawn_node, const object_patrol* patrol, squad_spawn_parameters& spawn_params)
{
    if (patrol == nullptr)
    {
        return false;
    }
    spawn_params.orders.orders_type = GFM_ORDERS_TYPE_PATROL;
    spawn_params.orders.location_target = patrol->pos;
    spawn_params.orders.object_target = patrol->get_handle();
    return true;
}

// 0x00758A90 (:1736)
void activity_house_arrest::create_orders_squad_node(const object_squad_spawn_node* spawn_node, squad_spawn_parameters& spawn_params)
{
    spawn_params.orders.orders_type = GFM_ORDERS_TYPE_DEFEND;
    spawn_params.orders.location_target = spawn_node->pos;
    spawn_params.orders.object_target = spawn_node->get_handle();
}

// 0x007986B0 (:1745)
void activity_house_arrest::create_orders(const object_squad_spawn_node* spawn_node, squad_spawn_parameters& spawn_params)
{
    if (spawn_node->get_parent() != 0xFFFFFFFF)
    {
        const object_patrol* patrol = World.object_from_handle<object_patrol>(spawn_node->get_parent());
        if (patrol != nullptr)
        {
            create_orders_patrol(spawn_node, patrol, spawn_params); // rfg.exe inlines it
            return;
        }
    }
    const object* parent_objp = World.object_from_handle(spawn_node->get_parent());
    if (parent_objp != nullptr)
    {
        farray<object_guard_node*, 1> guards;
        parent_objp->collect_children<object_guard_node>(guards, nullptr, nullptr);
        if (guards.size() > 0)
        {
            create_orders_squad_node(spawn_node, spawn_params); // rfg.exe inlines it
            return;
        }
    }
    const object* ha_node = World.object_from_handle(ha_node_handle);
    if (ha_node != nullptr)
    {
        spawn_params.orders.orders_type = GFM_ORDERS_TYPE_DEFEND;
        spawn_params.orders.location_target = ha_node->pos;
        spawn_params.orders.object_target = ha_node->get_handle();
        return;
    }
    create_orders_squad_node(spawn_node, spawn_params); // rfg.exe inlines it
}

// 0x0076DE30 (:1790)
void activity_house_arrest::set_squad_flags(object_squad* squad)
{
    squad->hflags = hflags;
    squad->npc_flags = npc_flags;
    squad->prevent_reassignment();
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        const unsigned int handle = squad->member_array[i];
        npc* np = handle != 0xFFFFFFFF ? World.object_from_handle<npc>(handle) : nullptr;
        if (np != nullptr)
        {
            npc_flags_bitwise_or(npc_flags, np->npc_flags, np->npc_flags);
            human_flags_bitwise_or(hflags, np->hflags, np->hflags);
        }
    }
}

// 0x007B2470 (:1815). A squad sent after the player from a spawn region.
void activity_house_arrest::spawn_reinforcement(house_arrest_squad_info& squad_info)
{
    squad_spawn_parameters params;
    const house_arrest_squad_data* data = squad_info.base_squad_data;
    params.squad.osrp = find_spawn_region(squad_definition_get_team(data->squad_def), data->veh_group != nullptr);
    if (params.squad.osrp == nullptr)
    {
        return;
    }
    const player* pp = player_get_local_player();
    params.squad.squad_def = data->squad_def;
    params.squad.squad_pos = params.squad.osrp->pos;
    params.squad.npc_flags = npc_flags;
    params.squad.veh_spawn_group = data->veh_group;
    params.squad.squad_flags.disallow_reassignment = 1;
    params.orders.location_target = pp->pos;
    params.orders.object_target = pp->get_handle();
    params.orders.orders_type = GFM_ORDERS_TYPE_ATTACK;
    params.spawn_squad(); // rfg.exe inlines it
    if (params.created_orders != nullptr && params.spawned_squad != nullptr)
    {
        set_squad_flags(params.spawned_squad);
        squad_info.squad_handle = params.spawned_squad->get_handle();
        squad_info.orders_handle = params.created_orders->get_orders_handle();
    }
}

// 0x0076DEF0 (:1851). Barricades of a roadblock come with the roadblock instead.
bool activity_house_arrest::do_spawn_barricade(const object_dummy* dummy_node)
{
    const unsigned int parent = dummy_node->get_parent();
    if (parent != 0xFFFFFFFF && parent != ha_node_handle)
    {
        const object* parent_objp = World.object_from_handle(parent);
        if (parent_objp != nullptr && parent_objp->get_obj_type() == OT_ROADBLOCK_NODE)
        {
            return true;
        }
        if (is_roadblock_child(ha_node_handle, parent_objp))
        {
            return true;
        }
    }
    dummy_node->create_dummy_object();
    return true;
}

// 0x007B25C0 (:1862)
void activity_house_arrest::do_frame_reinforcements()
{
    if (!post_spawn_duration.valid() || ha_flags.all_reinforcements_spawned)
    {
        return;
    }
    ha_flags.all_reinforcements_spawned = true;
    const int elapsed_ms = post_spawn_duration.time_since();
    for (int i = 0; i < reinforcement_squad_list.size(); i++)
    {
        house_arrest_squad_info& info = reinforcement_squad_list[i];
        if (info.base_squad_data == nullptr)
        {
            continue;
        }
        if (info.base_squad_data->spawn_delay_ms < elapsed_ms)
        {
            spawn_reinforcement(info);
            info.base_squad_data = nullptr;
        }
        else
        {
            ha_flags.all_reinforcements_spawned = false;
        }
    }
}

// 0x00758AD0 (:1899)
bool activity_house_arrest::do_spawn_roadblock(object_roadblock_node* roadblock)
{
    return roadblock_spawn(roadblock->get_handle());
}

// 0x007987D0 (:1905)
void activity_house_arrest::do_spawn_post(const object_house_arrest_node* ha_node)
{
    if (ha_node == nullptr)
    {
        return;
    }
    farray<object_roadblock_node*, 128> roadblocks;
    collect_layer_objects<object_roadblock_node>(roadblocks);
    for (int i = 0; i < roadblocks.size(); i++)
    {
        do_spawn_roadblock(roadblocks[i]); // rfg.exe inlines it
    }
}

// 0x0076DF40 (:1925)
void activity_house_arrest::do_spawn_pre(const object_house_arrest_node* ha_node)
{
    for (int i = 0; i < layer_objects.size(); i++)
    {
        const object_dummy* dummy = World.object_from_handle<object_dummy>(layer_objects[i]);
        if (dummy != nullptr && (dummy->get_dummy_type() == DUMMY_OBJECT_TYPE_ACTIVITY_BARRICADE || dummy->get_dummy_type() == DUMMY_OBJECT_TYPE_REINFORCED_FENCE))
        {
            do_spawn_barricade(dummy); // rfg.exe inlines it
        }
    }
}

// 0x00782D50 (:1959). Original: a disabled node counts; only the completed flag and the distance do.
bool activity_house_arrest::collect_spawn_objects(farray<object const*, 32>& spawn_objects, const vector& player_pos, unsigned int district_handlep) const
{
    spawn_objects.clear();
    for (int i = 0; i < World.get_num_objects<object_house_arrest_node>(); i++)
    {
        const object_house_arrest_node* node = World.get_object<object_house_arrest_node>(i);
        if (node->get_completed_flag())
        {
            continue;
        }
        const float dist = vec_dist_squared(player_pos, node->pos);
        if (dist >= 22500.0f && dist <= 160000.0f) // Magic numbers: 150 to 400 metres
        {
            spawn_objects.add(node);
        }
    }
    return spawn_objects.size() > 0;
}

// 0x007B6240 (:1983). The morale reward drops by the penalty for each hostage not rescued and again for each the player
// killed, so a hostage the player killed costs twice.
void activity_house_arrest::record_victory()
{
    object_house_arrest_node* top_objp = ha_node_handle != 0xFFFFFFFF ? World.object_from_handle<object_house_arrest_node>(ha_node_handle) : nullptr;
    if (top_objp != nullptr)
    {
        if (base_house_arrest_data != nullptr)
        {
            activity_standard_rewards rewards = base_house_arrest_data->standard_rewards;
            const float missing = (float)((double)(float)hostage_info_list.size() - (double)(float)prisoners_rescued_total);
            const float after_missing = (float)((double)rewards.morale - (double)missing * (double)Player_kills_hostage_morale_penalty);
            const float morale = (float)((double)after_missing - (double)(float)hostages_killed_by_player * (double)Player_kills_hostage_morale_penalty);
            rewards.morale = (float)(int)morale;
            apply_standard_rewards(rewards, district_get_from_pos(top_objp->pos));
        }
        while (true)
        {
            object_house_arrest_node* parent_node = World.object_from_handle<object_house_arrest_node>(top_objp->get_parent());
            if (parent_node == nullptr)
            {
                break;
            }
            top_objp = parent_node;
        }
        farray<object_house_arrest_node*, 16> nodes;
        world_collect_descendants<object_house_arrest_node>(top_objp, nodes, nullptr, nullptr);
        nodes.add(top_objp);
        for (int i = 0; i < nodes.size(); i++)
        {
            nodes[i]->set_completed_flag();
        }
    }
    activity_base_modal::record_victory();
}

// 0x00758AF0 (:2041)
void activity_house_arrest::record_loss()
{
    activity_base::record_loss(); // rfg.exe inlines it
}

// 0x0076DFD0 (:2047). Original quirk kept: nothing rearms defender_count_event, so this counts every frame.
void activity_house_arrest::do_frame_edf_count()
{
    if (!defender_count_event.elapsed())
    {
        return;
    }
    int count = 0;
    for (int i = 0; i < squad_node_list.size(); i++)
    {
        const unsigned int node_handle = squad_node_list[i];
        const object_squad_spawn_node* node = node_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad_spawn_node>(node_handle) : nullptr;
        if (node == nullptr || node->get_child_squad_handle() == 0xFFFFFFFF)
        {
            continue;
        }
        object_squad* squad = World.object_from_handle<object_squad>(node->get_child_squad_handle());
        if (squad == nullptr)
        {
            continue;
        }
        if (squad->simulated())
        {
            count += squad->get_max_member_count();
            continue;
        }
        for (int m = 0; m < squad->member_array.size(); m++)
        {
            const unsigned int member = squad->member_array[m];
            const npc* np = member != 0xFFFFFFFF ? World.object_from_handle<npc>(member) : nullptr;
            if (np != nullptr && !np->is_killed())
            {
                count++;
            }
        }
    }
    if (defender_count != count)
    {
        defender_count = count;
        ha_state_change_event event;
        event.ha_handle = ha_node_handle;
        event.defender_count = count;
        gem_post_event(event);
    }
}

// 0x00782E40 (:2108). Original quirk kept: the player is not checked for null.
void activity_house_arrest::do_frame_squad_management()
{
    for (int i = 0; i < squad_node_list.size(); i++)
    {
        const unsigned int node_handle = squad_node_list[i];
        const object_squad_spawn_node* node = node_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad_spawn_node>(node_handle) : nullptr;
        if (node == nullptr || node->get_child_squad_handle() == 0xFFFFFFFF)
        {
            continue;
        }
        object_squad* squad = World.object_from_handle<object_squad>(node->get_child_squad_handle());
        if (squad != nullptr && !squad->npc_flags.activity_ha)
        {
            set_squad_flags(squad);
        }
    }
    const player* pp = player_get_local_player();
    for (int i = 0; i < reinforcement_squad_list.size(); i++)
    {
        gfm_orders* orders = Orders_manager.get_orders_from_handle(reinforcement_squad_list[i].orders_handle);
        if (orders != nullptr && vec_dist_squared(pp->pos, orders->location_target) > 400.0f) // Magic number: 20 metres
        {
            orders->location_target = pp->pos;
        }
    }
}

// 0x0076E110 (:2160)
void activity_house_arrest::spawn_all_hostages()
{
    for (int i = 0; i < hostage_info_list.size(); i++)
    {
        if (!hostage_info_list[i].hostage_flags.spawned)
        {
            do_spawn_hostage(hostage_info_list[i]);
        }
    }
}

// 0x0076E150 (:2179). Before the activity runs, hostages follow the player's distance.
void activity_house_arrest::do_frame_hostage_spawn()
{
    const player* pp = player_get_local_player();
    for (int i = 0; i < hostage_info_list.size(); i++)
    {
        house_arrest_hostage_info& info = hostage_info_list[i];
        if (info.hostage_flags.killed || info.spawn_node_handle == 0xFFFFFFFF)
        {
            continue;
        }
        const object_npc_spawn_node* node = World.object_from_handle<object_npc_spawn_node>(info.spawn_node_handle);
        if (node == nullptr)
        {
            continue;
        }
        const float dist = vec_xz_dist_squared(pp->pos, node->pos);
        if (info.hostage_flags.spawned)
        {
            if (!node->obj_flags.streamed || dist > 10000.0f) // Magic number: 100 metres
            {
                do_despawn_hostage(info); // rfg.exe inlines it
            }
        }
        else if (node->obj_flags.streamed && dist < 6400.0f) // Magic number: 80 metres
        {
            do_spawn_hostage(info);
        }
    }
}

// 0x00782F50 (:2223). Original: the stop calls go through House_arrest_activity, the same object.
void activity_house_arrest::do_frame_hostages(bool active_state)
{
    const player* pp = player_get_local_player();
    for (int i = 0; i < hostage_info_list.size(); i++)
    {
        house_arrest_hostage_info& info = hostage_info_list[i];
        if (!info.hostage_flags.spawned)
        {
            continue;
        }
        npc* np = info.hostage_handle != 0xFFFFFFFF ? World.object_from_handle<npc>(info.hostage_handle) : nullptr;
        if (np == nullptr)
        {
            if (!info.hostage_flags.rescued)
            {
                info.hostage_flags.killed = true;
                ha_flags.all_hostages_freed = are_all_hostages_freed();
                stop_objective_highlight(info.hostage_handle);
            }
            continue;
        }
        if (active_state && ha_flags.hostage_freed)
        {
            const float dist = vec_xz_dist_squared(pp->pos, np->pos);
            if (dist > (float)((double)Hostage_death_distance * (double)Hostage_death_distance))
            {
                human_damage(np, nullptr, np->hit_points, DBT_NONE, nullptr, nullptr, true, false);
            }
            else if (!np->is_killed() && (float)((double)Hostage_death_distance * 0.75 * (double)Hostage_death_distance * 0.75) < dist && leaving_hostage_message_timer.elapsed())
            {
                // Original: the wide overload gets the string table key as it is, not its localized text.
                hud_message_add(L"ACT_HA_LEAVING_HOSTAGE", 4.0f, nullptr, nullptr, nullptr);
                leaving_hostage_message_timer.set(15000); // Magic number: fifteen seconds
            }
        }
        else
        {
            check_hostage_flags(info.hostage_handle);
        }
        if (np->is_killed() && !info.hostage_flags.killed)
        {
            info.hostage_flags.killed = true;
            ha_flags.all_hostages_freed = are_all_hostages_freed();
            stop_objective_highlight(np->get_handle());
            if (np->combat.last_killed_by_other_handle == player_get_local_player()->get_handle())
            {
                hostages_killed_by_player++;
            }
        }
    }
    if (!active_state)
    {
        do_frame_hostage_spawn();
    }
    enable_objective_highlights();
}

// 0x00783180 (:2307). Original quirk kept: a squad node with no parent is not a defender.
void activity_house_arrest::parse_activity_objects()
{
    for (int i = 0; i < layer_objects.size(); i++)
    {
        const object* objp = World.object_from_handle(layer_objects[i]);
        if (objp == nullptr)
        {
            continue;
        }
        if (objp->get_obj_type() == OT_NPC_SPAWN_NODE)
        {
            const object_npc_spawn_node* npc_node = static_cast<const object_npc_spawn_node*>(objp);
            if (is_hostage_spawn(npc_node) && !load_single_hostage_node(npc_node))
            {
                return;
            }
        }
        else if (objp->get_obj_type() == OT_SQUAD_SPAWN_NODE && objp->get_parent() != 0xFFFFFFFF && static_cast<const object_squad_spawn_node*>(objp)->get_spawn_prob() > 0.0f)
        {
            squad_node_list.add(objp->get_handle());
        }
    }
}

// 0x007A1C60 (:2354). The informant plays its intro, then the start-up squad is released.
bool activity_house_arrest::start_activity_spawn(const object* spawn_objp, const district* dp)
{
    if (!activity_base::start_activity_spawn(spawn_objp, dp))
    {
        release_and_reset_squad();
        return false;
    }
    const player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        const unsigned int informant = get_informant_npc();
        const object_house_arrest_node* node = spawn_objp != nullptr && spawn_objp->get_obj_type() == OT_HOUSE_ARREST_NODE ? static_cast<const object_house_arrest_node*>(spawn_objp) : nullptr;
        const house_arrest_level_data* data = node != nullptr ? node->base_house_arrest_data : nullptr;
        if (informant != 0xFFFFFFFF && data != nullptr && data->informant_line != INVALID_VOICE_LINE_HANDLE && data->informant_anim_action != ANIMATION_ACTION_NONE)
        {
            npc* np = World.object_from_handle<npc>(informant);
            if (np != nullptr)
            {
                np->npc_flags.amb_watch_player = 0;
            }
            ai_scripting_trigger_behavior_on_character(informant, AI_SB_ACTIVITY_INTRO, pp->get_handle(), (int)data->informant_line, (int)data->informant_anim_action);
        }
        release_and_reset_squad();
    }
    return true;
}

// 0x00798840 (:2406)
bool activity_house_arrest::activity_initialize(const object* spawn_objp, unsigned int new_district_handle)
{
    if (spawn_objp == nullptr || Multiplayer || is_active() || hostage_info_list.size() >= 1 || !activity_base_modal::activity_initialize(spawn_objp, new_district_handle) || spawn_objp->get_obj_type() != OT_HOUSE_ARREST_NODE)
    {
        return false;
    }
    reset();
    const object_house_arrest_node* node = static_cast<const object_house_arrest_node*>(spawn_objp);
    ha_node_handle = node->get_handle();
    base_house_arrest_data = node->base_house_arrest_data;
    if (base_house_arrest_data == nullptr)
    {
        return false;
    }
    house_center = node->pos;
    parse_activity_objects();
    if (hostage_info_list.size() < 1)
    {
        return false;
    }
    do_spawn_pre(node);
    enable_restricted_areas();
    ha_flags.all_reinforcements_spawned = true;
    for (int i = 0; i < base_house_arrest_data->reinforcement_squad_list.size(); i++)
    {
        if (reinforcement_squad_list.size() < reinforcement_squad_list.capacity())
        {
            house_arrest_squad_info info;
            info.base_squad_data = &base_house_arrest_data->reinforcement_squad_list[i];
            reinforcement_squad_list.add(info);
            ha_flags.all_reinforcements_spawned = false;
        }
    }
    change_interface_state(ACTIVITY_INTERFACE_STATE_RUNNING);
    return true;
}

// 0x00783220 (:2486). Original quirk kept: the completed look reads activity_spawn_node_handle, which
// start_activity_spawn clears when it releases the informant, so these icons normally use the default color.
void activity_house_arrest::render_icons() const
{
    if (hostage_info_list.size() < 1)
    {
        return;
    }
    // rfg.exe writes two ha_flags from this const method.
    activity_house_arrest* self = const_cast<activity_house_arrest*>(this);
    object_activity_spawn* spawn = activity_spawn_node_handle != 0xFFFFFFFF ? World.object_from_handle<object_activity_spawn>(activity_spawn_node_handle) : nullptr;
    const bool completed = spawn != nullptr && spawn->get_completed_flag();
    if (ha_flags.all_hostages_freed)
    {
        const object* safehouse = World.object_from_handle(Cartographer.m_nearest_safehouse);
        if (safehouse != nullptr && (!Cartographer.has_player_marker() || !ha_flags.added_safehouse_marker) && !Cartographer.has_mission_marker())
        {
            Cartographer.add_player_marker(safehouse->pos, true, 0xFFFFFFFF, false, true);
            self->ha_flags.added_safehouse_marker = true;
        }
        return;
    }
    const color& icon_color = completed ? Completed_icon_color : Default_map_item_color;
    if (!Render_hostage_icons)
    {
        Cartographer.add_temporary_icon(house_center, MAP_ELEMENT_HOUSE_ICON_ALWAYS_SHOW, true, ha_node_handle, icon_color, false, get_spawn_object_handle());
    }
    else
    {
        for (int i = 0; i < hostage_info_list.size(); i++)
        {
            const house_arrest_hostage_info& info = hostage_info_list[i];
            if (info.hostage_flags.rescued || info.hostage_flags.killed)
            {
                continue;
            }
            const npc* np = info.hostage_handle != 0xFFFFFFFF ? World.object_from_handle<npc>(info.hostage_handle) : nullptr;
            const object* icon_objp = nullptr;
            if (np != nullptr)
            {
                if (np->npc_flags.tied_up)
                {
                    icon_objp = np;
                }
            }
            else if (info.spawn_node_handle != 0xFFFFFFFF)
            {
                icon_objp = World.object_from_handle<object_npc_spawn_node>(info.spawn_node_handle);
            }
            if (icon_objp != nullptr)
            {
                Cartographer.add_temporary_icon(icon_objp->pos, MAP_ELEMENT_HOUSE_ICON_ALWAYS_SHOW, true, ha_node_handle, icon_color, false, get_spawn_object_handle());
            }
        }
    }
    if (!ha_flags.gps_triggered)
    {
        Cartographer.add_player_marker(house_center, false, ha_node_handle, false, false);
        self->ha_flags.gps_triggered = true;
    }
}

// 0x0076E250 (:2577)
bool activity_house_arrest::all_hostages_dead()
{
    for (int i = 0; i < hostage_info_list.size(); i++)
    {
        if (!hostage_info_list[i].hostage_flags.spawned || !hostage_info_list[i].hostage_flags.killed)
        {
            return false;
        }
    }
    gamelog_set_activity_failed_reason(GLCR_HOSTAGES_KILLED);
    return true;
}

// 0x0076E290 (:2598)
bool activity_house_arrest::all_hostages_saved()
{
    for (int i = 0; i < hostage_info_list.size(); i++)
    {
        const house_arrest_hostage_info& info = hostage_info_list[i];
        if (!info.hostage_flags.spawned || (!info.hostage_flags.rescued && !info.hostage_flags.killed))
        {
            return false;
        }
    }
    return true;
}

// 0x0076E2D0 (:2622). A hostage not spawned, or spawned and alive, counts in num_total. One whose npc is gone counts in
// neither.
void activity_house_arrest::count_prisoners(int& num_total, int& num_killed) const
{
    num_total = 0;
    num_killed = 0;
    for (int i = 0; i < hostage_info_list.size(); i++)
    {
        const house_arrest_hostage_info& info = hostage_info_list[i];
        if (!info.hostage_flags.spawned)
        {
            num_total++;
            continue;
        }
        const npc* np = info.hostage_handle != 0xFFFFFFFF ? World.object_from_handle<npc>(info.hostage_handle) : nullptr;
        if (np == nullptr)
        {
            continue;
        }
        if (np->is_killed())
        {
            num_killed++;
        }
        else
        {
            num_total++;
        }
    }
}

// 0x0076E360 (:2648)
bool activity_house_arrest::are_all_hostages_freed() const
{
    if (get_state() != BASE_ACTIVITY_STATE_RUNNING)
    {
        return false;
    }
    for (int i = 0; i < hostage_info_list.size(); i++)
    {
        const house_arrest_hostage_info& info = hostage_info_list[i];
        if (!info.hostage_flags.spawned)
        {
            return false;
        }
        const npc* np = info.hostage_handle != 0xFFFFFFFF ? World.object_from_handle<npc>(info.hostage_handle) : nullptr;
        if (np != nullptr && !np->is_killed() && np->npc_flags.tied_up && !np->npc_flags.freed_from_bonds)
        {
            return false;
        }
    }
    return true;
}

// 0x0076E3F0 (:2678). A marker effect on each hostage once it has streamed in, half a metre behind it.
void activity_house_arrest::enable_objective_highlights()
{
    if (ha_flags.all_objective_highlights_started)
    {
        return;
    }
    matrix identity;
    identity.make_identity();
    ha_flags.all_objective_highlights_started = true;
    for (int i = 0; i < hostage_info_list.size(); i++)
    {
        house_arrest_hostage_info& info = hostage_info_list[i];
        const npc* np = info.hostage_handle != 0xFFFFFFFF ? World.object_from_handle<npc>(info.hostage_handle) : nullptr;
        if (np == nullptr || !np->obj_flags.streamed)
        {
            ha_flags.all_objective_highlights_started = false;
            continue;
        }
        if (info.objective_highlight_handle != 0xFFFFFFFF)
        {
            continue;
        }
        const vector effect_pos((float)((double)np->pos.x - (double)np->orient.fvec.x * 0.5), (float)((double)np->pos.y - (double)np->orient.fvec.y * 0.5), (float)((double)np->pos.z - (double)np->orient.fvec.z * 0.5));
        info.objective_highlight_handle = effects_play(Hostage_effect_info, effect_pos, identity, true, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
        const object_effect* effect = info.objective_highlight_handle != 0xFFFFFFFF ? World.object_from_handle<object_effect>(info.objective_highlight_handle) : nullptr;
        vfx_effect* vfx = effect != nullptr ? effect->get_vfx_effect() : nullptr;
        if (vfx == nullptr)
        {
            continue;
        }
        vector& highlight_color = Highlight_colors[HIGHLIGHT_PURPOSE_NEUTRAL];
        rl_color_float tint;
        tint.set(highlight_color.x, highlight_color.y, highlight_color.z, 1.0f);
        vfx->set_effect_material_tint(tint);
        rl_renderable_instance* head = vfx->get_mesh_instances().first();
        rl_renderable_instance* ri = head;
        while (ri != nullptr)
        {
            ri->set_tint(highlight_color);
            ri = ri->next;
            if (ri == head)
            {
                break;
            }
        }
    }
}

// 0x0076E640 (:2719)
void activity_house_arrest::stop_objective_highlight(unsigned int npc_handle)
{
    for (int i = 0; i < hostage_info_list.size(); i++)
    {
        house_arrest_hostage_info& info = hostage_info_list[i];
        if (npc_handle != info.hostage_handle || info.objective_highlight_handle == 0xFFFFFFFF)
        {
            continue;
        }
        object_effect* effect = World.object_from_handle<object_effect>(info.objective_highlight_handle);
        if (effect != nullptr)
        {
            effect->stop(object_effect::SS_STOP_EMITTING);
            info.objective_highlight_handle = 0xFFFFFFFF;
        }
    }
}

// 0x00758B30 (:2735)
void activity_house_arrest::process_results()
{
    if (prisoners_rescued_total > 0)
    {
        ui_tool_tips_add_tip("Guerrilla_Action_Complete", 2000, nullptr); // Magic number: two seconds
        record_victory();
        activity_end();
        return;
    }
    record_loss();
    if (hud_ui_activity_complete_start(ACTIVITY_MODE_HOUSE_ARREST, false))
    {
        ha_flags.splash_screen_active = true;
        return;
    }
    activity_end();
}

// 0x007A9070 (:2758). Original: also builds an unused farray<unsigned int,100>.
void activity_house_arrest::clean_house()
{
    for (int i = 0; i < reinforcement_squad_list.size(); i++)
    {
        Orders_manager.destroy_orders(reinforcement_squad_list[i].orders_handle); // rfg.exe inlines it
        const unsigned int squad_handle = reinforcement_squad_list[i].squad_handle;
        object_squad* squad = squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(squad_handle) : nullptr;
        if (squad != nullptr)
        {
            squad->request_despawn_all_members(NPC_DESPAWN_TYPE_MEDUIM, nullptr, false);
        }
    }
    for (int i = 0; i < squad_node_list.size(); i++)
    {
        const unsigned int node_handle = squad_node_list[i];
        const object_squad_spawn_node* node = node_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad_spawn_node>(node_handle) : nullptr;
        if (node == nullptr || node->get_child_squad_handle() == 0xFFFFFFFF)
        {
            continue;
        }
        object_squad* squad = World.object_from_handle<object_squad>(node->get_child_squad_handle());
        if (squad != nullptr)
        {
            Orders_manager.destroy_orders(squad->get_orders_handle()); // rfg.exe inlines it
            squad->request_despawn_all_members(NPC_DESPAWN_TYPE_MEDUIM, nullptr, false);
        }
    }
    // Hostages still tied up go; freed ones stay as ambient npcs.
    const int num_npcs = World.get_num_objects<npc>();
    for (int i = 0; i < num_npcs; i++)
    {
        npc* np = World.get_object<npc>(i);
        if (!np->npc_flags.activity_ha)
        {
            continue;
        }
        if (np->npc_flags.tied_up)
        {
            if (!np->is_killed())
            {
                World.destroy_object(np, false);
            }
            continue;
        }
        np->npc_flags.activity_ha = 0;
        if (np->hflags.activity_ha_hostage)
        {
            np->hflags.activity_ha_hostage = 0;
            np->npc_flags.ambient_spawn = 1;
        }
        np->hflags.considered_armed = 0;
    }
    const object_house_arrest_node* node = ha_node_handle != 0xFFFFFFFF ? World.object_from_handle<object_house_arrest_node>(ha_node_handle) : nullptr;
    if (node != nullptr)
    {
        farray<object_restricted_area*, 64> areas;
        world_collect_descendants<object_restricted_area>(node, areas, nullptr, nullptr);
        for (int i = 0; i < areas.size(); i++)
        {
            areas[i]->disable();
        }
        farray<object_roadblock_node*, 64> roadblocks;
        world_collect_descendants<object_roadblock_node>(node, roadblocks, nullptr, nullptr);
        for (int i = 0; i < roadblocks.size(); i++)
        {
            roadblocks[i]->cleanup_roadblock();
        }
    }
    Cartographer.remove_player_marker(true);
    reset();
    player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        object_action_node* action_node = pp->get_current_action_node(false);
        if (action_node != nullptr && action_node->is_free_hostage_action_node())
        {
            action_node->emergency_exit(pp);
            pp->action_node_reset(nullptr);
        }
    }
    // Samanya stays quiet for ten seconds, through the first npc with guerrilla tech.
    for (int i = 0; i < World.get_num_objects<npc>(); i++)
    {
        npc* np = World.get_object<npc>(i);
        if (np->h_info->flags.guerrilla_tech)
        {
            voice_line_unlocking_set_global_cooldown(np, VLUG_SAMANYA, 10000);
            break;
        }
    }
    release_and_reset_squad(); // rfg.exe inlines it
}

// 0x0076E6B0 (:2873)
void activity_house_arrest::delete_prisoners()
{
    for (int i = 0; i < hostage_info_list.size(); i++)
    {
        stop_objective_highlight(hostage_info_list[i].hostage_handle);
        const unsigned int handle = hostage_info_list[i].hostage_handle;
        npc* np = handle != 0xFFFFFFFF ? World.object_from_handle<npc>(handle) : nullptr;
        if (np != nullptr && !np->is_killed())
        {
            World.destroy_object(np, false);
        }
    }
    hostage_info_list.clear();
}

// 0x0076E740 (:2892)
bool activity_house_arrest::house_ready() const
{
    return hostage_info_list.size() > 0;
}

// 0x00758BA0 (:2897)
unsigned int activity_house_arrest::get_activity_start_node_from_handle(unsigned int base_handle)
{
    if (base_handle != 0xFFFFFFFF && get_state() == BASE_ACTIVITY_STATE_WAIT && (base_handle == activity_spawn_node_handle || base_handle == start_up_squad_handle))
    {
        return activity_spawn_node_handle;
    }
    return base_handle;
}

// 0x007AD8C0 (:2922)
bool activity_house_arrest_start(const object_house_arrest_node* ha_node)
{
    if (House_arrest_activity.mode == activity_get_mode() || !House_arrest_activity.can_initialize())
    {
        return false;
    }
    activity_kill_all_except(ACTIVITY_MODE_HOUSE_ARREST);
    const district* dp = district_get_from_pos(ha_node->pos);
    if (dp == nullptr)
    {
        return false;
    }
    if (House_arrest_activity.start_activity_spawn(ha_node, dp))
    {
        return true;
    }
    House_arrest_activity.activity_kill(); // rfg.exe inlines it
    return false;
}

// 0x00758BD0 (:2950). Empty in rfg.exe.
void activity_house_arrest_report_streaming(const object_house_arrest_node* ha_node)
{
}

// 0x007833F0 (:2956)
void activity_house_arrest_play_voice_line(const vector& pos, human_teams team, voice_lines line)
{
    House_arrest_activity.play_house_arrest_line(pos, team, line);
}

// 0x0076E750 (:2961)
void activity_house_arrest_npc_at_safehouse(npc* np)
{
    if (np->npc_flags.activity_ha)
    {
        House_arrest_activity.notify_npc_at_safehouse(np);
    }
}

// 0x0076E770 (:2968)
bool activity_house_arrest_is_house(const rfg_mover* house_target)
{
    if (!House_arrest_activity.was_initialized() && !House_arrest_activity.is_active())
    {
        return false;
    }
    for (int i = 0; i < House_arrest_activity.hostage_info_list.size(); i++)
    {
        const object* node = World.object_from_handle(House_arrest_activity.hostage_info_list[i].spawn_node_handle);
        if (node != nullptr && node->get_parent() == house_target->get_handle())
        {
            return true;
        }
    }
    return false;
}

// 0x0076E7F0 (:2993)
bool activity_house_arrest_nodes_are_relatives(unsigned int node_handle_a, unsigned int node_handle_b)
{
    if (node_handle_a == 0xFFFFFFFF || node_handle_b == 0xFFFFFFFF)
    {
        return false;
    }
    const object_house_arrest_node* node_a = World.object_from_handle<object_house_arrest_node>(node_handle_a);
    if (node_a == nullptr)
    {
        return false;
    }
    const object_house_arrest_node* node_b = World.object_from_handle<object_house_arrest_node>(node_handle_b);
    if (node_b == nullptr)
    {
        return false;
    }
    return node_handle_a == node_handle_b || node_a->get_parent() == node_handle_b || node_b->get_parent() == node_handle_a;
}

// 0x00758BE0 (:3022)
bool activity_house_arrest_all_hostages_freed()
{
    return House_arrest_activity.ha_flags.all_hostages_freed;
}

// 0x0076E860 (:3028)
bool activity_house_arrest_is_human_hostage(const human* hp)
{
    for (int i = 0; i < House_arrest_activity.hostage_info_list.size(); i++)
    {
        if (House_arrest_activity.hostage_info_list[i].hostage_handle == hp->get_handle())
        {
            return true;
        }
    }
    return false;
}

// 0x007AD940 (:3041). Original quirk kept: with a dead player the morale still drops, but the activity does not end here.
void activity_house_arrest_exit()
{
    if (House_arrest_activity.prisoners_rescued_total == 0)
    {
        gfm_change_district_morale((float)-House_arrest_activity.hostages_killed_by_player * Player_kills_hostage_morale_penalty, nullptr, nullptr, nullptr);
    }
    const player* pp = player_get_local_player();
    if (pp != nullptr && !pp->is_killed())
    {
        House_arrest_activity.activity_house_arrest::activity_end(); // rfg.exe inlines it
    }
}

// 0x00758BF0 (:3062)
void activity_house_arrest_replay()
{
    Activity_node_start_on_load = House_arrest_activity.get_spawn_object_handle();
    World.save_and_reload();
}

// 0x00758C10 (:3074)
void activity_house_arrest_process_player_death()
{
    gamelog_set_activity_failed_reason(GLCR_PLAYER_DIED);
    House_arrest_activity.process_results();
}

// 0x00758C30 (:3081)
bool activity_house_arrest_has_interface()
{
    return !House_arrest_activity.ha_flags.does_not_have_interface;
}

// 0x00783410 (:3087). The status order: tied up, dead, rescued (no longer the activity's), passenger, freed.
void activity_house_arrest_hostage_status(farray<hostage_info, 3>& status_list)
{
    status_list.clear();
    for (int i = 0; i < House_arrest_activity.hostage_info_list.size(); i++)
    {
        const house_arrest_hostage_info& info = House_arrest_activity.hostage_info_list[i];
        hostage_info entry;
        entry.status = HOSTAGE_STATUS_TIED_UP;
        if (info.hostage_flags.spawned)
        {
            const npc* np = info.hostage_handle != 0xFFFFFFFF ? World.object_from_handle<npc>(info.hostage_handle) : nullptr;
            entry.np = np;
            if (np == nullptr || np->is_killed())
            {
                entry.status = HOSTAGE_STATUS_DEAD;
            }
            else if (!np->npc_flags.tied_up)
            {
                if (!np->npc_flags.activity_ha)
                {
                    entry.status = HOSTAGE_STATUS_RESCUED;
                }
                else
                {
                    entry.status = HOSTAGE_STATUS_FREED;
                    const player* pp = player_get_local_player();
                    if (pp != nullptr)
                    {
                        const vehicle* vp = human_in_vehicle(pp);
                        if (vp != nullptr && human_in_vehicle(np, vp))
                        {
                            entry.status = HOSTAGE_STATUS_PASSANGER;
                        }
                    }
                }
            }
        }
        status_list.add(entry);
    }
}

// 0x00758C40 (:3170)
unsigned int house_arrest_get_activity_node_startup_squad(unsigned int base_handle)
{
    return House_arrest_activity.get_activity_start_node_from_handle(base_handle); // rfg.exe inlines it
}
