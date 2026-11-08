// Original: volition/rfg/code/gameplay/activities/activity_delivery.cpp
//
// gameflow.md GF4: Guerrilla Express, driving a vehicle from a start node to its pair within a time. The player starts it
// by getting into the node's vehicle, with no briefing.

#include "volition/rfg/code/gameplay/activities/activity_delivery.h"
#include "volition/rfg/code/gameplay/activities/activity_delivery_internal.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/ai/ai_scripting.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_rewards.h"
#include "volition/rfg/code/gameplay/activities/base_activity_data.h"
#include "volition/rfg/code/gameplay/activities/delivery_common.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/hud/hud_ui_activity_complete.h"
#include "volition/rfg/code/hud/hud_ui_district_liberation.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/hud/minimap_icon_info.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_voice/voice_line_unlocking.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_delivery_node.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/rfg/code/scripting/dialogue_manager.h"
#include "volition/rfg/code/scripting/objective_highlight.h"
#include "volition/rfg/code/scripting/public_address.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_instance.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/vlib/code/util/string_util.h"

extern unsigned int Activity_node_start_on_load; // 0x01651244 (activity.cpp)

activity_delivery_data Delivery_activity_data; // 0x01652E00

static int Delivery_sound_timer_tick; // 0x022258BC
static checksum_stri GE_START_TOOL_TIP_NAME_CHECKSUM("Guerrilla_Express", 0); // 0x02231B8C
static const char* Delivery_message_persona_names[NUM_DELIVERY_RADIO_LINES] = {"PCM01", "TIMED", "TIMED"}; // 0x012DAFE0

static const float DELIVERY_COMPLETE_SPEED_SQ = 2.25f; // 0x012DAFDC: the vehicle must be under 1.5 m/s to finish
static const unsigned int DELIVERY_NODE_RUNNING = 0x7FFFFFFD; // Magic number: current_completion_time during a run

// 0x007970B0 (:78). rfg.exe inlines the lookup in activity_data_find_handle.
bool this_instance_of_delivery_is_disabled(object_delivery_node* delivery_node)
{
    const base_activity_data* data = activity_data_find_handle(object_delivery_node_get_name(delivery_node));
    return data != nullptr && data->activity_flags.disabled;
}

// 0x00758000 (:88)
bool vehicle_is_ready_to_start(vehicle* vp, const object_vehicle_spawn_node* spawn_node)
{
    if (vp == nullptr || vp->m_flags.m_on_fire || vp->is_killed() || spawn_node == nullptr)
    {
        return false;
    }
    return vec_xz_dist_squared(vp->pos, spawn_node->pos) <= 25.0f; // Magic number: within five metres of the node
}

// 0x0076C4B0 (:96). Original: only an automobile with its resources loaded, the reverse speed left to -1.
void set_vehicle_max_speed(unsigned int vehicle_handle, float speed)
{
    if (vehicle_handle == 0xFFFFFFFF)
    {
        return;
    }
    automobile* ap = World.object_from_handle<automobile>(vehicle_handle);
    if (ap != nullptr && ap->a_rdd.is_loaded())
    {
        ap->a_rdd.get_data()->m_hk_vehicle_instance->set_enforced_max_speed(speed, -1.0f);
    }
}

// 0x0077E630 (.h:28). Original: the rest stays unset until level_init calls reset.
activity_delivery_data::activity_delivery_data()
    : activity_base_modal(ACTIVITY_MODE_DELIVERY, false)
{
}

// 0x00769100
activity_delivery_data::~activity_delivery_data()
{
}

// 0x007A0F90 (:119)
void activity_delivery_data::system_init()
{
    audiolib_cue_get_id("HUD_TIMER_STD_SECOND", &Delivery_sound_timer_tick);
    load_delivery_data(-1);
}

// 0x00797120 (:129)
void activity_delivery_data::level_init()
{
    activity_base_modal::level_init(); // rfg.exe inlines it
    reset();
}

