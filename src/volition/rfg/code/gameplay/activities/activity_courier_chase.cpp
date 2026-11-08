// Original: volition/rfg/code/gameplay/activities/activity_courier_chase.cpp
//
// gameflow.md GF4: Catch the Courier. An EDF squad drives a patrol, the player catches it or kills it, picks up the case
// it drops and then has to get the alert level back to green.

#include "volition/rfg/code/gameplay/activities/activity_courier_chase.h"
#include "volition/rfg/code/gameplay/activities/activity_courier_chase_internal.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/gr/gr_font.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_rewards.h"
#include "volition/rfg/code/gameplay/activities/base_activity_data.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager_internal.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gameplay/squad_spawn.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/hud/minimap_icon_info.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_courier_end_point.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_patrol.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/object_trigger_region.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/scripting/dialogue_manager.h"
#include "volition/rfg/code/scripting/objective_highlight.h"
#include "volition/rfg/code/scripting/public_address.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/util/string_util.h"

#include <stdio.h>

extern gfm_orders_flags ACTIVITY_ORDERS_FLAGS; // 0x02231B21 (gfm_smash_building_coordinator.cpp)

activity_courier_chase_data Courier_chase_data; // 0x02242BA8

static float ACTIVITY_START_DISTANCE; // 0x022278B0, from the tweak table
static float ACTIVITY_END_DISTANCE; // 0x02225AE4, from the tweak table
static int CC_OUT_OF_BOUNDS_PERIOD_MS; // 0x02225814, from the tweak table
static float CC_CONTROL_DELTA; // 0x02226BF4, from the tweak table
static tweak_table_item Tweak_table_item_Courier_chase_start_distance("Courier_chase_start_distance", &ACTIVITY_START_DISTANCE);
static tweak_table_item Tweak_table_item_Courier_chase_end_distance("Courier_chase_end_distance", &ACTIVITY_END_DISTANCE);
static tweak_table_item Tweak_table_item_Courier_chase_out_of_bounds_ms("Courier_chase_out_of_bounds_ms", &CC_OUT_OF_BOUNDS_PERIOD_MS);
static tweak_table_item Tweak_table_item_Courier_victory_control_delta("Courier_victory_control_delta", &CC_CONTROL_DELTA);

static const float CC_MORALE_DELTA = 20.0f; // 0x012DAFC8
static const float MIN_COURIER_SPAWN_DIST_SQRD = 16900.0f; // 0x012DAFCC, 130 metres
static const float MAX_COURIER_SPAWN_DIST_SQRD = 202500.0f; // Magic number: 450 metres
static const float SPAWNED_NEAR_DEST_DIST_SQRD = 400.0f; // 0x012DD034
static const float SIMULATED_NEAR_DEST_DIST_SQRD = 9.0f; // 0x012DD038
static const float NEAR_DEST_DIST_SQRD = 225.0f; // Magic number: 15 metres
static const float CASE_LOST_DIST_SQRD = 90000.0f; // 0x012DE7DC, 300 metres
static const float Briefcase_pickup_distance_sq = 6.25f; // 0x0165C934, 2.5 metres

static const squad_definition* Default_squad_def; // 0x022295BC
static const vehicle_spawn_group_info* Default_vehicle_group; // 0x022295C0
static int Courier_case_pickup_sound; // 0x02226BDC
static bool Done_one_time_vehicle_init; // 0x022295B2
static bool Damage_causes_start; // 0x022295B3

// 0x012DAF68
static const char* const Persona_names_chase[NUM_CTC_RADIO_LINES] = {
    "OFFCAM", "OFFCAM",
    "PCM01", "ACTIVE", "ACTIVE",
    "PCM01", "ACTIVE", "ACTIVE",
    "PCM01", "ACTIVE", "ACTIVE",
    "PCM01", "ACTIVE", "ACTIVE",
    "PCM01", "ACTIVE", "ACTIVE",
};

// 0x00797000 (:140)
void unused_dcf_make_courier_stuck()
{
    Courier_chase_data.make_courier_stuck(); // rfg.exe inlines it
}

// 0x00781440 (:185)
static bool all_courier_chase_complete()
{
    for (int i = 0; i < World.get_num_objects<object_courier_end_point>(); i++)
    {
        if (!World.get_object<object_courier_end_point>(i)->get_completed_flag())
        {
            return false;
        }
    }
    return true;
}

// The node's distance test both collect functions inline. Original: from the local player, unchecked.
static bool courier_node_in_range(const object_courier_end_point* node, const player* pp)
{
    const float dist = vec_dist_squared(pp->pos, node->pos);
    return dist > MIN_COURIER_SPAWN_DIST_SQRD && dist < MAX_COURIER_SPAWN_DIST_SQRD;
}

// 0x00781490 (:199). rfg.exe: LTCG, spawn_objects in EDI. Original: ignores the player position and district it is given.
static bool collect_nodes_complete(farray<object const*, 32>& spawn_objects)
{
    spawn_objects.clear();
    const player* pp = player_get_local_player();
    for (int i = 0; i < World.get_num_objects<object_courier_end_point>(); i++)
    {
        const object_courier_end_point* node = World.get_object<object_courier_end_point>(i);
        if (node->can_spawn() && (!node->end_game_only_node() || mission_final_mission_complete()) && courier_node_in_range(node, pp))
        {
            spawn_objects.add(node);
        }
    }
    return spawn_objects.size() > 0;
}