// 0x00797170 (:138)
bool activity_delivery_data::activity_initialize(const object* spawn_obj, unsigned int new_district_handle)
{
    if (spawn_obj == nullptr || !activity_base_modal::activity_initialize(spawn_obj, new_district_handle))
    {
        return false;
    }
    if (spawn_obj->get_obj_type() != OT_DELIVERY_NODE)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
        return false;
    }
    reset();
    const object_delivery_node* start = static_cast<const object_delivery_node*>(spawn_obj);
    if (!start->is_start_node())
    {
        return false;
    }
    delivery_start_node = start->get_handle();
    delivery_start_pos = start->pos;
    delivery_info = start->get_delivery_def();
    if (delivery_info == nullptr)
    {
        return false;
    }
    const object_delivery_node* end = start->find_pair_node();
    if (end == nullptr)
    {
        reset();
        return false;
    }
    delivery_end_node = end->get_handle();
    delivery_end_pos = end->pos;
    vector to_end = delivery_start_pos - delivery_end_pos;
    distance_to_end = to_end.mag() - delivery_info->end_node_radius;
    const object_vehicle_spawn_node* spawn_node = activity_delivery_get_vehicle_spawn(start);
    if (spawn_node == nullptr)
    {
        return false;
    }
    delivery_vehicle = spawn_node->get_child_vehicle_handle();
    if (delivery_vehicle == 0xFFFFFFFF)
    {
        return false;
    }
    if (delivery_info->vehicle_speed != -1.0f)
    {
        set_vehicle_max_speed(delivery_vehicle, delivery_info->vehicle_speed);
    }
    if (delivery_info->vehicle_max_hitpoints != -1)
    {
        vehicle_set_max_hp(delivery_info->vehicle_max_hitpoints, delivery_vehicle);
        // rfg.exe: x87 product truncated through a 64-bit integer.
        vehicle_set_hit_points(delivery_vehicle, (unsigned int)(__int64)((double)delivery_info->vehicle_max_hitpoints * (double)delivery_info->vehicle_health));
    }
    ai_scripting_set_vehicle_enabled_for_npc_drivers(delivery_vehicle, false);
    time_left_to_complete.invalidate();
    time_left_to_fail.invalidate();
    post_death_failure_time.invalidate();
    previous_time_left_ms = 0;
    return true;
}

// 0x007AD670 (:226). Original: no null check of the end node.
bool activity_delivery_data::activity_start()
{
    if (!activity_base_modal::activity_start() || delivery_info == nullptr || delivery_vehicle == 0xFFFFFFFF)
    {
        return false;
    }
    const vehicle* vp = World.object_from_handle<vehicle>(delivery_vehicle);
    if (vp == nullptr || !vehicle_player_in_vehicle(vp))
    {
        return false;
    }
    const gfm_alert_levels new_alert_level = delivery_info->alert_level;
    if (new_alert_level >= 0 && gsm_get_alert_level() < new_alert_level)
    {
        gsm_set_alert_level(new_alert_level);
    }
    object_delivery_node* end_node = delivery_end_node != 0xFFFFFFFF ? World.object_from_handle<object_delivery_node>(delivery_end_node) : nullptr;
    if (end_node->streamed())
    {
        activity_start_end_effect(delivery_end_node);
    }
    end_node->current_completion_time = DELIVERY_NODE_RUNNING;
    time_left_to_complete.set(delivery_info->time_to_complete);
    time_left_to_fail.invalidate();
    post_death_failure_time.invalidate();
    previous_time_left_ms = 0;
    time_warning_first_time = true;
    timer_started = false;
    ui_tool_tips_add_tip(GE_START_TOOL_TIP_NAME_CHECKSUM, 2750, nullptr); // Magic number: 2.75 seconds in
    return true;
}

// 0x007973B0 (:281)
void activity_delivery_data::activity_end()
{
    activity_base_modal::activity_end(); // rfg.exe inlines it
    Cartographer.remove_player_marker(false);
    // Samanya stays quiet for ten seconds, through the first guerrilla tech in the world.
    for (int i = 0; i < World.get_num_objects<npc>(); i++)
    {
        npc* np = World.get_object<npc>(i);
        if (np->h_info->flags.guerrilla_tech)
        {
            voice_line_unlocking_set_global_cooldown(np, VLUG_SAMANYA, 10000);
            break;
        }
    }
    const object_delivery_node* start = delivery_start_node != 0xFFFFFFFF ? World.object_from_handle<object_delivery_node>(delivery_start_node) : nullptr;
    if (start != nullptr)
    {
        object_vehicle_spawn_node* spawn_node = activity_delivery_get_vehicle_spawn(start); // rfg.exe inlines it
        vehicle* vp = delivery_vehicle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(delivery_vehicle) : nullptr;
        if (spawn_node != nullptr && vp != nullptr)
        {
            spawn_node->force_abandon_child(vp);
        }
    }
    reset();
}