// 0x00781590 (:219)
static bool collect_nodes_incomplete(farray<object const*, 32>& spawn_objects)
{
    spawn_objects.clear();
    const player* pp = player_get_local_player();
    for (int i = 0; i < World.get_num_objects<object_courier_end_point>(); i++)
    {
        const object_courier_end_point* node = World.get_object<object_courier_end_point>(i);
        if (node->can_spawn() && !node->get_completed_flag() && (!node->end_game_only_node() || mission_final_mission_complete()) && courier_node_in_range(node, pp))
        {
            spawn_objects.add(node);
        }
    }
    return spawn_objects.size() > 0;
}

// 0x0077E770 (.h:20). Original: the rest stays unset until level_init calls reset.
activity_courier_chase_data::activity_courier_chase_data()
    : activity_base_modal(ACTIVITY_MODE_COURIER, true)
{
}

// 0x00769120
activity_courier_chase_data::~activity_courier_chase_data()
{
}

// 0x00757C60 (:248). Original: a missing default squad or group skips the sound and the gender too.
void activity_courier_chase_data::system_init()
{
    Default_squad_def = squad_definition_from_name("basic_squad_1");
    if (Default_squad_def == nullptr)
    {
        return;
    }
    Default_vehicle_group = sog_get_vehicle_group("Default EDF");
    if (Default_vehicle_group == nullptr)
    {
        return;
    }
    audiolib_cue_get_id("HUD_CTC_BRIEFCASE_PICKUP", &Courier_case_pickup_sound);
    radio_gender_male = true;
}

// 0x00797040 (:266)
void activity_courier_chase_data::level_init()
{
    activity_base_modal::level_init(); // rfg.exe inlines it
    reset();
}

// 0x00757CB0 (:274)
void activity_courier_chase_data::play_radio_line(courier_chase_radio_lines courier_line, courier_chase_radio_lines to_follow_line)
{
    if ((unsigned int)courier_line >= NUM_CTC_RADIO_LINES)
    {
        return;
    }
    const voice_line_handle handle = courier_def->radio_lines[courier_line];
    if (handle == INVALID_VOICE_LINE_HANDLE)
    {
        return;
    }
    const human_voice_persona* persona = human_voice_persona_from_name(Persona_names_chase[courier_line]);
    if (to_follow_line != CTC_RL_NONE && courier_def->radio_lines[to_follow_line] != INVALID_VOICE_LINE_HANDLE)
    {
        dialogue_manager_add_line(handle, persona, courier_def->radio_lines[to_follow_line], 500, INVALID_PA_HANDLE, nullptr); // Magic number: half a second after
        return;
    }
    const char* icon = nullptr;
    if (courier_line == CTC_RL_START_FEMALE || courier_line == CTC_RL_START_MALE)
    {
        icon = get_minimap_bitmap_icon_name(0x12); // Magic number: the courier map element
    }
    dialogue_manager_start_dialogue(handle, persona, DIALOGUE_PRIORITY_RADIO, INVALID_PA_HANDLE, icon);
}

// 0x007816B0 (:300). Original: leaves spawned, simulated_last_frame, last_courier_escaped and the route as they were.
void activity_courier_chase_data::reset()
{
    vehicle* vp = get_courier_vehicle();
    if (vp != nullptr)
    {
        vp->set_extra_mass(false);
    }
    vehicle_handle = 0xFFFFFFFF;
    courier_case_handle = 0xFFFFFFFF;
    out_of_bounds_timestamp.invalidate();
    escaping_timestamp.invalidate();
    start_delay_timestamp.invalidate();
    squad_handle = 0xFFFFFFFF;
    orders_handle = 0xFFFFFFFF;
    polling_period.set(250); // Magic number: a quarter of a second
    courier_stuck_timestamp.invalidate();
    courier_stuck_pos.zero();
    flags.chased = false;
    flags.completed = false;
    flags.data_retrieved = false;
    flags.data_dropped = false;
    flags.at_destination = false;
    flags.allow_catch_up = false;
    flags.escaping = false;
    end_activity_next_frame = false;
    spawned_from_console = false;
    Damage_causes_start = false;
}

// 0x007B5AD0 (:331)
void activity_courier_chase_data::check_for_start()
{
    if (squad_handle == 0xFFFFFFFF)
    {
        return;
    }
    const vector courier_pos = get_courier_position();
    if (!courier_pos.is_null())
    {
        data_debug_location = courier_pos;
    }
    if (!polling_period.elapsed())
    {
        return;
    }
    polling_period.set(250);
    object_squad* squad = World.object_from_handle<object_squad>(squad_handle);
    if (squad == nullptr)
    {
        // The squad is gone. Original: one that was simulated when last seen ends the activity, or else it has been
        // killed while it was away from the player, and the case drops at the last position.
        if (!flags.simulated_last_frame)
        {
            start_delay_timestamp.invalidate();
            if (activity_begin_start_up())
            {
                create_courier_case(data_debug_location);
                return;
            }
        }
        activity_kill();
        return;
    }
    const bool was_simulated = flags.simulated_last_frame;
    flags.simulated_last_frame = squad->simulated();
    // Original: the rest only runs for a squad that is not simulated.
    if (flags.simulated_last_frame)
    {
        return;
    }
    if (was_simulated && Done_one_time_vehicle_init)
    {
        vehicle* vp = get_courier_vehicle();
        if (vp != nullptr)
        {
            objective_highlight_add(vp->get_handle(), HIGHLIGHT_PURPOSE_NEUTRAL, HIGHLIGHT_STYLE_ARROW);
        }
    }
    apply_vehicle_health_and_mass_attributes();
    if (!Damage_causes_start)
    {
        const npc* leader = squad->get_leader();
        // A damaged leader starts it at once.
        if (leader == nullptr || leader->hit_points / (float)leader->max_hit_points == 1.0f)
        {
            if (player_find_shortest_player_dist_sqrd(squad->pos) > ACTIVITY_START_DISTANCE * ACTIVITY_START_DISTANCE)
            {
                return;
            }
            const player* pp = player_get_local_player();
            if (pp == nullptr || human_in_vehicle(pp) == nullptr)
            {
                start_delay_timestamp.invalidate();
                return;
            }
            if (!start_delay_timestamp.elapsed())
            {
                if (!start_delay_timestamp.valid())
                {
                    start_delay_timestamp.set(courier_def->start_delay * 1000);
                }
                return;
            }
        }
    }
    start_delay_timestamp.invalidate();
    if (!activity_begin_start_up())
    {
        activity_kill();
    }
}