// 0x007974C0 (:320). Original: runs the common end, then activity_end runs it again.
void activity_delivery_data::activity_kill()
{
    activity_base_modal::activity_kill(); // rfg.exe inlines it
    activity_end();
}

// 0x00758050 (:328)
const char* activity_delivery_data::get_title() const
{
    return "ACT_GEXPRESS_NAME";
}

// 0x00758060 (:342)
const char* activity_delivery_data::get_subtitle() const
{
    if (delivery_info == nullptr)
    {
        return nullptr;
    }
    if (time_left_to_fail.valid() && !time_left_to_fail.elapsed())
    {
        return "ACT_GEXPRESS_GET_IN";
    }
    return delivery_info->subtitle;
}

// 0x007580B0 (:356)
const char* activity_delivery_data::get_icon_image_name() const
{
    return get_minimap_bitmap_icon_name(0x1A); // Magic number: the Guerrilla Express map element
}

// 0x007974E0 (:362). Keeps the nodes' spawners enabled, and answers the nearest start node whose vehicle waits on it.
object_delivery_node* activity_delivery_data::find_best_delivery_node(const player* pp)
{
    object_delivery_node* best = nullptr;
    float best_dist = 0.0f;
    for (int i = 0; i < World.get_num_objects<object_delivery_node>(); i++)
    {
        object_delivery_node* node = World.get_object<object_delivery_node>(i);
        if (!node->is_start_node())
        {
            continue;
        }
        object_vehicle_spawn_node* spawn_node = activity_delivery_get_vehicle_spawn(node); // rfg.exe inlines it
        if (spawn_node == nullptr)
        {
            continue;
        }
        if (this_instance_of_delivery_is_disabled(node))
        {
            spawn_node->disable_spawn();
            continue;
        }
        if (spawn_node->is_disabled())
        {
            spawn_node->enable_spawn();
        }
        if (spawn_node->get_child_vehicle_handle() == 0xFFFFFFFF)
        {
            continue;
        }
        vehicle* vp = World.object_from_handle<vehicle>(spawn_node->get_child_vehicle_handle());
        if (vehicle_is_ready_to_start(vp, spawn_node))
        {
            const float dist = vec_xz_dist_squared(node->pos, pp->pos);
            if (best == nullptr || dist < best_dist)
            {
                best = node;
                best_dist = dist;
            }
            node->set_disabled(false);
        }
        else if ((spawn_node->in_spawn_range(pp) && !spawn_node->should_spawn()) || spawn_node->is_disabled())
        {
            node->set_disabled(true);
        }
    }
    return best;
}

// 0x00797670 (:426). Original: the activity starts with the player already driving the node's vehicle.
void activity_delivery_data::activity_do_frame_inactive()
{
    if (!can_initialize())
    {
        return;
    }
    render_icons();
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    object_delivery_node* node = find_best_delivery_node(pp);
    if (node == nullptr || activity_get_mode() != ACTIVITY_MODE_NONE || node->is_disabled())
    {
        return;
    }
    const district* dp = activity_int_get_district_from_node(node);
    if (dp == nullptr)
    {
        dp = district_get_from_pos(pp->pos);
    }
    const object_vehicle_spawn_node* spawn_node = activity_delivery_get_vehicle_spawn(node);
    if (spawn_node == nullptr)
    {
        return;
    }
    delivery_vehicle = spawn_node->get_child_vehicle_handle();
    if (delivery_vehicle == 0xFFFFFFFF)
    {
        return;
    }
    vehicle* vp = World.object_from_handle<vehicle>(delivery_vehicle);
    if (vp == nullptr || vehicle_passenger_in_seat(vp, VSI_DRIVER) != pp)
    {
        return;
    }
    vector to_node = vp->pos - node->pos;
    if (to_node.mag() < 5.0f && !vp->m_flags.m_on_fire && !vp->is_killed()) // Magic number: five metres
    {
        activity_kill_all_except(mode);
        start_activity_spawn(node, dp);
    }
}

// 0x0076C510 (:495). Original: no null check of the start node.
void activity_delivery_data::activity_do_frame_ready()
{
    activity_base::activity_do_frame_ready();
    if (player_get_local_player() == nullptr || delivery_vehicle == 0xFFFFFFFF)
    {
        activity_kill();
        return;
    }
    const vehicle* vp = World.object_from_handle<vehicle>(delivery_vehicle);
    if (vp == nullptr || vp->is_killed())
    {
        activity_kill();
        return;
    }
    object_delivery_node* start = delivery_start_node != 0xFFFFFFFF ? World.object_from_handle<object_delivery_node>(delivery_start_node) : nullptr;
    start->set_disabled(true);
    if (!activity_begin_start_up())
    {
        activity_kill();
    }
}