// 0x007B1C10 (:431)
void activity_courier_chase_data::create_courier_case(const vector& case_pos)
{
    if (courier_case_handle != 0xFFFFFFFF)
    {
        return;
    }
    if (object_item_get_info("courier_case") == nullptr)
    {
        record_loss();
        activity_end_common(false);
        return;
    }
    if (!flags.at_destination)
    {
        gsm_set_alert_value(Alert_level_ranges[ALERT_LEVEL_RED].min_value);
    }
    obj_item_flags item_flags;
    item_flags.simulate = 1;
    item_flags.cast_shadows = 1;
    item_flags.cast_drop_shadows = 1;
    const vector pos = case_pos + UP_VECTOR * 0.5f; // Magic number: half a metre up
    obj_item_props props("courier_case", pos, Rfg_camera.real_orient, item_flags, COLLIDE_ITEM, -1.0f, INVALID_SALVAGE_MATERIAL);
    const item* case_item = World.create_object_global<item>(props, 0xFFFFFFFF);
    if (case_item != nullptr)
    {
        courier_case_handle = case_item->get_handle();
        objective_highlight_add(courier_case_handle, HIGHLIGHT_PURPOSE_NEUTRAL, HIGHLIGHT_STYLE_GLOW);
    }
    vehicle* vp = get_courier_vehicle();
    if (vp != nullptr)
    {
        objective_highlight_remove(vp->get_handle());
    }
    const object_squad* squad = get_courier_squad(); // rfg.exe inlines it
    if (squad != nullptr)
    {
        const npc* leader = squad->get_leader();
        if (leader != nullptr)
        {
            objective_highlight_remove(leader->get_handle());
        }
    }
    flags.data_dropped = true;
    out_of_bounds_timestamp.invalidate();
}