// 0x0076C5B0 (:524). Answers true when the activity is ending, so the frame does nothing else.
bool activity_delivery_data::do_end_processing(bool player_died)
{
    if (successfully_completed)
    {
        if (!end_processing_started)
        {
            dialogue_manager_cancel_all();
            play_radio_line(DELIVERY_RADIO_LINE_WIN_RESPONSE);
            activity_stop_end_effect(delivery_end_node); // rfg.exe inlines it
            end_processing_started = true;
            return true;
        }
        if (dialogue_manager_get_num_dialogues(DIALOGUE_PRIORITY_UNDEFINED) == 0)
        {
            if (hud_ui_activity_complete_start(ACTIVITY_MODE_DELIVERY, true))
            {
                splash_screen_active = true;
                return true;
            }
            activity_end();
        }
        return true;
    }
    if (player_died)
    {
        activity_failed = true;
    }
    if (!activity_failed)
    {
        return false;
    }
    if (!end_processing_started)
    {
        dialogue_manager_cancel_all();
        activity_stop_end_effect(delivery_end_node);
        end_processing_started = true;
        return true;
    }
    if (dialogue_manager_get_num_dialogues(DIALOGUE_PRIORITY_UNDEFINED) == 0)
    {
        if (hud_ui_activity_complete_start(ACTIVITY_MODE_DELIVERY, false))
        {
            splash_screen_active = true;
            return true;
        }
        activity_end();
    }
    return true;
}

// 0x007580C0 (:576). Original: after a win the first asks only arms it, so the liberation screen can finish.
bool activity_delivery_data::is_retry_ready()
{
    if (!successfully_completed)
    {
        return true;
    }
    if (pending_retry)
    {
        pending_retry = false;
        return true;
    }
    pending_retry = true;
    return false;
}

// 0x007B5E10 (:596). Original: one share of the rewards per first win and per first par time beaten.
void activity_delivery_data::give_rewards()
{
    activity_standard_rewards rewards;
    rewards.control = delivery_info->standard_rewards.control * (float)standard_rewards_count;
    rewards.salvage = delivery_info->standard_rewards.salvage * standard_rewards_count;
    rewards.morale = delivery_info->standard_rewards.morale * (float)standard_rewards_count;
    apply_standard_rewards(rewards, nullptr);
}

// 0x007B7D20 (:609)
void activity_delivery_data::activity_do_frame_active()
{
    static int s_previous_second; // 0x022258C0, a static where the member previous_second goes unused
    if (pending_retry)
    {
        if (hud_ui_district_liberation_screen_active())
        {
            return;
        }
        activity_guerrilla_express_replay();
    }
    if (splash_screen_active || do_end_processing(false))
    {
        return;
    }
    if (!queued_start_conversation && !ui_tool_tips_tip_is_queued(GE_START_TOOL_TIP_NAME_CHECKSUM))
    {
        play_radio_line(DELIVERY_RADIO_LINE_START);
        append_radio_line(DELIVERY_RADIO_LINE_START_RESPONSE, 500); // Magic number: half a second after
        queued_start_conversation = true;
    }
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
        activity_kill();
        return;
    }
    if (pp->is_killed())
    {
        if (post_death_failure_time.valid() && post_death_failure_time.elapsed())
        {
            activity_lost();
            return;
        }
        // Original: set again every frame while the player is dead, so the two seconds never run out.
        post_death_failure_time.set(2000);
        return;
    }
    vehicle* vp = delivery_vehicle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(delivery_vehicle) : nullptr;
    if (vp == nullptr)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
        activity_kill();
        return;
    }
    if (vp->m_flags.m_corpse)
    {
        gamelog_set_activity_failed_reason(GLCR_VEHICLE_DESTROYED);
        activity_lost();
        return;
    }
    // Original bug kept: until the vehicle moves, the clock is set again every frame, so a full clock waits for the first move.
    if (!timer_started)
    {
        float speed;
        vehicle_get_speed(vp, speed);
        if (fabsf(speed) >= 0.5f) // Magic number: the clock starts when the vehicle moves
        {
            timer_started = true;
        }
        time_left_to_complete.set(delivery_info->time_to_complete);
    }
    if (vehicle_player_is_occupant(vp))
    {
        if (time_left_to_fail.valid())
        {
            time_left_to_fail.invalidate();
        }
    }
    else if (!time_left_to_fail.valid())
    {
        time_left_to_fail.set(delivery_info->time_to_fail);
    }
    else
    {
        if (time_left_to_fail.elapsed())
        {
            gamelog_set_activity_failed_reason(GLCR_VEHICLE_ABANDONED);
            activity_lost();
            return;
        }
        const int second = time_left_to_fail.time_until() / 1000;
        audiolib_cue_info info;
        if (second < 5 && s_previous_second != second && audiolib_cue_get_info(Ga_cue_ids.timer_final_seconds_remains, &info) == AUDIOLIB_ERR_NONE)
        {
            audiolib_params params;
            params.flags = 0x30; // Magic number: rfg.exe's flags for a 2D interface sound
            audiolib_cue_play(Ga_cue_ids.timer_final_seconds_remains, &params, nullptr);
        }
        s_previous_second = second;
    }
    if (time_left_to_complete.elapsed())
    {
        gamelog_set_activity_failed_reason(GLCR_OUT_OF_TIME);
        activity_lost();
        return;
    }
    // Original: the warning only clears its flag, nothing plays.
    if (time_left_to_complete.time_until() < 10001 && time_warning_first_time) // Magic number: ten seconds
    {
        time_warning_first_time = false;
    }
    vector to_end = vp->pos - delivery_end_pos;
    distance_to_end = to_end.mag() - delivery_info->end_node_radius;
    if (distance_to_end > 0.0f)
    {
        if (distance_to_end < 25.0f && !alert_set_green) // Magic number: 25 metres from the end
        {
            gsm_set_alert_value(0.0f);
            alert_set_green = true;
        }
    }
    else
    {
        vector velocity;
        vehicle_get_velocity(vp, velocity);
        if (velocity.mag_squared() < DELIVERY_COMPLETE_SPEED_SQ)
        {
            record_victory();
        }
    }
}

// 0x007580F0 (:760). Original: cleans up with no shutdown delay; the second set overrides the base's second.
void activity_delivery_data::activity_clean_up_once()
{
    activity_base_modal::activity_clean_up_once(); // rfg.exe inlines it
    activity_shutdown_period_override(0);
}

// 0x00758120 (:767)
void activity_delivery_data::activity_do_frame_paused()
{
    if (can_initialize())
    {
        render_icons();
    }
}

// 0x00758140 (:780)
bool activity_delivery_data::collect_spawn_objects(farray<object const*, 32>& spawn_objects, const vector& player_pos, unsigned int district_handlep) const
{
    return false;
}

// 0x007819E0 (:810)
void activity_delivery_data::record_victory()
{
    object_delivery_node* start = delivery_start_node != 0xFFFFFFFF ? World.object_from_handle<object_delivery_node>(delivery_start_node) : nullptr;
    if (start == nullptr)
    {
        return;
    }
    const bool was_completed = start->is_completed();
    const bool had_beat_par = start->has_beat_par_time(); // the decompile drops this read: BL at 0x00781A21
    successfully_completed = true;
    vi_exit_data exit_data;
    exit_data.initialize(); // rfg.exe clears the flags byte
    vehicle_interaction_request_exit(player_get_local_player(), &exit_data);
    vehicle_freeze(delivery_vehicle);
    vehicle* vp = delivery_vehicle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(delivery_vehicle) : nullptr;
    if (vp != nullptr)
    {
        vehicle_reposition_on_ground(vp);
    }
    player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        human_equip_inventory_item(pp, pp->last_equipped_weapon);
    }
    const unsigned int completion_time = delivery_info->time_to_complete - time_left_to_complete.time_until();
    stats_update_guerrilla_express_time(delivery_info->delivery_name, (float)completion_time);
    start->record_victory(completion_time, delivery_info->par_time);
    if (!was_completed)
    {
        standard_rewards_count++;
    }
    if (!had_beat_par && start->has_beat_par_time())
    {
        standard_rewards_count++;
    }
    activity_base_modal::record_victory();
}

// 0x00758150 (:867)
bool activity_delivery_data::on_alert(const npc* np) const
{
    return false;
}

// 0x00758160 (:877)
bool activity_delivery_data::is_player_cover_blown() const
{
    return false;
}