// 0x007B74A0 (:474)
void activity_courier_chase_data::check_for_end()
{
    const vector courier_pos = get_courier_position();
    if (!courier_pos.is_null())
    {
        data_debug_location = courier_pos;
    }
    const courier_chase_radio_lines response_offset = (courier_chase_radio_lines)(radio_gender_male ? 0 : 1);
    if (flags.data_retrieved)
    {
        if (gsm_get_alert_level() == ALERT_LEVEL_GREEN && !flags.completed)
        {
            flags.completed = true;
            // rfg.exe inlines these two.
            play_radio_line(CTC_RL_TRANSMITTING_DATA_PC, CTC_RL_NONE);
            play_radio_line((courier_chase_radio_lines)(CTC_RL_TRANSMITTING_DATA_RES_MALE + response_offset), CTC_RL_TRANSMITTING_DATA_PC);
        }
        if (flags.completed && dialogue_manager_get_num_dialogues(DIALOGUE_PRIORITY_UNDEFINED) == 0)
        {
            activity_end_victory();
        }
        return;
    }
    if (flags.data_dropped)
    {
        const object* case_objp = courier_case_handle != 0xFFFFFFFF ? World.object_from_handle<item>(courier_case_handle) : nullptr;
        if (case_objp != nullptr)
        {
            data_debug_location = case_objp->pos;
        }
        const float dist = player_find_shortest_player_dist_sqrd(data_debug_location);
        if (dist > CASE_LOST_DIST_SQRD)
        {
            record_loss();
            activity_end_common(false);
            return;
        }
        // A case that lands in a region that kills humans is lost.
        for (int i = 0; i < World.get_num_objects<trigger_region>(); i++)
        {
            const trigger_region* region = World.get_object<trigger_region>(i);
            if (!region->is_enabled() || region->get_trigger_region_type() != TRIGGER_REGION_TYPE_KILL_HUMAN)
            {
                continue;
            }
            bool inside = false;
            const trigger_region_shape shape = region->get_trigger_region_shape();
            if (shape == TRIGGER_REGION_SHAPE_BOX)
            {
                vector bmin;
                vector bmax;
                region->get_local_bbox(bmin, bmax);
                inside = ix_point_in_box_oriented(data_debug_location, region->pos, region->orient, bmin, bmax);
            }
            else if (shape == TRIGGER_REGION_SHAPE_SPHERE)
            {
                inside = ix_point_in_sphere(region->pos, region->get_radius(), data_debug_location);
            }
            if (inside)
            {
                record_loss();
                activity_end_common(false);
                return;
            }
        }
        if (dist <= Briefcase_pickup_distance_sq)
        {
            flags.data_dropped = false;
            flags.data_retrieved = true;
            World.destroy_object(courier_case_handle, false);
            courier_case_handle = 0xFFFFFFFF;
            audiolib_cue_play(Courier_case_pickup_sound, nullptr, nullptr);
            play_radio_line(CTC_RL_CAPTURED_DATA_PC, CTC_RL_NONE);
            play_radio_line((courier_chase_radio_lines)(CTC_RL_CAPTURED_DATA_RES_MALE + response_offset), CTC_RL_CAPTURED_DATA_PC);
            if (gsm_get_alert_level() < ALERT_LEVEL_YELLOW)
            {
                gsm_set_alert_value(Alert_level_ranges[ALERT_LEVEL_YELLOW].min_value);
            }
        }
        return;
    }
    object_squad* squad = squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(squad_handle) : nullptr;
    if (flags.escaping)
    {
        activity_end_common(true);
        return;
    }
    if (squad == nullptr || squad->is_killed())
    {
        vehicle* vp = get_courier_vehicle();
        if (vp != nullptr && !flags.at_destination)
        {
            vehicle_blow_up(vp, nullptr, false, nullptr, true);
        }
        create_courier_case(data_debug_location);
        return;
    }
    float player_dist = player_find_shortest_player_xz_dist_sqrd(squad->pos);
    npc* leader = squad->get_leader();
    if (!squad->streamed() || leader == nullptr || !leader->streamed())
    {
        // Out of the streamed world counts as out of range, and the timer runs out at once.
        player_dist = 3.4028235e+38f;
        out_of_bounds_timestamp.set(0);
    }
    if (player_dist <= ACTIVITY_END_DISTANCE * ACTIVITY_END_DISTANCE)
    {
        out_of_bounds_timestamp.invalidate();
    }
    else if (!out_of_bounds_timestamp.valid())
    {
        out_of_bounds_timestamp.set(CC_OUT_OF_BOUNDS_PERIOD_MS);
    }
    else if (out_of_bounds_timestamp.elapsed())
    {
        flags.escaping = true;
        flags.allow_catch_up = false;
        record_loss();
        activity_end_common(true);
        gamelog_set_activity_failed_reason(GLCR_ESCAPED);
        return;
    }
    // Original: no null check of the leader; human_in_vehicle answers null for it.
    if (!flags.at_destination && human_in_vehicle(leader) == nullptr)
    {
        // The courier has left the vehicle and turns on the player.
        flags.at_destination = true;
        gfm_orders_creation_params params;
        const player* pp = player_get_local_player();
        vehicle* vp = get_courier_vehicle();
        if (vp != nullptr)
        {
            vp->set_extra_mass(false);
            objective_highlight_remove(vp->get_handle());
        }
        objective_highlight_add(leader->get_handle(), HIGHLIGHT_PURPOSE_NEUTRAL, HIGHLIGHT_STYLE_ARROW);
        gsm_set_alert_value(Alert_level_ranges[ALERT_LEVEL_RED].min_value);
        params.location_target = pp->pos;
        params.object_target = pp->get_handle();
        params.orders_type = GFM_ORDERS_TYPE_ATTACK;
        params.team = squad->get_squad_team();
        params.ofp = ACTIVITY_ORDERS_FLAGS;
        const unsigned int orders = Orders_manager.create_new_orders(params);
        if (orders != 0xFFFFFFFF)
        {
            squad->reassign_orders(orders, nullptr, false);
            courier_stuck_timestamp.invalidate();
            courier_stuck_pos.zero();
        }
    }
}

// 0x007AD560 (:636)
bool activity_courier_chase_data::activity_start()
{
    if (!activity_base_modal::activity_start())
    {
        return false;
    }
    flags.chased = true;
    play_radio_line(CTC_RL_COURIER_SPOTTED_PC, CTC_RL_NONE); // rfg.exe inlines it
    out_of_bounds_timestamp.invalidate();
    object_squad* squad = squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(squad_handle) : nullptr;
    if (squad != nullptr)
    {
        squad->vflags.no_ragdoll = 1;
        squad->request_ai_replan(0xFFFFFFFF); // rfg.exe inlines it
        ui_tool_tips_add_tip("Courier", 4000, nullptr); // Magic number: four seconds in
    }
    return true;
}

// 0x00797090 (:878)
void activity_courier_chase_data::activity_end()
{
    activity_base_modal::activity_end(); // rfg.exe inlines it
    reset();
}

// 0x007A8DF0 (:674)
void activity_courier_chase_data::activity_kill()
{
    activity_base_modal::activity_kill(); // rfg.exe inlines it
    if (courier_def != nullptr)
    {
        dialogue_manager_cancel(courier_def->radio_lines[radio_gender_male ? 0 : 1], true);
    }
    if (orders_handle != 0xFFFFFFFF)
    {
        // Deviation: rfg.exe inlines these and reads through null for a handle beyond the pool.
        Orders_manager.destroy_orders(Orders_manager.get_orders_from_handle(orders_handle));
    }
    const object_squad* squad = squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(squad_handle) : nullptr;
    if (squad != nullptr)
    {
        World.destroy_object(squad->squad_vehicle, false);
    }
    World.destroy_object(squad_handle, false);
    reset();
}

// 0x00781740 (:698)
void activity_courier_chase_data::reinstate(activity_disable_reason disable_reason)
{
    activity_base::reinstate(disable_reason); // rfg.exe inlines it
    reset();
}

// 0x007B5D10 (:706)
void activity_courier_chase_data::activity_do_frame_ready()
{
    if (end_activity_next_frame)
    {
        activity_end_common(true);
        return;
    }
    activity_base::activity_do_frame_ready();
    if (vec_xz_dist_squared(get_courier_position(), patrol_end_pos) < NEAR_DEST_DIST_SQRD)
    {
        activity_end_common(true);
        return;
    }
    check_for_start();
}

// 0x00781780 (:726)
void activity_courier_chase_data::make_courier_stuck()
{
    courier_stuck_pos = get_courier_position();
    courier_stuck_timestamp.set(0);
}

// 0x007B7AE0 (:732)
void activity_courier_chase_data::activity_do_frame_active()
{
    if (end_activity_next_frame)
    {
        activity_end_common(true);
        return;
    }
    vehicle* vp = get_courier_vehicle();
    if (vp != nullptr)
    {
        if (!courier_stuck_timestamp.valid())
        {
            if (vehicle_ai_is_vehicle_stuck(&vp->vai))
            {
                courier_stuck_timestamp.set(3000); // Magic number: three seconds
                courier_stuck_pos = get_courier_position();
            }
        }
        else if (vec_dist(courier_stuck_pos, get_courier_position()) > 5.0f) // Magic number: five metres
        {
            courier_stuck_timestamp.invalidate();
            courier_stuck_pos.zero();
        }
        else if (courier_stuck_timestamp.elapsed())
        {
            // Original: leaves the whole frame when the squad or the leader is gone.
            object_squad* squad = get_courier_squad();
            if (squad == nullptr)
            {
                return;
            }
            npc* leader = squad->get_leader();
            if (leader == nullptr)
            {
                return;
            }
            leader->npc_flags.force_out_of_vehicle = 1;
            leader->hflags.disallow_vehicle_exit = 0;
            leader->hflags.limited_vehicle_exit = 0;
            const vector back = -leader->orient.fvec;
            leader->request_avoidance(nullptr, back, ZERO_VECTOR);
        }
    }
    if (!flags.escaping)
    {
        if (!out_of_bounds_timestamp.valid())
        {
            flags.allow_catch_up = false;
        }
        else if (out_of_bounds_timestamp.elapsed())
        {
            flags.escaping = true;
            flags.allow_catch_up = false;
        }
        else
        {
            flags.allow_catch_up = true;
        }
    }
    if (flags.escaping)
    {
        record_loss();
        activity_end_common(true);
        gamelog_set_activity_failed_reason(GLCR_ESCAPED);
        return;
    }
    if (!flags.at_destination && is_at_final_destination())
    {
        object_squad* squad = get_courier_squad();
        npc* leader = squad != nullptr ? squad->get_leader() : nullptr;
        if (leader != nullptr)
        {
            leader->npc_flags.courier_run_to_destination = 1;
            leader->hflags.invulnerable = 0;
            objective_highlight_add(leader->get_handle(), HIGHLIGHT_PURPOSE_NEUTRAL, HIGHLIGHT_STYLE_ARROW);
            flags.at_destination = true;
            if (vp != nullptr)
            {
                vp->set_extra_mass(false);
                objective_highlight_remove(vp->get_handle());
            }
        }
    }
    check_for_end();
}

// 0x007B5D70 (:805). Original: no null check of the end point.
void activity_courier_chase_data::activity_end_victory()
{
    ui_tool_tips_add_tip("Guerrilla_Action_Complete", 4000, nullptr); // Magic number: four seconds in
    activity_standard_rewards rewards;
    rewards.control = CC_CONTROL_DELTA;
    rewards.morale = CC_MORALE_DELTA;
    rewards.salvage = 0;
    object_courier_end_point* end_point = courier_end_handle != 0xFFFFFFFF ? World.object_from_handle<object_courier_end_point>(courier_end_handle) : nullptr;
    apply_standard_rewards(rewards, district_get_from_pos(end_point->pos));
    end_point->set_completed_flag(true);
    record_victory();
    activity_end_common(false);
}

// 0x007AD640 (:824)
void activity_courier_chase_data::activity_end_loss(bool courier_escaped)
{
    record_loss();
    activity_end_common(courier_escaped);
    if (courier_escaped)
    {
        gamelog_set_activity_failed_reason(GLCR_ESCAPED);
    }
}

// 0x00757D40 (:834)
void activity_courier_chase_data::scripted_activity_courier_escape()
{
    if (!flags.chased)
    {
        end_activity_next_frame = true;
    }
}

// 0x007A8EC0 (:843)
void activity_courier_chase_data::activity_end_common(bool courier_escaped)
{
    last_courier_escaped = courier_escaped;
    if (flags.chased && courier_escaped)
    {
        // rfg.exe inlines these two.
        play_radio_line(CTC_RL_OUT_OF_RANGE_PC, CTC_RL_NONE);
        play_radio_line((courier_chase_radio_lines)(CTC_RL_OUT_OF_RANGE_RES_MALE + (radio_gender_male ? 0 : 1)), CTC_RL_OUT_OF_RANGE_PC);
    }
    if (orders_handle != 0xFFFFFFFF)
    {
        // Deviation: rfg.exe inlines these and reads through null for a handle beyond the pool.
        Orders_manager.destroy_orders(Orders_manager.get_orders_from_handle(orders_handle));
    }
    object_squad* squad = squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(squad_handle) : nullptr;
    if (squad != nullptr)
    {
        squad->request_despawn_all_members(NPC_DESPAWN_TYPE_LOW, nullptr, true);
    }
    // The layer's other squads go too.
    for (int i = 0; i < layer_objects.size(); i++)
    {
        const object_squad_spawn_node* node = layer_objects[i] != 0xFFFFFFFF ? World.object_from_handle<object_squad_spawn_node>(layer_objects[i]) : nullptr;
        if (node == nullptr)
        {
            continue;
        }
        const unsigned int child = node->get_child_squad_handle();
        if (child == squad_handle || child == 0xFFFFFFFF)
        {
            continue;
        }
        object_squad* child_squad = World.object_from_handle<object_squad>(child);
        if (child_squad != nullptr)
        {
            child_squad->request_despawn_all_members(NPC_DESPAWN_TYPE_LOW, nullptr, true);
        }
    }
    activity_end();
}