// 0x00758170 (:887)
void activity_delivery_data::activity_start_end_effect(unsigned int end_node)
{
    if (is_active() && end_node == delivery_end_node && end_node_effect == 0xFFFFFFFF)
    {
        end_node_effect = objective_highlight_ground_marker_add(end_node, delivery_info->end_node_radius, ZERO_VECTOR, false, true);
    }
}

// 0x007581C0 (:904)
void activity_delivery_data::activity_stop_end_effect(unsigned int end_node)
{
    if (is_active() && end_node == delivery_end_node && end_node_effect != 0xFFFFFFFF)
    {
        objective_highlight_ground_marker_remove(end_node);
        end_node_effect = 0xFFFFFFFF;
    }
}

// 0x0076C6E0 (:918). Original: the clock freezes once the player has died, and answers the sooner of the two timers.
unsigned int activity_delivery_data::get_remaining_time()
{
    if (post_death_failure_time.valid())
    {
        return previous_time_left_ms;
    }
    int remaining = 0;
    if (time_left_to_complete.valid() && !time_left_to_complete.elapsed())
    {
        remaining = time_left_to_complete.time_until();
        if (time_left_to_fail.valid() && !time_left_to_fail.elapsed())
        {
            const int to_fail = time_left_to_fail.time_until();
            if (to_fail <= remaining)
            {
                remaining = to_fail;
            }
        }
        if (remaining < 0)
        {
            remaining = 0;
        }
    }
    previous_time_left_ms = remaining;
    return remaining;
}

// 0x00758200 (:950). Original: the marauder team hears no radio.
void activity_delivery_data::play_radio_line(delivery_radio_lines line)
{
    if (line >= NUM_DELIVERY_RADIO_LINES)
    {
        return;
    }
    const player* pp = player_get_local_player();
    if (pp == nullptr || pp->current_team == HUMAN_TEAM_MARAUDER || delivery_info == nullptr)
    {
        return;
    }
    const voice_line_handle handle = delivery_info->radio_lines[line];
    if (handle == INVALID_VOICE_LINE_HANDLE)
    {
        return;
    }
    const human_voice_persona* persona = human_voice_persona_from_name(Delivery_message_persona_names[line]);
    if (persona != nullptr)
    {
        most_recent_radio_line = dialogue_manager_start_dialogue(handle, persona, DIALOGUE_PRIORITY_RADIO, INVALID_PA_HANDLE, nullptr) ? handle : INVALID_VOICE_LINE_HANDLE;
    }
}

// 0x00758280 (:983)
void activity_delivery_data::append_radio_line(delivery_radio_lines line, int time_delay)
{
    if (line >= NUM_DELIVERY_RADIO_LINES || most_recent_radio_line == INVALID_VOICE_LINE_HANDLE)
    {
        return;
    }
    const player* pp = player_get_local_player();
    if (pp == nullptr || pp->current_team == HUMAN_TEAM_MARAUDER || delivery_info == nullptr)
    {
        return;
    }
    const voice_line_handle handle = delivery_info->radio_lines[line];
    if (handle == INVALID_VOICE_LINE_HANDLE)
    {
        return;
    }
    const human_voice_persona* persona = human_voice_persona_from_name(Delivery_message_persona_names[line]);
    if (persona != nullptr)
    {
        most_recent_radio_line = dialogue_manager_add_line(handle, persona, most_recent_radio_line, time_delay, INVALID_PA_HANDLE, nullptr) ? handle : INVALID_VOICE_LINE_HANDLE;
    }
}

// 0x00781B40 (:1026)
void activity_delivery_data::reset()
{
    delivery_info = nullptr;
    delivery_start_pos = ZERO_VECTOR;
    delivery_end_pos = ZERO_VECTOR;
    delivery_vehicle = 0xFFFFFFFF;
    delivery_start_node = 0xFFFFFFFF;
    delivery_end_node = 0xFFFFFFFF;
    time_left_to_complete.invalidate();
    time_left_to_fail.invalidate();
    post_death_failure_time.invalidate();
    previous_time_left_ms = 0;
    distance_to_end = 0.0f;
    time_warning_first_time = false;
    alert_set_green = false;
    timer_started = false;
    activity_cooldown_period.set(0);
    time_between_distance_checks.invalidate();
    splash_screen_active = false;
    queued_start_conversation = false;
    standard_rewards_count = 0;
    successfully_completed = false;
    activity_failed = false;
    end_processing_started = false;
    pending_retry = false;
    activity_stop_effect(); // rfg.exe inlines it
}