// 0x007817C0 (:885)
void activity_courier_chase_data::render_icons() const
{
    if (squad_handle == 0xFFFFFFFF || flags.escaping)
    {
        return;
    }
    if (flags.data_dropped && !flags.data_retrieved && courier_case_handle != 0xFFFFFFFF)
    {
        Cartographer.add_temporary_icon(data_debug_location, MAP_ELEMENT_MISSION_OBJECTIVE, true, courier_case_handle, Default_map_item_color, false, 0xFFFFFFFF);
        const color area_color(0, 0, 0, 0xAA);
        Cartographer.add_decal(data_debug_location, MAP_ELEMENT_CIRCLE_FILLED_OUTLINE, 300.0f, area_color, true); // Magic number: where the case is lost
    }
    const vector courier_pos = get_courier_position();
    if (!courier_pos.is_null())
    {
        Cartographer.add_temporary_icon(courier_pos, MAP_ELEMENT_COURIER_ICON, true, squad_handle, flags.completed ? Completed_icon_color : Default_map_item_color, false, get_spawn_object_handle());
    }
}

// 0x0076BEC0 (:918). Debug text in rfg.exe, on the stack here where rfg.exe uses its thread-local buffer.
bool activity_courier_chase_data::render_hud(int x_pos, int y_pos)
{
    if (is_active() && out_of_bounds_timestamp.valid())
    {
        const int remaining = out_of_bounds_timestamp.time_until();
        if (remaining >= 0)
        {
            char text[256];
            sprintf(text, "Courier will Escape in: %02i.%i seconds", remaining / 1000, (remaining % 1000) / 100);
            gr_set_color(255, 255, 255, 255);
            gr_string(0x2D, 0x118, text, General_font, GM_FILTER);
        }
    }
    return false;
}

// 0x007818A0 (:950)
void activity_courier_chase_data::render_regions() const
{
    if (!is_active())
    {
        return;
    }
    const vector courier_pos = get_courier_position();
    if (courier_pos.is_null() || flags.escaping)
    {
        return;
    }
    static const color CC_AREA_INSIDE_COLOR(100, 230, 100, 100); // 0x02273DA0
    static const color CC_AREA_OUTSIDE_COLOR(230, 100, 100, 100); // 0x02273D9C
    const bool outside = out_of_bounds_timestamp.valid();
    Cartographer.add_decal(courier_pos, MAP_ELEMENT_CIRCLE_FILLED_OUTLINE, ACTIVITY_END_DISTANCE, outside ? CC_AREA_OUTSIDE_COLOR : CC_AREA_INSIDE_COLOR, !outside);
}

// 0x00757D60 (:974)
const char* activity_courier_chase_data::get_subtitle() const
{
    if (!is_active())
    {
        return nullptr;
    }
    if (flags.allow_catch_up)
    {
        return "ACT_COURIER_CATCH";
    }
    if (flags.escaping)
    {
        return "ACT_COURIER_ESCAPING";
    }
    if (flags.data_retrieved)
    {
        return "ACT_COURIER_LOSE_ALERT";
    }
    if (flags.data_dropped)
    {
        return "ACT_COURIER_GET_DATA";
    }
    if (flags.at_destination)
    {
        return "ACT_COURIER_KILL";
    }
    return flags.chased ? "ACT_COURIER_FOLLOW_COURIER" : "";
}

// 0x00757DC0 (:1010)
const char* activity_courier_chase_data::get_icon_image_name() const
{
    return get_minimap_bitmap_icon_name(0x12); // Magic number: the courier map element
}

// 0x0076BFA0 (:1016)
object_squad* activity_courier_chase_data::get_courier_squad() const
{
    object_squad* squad = squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(squad_handle) : nullptr;
    return squad != nullptr && !squad->is_killed() ? squad : nullptr;
}

// 0x0076BFE0 (:1027). Original: the squad's position while simulated, else its leader's.
vector activity_courier_chase_data::get_courier_position() const
{
    const object_squad* squad = squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(squad_handle) : nullptr;
    if (squad != nullptr)
    {
        if (squad->simulated())
        {
            return squad->pos;
        }
        const npc* leader = squad->get_leader();
        if (leader != nullptr)
        {
            return leader->pos;
        }
    }
    return ZERO_VECTOR;
}

// 0x0076C080 (:1048). Original: remembers the squad's vehicle, so a dead squad's vehicle is still found.
vehicle* activity_courier_chase_data::get_courier_vehicle()
{
    vehicle* vp = vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(vehicle_handle) : nullptr;
    if (vp != nullptr)
    {
        return vp;
    }
    const object_squad* squad = get_courier_squad(); // rfg.exe inlines it
    if (squad != nullptr)
    {
        vp = squad->get_vehicle();
        if (vp != nullptr)
        {
            vehicle_handle = vp->get_handle();
            return vp;
        }
    }
    return nullptr;
}

// 0x00757DD0 (:1071)
float activity_courier_chase_data::get_move_speed() const
{
    if (flags.escaping)
    {
        return courier_def->escape_speed;
    }
    if (flags.allow_catch_up)
    {
        return courier_def->catch_up_speed;
    }
    return flags.chased ? courier_def->evade_speed : courier_def->normal_speed;
}