// 0x007977F0 (:1062)
void activity_delivery_data::render_icons() const
{
    if (is_active())
    {
        const color end_color(0, 0x77, 0x77, 0xFF);
        Cartographer.add_temporary_icon(delivery_end_pos, MAP_ELEMENT_GUERILLA_EXP_END_POINT, true, 0xFFFFFFFF, end_color, false, get_spawn_object_handle());
        if (!Cartographer.has_mission_marker())
        {
            Cartographer.add_player_marker(delivery_end_pos, true, get_spawn_object_handle(), false, false);
        }
        return;
    }
    if (!activity_none())
    {
        return;
    }
    for (int i = 0; i < World.get_num_objects<object_delivery_node>(); i++)
    {
        object_delivery_node* node = World.get_object<object_delivery_node>(i);
        if (!node->is_start_node() || node->is_disabled() || this_instance_of_delivery_is_disabled(node))
        {
            continue;
        }
        const object_vehicle_spawn_node* spawn_node = activity_delivery_get_vehicle_spawn(node); // rfg.exe inlines it
        vehicle* vp = nullptr;
        if (spawn_node != nullptr && spawn_node->get_child_vehicle_handle() != 0xFFFFFFFF)
        {
            vp = World.object_from_handle<vehicle>(spawn_node->get_child_vehicle_handle());
        }
        // The icon sits on the waiting vehicle, else on the node.
        const vector pos = vehicle_is_ready_to_start(vp, spawn_node) ? vp->pos : node->pos;
        Cartographer.add_temporary_icon(pos, MAP_ELEMENT_GUERRILLA_EXPRESS, false, 0xFFFFFFFF, node->is_completed() ? Completed_icon_color : Default_map_item_color, false, node->get_handle());
    }
}

// 0x0076C780 (:1121)
void activity_delivery_data::render_regions() const
{
    if (is_active())
    {
        static const color DELIVERY_DEST_COLOR(0, 0, 0xFF, 0xC0); // 0x02273D68
        Cartographer.add_decal(delivery_end_pos, MAP_ELEMENT_CIRCLE_FILLED_OUTLINE, delivery_info->end_node_radius, DELIVERY_DEST_COLOR, false);
    }
}

// 0x0076C7E0 (:1134). Original: no null check of the start node.
float activity_delivery_data::get_best_time()
{
    object_delivery_node* start = delivery_start_node != 0xFFFFFFFF ? World.object_from_handle<object_delivery_node>(delivery_start_node) : nullptr;
    unsigned int best;
    return start->get_best_time(best) ? (float)best : 0.0f;
}

// 0x0076C830 (:1148). Original: no null check of the start node.
float activity_delivery_data::get_current_time()
{
    const object_delivery_node* start = delivery_start_node != 0xFFFFFFFF ? World.object_from_handle<object_delivery_node>(delivery_start_node) : nullptr;
    // Original: activity_start marks the end node as running and this reads the start node, so the mark is not seen here.
    if (start->current_completion_time == DELIVERY_NODE_RUNNING)
    {
        return (float)(delivery_info->time_to_complete - time_left_to_complete.time_until());
    }
    return (float)start->current_completion_time;
}

// 0x00758310 (:1161)
float activity_delivery_data::get_target_time()
{
    return (float)delivery_info->time_to_complete;
}

// 0x00758320 (:1166)
float activity_delivery_data::get_par_time()
{
    return (float)delivery_info->par_time;
}

// 0x00758330 (:1172)
void activity_delivery_data::activity_lost()
{
    record_loss();
    activity_failed = true;
}

// 0x0076C8A0 (:1181)
void activity_delivery_data::activity_stop_effect()
{
    if (end_node_effect != 0xFFFFFFFF)
    {
        object_effect* effect = World.object_from_handle<object_effect>(end_node_effect);
        if (effect != nullptr)
        {
            effect->stop(object_effect::SS_STOP_EMITTING);
        }
        end_node_effect = 0xFFFFFFFF;
    }
}

// 0x00781C20 (:1195)
void activity_delivery_data::activity_delivery_get_total_statistics(unsigned int& total_time, int& num_activities, int& num_completed)
{
    num_activities = 0;
    num_completed = 0;
    total_time = 0;
    for (int i = 0; i < World.get_num_objects<object_delivery_node>(); i++)
    {
        object_delivery_node* node = World.get_object<object_delivery_node>(i);
        if (!node->is_start_node() || node->get_delivery_def() == nullptr)
        {
            continue;
        }
        num_activities++;
        if (node->is_completed())
        {
            num_completed++;
            unsigned int best;
            if (node->get_best_time(best))
            {
                total_time += best;
            }
        }
    }
}