// 0x0076C0F0 (:1098). Original: nothing fills courier_route in rfg.exe, so this returns at once.
void activity_courier_chase_data::update_destination_ghost(bool simulated)
{
    const vector courier_pos = get_courier_position();
    const float near_dist = simulated ? SIMULATED_NEAR_DEST_DIST_SQRD : SPAWNED_NEAR_DEST_DIST_SQRD;
    for (int i = 0; i < 3; i++) // Magic number: looks three points ahead
    {
        const int index = next_visited_index + i;
        if (index >= courier_route.size())
        {
            return;
        }
        if (vec_dist_squared(courier_pos, courier_route[index]) <= near_dist)
        {
            next_visited_index = index + 1;
            update_destination_ghost(simulated);
            return;
        }
    }
}

// 0x0076C190 (:1133)
void activity_courier_chase_data::update_destination_patrol()
{
    const vector courier_pos = get_courier_position();
    vector dest_pos;
    if (!current_destination.get_pos(dest_pos) || vec_dist_squared(courier_pos, dest_pos) >= NEAR_DEST_DIST_SQRD || patrol_handle == 0xFFFFFFFF)
    {
        return;
    }
    const object_patrol* patrol = World.object_from_handle<object_patrol>(patrol_handle);
    if (patrol != nullptr)
    {
        current_destination = patrol->get_next_patrol_point_handle(current_destination, courier_pos);
        update_destination_patrol();
    }
}

// 0x0076C230 (:1155). Original: new_pos goes unused.
void activity_courier_chase_data::update_destination(const vector& new_pos, bool simulated)
{
    update_destination_ghost(simulated);
    update_destination_patrol();
}

// 0x0076C250 (:1165). Original: my_pos goes unused.
bool activity_courier_chase_data::get_ghost_destination(const vector& my_pos, vector& dest_pos)
{
    update_destination_ghost(true);
    update_destination_patrol();
    if (next_visited_index >= 0 && next_visited_index < courier_route.size())
    {
        dest_pos = courier_route[next_visited_index];
        return true;
    }
    return false;
}

// 0x0076C2A0 (:1183)
bool activity_courier_chase_data::is_at_current_destination() const
{
    const vector courier_pos = get_courier_position();
    vector dest_pos;
    return current_destination.get_pos(dest_pos) && vec_xz_dist_squared(dest_pos, courier_pos) < NEAR_DEST_DIST_SQRD;
}

// 0x0076C300 (:1202)
bool activity_courier_chase_data::is_at_final_destination()
{
    return vec_xz_dist_squared(get_courier_position(), patrol_end_pos) < NEAR_DEST_DIST_SQRD;
}

// 0x007819B0 (:1209)
bool activity_courier_chase_data::collect_spawn_objects(farray<object const*, 32>& spawn_objects, const vector& player_pos, unsigned int district_handlep) const
{
    return all_courier_chase_complete() ? collect_nodes_complete(spawn_objects) : collect_nodes_incomplete(spawn_objects);
}

// 0x007B1E00 (:1218)
bool activity_courier_chase_data::activity_initialize(const object* spawn_objp, unsigned int new_district_handle)
{
    if (spawn_objp == nullptr || is_disabled())
    {
        return false;
    }
    object_courier_end_point* end_point = spawn_objp->get_handle() != 0xFFFFFFFF ? World.object_from_handle<object_courier_end_point>(spawn_objp->get_handle()) : nullptr;
    if (end_point == nullptr || end_point->get_completed_flag() || !activity_base_modal::activity_initialize(spawn_objp, new_district_handle))
    {
        return false;
    }
    reset();
    object_patrol* patrol = courier_find_patrol(end_point);
    // Original: a patrol in the activity's layer wins over the node's; the last one found.
    for (int i = 0; i < layer_objects.size(); i++)
    {
        object_patrol* layer_patrol = World.object_from_handle<object_patrol>(layer_objects[i]);
        if (layer_patrol != nullptr)
        {
            patrol = layer_patrol;
        }
    }
    if (patrol == nullptr)
    {
        return false;
    }
    squad_spawn_parameters params;
    params.orders.location_target = patrol->pos;
    params.orders.ofp = ACTIVITY_ORDERS_FLAGS;
    params.orders.orders_type = GFM_ORDERS_TYPE_PATROL;
    params.orders.object_target = patrol->get_handle();
    params.squad.squad_def = end_point->courier_def->squad_def != nullptr ? end_point->courier_def->squad_def : Default_squad_def;
    params.squad.squad_pos = patrol->pos;
    params.squad.squad_orient = patrol->orient;
    params.squad.veh_spawn_group = end_point->courier_def->vehicle_group != nullptr ? end_point->courier_def->vehicle_group : Default_vehicle_group;
    params.squad.squad_flags.disallow_reassignment = 1;
    params.squad.vflags.no_ragdoll = 1;
    if (params.spawn_squad() == nullptr || params.spawned_squad == nullptr)
    {
        return false;
    }
    if (params.created_orders == nullptr || params.created_orders->handle == 0xFFFFFFFF)
    {
        params.destroy_squad();
        return false;
    }
    squad_handle = params.spawned_squad->get_handle();
    orders_handle = params.created_orders->handle;
    courier_end_handle = end_point->get_handle();
    const bool spawned = spawn_courier(end_point);
    patrol_handle = patrol->get_handle();
    if (!patrol->get_last_patrol_node_pos(patrol_end_pos))
    {
        return false;
    }
    if (spawned)
    {
        radio_gender_male = int_rand_chance(50); // Magic number: an even chance
        play_radio_line(radio_gender_male ? CTC_RL_START_MALE : CTC_RL_START_FEMALE, CTC_RL_NONE);
    }
    Done_one_time_vehicle_init = false;
    return spawned;
}

// 0x00757E10 (:1385). Original: asks the global instance, which is the only one.
bool activity_courier_chase_data::spawn_courier(const object_courier_end_point* start_point)
{
    if (Courier_chase_data.is_disabled() || start_point == nullptr)
    {
        return false;
    }
    courier_def = start_point->courier_def;
    return courier_def != nullptr;
}

// 0x0076C340 (:1401)
void activity_courier_chase_data::apply_vehicle_health_and_mass_attributes()
{
    if (Done_one_time_vehicle_init)
    {
        return;
    }
    vehicle* vp = get_courier_vehicle();
    if (vp == nullptr || vp->most_recent_driver == 0xFFFFFFFF)
    {
        return;
    }
    npc* driver = World.object_from_handle<npc>(vp->most_recent_driver);
    if (driver == nullptr)
    {
        return;
    }
    // rfg.exe: an x87 product of the unsigned hit points and the multiplier.
    vehicle_set_max_hp((int)((double)vehicle_get_max_hit_points(vp) * (double)courier_def->vehicle_health_multiplier), vp->get_handle());
    vp->set_extra_mass(true);
    // Original: any multiplier of at most 1 becomes 1.
    vp->set_extra_mass_value(1.0f - courier_def->vehicle_mass_multiplier >= 0.0f ? 1.0f : courier_def->vehicle_mass_multiplier);
    objective_highlight_add(vp->get_handle(), HIGHLIGHT_PURPOSE_NEUTRAL, HIGHLIGHT_STYLE_ARROW);
    // Original: this one delivery also toughens the driver and keeps the player out of the vehicle.
    if (v_stricmp(get_instance_name(), "je_cu_08") == 0)
    {
        driver->max_hit_points = (int)(courier_def->vehicle_health_multiplier * (float)driver->max_hit_points);
        driver->refill_health(1.0f);
        vp->m_flags.disable_for_player = 1;
    }
    Done_one_time_vehicle_init = true;
}

// 0x00757E50 (:1430)
void activity_cc_courier_damaged_start()
{
    Damage_causes_start = true;
    Courier_chase_data.polling_period.set(0);
}

// 0x00757E70 (:1437)
bool activity_cc_is_courier(const npc* np)
{
    if (np == nullptr || Courier_chase_data.get_courier_squad_handle() == 0xFFFFFFFF || np->is_killed() || np->get_ai() == nullptr)
    {
        return false;
    }
    return np->get_ai()->get_squad_handle() == Courier_chase_data.get_courier_squad_handle();
}

// 0x00757EB0 (:1458)
bool activity_cc_is_active_courier(const npc* np)
{
    // Deviation from rfg.exe: activity_get_mode, where it reads activity.cpp's private static.
    return Courier_chase_data.mode == activity_get_mode() && activity_cc_is_courier(np); // rfg.exe inlines it
}

// 0x00757F00 (:1468)
bool activity_cc_most_recent_courier_escaped()
{
    return Courier_chase_data.most_recent_courier_escaped();
}

// 0x00757F10 (:1474). Empty in rfg.exe.
bool activity_cc_abandon_vehicle(const vehicle* vp)
{
    return false;
}

// 0x007B2050 (:1489). Original: with no player it asks the first district, unchecked.
bool activity_cc_spawn_courier(const object_courier_end_point* end_point)
{
    const player* pp = player_get_local_player();
    const district* dp = pp != nullptr ? district_get_from_pos(pp->pos) : World.get_object<district>(0);
    return Courier_chase_data.activity_courier_chase_data::activity_initialize(end_point, dp != nullptr ? dp->get_handle() : 0xFFFFFFFF);
}

// 0x00757F20 (:1499)
void activity_courier_spawned_this_frame()
{
    Courier_chase_data.radio_gender_male = vrand() % 100 < 51; // Magic number: rfg.exe compares with 0x33
    Courier_chase_data.play_radio_line(Courier_chase_data.radio_gender_male ? CTC_RL_START_MALE : CTC_RL_START_FEMALE, CTC_RL_NONE); // rfg.exe inlines it
}

// 0x00757FA0 (:1511)
void activity_cc_scripted_escape()
{
    Courier_chase_data.scripted_activity_courier_escape(); // rfg.exe inlines it
}

// 0x00757FC0 (:1569)
unsigned int activity_cc_get_courier_squad_handle()
{
    return Courier_chase_data.get_courier_squad_handle();
}

// 0x00757FD0 (:1578)
void activity_cc_get_rewards(float& control, float& morale)
{
    control = CC_CONTROL_DELTA;
    morale = CC_MORALE_DELTA;
}

// 0x00754500 (.h:115)
bool activity_courier_chase_data::chased() const
{
    return flags.chased;
}

// 0x00754510 (.h:117)
bool activity_courier_chase_data::escaping() const
{
    return flags.escaping;
}

// 0x00754520 (.h:119)
bool activity_courier_chase_data::allow_follower_catch_up() const
{
    return flags.allow_catch_up;
}

// 0x00754530 (.h:121)
bool activity_courier_chase_data::most_recent_courier_escaped() const
{
    return last_courier_escaped;
}

// 0x00754540 (.h:123)
unsigned int activity_courier_chase_data::get_courier_squad_handle() const
{
    return squad_handle;
}