// 0x007A0FB0 (:1232). Original: answers whether it could start, and starts nothing.
bool activity_delivery_start_activity(const object_delivery_node* delivery_node)
{
    // Deviation from rfg.exe: activity_get_mode, where it reads activity.cpp's private static.
    return delivery_node != nullptr && Delivery_activity_data.mode != activity_get_mode() && Delivery_activity_data.activity_base_modal::can_initialize();
}

// 0x007B5E70 (:1245)
void activity_guerrilla_express_exit()
{
    Delivery_activity_data.give_rewards(); // rfg.exe inlines it
    Delivery_activity_data.activity_delivery_data::activity_end();
}

// 0x007B5EE0 (:1254)
void activity_guerrilla_express_replay()
{
    if (!Delivery_activity_data.is_retry_ready()) // rfg.exe inlines it
    {
        return;
    }
    Delivery_activity_data.give_rewards(); // rfg.exe inlines it
    Activity_node_start_on_load = Delivery_activity_data.get_spawn_object_handle();
    // Original: these two deliveries' start needs a building back if the player died breaking it. Hard-coded handles.
    unsigned int mover_handle = 0xFFFFFFFF;
    if (v_stricmp(Delivery_activity_data.get_instance_name(), "je_td_08") == 0)
    {
        mover_handle = 0xA60004AF;
    }
    else if (v_stricmp(Delivery_activity_data.get_instance_name(), "je_td_09") == 0)
    {
        mover_handle = 0x9800004C;
    }
    if (mover_handle != 0xFFFFFFFF)
    {
        rfg_mover* mover = World.object_from_handle<rfg_mover>(mover_handle);
        player* pp = player_get_local_player();
        if (mover != nullptr && pp != nullptr && pp->is_killed() && !mover->is_killed() && !mover->flags.pristine)
        {
            World.restore_object(mover->get_handle());
        }
    }
    World.save_and_reload();
}

// 0x0076C8E0 (:1297)
void activity_guerrilla_express_process_player_death()
{
    Delivery_activity_data.do_end_processing(true);
}

// 0x00758350 (:1304)
void activity_delivery_end_node_stream_in(const object_delivery_node* delivery_node)
{
    Delivery_activity_data.activity_start_end_effect(delivery_node->get_handle()); // rfg.exe inlines it
}

// 0x007583B0 (:1313)
void activity_delivery_end_node_stream_out(const object_delivery_node* delivery_node)
{
    Delivery_activity_data.activity_stop_end_effect(delivery_node->get_handle()); // rfg.exe inlines it
}

// 0x00758400 (:1320)
float activity_delivery_get_destination_dist()
{
    return Delivery_activity_data.get_destination_dist();
}

// 0x00754480 (.h:103)
float activity_delivery_data::get_destination_dist()
{
    return distance_to_end;
}

// 0x0076C8F0 (:1326)
unsigned int activity_delivery_get_remaining_time()
{
    return Delivery_activity_data.get_remaining_time();
}

// 0x00758410 (:1331)
void activity_delivery_get_display_time(int& num_minutes, int& num_seconds, unsigned int time)
{
    num_minutes = time / 60000;
    num_seconds = (time % 60000) / 1000;
}

// 0x0076C900 (:1338)
void activity_guerrilla_express_get_score_results(float& par_time, float& target_time, float& best_time, float& current_time)
{
    par_time = Delivery_activity_data.get_par_time(); // rfg.exe inlines these
    target_time = Delivery_activity_data.get_target_time();
    best_time = Delivery_activity_data.get_best_time();
    current_time = Delivery_activity_data.get_current_time();
}

// 0x0076C9A0 (:1347). Original: the first child that is a vehicle spawn node.
object_vehicle_spawn_node* activity_delivery_get_vehicle_spawn(const object_delivery_node* delivery_node)
{
    object* first = delivery_node->child_ptr;
    object* child = first;
    while (child != nullptr)
    {
        if (child->get_obj_type() == OT_VEHICLE_SPAWN_NODE)
        {
            return static_cast<object_vehicle_spawn_node*>(child);
        }
        child = child->child_next;
        if (child == first)
        {
            return nullptr;
        }
    }
    return nullptr;
}
