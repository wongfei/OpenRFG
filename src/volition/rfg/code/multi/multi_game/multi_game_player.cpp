// Original: volition/rfg/code/multi/multi_game/multi_game_player.cpp
//
// Partial (multiplayer.md MP6): a remote player played back from its received states (movement, camera, crouch,
// jump, ladder, vehicle and turret, ragdoll, weapon actions), the server's correction of a client's player towards the
// client's own reported state (edge and block corrections, smooth or popped), the per-frame player work, deaths,
// teams, desired weapons, the default weapon and damage requests. The weapon slot sync (MP8): pickups, ammo and
// removals are recorded per connection, player and slot, and the server sends each client its pending records
// (PT_PLAYER_WEAPONS_UPDATE, multi_game_player_process_weapons; all weapons to a new client); a received update is
// applied at its network time as a sim event.
// The ragdoll packets (MP7): the server sums a player's ragdoll pushes and sends them once a frame
// (PT_PLAYER_RAGDOLL, multi_game_player_do_frame), point impulses go out at once (PT_PLAYER_RAGDOLL_POINT).
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_player.h"

#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/havok/api/hkcharactercontroller.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_convert.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/interface/ingame/ovl_inventory.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_compression.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_pool.inl"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_character.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_game_sim_event.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_move.h"
#include "volition/rfg/code/objects/humans/human_ragdoll.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/ladder.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/util/bitarray.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ---- file-private types (rfg.pdb)
// size 0xC
struct player_weapon_slot
{
    unsigned char inventory_slot; // 0x00
    const inv_item_info* item_info; // 0x04
    unsigned char change_mask; // 0x08
    unsigned char cur_rounds_in_magazine; // 0x09
    unsigned short rounds_in_reserve; // 0x0A
};

// The weapon slot changes waiting to go to each connection (by connection index, player data index, slot), the
// reliable ids of the last updates and the received updates' events.
// size 0x30B0
struct player_weapon_sync_data
{
    player_weapon_slot m_player_weapon_slot_changes[16][16][4]; // 0x00
    static const int m_equip_weapon_slot_on_respawn; // 0x01309EF0
    unsigned int m_weapons_update_reliable_id[16]; // 0x3000
    multi_pool<player_weapon_event_data, 320> m_event_player_weapon_allocator; // 0x3040
    player_weapon_event_data m_temp_player_weapon_event_data; // 0x3080
};

// ---- file-static globals
// const float SERVER_FUEL_FUDGE_FACTOR (0x01309DE0) is used here too; placed in multi_game_object_backpack.cpp.
// unsigned int Local_game_data_handles[4] (0x0167E5F4) is used here too; placed in multi_join.cpp.
// The server corrects a client's player towards its reported state: the fastest velocity change of a ragdoll (m/s per
// second), and the velocity and position within which no correction is needed.
static float Max_velocity_correction_rate = 5.0f; // 0x0167F28C
static float No_sync_velocity_threshold = 0.1f; // 0x0167F290
static float No_sync_position_threshold = 0.35f; // 0x0167F294
// The server puts a client's falling player back on the edge it slid off (the name is Ghidra's; rfg.pdb has no symbol).
static bool Mp_player_edge_correct = true; // 0x0167F10F
// const char* Multi_custom_weapon_names[10] (0x0167F4CC) is used here too; placed in multi_game_mode_common.cpp.
// dynamic_uint_hash_table<remote_object*> Game_handle_to_rop_hash (0x01691B40) is used here too; placed in multi_core_delta_compression.cpp.
// dynamic_uint_hash_table<remote_object*> Network_handle_to_rop_hash (0x01691B88) is used here too; placed in multi_core_delta_compression.cpp.
// dynamic_uint_hash_table<network_game_handle*> Reserved_game_to_network_hash (0x01691C18) is used here too; placed in multi_core_objects.cpp.
// dynamic_uint_hash_table<network_game_handle*> Reserved_network_to_game_hash (0x01691C60) is used here too; placed in multi_core_delta_compression.cpp.
// multi_pool<multi_game_sim_event,1024> Sim_event_allocator (0x01693A10) is used here too; placed in multi_game.cpp.
// object_pool<rfg_mover,4000,1> rfg_moverpool (0x01F59580) is used here too; placed in rfg_mover.cpp.
// multi_game_sim_event* Event_queue[6] (0x02FE5BFC) is placed in rfg_multi_unity_pending_early.inl (earlier files use it).
static player_weapon_sync_data* Weapon_sync_data; // 0x02FE6D44

// The PT_PLAYER_RAGDOLL velocity packing (inlined in multi_game_player_do_frame and
// multi_game_player_process_player_ragdoll; defined with them).
static void multi_game_player_write_ragdoll_vector(bit_array& bits, int& bit, const vector& v, double max_magnitude, double magnitude_bias, int magnitude_bits);
static vector multi_game_player_read_ragdoll_vector(const bit_array& bits, int& bit, double max_magnitude, double magnitude_bias, int magnitude_bits);
// unsigned int Clock_current_time (0x02FEB538) is used here too; placed in multi.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
// bool Spectating (0x02FEB659) is used here too; placed in multi.cpp.
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.
// object_pool<multi_object_backpack,76,1> multi_object_backpackpool (0x02FEDC90) is used here too; placed in multi_game_mode_king_of_the_hill.cpp.

// The player of a game handle (null for any other object).
static player* player_of_handle(unsigned int game_handle)
{
    if (game_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(game_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_PLAYER)
    {
        return nullptr;
    }
    return (player*)objp;
}

// The weapon of an inventory item (null when it has none).
static weapon* weapon_of_item(const inventory_item* iip)
{
    if (iip == nullptr || iip->weapon_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(iip->weapon_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_ITEM || objp->get_sub_type() != OT_SUB_ITEM_WEAPON)
    {
        return nullptr;
    }
    return (weapon*)objp;
}

// Port-only: apply_weapon_state's write of a private weapon flag (weapon befriends this struct).
struct multi_game_player_weapon_access
{
    static void clear_delay_fire(weapon* wp)
    {
        wp->flags.delay_fire = false;
    }
};

// The ammo types that fire with the trigger (bullets, projectiles, electricity, repair, blanks, flame).
static bool weapon_fires_with_trigger(const weapon_info* w_info)
{
    const weapon_ammo_type type = w_info->ammo_type;
    return type == WEAPON_AMMO_TYPE_BULLET || type == WEAPON_AMMO_TYPE_PROJECTILE || type == WEAPON_AMMO_TYPE_ELECTRICITY ||
           type == WEAPON_AMMO_TYPE_REPAIR || type == WEAPON_AMMO_TYPE_BLANKS || type == WEAPON_AMMO_TYPE_FLAME;
}

// The seconds since a state's network time (unsigned to float; the clock is in ms), at most 2.5 s.
static float player_state_age(unsigned int network_time)
{
    // Original: the subtraction and division in x87 precision, stored to a float, then compared with 2.5 in double.
    float age = (float)(((double)Clock_current_time - (double)network_time) / 1000.0);
    if ((float)((double)age - 2.5) >= 0.0f)
    {
        age = 2.5f;
    }
    return age;
}

// ---- internal functions
static void debug_spew_game_player(const char*, ...); // 0x0095F850 (:222)
static bool collide_reject_current_player(object* hit_objp, void* filter_params); // 0x0095F860 (:240)
static bool collide_obstructions(object* hit_objp, void* filter_params); // 0x009CE3A0 (:253)
static bool should_pop_towards_state(const player* pp, const multi_player_state& player_state); // 0x009823C0 (:269)
static void extrapolate_player_state_pos_orient(multi_player_state& dest, const multi_player_state& src, const player* pp); // 0x0095F880 (:312)
static void reset_edge_correction(player* pp); // 0x0095F9A0 (:321)
static bool get_edge_correction_position(player* pp, const multi_player_state& player_state, vector& corrected_position); // 0x009B1A40 (:327)
static bool player_correct_edge(player* pp, const multi_player_state& player_state); // 0x009B1C40 (:354)
static bool player_correct_block(player* pp, const vector& dest_pos, const vector& player_center_pos); // 0x009CE3F0 (:425)
static void multi_game_player_correct_player_movement(player* pp, multi_player_state& player_state); // 0x009CE560 (:495)
static void do_frame_movement(player* pp, multi_player_state& player_state); // 0x009CF140 (:796)
static void apply_weapon_state(player* pp, const multi_player_state& player_state, bool was_attacking_in_apply_call, bool was_charging_in_apply_call); // 0x00982470 (:836)
static float compute_ladder_stick_value(player* pp, multi_player_state& player_state); // 0x0095F9D0 (:1002)
static void apply_movement_state(player* pp, multi_player_state& player_state); // 0x009CF3D0 (:1042)

// 0x0095F850 (:222): empty in the shipped build
static void debug_spew_game_player(const char*, ...)
{
}

// 0x00982360 (:226): the selection slot of the player's weapon of this kind (-1: none)
int inv_item_get_weapon_slot(const player* pp, const weapon_info* winfo)
{
    inventory_item* iip = pp->inventory;
    if (iip == nullptr)
    {
        return -1;
    }
    for (;;)
    {
        // Original: the weapon is read through without a null check.
        if (weapon_of_item(iip)->w_info == winfo)
        {
            return iip->selection_slot;
        }
        iip = iip->next;
        if (iip == pp->inventory)
        {
            return -1;
        }
    }
}

// 0x0095F860 (:240): a ray of the correction ignores the player itself
static bool collide_reject_current_player(object* hit_objp, void* filter_params)
{
    if (hit_objp == nullptr)
    {
        return true;
    }
    return hit_objp != filter_params;
}

// 0x009CE3A0 (:253): the block test hits everything but the player itself; building pieces and the replicated general
// movers always count
static bool collide_obstructions(object* hit_objp, void* filter_params)
{
    if (hit_objp != nullptr &&
        (hit_objp->get_obj_type() != OT_MOVER ||
         (hit_objp->get_sub_type() != OT_SUB_MOVER_RFG && (hit_objp->get_sub_type() != OT_SUB_MOVER_GENERAL || multi_core_network_handle_from_game_handle(hit_objp->get_handle(), true) == 0xFFFF))))
    {
        return hit_objp != filter_params;
    }
    return true;
}

// 0x009823C0 (:269): pop to the state (no smoothing) when it is more than 5 m away, or on a client when the local
// camera cannot see the player
static bool should_pop_towards_state(const player* pp, const multi_player_state& player_state)
{
    vector pos = pp->pos; // the other ragdoll states keep it (original: unset)
    switch (pp->ragdoll_state)
    {
    case HUMAN_RAGDOLL_STATE_NONE:
    case HUMAN_RAGDOLL_STATE_BLEND_OUT:
    case HUMAN_RAGDOLL_STATE_FLINCH:
    case HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT:
    case HUMAN_RAGDOLL_STATE_DEACTIVATED:
        pos = pp->pos;
        break;
    case HUMAN_RAGDOLL_STATE_ACTIVE:
        havok_ragdoll_get_pos(pp->ragdoll_idx, pos, RAGDOLL_BODY);
        break;
    default:
        break;
    }
    const float dist2 = vec_dist_squared(player_state.position, pos);
    // Original: the viewer argument is null (the local player) and the object the player.
    if (Multiplayer && !Server && !multi_game_player_can_view_object(nullptr, pp))
    {
        return true;
    }
    // Magic number: 5 m squared.
    return dist2 > 25.0f;
}

// 0x0095F880 (:312): the state moved on by its velocity to now (at most 2.5 s)
// LTCG: src in EDX, dest on the stack; pp is not passed (unused). The port has no register convention.
static void extrapolate_player_state_pos_orient(multi_player_state& dest, const multi_player_state& src, const player* pp)
{
    const float age = player_state_age(src.network_time);
    dest = src;
    dest.position.x = src.position.x + age * src.linear_velocity.x;
    dest.position.y = src.position.y + src.linear_velocity.y * age;
    dest.position.z = src.position.z + src.linear_velocity.z * age;
}

// 0x0095F9A0 (:321)
// LTCG: pp in EAX (the port has no register convention).
static void reset_edge_correction(player* pp)
{
    pp->mp_edge_correction_info.increasing_velocity_count = 0;
    pp->mp_edge_correction_info.vertical_velocity = 0.0f;
}

// 0x009B1A40 (:327): the ground under the state's position (a sphere of the controller's radius cast 1.5 m down from
// 0.75 m above it), as the controller's position
// LTCG: pp in EAX, player_state in EDI, corrected_position in ESI (the port has no register convention).
static bool get_edge_correction_position(player* pp, const multi_player_state& player_state, vector& corrected_position)
{
    if (!pp->rdd.is_loaded())
    {
        return false;
    }
    vector bottom;
    vector top;
    float radius;
    pp->rdd.get_data()->controller->get_capsule_data(bottom, top, radius);
    // Magic numbers: from 0.75 m above to 0.75 m below the position, 5 cm wider than the capsule.
    raycast_input in;
    in.start.x = player_state.position.x + UP_VECTOR.x * 0.75f;
    in.start.y = player_state.position.y + UP_VECTOR.y * 0.75f;
    in.start.z = player_state.position.z + UP_VECTOR.z * 0.75f;
    in.end.x = in.start.x - UP_VECTOR.x * 1.5f;
    in.end.y = in.start.y - UP_VECTOR.y * 1.5f;
    in.end.z = in.start.z - UP_VECTOR.z * 1.5f;
    in.filter_params = nullptr;
    in.filter_callback = nullptr;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.m_flags = 0;
    in.radius = radius + 0.05f;
    in.m_filter_info = COLLIDE_RAY_PLAYER_COVER;
    raycast_output out;
    if (!havok_spherecast(in, out))
    {
        return false;
    }
    corrected_position = out.m_hits[0].hit_pos;
    corrected_position.y = corrected_position.y + radius;
    return true;
}

// 0x009B1C40 (:354)
// A client's player that falls (1 m/s down or faster) while its reported state stands still within 35 cm sideways and
// 30 cm or more above it, for 4 frames of growing fall speed, is put back on the ground under the state.
// LTCG: player_state in EAX, pp on the stack (the port has no register convention).
static bool player_correct_edge(player* pp, const multi_player_state& player_state)
{
    if (!pp->hflags.is_on_ladder && !human_controller_is_on_ground(pp) && pp->ragdoll_state == HUMAN_RAGDOLL_STATE_NONE && !pp->is_killed() &&
        !pp->pflags.jetpack_started)
    {
        const float vertical_velocity = pp->actual_velocity.y;
        // Magic numbers: falling at 1 m/s, a state slower than 0.1 m/s.
        if (vertical_velocity <= -1.0f && player_state.linear_velocity.mag() <= 0.1f)
        {
            vector offset(pp->pos.x - player_state.position.x, 0.0f, pp->pos.z - player_state.position.z);
            // Magic numbers: within 35 cm sideways, 30 cm or more below the state.
            if (offset.mag() <= 0.35f && pp->pos.y - player_state.position.y <= -0.3f && vertical_velocity < pp->mp_edge_correction_info.vertical_velocity)
            {
                pp->mp_edge_correction_info.increasing_velocity_count++;
                pp->mp_edge_correction_info.vertical_velocity = vertical_velocity;
                // Magic number: 4 frames.
                if (pp->mp_edge_correction_info.increasing_velocity_count < 4)
                {
                    return false;
                }
                vector corrected;
                if (!get_edge_correction_position(pp, player_state, corrected))
                {
                    return false;
                }
                v_animate* vp = pp->char_inst->vp;
                vp->start_pos.x = vp->start_pos.x + (corrected.x - pp->pos.x);
                vp->start_pos.y = vp->start_pos.y + (corrected.y - pp->pos.y);
                vp->start_pos.z = (corrected.z - pp->pos.z) + vp->start_pos.z;
                human_controller_set_pos(pp, corrected);
                human_controller_set_vel(pp, ZERO_VECTOR);
                return true;
            }
        }
    }
    reset_edge_correction(pp); // inlined
    return false;
}

// 0x009CE3F0 (:425): a ray from the corrected center to the current one that is blocked after 10 cm, 3 times in a
// row, means the correction would go through a wall
// LTCG: pp in ESI, dest_pos in EDI, player_center_pos on the stack (the port has no register convention).
static bool player_correct_block(player* pp, const vector& dest_pos, const vector& player_center_pos)
{
    if (pp->hflags.is_on_ladder || pp->ragdoll_state != HUMAN_RAGDOLL_STATE_NONE || pp->is_killed() || pp->pflags.jetpack_started)
    {
        pp->mp_blocked_info.raycast_blocked_count = 0;
        return false;
    }
    raycast_input in;
    in.start = player_center_pos;
    in.end = dest_pos;
    in.radius = 0.0f;
    in.filter_params = pp;
    in.filter_callback = collide_obstructions;
    in.m_filter_info = COLLIDE_RAY_DEFAULT;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.m_flags = 0;
    const vector ray(dest_pos.x - player_center_pos.x, dest_pos.y - player_center_pos.y, dest_pos.z - player_center_pos.z);
    raycast_output out;
    if (!havok_raycast(in, out))
    {
        pp->mp_blocked_info.raycast_blocked_count = 0;
        return false;
    }
    // Magic numbers: blocked after 10 cm, 3 times.
    if ((double)(ray.mag() * out.m_hits[0].hit_fraction) > 0.1)
    {
        pp->mp_blocked_info.raycast_blocked_count++;
    }
    return pp->mp_blocked_info.raycast_blocked_count > 2;
}

// 0x009CE560 (:495)
// The server moves a client's player towards the client's reported state (extrapolated to now) once it is off by more
// than 35 cm or the player moves: popped there when should_pop_towards_state says so, else blended (95% per second,
// through exp_lerp_factor) to where a ray from the reported position towards the extrapolated one ends; a correction
// that would go through a wall (player_correct_block) puts the player at the reported position at once. On a ladder
// only the velocity is corrected. A ragdoll's velocity steps towards the state's.
// LTCG: pp in EAX, player_state on the stack (the port has no register convention).
static void multi_game_player_correct_player_movement(player* pp, multi_player_state& player_state)
{
    multi_player_state target;
    target.turret_handle.clear();
    target.vehicle_handle.clear();
    extrapolate_player_state_pos_orient(target, player_state, pp);
    if (Mp_player_edge_correct && player_correct_edge(pp, player_state))
    {
        return;
    }
    const bool moving = fabsf(pp->velocity.x - ZERO_VECTOR.x) > No_sync_velocity_threshold || fabsf(pp->velocity.y - ZERO_VECTOR.y) > No_sync_velocity_threshold ||
                        fabsf(pp->velocity.z - ZERO_VECTOR.z) > No_sync_velocity_threshold;
    const bool off = fabsf(pp->pos.x - target.position.x) > No_sync_position_threshold || fabsf(pp->pos.y - target.position.y) > No_sync_position_threshold ||
                     fabsf(pp->pos.z - target.position.z) > No_sync_position_threshold;
    if (moving || off)
    {
        // Magic number: 95% of the way per second.
        const float blend = exp_lerp_factor(0.95f, Frametime); // inlined
        vector new_pos = ZERO_VECTOR;
        if (should_pop_towards_state(pp, target))
        {
            switch (pp->ragdoll_state)
            {
            case HUMAN_RAGDOLL_STATE_NONE:
            case HUMAN_RAGDOLL_STATE_BLEND_OUT:
            case HUMAN_RAGDOLL_STATE_FLINCH:
            case HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT:
            case HUMAN_RAGDOLL_STATE_DEACTIVATED:
            {
                new_pos = target.position;
                human_controller_set_vel(pp, target.linear_velocity);
                v_animate* vp = pp->char_inst->vp;
                vp->start_pos.x = vp->start_pos.x + (new_pos.x - pp->pos.x);
                vp->start_pos.y = (new_pos.y - pp->pos.y) + vp->start_pos.y;
                vp->start_pos.z = (new_pos.z - pp->pos.z) + vp->start_pos.z;
                pp->update_pos_and_orient(new_pos, pp->orient, true);
                if (pp->ragdoll_state != HUMAN_RAGDOLL_STATE_NONE)
                {
                    havok_ragdoll_set_pos(pp->ragdoll_idx, new_pos);
                }
                if (check_if_pos_is_invalid(pp->pos, pp, "multi_game_player.cpp:529"))
                {
                    return;
                }
                break;
            }
            case HUMAN_RAGDOLL_STATE_ACTIVE:
                havok_ragdoll_set_pos(pp->ragdoll_idx, target.position);
                havok_ragdoll_set_velocity(pp->ragdoll_idx, target.linear_velocity);
                if (check_if_pos_is_invalid(pp->pos, pp, "multi_game_player.cpp:537"))
                {
                    return;
                }
                break;
            default:
                break;
            }
        }
        else
        {
            vector cur_pos = ZERO_VECTOR;
            vector cur_vel = ZERO_VECTOR;
            if (pp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE)
            {
                havok_ragdoll_get_pos(pp->ragdoll_idx, cur_pos, RAGDOLL_BODY);
                havok_ragdoll_get_vel(pp->ragdoll_idx, cur_vel);
            }
            else
            {
                cur_pos = pp->pos;
                cur_vel = pp->velocity;
            }
            if (check_if_pos_is_invalid(pp->pos, pp, "multi_game_player.cpp:556"))
            {
                return;
            }
            if (!target.is_on_ladder)
            {
                vector dest = target.position;
                vector center;
                pp->get_center_pos(center);
                const vector offset(center.x - pp->pos.x, center.y - pp->pos.y, center.z - pp->pos.z);
                // From the reported position (not extrapolated) to the extrapolated one, at the player's center.
                raycast_input in;
                in.radius = 0.0f;
                in.start.x = offset.x + player_state.position.x;
                in.start.y = offset.y + player_state.position.y;
                in.start.z = player_state.position.z + offset.z;
                in.end.x = offset.x + dest.x;
                in.end.y = offset.y + dest.y;
                in.end.z = offset.z + dest.z;
                in.m_filter_info = COLLIDE_RAY_DEFAULT;
                in.m_flags = 0;
                in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
                in.filter_callback = collide_reject_current_player;
                in.filter_params = pp;
                raycast_output out;
                if (havok_raycast(in, out))
                {
                    dest.x = out.m_hits[0].hit_pos.x - offset.x;
                    dest.y = out.m_hits[0].hit_pos.y - offset.y;
                    dest.z = out.m_hits[0].hit_pos.z - offset.z;
                }
                const vector dest_center(offset.x + dest.x, offset.y + dest.y, offset.z + dest.z);
                if (player_correct_block(pp, center, dest_center))
                {
                    v_animate* vp = pp->char_inst->vp;
                    vp->start_pos.x = vp->start_pos.x + (player_state.position.x - pp->pos.x);
                    vp->start_pos.y = (player_state.position.y - pp->pos.y) + vp->start_pos.y;
                    vp->start_pos.z = vp->start_pos.z + (player_state.position.z - pp->pos.z);
                    pp->update_pos_and_orient(player_state.position, pp->orient, true);
                    return;
                }
                new_pos.x = (dest.x - cur_pos.x) * blend + cur_pos.x;
                new_pos.y = (dest.y - cur_pos.y) * blend + cur_pos.y;
                new_pos.z = (dest.z - cur_pos.z) * blend + cur_pos.z;
                if (pp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE)
                {
                    havok_ragdoll_set_pos(pp->ragdoll_idx, new_pos);
                }
                else
                {
                    v_animate* vp = pp->char_inst->vp;
                    vp->start_pos.x = vp->start_pos.x + (new_pos.x - pp->pos.x);
                    vp->start_pos.y = (new_pos.y - pp->pos.y) + vp->start_pos.y;
                    vp->start_pos.z = vp->start_pos.z + (new_pos.z - pp->pos.z);
                    pp->update_pos_and_orient(new_pos, pp->orient, true);
                }
            }
            if (pp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE)
            {
                vector delta(player_state.linear_velocity.x - cur_vel.x, player_state.linear_velocity.y - cur_vel.y, player_state.linear_velocity.z - cur_vel.z);
                const float max_step = Max_velocity_correction_rate * Frametime;
                // Original: a difference larger than the step takes the state's velocity at once, a smaller one moves
                // by a whole step (the comparison looks reversed).
                if (Max_velocity_correction_rate * Frametime * Max_velocity_correction_rate * Frametime <= delta.x * delta.x + delta.y * delta.y + delta.z * delta.z)
                {
                    cur_vel = player_state.linear_velocity;
                }
                else
                {
                    delta.normalize_safe();
                    cur_vel.y = delta.y * max_step + cur_vel.y;
                    cur_vel.x = delta.x * max_step + cur_vel.x;
                    cur_vel.z = delta.z * max_step + cur_vel.z;
                }
                havok_ragdoll_set_avg_velocity(pp->ragdoll_idx, cur_vel);
            }
        }
    }
    check_if_pos_is_invalid(pp->pos, pp, "multi_game_player.cpp:616");
}

// 0x009CF140 (:796): the server's correction, then the state's sticks as the player's movement in its camera frame
// (not on a ladder; cover ends), the movement animation and the jetpack
// LTCG: pp in ESI, player_state in EDI (the port has no register convention).
static void do_frame_movement(player* pp, multi_player_state& player_state)
{
    if (check_if_pos_is_invalid(pp->pos, pp, "multi_game_player.cpp:797"))
    {
        return;
    }
    multi_game_player_correct_player_movement(pp, player_state);
    if (check_if_pos_is_invalid(pp->pos, pp, "multi_game_player.cpp:802") || pp->ladder_handle != 0xFFFFFFFF)
    {
        return;
    }
    if (pp->cur_state == HS_COVER)
    {
        human_set_state(pp, HS_STANDARD, HMS_IDLE, pp->move_speed);
    }
    const bool stopped = player_state.forward == 0.0f && player_state.sideways == 0.0f;
    vector forward_dir(pp->mp_camera_orient.fvec.x, 0.0f, pp->mp_camera_orient.fvec.z);
    vector right_dir(pp->mp_camera_orient.rvec.x, 0.0f, pp->mp_camera_orient.rvec.z);
    forward_dir.normalize_safe();
    right_dir.normalize_safe();
    const float sideways = player_state.sideways;
    const float forward = player_state.forward;
    vector new_dest_point;
    new_dest_point.y = pp->pos.y + forward * forward_dir.y + sideways * right_dir.y;
    new_dest_point.x = forward_dir.x * forward + pp->pos.x + sideways * right_dir.x;
    new_dest_point.z = pp->pos.z + forward_dir.z * forward + right_dir.z * sideways;
    player_process_normal_movement(pp, new_dest_point, forward * forward + sideways * sideways);
    player_move_set_anim_direct(pp, player_state.sideways, player_state.forward, player_state.do_sprint != 0, stopped);
    player_update_jetpack(pp, player_state.jetpack != 0);
}

// 0x00982470 (:836)
// A remote player's weapon follows the state: the server sets the desired weapon's ammo from the state (a client
// keeps one round in the magazine and in reserve), a reload starts once, a changed melee id swings (the player is put
// at the state's position first), a changed trigger id pulls or releases the trigger of a firearm (an automatic one
// also while its trigger ids differ), a changed throw id throws, and the charge (a grinder plays its effects).
// was_attacking_in_apply_call/was_charging_in_apply_call: the player fired/charged earlier this frame (no second time).
// LTCG: pp in ESI, the rest on the stack (the port has no register convention).
static void apply_weapon_state(player* pp, const multi_player_state& player_state, bool was_attacking_in_apply_call, bool was_charging_in_apply_call)
{
    weapon* desired = weapon_of_item(pp->desired_equipped_inv_item);
    if (desired != nullptr)
    {
        int reserve;
        if (Multiplayer && Server)
        {
            int magazine = desired->get_magazine_size();
            if ((int)player_state.weapon_magazine_ammo < magazine)
            {
                magazine = player_state.weapon_magazine_ammo;
            }
            desired->set_rounds_in_magazine(magazine);
            reserve = desired->get_max_rounds();
            if ((int)player_state.weapon_reserve_ammo < reserve)
            {
                reserve = player_state.weapon_reserve_ammo;
            }
        }
        else
        {
            // Magic number: a client's remote weapon always has one round (the server owns the ammo).
            desired->set_rounds_in_magazine(1); // inlined
            reserve = 1;
        }
        desired->set_rounds_in_reserve(reserve);
    }
    weapon* wp = pp->get_equipped_weapon();
    if (wp != nullptr)
    {
        if (!player_state.reload)
        {
            pp->pflags.mp_has_reloaded = 0;
        }
        else if (!pp->pflags.mp_has_reloaded && wp->w_info->is_firearm())
        {
            human_weapons_firearm_reload(pp, true, nullptr);
            pp->pflags.mp_has_reloaded = 1;
        }
        if (!was_attacking_in_apply_call)
        {
            pp->mp_flags.set_running_attack(player_state.running_attack);
            for (int i = 0; i < 4; i++)
            {
                human_melee_attack_types attack_type;
                bool changed;
                switch (i)
                {
                case 0:
                    changed = player_state.melee_primary_id != pp->mp_flags.melee_primary_id;
                    attack_type = MELEE_ATTACK_PRIMARY;
                    break;
                case 1:
                    changed = player_state.melee_secondary_id != pp->mp_flags.melee_secondary_id;
                    attack_type = MELEE_ATTACK_SECONDARY;
                    break;
                case 2:
                    changed = player_state.melee_tertiary_id != pp->mp_flags.melee_tertiary_id;
                    attack_type = MELEE_ATTACK_TERTIARY;
                    break;
                default:
                    changed = player_state.melee_firearm_id != pp->mp_flags.melee_firearm_id;
                    attack_type = MELEE_ATTACK_FIREARM;
                    break;
                }
                if (changed)
                {
                    if (human_weapons_is_melee_weapon_equipped(pp) || attack_type == MELEE_ATTACK_FIREARM)
                    {
                        pp->pos = player_state.position;
                        human_melee_attack(pp, attack_type, true, pp->mp_flags.get_running_attack() != 0);
                    }
                    break;
                }
            }
        }
        bool pull = false;
        bool release = false;
        bool throw_weapon = false;
        if (weapon_fires_with_trigger(wp->w_info))
        {
            const weapon_trigger_type trigger_type = wp->w_info->trigger_type;
            const bool single = trigger_type == WEAPON_TRIGGER_TYPE_SINGLE || wp->w_info->weapon_class == WEAPON_CLASS_GRINDER;
            const bool same_down = player_state.firearm_trigger_down_id == pp->mp_flags.firearm_trigger_down_id;
            const bool trigger_down = wp->is_trigger_pulled(); // inlined
            const bool single_pull = single && !same_down;
            const bool automatic_pull = trigger_type == WEAPON_TRIGGER_TYPE_AUTOMATIC && !(same_down && player_state.firearm_trigger_up_id == player_state.firearm_trigger_down_id);
            pull = single_pull || automatic_pull;
            if (!pull || (single && trigger_down))
            {
                release = true;
                pull = false;
            }
            if (trigger_type == WEAPON_TRIGGER_TYPE_AUTOMATIC && pull && trigger_down)
            {
                pp->mp_flags.firearm_trigger_down_id = player_state.firearm_trigger_down_id;
            }
        }
        if (wp->w_info->ammo_type == WEAPON_AMMO_TYPE_THROWN)
        {
            throw_weapon = player_state.throw_attack_id != pp->mp_flags.throw_attack_id;
        }
        if (pull)
        {
            if (weapon_fires_with_trigger(wp->w_info))
            {
                if (wp->get_rounds_in_magazine() < 1)
                {
                    wp->set_rounds_in_magazine(1); // inlined
                }
                if (human_weapons_is_reloading(pp, false))
                {
                    human_weapons_cancel_reload(pp);
                }
                multi_game_player_weapon_access::clear_delay_fire(wp);
                human_weapons_firearm_pull_trigger(pp);
                pp->pflags.mp_remote_player_fired_this_frame = 1;
            }
        }
        else if (release && weapon_fires_with_trigger(wp->w_info) && human_weapons_is_trigger_pulled(pp))
        {
            human_weapons_firearm_release_trigger(pp);
        }
        if (throw_weapon && wp->w_info->ammo_type == WEAPON_AMMO_TYPE_THROWN)
        {
            human_weapons_start_throw(pp, wp, nullptr, false);
        }
        if (!was_charging_in_apply_call)
        {
            pp->mp_flags.set_charge_weapon(player_state.charge_weapon);
            if (wp->w_info->weapon_class == WEAPON_CLASS_GRINDER)
            {
                const unsigned int charge = pp->mp_flags.get_charge_weapon();
                if (charge != 1 && charge != 2)
                {
                    goto flags;
                }
                wp->process_grinder_effects(charge == 1);
                pp->pflags.mp_remote_player_charged_this_frame = 1;
            }
        }
    }
flags:
    if (!object_is_local_player(pp))
    {
        pp->mp_flags.set_detonated_charges(player_state.detonated_charges);
        pp->mp_flags.set_in_air_detonate(player_state.in_air_detonate);
    }
}

// 0x0095F9D0 (:1002): the ladder stick from the state's ladder state and rung against the player's current rung: up
// while climbing below the rung, idle holds the rung (up or down when more than 2 rungs off), slide is down, a stopped
// slide holds the rung
// LTCG: pp in EDI, player_state in ESI, the result in XMM0 (the port has no register convention).
static float compute_ladder_stick_value(player* pp, multi_player_state& player_state)
{
    const int rung = ladder_compute_current_rung(pp, pp->ladder_handle);
    float stick = 0.0f;
    const unsigned char state = player_state.ladder_state.l_state;
    const int state_rung = player_state.ladder_state.l_rung;
    if (state == MP_LADDER_STATE_CLIMB && state_rung >= rung)
    {
        stick = 1.0f;
    }
    if (state == MP_LADDER_STATE_IDLE)
    {
        pp->ladder_grab_rung = state_rung;
        // Magic number: 2 rungs of slack.
        if (rung < state_rung - 2)
        {
            stick = 1.0f;
        }
        else if (rung > state_rung + 2)
        {
            stick = -1.0f;
        }
    }
    if (state == MP_LADDER_STATE_SLIDE_DOWN)
    {
        stick = -1.0f;
    }
    if (state == MP_LADDER_STATE_SLIDE_STOP)
    {
        pp->ladder_grab_rung = state_rung;
        stick = 0.0f;
    }
    return stick;
}

// 0x009CF3D0 (:1042)
// The state's camera (its angles moved on by the angular velocity to now) and the aim from the head along it (a test
// bullet to the far clip with a weapon, 10 m without), the sticks, the jetpack; for a remote player a changed jump id
// jumps, the ladder is mounted, climbed (the stick from the ladder state) or left; crouching follows the state; a
// ragdoll forced by the state starts (with the state's velocity at the chest) or a resting one gets up; outside a
// vehicle the movement follows.
// LTCG: player_state in EAX, pp on the stack (the port has no register convention).
static void apply_movement_state(player* pp, multi_player_state& player_state)
{
    const float age = player_state_age(player_state.network_time);
    float pitch;
    float bank;
    float heading;
    player_state.camera_orient.extract_angles(pitch, bank, heading);
    pitch = player_state.camera_angular_velocity.x * age + pitch;
    heading = player_state.camera_angular_velocity.y * age + heading;
    pp->mp_camera_orient.set(pitch, 0.0f, heading);
    human_get_head_pos(pp, pp->mp_camera_pos);
    // Magic number: 10 m of aim without a weapon.
    float aim_distance = 10.0f;
    if (human_weapons_get_weapon(pp) != nullptr)
    {
        aim_distance = Rfg_camera.m_far_clip_dist;
    }
    weapon_fire_info fire_info;
    fire_info.start_pos = pp->mp_camera_pos;
    fire_info.end_pos.x = pp->mp_camera_orient.fvec.x * aim_distance + fire_info.start_pos.x;
    fire_info.end_pos.y = pp->mp_camera_orient.fvec.y * aim_distance + fire_info.start_pos.y;
    fire_info.end_pos.z = pp->mp_camera_orient.fvec.z * aim_distance + fire_info.start_pos.z;
    fire_info.w_info = nullptr;
    fire_info.hit_handle = 0xFFFFFFFF;
    if (weapons_test_bullet(fire_info, pp, true, 0.0f, vector(0.0f, 0.0f, 0.0f)))
    {
        object* hit = World.object_from_handle(fire_info.hit_handle);
        if (hit != nullptr && hit->get_obj_type() == OT_HUMAN && hit->is_killed())
        {
            pp->aim_target = 0xFFFFFFFF;
        }
        else
        {
            pp->aim_target = fire_info.hit_handle;
        }
        pp->aim_pos = fire_info.hit_pos;
    }
    else
    {
        pp->aim_pos = fire_info.end_pos;
        pp->aim_target = 0xFFFFFFFF;
    }
    pp->mp_current_command.forward = player_state.forward;
    pp->mp_current_command.sideways = player_state.sideways;
    pp->mp_flags.set_jetpack(player_state.jetpack);
    if (player_state.jetpack != 0)
    {
        pp->pflags.jetpack_ready = 1;
    }
    if (!object_is_local_player(pp->get_handle()))
    {
        if (player_state.jump_id != pp->mp_flags.jump_id)
        {
            human_jump(pp, false, false, false);
        }
        if (!player_state.is_on_ladder || player_state.ladder_handle == 0xFFFFFFFF ||
            (!object_is_local_player(pp) && !pp->mp_ladder_state.remote_ladder_mount_delay.elapsed()))
        {
            if (pp->hflags.is_on_ladder && !player_state.is_on_ladder)
            {
                human_dismount_ladder(pp);
            }
        }
        else if (pp->ladder_handle == 0xFFFFFFFF)
        {
            object* objp = World.object_from_handle(player_state.ladder_handle);
            ladder* lp = objp != nullptr && objp->get_obj_type() == OT_LADDER ? (ladder*)objp : nullptr;
            human_mount_ladder(pp, lp);
        }
        else
        {
            const float stick = compute_ladder_stick_value(pp, player_state);
            ladder_process_human(pp, stick);
        }
    }
    if (!human_move_is_crouching(pp) && player_state.is_crouched)
    {
        human_move_start_crouching(pp);
    }
    else if (human_move_is_crouching(pp) && !player_state.is_crouched)
    {
        human_move_stop_crouching(pp);
    }
    if (player_state.ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE)
    {
        if (pp->ragdoll_state != HUMAN_RAGDOLL_STATE_ACTIVE)
        {
            const int knockdown_left = player_state.knockdown_timestamp - (int)Clock_current_time;
            const unsigned int state_time = player_state.network_time;
            if (human_ragdoll_start(pp, true, 0xFFFFFFFF))
            {
                vector dir = player_state.linear_velocity;
                dir.normalize_safe();
                // Magic numbers: half of 1.9 m up (the chest), one meter against the velocity.
                const vector point((float)(((double)pp->pos.x + (double)UP_VECTOR.x * 1.9 * 0.5) - dir.x), (float)(((double)pp->pos.y + (double)UP_VECTOR.y * 1.9 * 0.5) - dir.y),
                                   (float)(((double)pp->pos.z + (double)UP_VECTOR.z * 1.9 * 0.5) - dir.z));
                havok_ragdoll_apply_point_velocity(pp->ragdoll_idx, player_state.linear_velocity, point);
            }
            // Original: the remaining time plus the state's time (both in ms) as the timestamp's duration.
            pp->knockdown_timestamp.set(knockdown_left + (int)state_time);
        }
    }
    else if (pp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE && !pp->is_killed() && !human_is_falling(pp))
    {
        human_ragdoll_get_up(pp, true);
    }
    if (pp->vehicle_handle == 0xFFFFFFFF)
    {
        do_frame_movement(pp, player_state);
    }
}

// 0x009E1E20 (:1153): the player leaves its vehicle (no vehicle in the state) or enters the state's seat
void multi_game_player_apply_vehicle_state(player* pp, const multi_player_state& player_state)
{
    multi_player_state& state = const_cast<multi_player_state&>(player_state); // the handle caches its game handle
    if (state.vehicle_handle.get_network_handle() == 0xFFFF)
    {
        if (pp->vehicle_handle != 0xFFFFFFFF)
        {
            vi_exit_data exit_data;
            exit_data.flags.teleport = 1;
            vehicle_interaction_request_exit(pp, &exit_data);
        }
        return;
    }
    const unsigned int vehicle_handle = state.vehicle_handle.get_game_handle();
    if (vehicle_handle == 0xFFFFFFFF || vehicle_handle == pp->vehicle_handle || pp->cur_state == HS_DEAD)
    {
        return;
    }
    object* objp = World.object_from_handle(vehicle_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE || !((vehicle*)objp)->rdd.is_loaded())
    {
        return;
    }
    vi_enter_data enter_data;
    enter_data.specified_seat = (vehicle_seat_index)state.vehicle_seat;
    enter_data.flags.teleport = 1;
    enter_data.destination_seat = VSI_INVALID;
    enter_data.seat_selection_method = VI_SEAT_SELECTION_METHOD_SPECIFIED;
    vehicle_interaction_request_enter(pp, (vehicle*)objp, &enter_data);
}

// 0x009E1F10 (:1204)
// A remote player plays its received states (the registration's move): the state at the current network time (later
// than half a second after its respawn), with an orthogonal camera. Within the extrapolation limit (always for the
// local player) a client takes the hit points (at least 10%), the vehicle and the turret come from the state, and a
// living remote player gets the bone deform flag, the movement and the weapon actions; past the limit only the
// movement and the weapons. A pathfinding player is left alone.
void multi_game_player_process_state(player* pp)
{
    if (pp == nullptr)
    {
        return;
    }
    const bool was_attacking = pp->pflags.mp_remote_player_fired_this_frame != 0;
    const bool was_charging = pp->pflags.mp_remote_player_charged_this_frame != 0;
    pp->pflags.mp_remote_player_fired_this_frame = 0;
    pp->pflags.mp_remote_player_charged_this_frame = 0;
    if (pp->movement_mode == HMM_PATHFINDING)
    {
        return;
    }
    multi_player_state state;
    state.turret_handle.clear();
    state.vehicle_handle.clear();
    bool exceeded_extrap_time;
    // Magic number: the states of the first 500 ms after a respawn are old positions.
    if (!pp->mp_sim_state.obtain_state(Clock_current_time, &state, exceeded_extrap_time) || pp->mp_last_respawn_time + 500 >= state.network_time)
    {
        return;
    }
    state.camera_orient.orthogonalize();
    if (!exceeded_extrap_time || object_is_local_player(pp))
    {
        if (Multiplayer && !Server && !human_is_dead(pp))
        {
            const float old_hit_points = pp->hit_points;
            state.hit_points_pct = (float)pp->max_hit_points * state.hit_points_pct;
            // Magic number: a living player keeps at least 0.1 hit points.
            if ((double)state.hit_points_pct - 0.1 < 0.0)
            {
                state.hit_points_pct = 0.1f;
            }
            pp->hit_points = state.hit_points_pct;
            pp->pflags.mp_lost_hp = old_hit_points > state.hit_points_pct ? 1 : 0;
            if (pp->pflags.mp_lost_hp)
            {
                // Deviation (human.h): the regeneration delay the server started with the damage.
                human_health_damage_register_from_network(pp);
            }
        }
        multi_game_player_apply_vehicle_state(pp, state);
        if (pp->vehicle_handle == 0xFFFFFFFF)
        {
            const unsigned int turret_handle = state.turret_handle.get_game_handle();
            if (turret_handle == 0xFFFFFFFF)
            {
                if (pp->combat.reserved_turret != 0xFFFFFFFF)
                {
                    human_combat_abandon_turret(pp);
                }
            }
            else
            {
                turret* tp = World.object_from_handle<turret>(turret_handle);
                if (tp != nullptr)
                {
                    human_combat_man_turret(pp, tp);
                }
            }
        }
        if (pp->cur_state != HS_DEAD && !object_is_local_player(pp->get_handle()))
        {
            pp->hflags.dont_deform_bones = state.dont_deform_bones;
            apply_movement_state(pp, state);
            apply_weapon_state(pp, state, was_attacking, was_charging);
        }
    }
    else if (pp->cur_state != HS_DEAD)
    {
        apply_movement_state(pp, state);
        apply_weapon_state(pp, state, was_attacking, was_charging);
    }
}

// 0x009CF990 (:1306): the weapon sync data (320 received update events) and empty slot records
void multi_game_player_system_init()
{
    Weapon_sync_data = new (MP_allocators_mempool.alloc(sizeof(player_weapon_sync_data), 0)) player_weapon_sync_data();
    Weapon_sync_data->m_event_player_weapon_allocator.init(MP_allocators_mempool);
    for (int connection = 0; connection < 16; connection++)
    {
        for (int data = 0; data < 16; data++)
        {
            for (int slot = 0; slot < 4; slot++)
            {
                player_weapon_slot& record = Weapon_sync_data->m_player_weapon_slot_changes[connection][data][slot];
                record.inventory_slot = 0xFF;
                record.item_info = nullptr;
                record.change_mask = 0;
                record.cur_rounds_in_magazine = 0;
                record.rounds_in_reserve = 0;
            }
        }
    }
    for (int i = 0; i < 16; i++)
    {
        Weapon_sync_data->m_weapons_update_reliable_id[i] = 0xFFFFFFFF;
    }
}

// 0x009B1EE0 (:1334)
void multi_game_player_system_shutdown()
{
}

// 0x009CFAA0 (:1340)
// The server sends a ragdoll's aggregated impulses of the network frame (PT_PLAYER_RAGDOLL, MP7); outside gameplay a
// held trigger is released.
void multi_game_player_do_frame(player* pp)
{
    if (Multiplayer && Server && (!pp->mp_ragdoll_linear_velocity_aggregate.is_null() || !pp->mp_ragdoll_angular_velocity_aggregate.is_null()))
    {
        unsigned char field[5];
        bit_array bits;
        bits.init(field, 40);
        int bit = 0;
        multi_game_player_write_ragdoll_vector(bits, bit, pp->mp_ragdoll_linear_velocity_aggregate, 250.0, 50.0, 13);
        multi_game_player_write_ragdoll_vector(bits, bit, pp->mp_ragdoll_angular_velocity_aggregate, 25.0, 25.0, 5);
        unsigned char packet[Packet_buffer_size];
        packet[0] = PT_PLAYER_RAGDOLL;
        unsigned short size = packet_header_size(PT_PLAYER_RAGDOLL);
        const unsigned int handle = pp->get_handle();
        write_u16_be(packet, size, handle == 0xFFFFFFFF ? 0xFFFF : multi_core_network_handle_from_game_handle(handle, true));
        memcpy(packet + size, field, sizeof(field));
        size += sizeof(field);
        packet_set_size(packet, size);
        multi_io_send_to_all(Game_session, packet, size);
        pp->mp_ragdoll_linear_velocity_aggregate.zero();
        pp->mp_ragdoll_angular_velocity_aggregate.zero();
    }
    if (Multi_match_info.match_state != MGMS_GAMEPLAY)
    {
        weapon* wp = pp->get_equipped_weapon();
        if (wp != nullptr && weapon_fires_with_trigger(wp->w_info) && human_weapons_is_trigger_pulled(pp))
        {
            human_weapons_firearm_release_trigger(pp);
        }
    }
}

// 0x0095FA80 (:1395): every connection's slot records of the player are emptied
void multi_game_player_clear_weapon_changes(const player* pp)
{
    // Original: the player is read through without a null check.
    if (pp->mp_data == 0xFFFFFFFF)
    {
        return;
    }
    const unsigned int data_index = pp->mp_data & 0xFFFF;
    for (int connection = 0; connection < 16; connection++)
    {
        for (int slot = 0; slot < 4; slot++)
        {
            player_weapon_slot& record = Weapon_sync_data->m_player_weapon_slot_changes[connection][data_index][slot];
            record.inventory_slot = 0xFF;
            record.item_info = nullptr;
            record.change_mask = 0;
            record.cur_rounds_in_magazine = 0;
            record.rounds_in_reserve = 0;
        }
    }
}

// The player human of a game data entry in the game session (null when the entry is unused or its player is not here).
static human* multi_game_player_weapons_human(unsigned int handle)
{
    const multi_game_data& data = Mp_game_data->data[handle & 0xFFFF][handle >> 16];
    if (!data.in_use)
    {
        return nullptr;
    }
    const unsigned int game_handle = multi_core_game_handle_from_network_handle(data.player_network_handle, true);
    if (game_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(game_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_PLAYER)
    {
        return nullptr;
    }
    return (human*)objp;
}

// 0x009D01E0 (:1419)
// PT_PLAYER_WEAPONS_UPDATE with every player's weapons (a new connection): the network time, then per player with
// weapons its network handle, the count and per weapon the slot, the item info handle, change 1 (picked up), the
// magazine and the reserve; 0xFFFF ends it. A packet that would pass 512 bytes is sent and restarted (the time
// kept). Nothing is sent without weapons.
void multi_game_player_weapons_sync(multi_connection* cp)
{
    bool any = false;
    unsigned char packet[Packet_buffer_size];
    packet[0] = PT_PLAYER_WEAPONS_UPDATE;
    unsigned short size = packet_header_size(PT_PLAYER_WEAPONS_UPDATE);
    write_u32_be(packet, size, Clock_current_time);
    const unsigned short players_start = size;
    for (unsigned int handle = multi_game_data_handles_begin(ST_GAME, false, false); handle != 0xFFFFFFFF; handle = multi_game_data_handles_next(handle, false, false))
    {
        human* hp = multi_game_player_weapons_human(handle);
        if (hp == nullptr)
        {
            continue;
        }
        int count = 0;
        for (int slot = 0; slot < 4; slot++)
        {
            const inventory_item* iip = inv_item_get_item_in_slot(hp, slot);
            if (iip != nullptr && iip->weapon_handle != 0xFFFFFFFF)
            {
                count++;
            }
        }
        if (count * 9 + 2 + size > 0x200) // Magic number: 512 bytes a packet
        {
            write_u16_be(packet, size, 0xFFFF);
            packet_set_size(packet, size);
            multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
            size = players_start;
        }
        if (count == 0)
        {
            continue;
        }
        any = true;
        const unsigned int game_handle = hp->get_handle();
        write_u16_be(packet, size, game_handle == 0xFFFFFFFF ? 0xFFFF : multi_core_network_handle_from_game_handle(game_handle, false)); // inlined
        packet[size++] = (unsigned char)count;
        for (int slot = 0; slot < 4; slot++)
        {
            const inventory_item* iip = inv_item_get_item_in_slot(hp, slot);
            if (iip == nullptr || iip->weapon_handle == 0xFFFFFFFF)
            {
                continue;
            }
            packet[size++] = (unsigned char)slot;
            write_u32_be(packet, size, inv_item_info_get_handle(iip->info));
            packet[size++] = 1;
            // Original: the weapon is used unchecked.
            const weapon* wp = World.object_from_handle<weapon>(iip->weapon_handle);
            packet[size++] = (unsigned char)wp->get_rounds_in_magazine();
            write_u16_be(packet, size, (unsigned short)wp->get_rounds_in_reserve());
        }
    }
    write_u16_be(packet, size, 0xFFFF);
    packet_set_size(packet, size);
    if (any)
    {
        multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
    }
}

// 0x009D0640 (:1493): a client asks the server to remove its weapon in the item's slot (PT_REMOVE_WEAPON)
void multi_game_player_send_remove_weapon(inventory_item* iip)
{
    if (Game_session == nullptr || Game_session->get_host_connection() == nullptr || iip->selection_slot == -1)
    {
        return;
    }
    player* pp = player_get_local_player();
    if (human_get_inventory_item(pp, iip->selection_slot) != iip)
    {
        return;
    }
    unsigned char packet[Packet_buffer_size];
    packet[0] = PT_REMOVE_WEAPON;
    unsigned short size = packet_header_size(PT_REMOVE_WEAPON);
    packet[size++] = (unsigned char)iip->selection_slot;
    packet_set_size(packet, size);
    multi_io_send_reliable(Game_session->get_host_connection(), packet, size, nullptr, nullptr, nullptr);
}

// 0x009D0710 (:1507)
void multi_game_player_process_remove_weapon(unsigned char* packet_data, const sender& from)
{
    if (!Multiplayer || !Server)
    {
        return;
    }
    const unsigned char slot = packet_data[0];
    player* pp = multi_game_data_get_player_instance(from.connection->game_data);
    if (pp == nullptr || pp->obj_flags.destroyed)
    {
        return;
    }
    inventory_item* iip = human_get_inventory_item(pp, slot);
    if (iip != nullptr)
    {
        human_delete_item_from_inventory(pp, iip, true, false);
    }
}

// The weapon of an inventory item (null without one).
static weapon* multi_game_player_item_weapon(const inventory_item* inv_item)
{
    if (inv_item == nullptr || inv_item->weapon_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    return World.object_from_handle<weapon>(inv_item->weapon_handle);
}

// The player's game data index for the slot records (0xFFFFFFFF without one, or past the 16 records).
static unsigned int multi_game_player_weapon_data_index(const player* pp)
{
    const unsigned int data_index = pp->mp_data == 0xFFFFFFFF ? 0xFFFFFFFF : pp->mp_data & 0xFFFF;
    return data_index < 16 ? data_index : 0xFFFFFFFF;
}

// 0x009829B0 (:1533): every connection's record of the slot becomes a removal (change 3)
void multi_game_player_remove_weapon(player* pp, const inventory_item* inv_item)
{
    const weapon* wp = multi_game_player_item_weapon(inv_item);
    if (wp == nullptr || wp->w_info == nullptr || pp == nullptr || pp->obj_flags.destroyed || (unsigned int)inv_item->selection_slot >= 4)
    {
        return;
    }
    const unsigned int data_index = multi_game_player_weapon_data_index(pp);
    if (data_index == 0xFFFFFFFF)
    {
        return;
    }
    for (int connection = 0; connection < 16; connection++)
    {
        player_weapon_slot& record = Weapon_sync_data->m_player_weapon_slot_changes[connection][data_index][inv_item->selection_slot];
        record.inventory_slot = (unsigned char)inv_item->selection_slot;
        record.item_info = inv_item->info;
        record.change_mask = 3;
        record.cur_rounds_in_magazine = 0;
        record.rounds_in_reserve = 0;
    }
}

// 0x00982B00 (:1574)
// Every connection's record of the slot becomes a pickup (change 1) with the weapon's ammo (negative ammo is zeroed
// first).
// Original: the weapon is read through (its item info) without a null check.
void multi_game_player_picked_up_weapon(player* pp, const inventory_item* inv_item)
{
    if (inv_item == nullptr || inv_item->weapon_handle == 0xFFFFFFFF)
    {
        return;
    }
    weapon* wp = multi_game_player_item_weapon(inv_item);
    if (wp->info == nullptr || pp == nullptr)
    {
        return;
    }
    const unsigned int slot = (unsigned int)inv_item->selection_slot;
    const unsigned int data_index = pp->mp_data == 0xFFFFFFFF ? 0xFFFFFFFF : pp->mp_data & 0xFFFF;
    if (wp->get_rounds_in_reserve() < 0)
    {
        wp->set_rounds_in_reserve(0);
    }
    if (wp->get_rounds_in_magazine() < 0)
    {
        wp->set_rounds_in_magazine(0);
    }
    if (slot >= 4 || data_index == 0xFFFFFFFF || data_index >= 16)
    {
        return;
    }
    for (int connection = 0; connection < 16; connection++)
    {
        player_weapon_slot& record = Weapon_sync_data->m_player_weapon_slot_changes[connection][data_index][slot];
        record.inventory_slot = (unsigned char)inv_item->selection_slot;
        record.item_info = inv_item->info;
        record.change_mask = 1;
        record.cur_rounds_in_magazine = (unsigned char)wp->get_rounds_in_magazine();
        record.rounds_in_reserve = (unsigned short)wp->get_rounds_in_reserve();
    }
}

// 0x00982CD0 (:1621)
// The picked up ammo is added to every connection's record of the slot (at most the weapon's maximum and magazine);
// a record that is not a pickup of this item becomes an ammo change (2).
// Original: the weapon is read through (its item info) without a null check.
void multi_game_player_picked_up_ammo(player* pp, const inventory_item* inv_item, unsigned int reserve_rounds, unsigned int magazine_rounds)
{
    if (inv_item == nullptr || inv_item->weapon_handle == 0xFFFFFFFF)
    {
        return;
    }
    const weapon* wp = multi_game_player_item_weapon(inv_item);
    if (wp->info == nullptr || pp == nullptr)
    {
        return;
    }
    const weapon_info* w_info = multi_game_player_item_weapon(inv_item)->w_info;
    const unsigned int data_index = pp->mp_data == 0xFFFFFFFF ? 0xFFFFFFFF : pp->mp_data & 0xFFFF;
    if ((unsigned int)inv_item->selection_slot >= 4 || data_index == 0xFFFFFFFF || data_index >= 16)
    {
        return;
    }
    for (int connection = 0; connection < 16; connection++)
    {
        player_weapon_slot& record = Weapon_sync_data->m_player_weapon_slot_changes[connection][data_index][inv_item->selection_slot];
        if (record.change_mask != 1 || record.item_info != inv_item->info)
        {
            record.inventory_slot = (unsigned char)inv_item->selection_slot;
            record.item_info = inv_item->info;
            record.change_mask = 2;
        }
        const unsigned short reserve = (unsigned short)(record.rounds_in_reserve + (unsigned short)reserve_rounds);
        record.rounds_in_reserve = reserve < w_info->max_rounds ? reserve : w_info->max_rounds;
        const unsigned short magazine = (unsigned short)(record.cur_rounds_in_magazine + (unsigned short)magazine_rounds);
        record.cur_rounds_in_magazine = (unsigned char)(magazine < w_info->magazine_size ? magazine : w_info->magazine_size);
    }
}

// 0x009D0770 (:1656)
// The server sends each client whose last update was acknowledged the pending slot records of every player
// (PT_PLAYER_WEAPONS_UPDATE, as multi_game_player_weapons_sync, with each record's change) and clears them.
// Original: an unknown change aborts the whole pass, and once one connection had changes the later ones get the
// packet even without (the flag is not reset per connection).
void multi_game_player_process_weapons()
{
    bool any = false;
    multi_session* session = multi_session_get_by_type(ST_GAME);
    // Original: the session is used unchecked.
    multi_connection* first = session->get_connection_list();
    for (multi_connection* cp = first; cp != nullptr;)
    {
        if ((cp->connection_status == CS_CONNECTED || cp->connection_status == CS_CONNECTED_GAMEPLAY) && cp != session->get_host_connection())
        {
            const unsigned int connection = cp->index_in_session;
            if (multi_connection_reliable_data_received(cp, Weapon_sync_data->m_weapons_update_reliable_id[connection]))
            {
                unsigned char packet[Packet_buffer_size];
                packet[0] = PT_PLAYER_WEAPONS_UPDATE;
                unsigned short size = packet_header_size(PT_PLAYER_WEAPONS_UPDATE);
                write_u32_be(packet, size, Clock_current_time);
                for (unsigned int handle = multi_game_data_handles_begin(ST_GAME, false, false); handle != 0xFFFFFFFF; handle = multi_game_data_handles_next(handle, false, false))
                {
                    const unsigned int data_index = handle & 0xFFFF;
                    player_weapon_slot* records = Weapon_sync_data->m_player_weapon_slot_changes[connection][data_index];
                    unsigned char count = 0;
                    for (int slot = 0; slot < 4; slot++)
                    {
                        if (records[slot].change_mask != 0)
                        {
                            count++;
                        }
                    }
                    if (count == 0)
                    {
                        continue;
                    }
                    human* hp = multi_game_player_weapons_human(handle);
                    if (hp == nullptr)
                    {
                        continue;
                    }
                    any = true;
                    const unsigned int game_handle = hp->get_handle();
                    write_u16_be(packet, size, game_handle == 0xFFFFFFFF ? 0xFFFF : multi_core_network_handle_from_game_handle(game_handle, false)); // inlined
                    packet[size++] = count;
                    for (int slot = 0; slot < 4; slot++)
                    {
                        player_weapon_slot& record = records[slot];
                        if (record.change_mask == 0)
                        {
                            continue;
                        }
                        if ((unsigned char)(record.change_mask - 1) > 2)
                        {
                            return;
                        }
                        packet[size++] = record.inventory_slot;
                        write_u32_be(packet, size, inv_item_info_get_handle(record.item_info));
                        packet[size++] = record.change_mask;
                        packet[size++] = record.cur_rounds_in_magazine;
                        write_u16_be(packet, size, record.rounds_in_reserve);
                        // (the slot index stays)
                        record.item_info = nullptr;
                        record.change_mask = 0;
                        record.cur_rounds_in_magazine = 0;
                        record.rounds_in_reserve = 0;
                    }
                }
                write_u16_be(packet, size, 0xFFFF);
                packet_set_size(packet, size);
                if (any)
                {
                    unsigned int* reliable_id = &Weapon_sync_data->m_weapons_update_reliable_id[connection];
                    *reliable_id = multi_io_send_reliable(cp, packet, size, nullptr, nullptr, reliable_id);
                }
            }
        }
        cp = cp->next;
        if (cp == first)
        {
            break;
        }
    }
}

// 0x009E1A50
// A player's received slot changes (not a dead one): 1 puts the item in the slot (replacing another item, which makes
// the last equipped weapon the second last), sets a remote player's (or a new item's) ammo and equips it when it is
// the desired slot, any slot is desired (7) or the slot had an item; the local player's inventory overlay opens.
// 2 adds the ammo to the slot's weapon of that kind (the local player is told, -1 when none fit); 3 removes the kind's
// weapon. An unknown change, or ammo for a missing weapon, ends the processing.
static bool player_weapon_update_event_process_callback(player_weapon_event_data* data)
{
    const unsigned int game_handle = data->player_ng_handle.get_game_handle();
    if (game_handle == 0xFFFFFFFF)
    {
        return true;
    }
    player* pp = World.object_from_handle<player>(game_handle);
    if (pp == nullptr || pp->is_killed())
    {
        return true;
    }
    for (int i = 0; i < (int)data->changes; i++)
    {
        const unsigned char change = data->change_mask[i];
        if (change == 1)
        {
            const inv_item_info* info = inv_item_info_from_handle(data->item_info_index[i]);
            inventory_item* iip = inv_item_get_item_in_slot(pp, data->inventory_slot[i]);
            const bool had_item = iip != nullptr;
            bool added = iip == nullptr;
            if (iip != nullptr && iip->info != info)
            {
                human_delete_item_from_inventory(pp, iip, true, false);
                pp->second_last_equipped_weapon = pp->last_equipped_weapon;
                added = true;
                iip = nullptr;
            }
            if (iip == nullptr)
            {
                iip = human_add_item_to_inventory(pp, info, 1, -1, -1, true, false, false);
            }
            // Original: the added item is used unchecked.
            iip->selection_slot = data->inventory_slot[i];
            if (!object_is_local_player(pp) || added)
            {
                weapon* wp = multi_game_player_item_weapon(iip);
                if (wp != nullptr)
                {
                    const int magazine_size = wp->get_magazine_size();
                    const int magazine = magazine_size <= (int)data->cur_rounds_in_magazine[i] ? magazine_size : (int)data->cur_rounds_in_magazine[i];
                    const int max_rounds = wp->get_max_rounds();
                    const int reserve = max_rounds <= (int)data->rounds_in_reserve[i] ? max_rounds : (int)data->rounds_in_reserve[i];
                    wp->set_rounds_in_magazine(magazine);
                    wp->set_rounds_in_reserve(reserve);
                }
                if ((int)data->inventory_slot[i] == pp->mp_desired_weapon_slot || pp->mp_desired_weapon_slot == 7 || had_item) // Magic number: 7, any slot
                {
                    player_equip_item(pp, iip, nullptr, false);
                }
            }
            if (object_is_local_player(pp))
            {
                gameseq_get_state(); // (unused)
                if (data->inventory_slot[i] == 7)
                {
                    player_equip_item(pp, iip, nullptr, false);
                }
                ovl_inventory_open(CBA_BUTTON_INVALID, 1.0f);
            }
        }
        else if (change == 2)
        {
            const inv_item_info* info = inv_item_info_from_handle(data->item_info_index[i]);
            const weapon_info* w_info = weapons_get_weapon_info(info);
            weapon* wp = multi_game_player_item_weapon(inv_item_get_item_in_slot(pp, data->inventory_slot[i]));
            if (wp == nullptr || wp->w_info != w_info)
            {
                return true;
            }
            const int rounds = (int)data->cur_rounds_in_magazine[i] + (int)data->rounds_in_reserve[i];
            int added_rounds = wp->add_ammo(rounds);
            if (rounds != 0 && added_rounds == 0)
            {
                added_rounds = -1;
            }
            if (object_is_local_player(pp))
            {
                weapon_acquired_event acquired;
                acquired.w_handle = weapons_get_weapon_info_handle(w_info);
                gem_post_event(acquired);
                item_pickup_info pickup;
                pickup.item_name = info->w_info->icon_name;
                pickup.total_val = 0;
                pickup.num_picked_up = added_rounds;
                hud_ui_add_item_pickup_message(&pickup, 1);
            }
        }
        else if (change == 3)
        {
            const inv_item_info* info = inv_item_info_from_handle(data->item_info_index[i]);
            const weapon_info* w_info = weapons_get_weapon_info(info);
            const int slot = inv_item_get_weapon_slot(pp, w_info);
            if (slot != -1)
            {
                inventory_item* iip = inv_item_get_item_in_slot(pp, slot);
                if (iip != nullptr)
                {
                    human_delete_item_from_inventory(pp, iip, true, false);
                }
            }
        }
        else
        {
            return true;
        }
    }
    return true;
}

// 0x009B1E90
static void player_weapon_update_event_destroy_callback(player_weapon_event_data* data)
{
    Weapon_sync_data->m_event_player_weapon_allocator.free(data);
}

// 0x0095F9C0
static bool player_weapon_update_event_allow_process_callback(player_weapon_event_data* data)
{
    return true;
}

// 0x009E2170 (:1755)
// Each player's changes become an event (the local player's, or with the pool full, are applied at once from a
// scratch record); timed ones wait as sim events for their network time, untimed ones are applied now.
void multi_game_process_player_weapons_update(unsigned char* packet_data, const sender& from)
{
    int offset = 0;
    const unsigned int network_time = read_u32_be(packet_data, offset);
    unsigned short net_handle = read_u16_be(packet_data, offset);
    while (net_handle != 0xFFFF)
    {
        const unsigned int game_handle = multi_core_game_handle_from_network_handle(net_handle, true);
        player_weapon_event_data* data = nullptr;
        bool pooled = false;
        if (!object_is_local_player(game_handle))
        {
            data = Weapon_sync_data->m_event_player_weapon_allocator.alloc(0);
            pooled = data != nullptr;
        }
        if (!pooled)
        {
            data = &Weapon_sync_data->m_temp_player_weapon_event_data;
        }
        data->player_ng_handle.set_network_handle(net_handle);
        data->player_ng_handle.get_game_handle(); // (the game handle looked up now, as the original stores it)
        data->changes = packet_data[offset++];
        for (int i = 0; i < (int)data->changes; i++)
        {
            data->inventory_slot[i] = packet_data[offset];
            offset++;
            data->item_info_index[i] = read_u32_be(packet_data, offset);
            data->change_mask[i] = packet_data[offset++];
            if ((unsigned char)(data->change_mask[i] - 1) < 3)
            {
                data->cur_rounds_in_magazine[i] = packet_data[offset++];
                data->rounds_in_reserve[i] = read_u16_be(packet_data, offset);
            }
        }
        if (!pooled)
        {
            player_weapon_update_event_process_callback(data);
        }
        else if (network_time == 0)
        {
            player_weapon_update_event_process_callback(data);
            Weapon_sync_data->m_event_player_weapon_allocator.free(data);
        }
        else
        {
            multi_game_sim_event_add(network_time, MULTI_GAME_SIM_EVENT_PLAYER_WEAPON_UPDATE, false, data, (bool (*)(void*))player_weapon_update_event_allow_process_callback, (bool (*)(void*))player_weapon_update_event_process_callback, (void (*)(void*))player_weapon_update_event_destroy_callback); // inlined
        }
        net_handle = read_u16_be(packet_data, offset);
    }
}

// 0x0095FAE0 (:1831): an object is out of view when it is behind the viewer's camera and more than 5 m away (the
// viewer null or the local player: the game camera)
bool multi_game_player_can_view_object(const player* pp, const object* obj)
{
    vector camera_dir;
    vector camera_pos;
    if ((pp == nullptr && (pp = player_get_local_player()) == nullptr) || object_is_local_player(pp))
    {
        camera_dir = Rfg_camera.real_orient.fvec;
        camera_pos = Rfg_camera.real_pos;
    }
    else
    {
        camera_dir = pp->mp_camera_orient.fvec;
        camera_pos = pp->mp_camera_pos;
    }
    vector pos;
    switch (obj->get_obj_type())
    {
    case OT_HUMAN:
        human_controller_get_pos((const human*)obj, pos);
        break;
    case OT_VEHICLE:
    {
        hkpRigidBody* body = vehicle_get_rigid_body(obj->get_handle());
        if (body == nullptr)
        {
            return true;
        }
        havok_to_game_vector(pos, body->getPosition());
        break;
    }
    default:
        pos = obj->pos;
        break;
    }
    pos.y = pos.y - camera_pos.y;
    pos.x = pos.x - camera_pos.x;
    pos.z = pos.z - camera_pos.z;
    // Magic number: 5 m squared.
    if (camera_dir.y * pos.y + camera_dir.x * pos.x + camera_dir.z * pos.z < 0.0f && pos.x * pos.x + pos.y * pos.y + pos.z * pos.z > 25.0f)
    {
        return false;
    }
    return true;
}

// 0x009D0C40 (:1896): the server tells everyone of a death (PT_PLAYER_DIED): time, victim, attacker, weapon (0xFF:
// none), damage type
void multi_game_human_died(const human* victim, const human* attacker, damaged_by_types damage_type, const weapon_info* weapon_used)
{
    unsigned char packet[Packet_buffer_size];
    packet[0] = PT_PLAYER_DIED;
    unsigned short size = packet_header_size(PT_PLAYER_DIED);
    write_u32_be(packet, size, Clock_current_time);
    if (victim == nullptr)
    {
        return;
    }
    write_u16_be(packet, size, multi_core_network_handle_from_game_handle(victim->get_handle(), false));
    // Original: the attacker is read through without a null check.
    write_u16_be(packet, size, multi_core_network_handle_from_game_handle(attacker->get_handle(), false));
    packet[size++] = weapon_used == nullptr ? 0xFF : (unsigned char)weapons_get_weapon_info_handle(weapon_used);
    packet[size++] = (unsigned char)damage_type;
    packet_set_size(packet, size);
    multi_io_send_to_all_reliable(multi_session_get_by_type(ST_GAME), packet, size);
}

// 0x009E2460 (:1945): a client kills the victim (a player only for a death later than half a second after its respawn)
void multi_game_player_process_player_death(unsigned char* packet_data, const sender& from)
{
    int offset = 0;
    const unsigned int death_time = read_u32_be(packet_data, offset);
    const unsigned short victim_net = read_u16_be(packet_data, offset);
    const unsigned short attacker_net = read_u16_be(packet_data, offset);
    human* hp = nullptr;
    if (victim_net != 0xFFFF)
    {
        object* objp = World.object_from_handle(multi_core_game_handle_from_network_handle(victim_net, true));
        hp = objp != nullptr && objp->get_obj_type() == OT_HUMAN ? (human*)objp : nullptr;
    }
    human* attacker = nullptr;
    if (attacker_net != 0xFFFF)
    {
        object* objp = World.object_from_handle(multi_core_game_handle_from_network_handle(attacker_net, true));
        attacker = objp != nullptr && objp->get_obj_type() == OT_HUMAN ? (human*)objp : nullptr;
    }
    // Magic number: a death within 500 ms of the respawn is an old one.
    if (object_is_player(hp) && ((player*)hp)->mp_last_respawn_time + 500 > death_time)
    {
        return;
    }
    const weapon_info* w_info = packet_data[8] != 0xFF ? weapons_get_weapon_info((unsigned int)packet_data[8]) : nullptr;
    const damaged_by_types damage_type = (damaged_by_types)packet_data[9];
    if (hp != nullptr && !hp->is_killed())
    {
        hp->hit_points = -1.0f;
        human_died(hp, attacker, damage_type, w_info, nullptr);
    }
}

// 0x009F6C10 (:2015): the next team (team modes: EDF, guerrilla, spectator in turn; else civilian or spectator)
void multi_game_player_increment_my_team(multi_session* session)
{
    if (Mp_game_data == nullptr)
    {
        return;
    }
    const unsigned int handle = Local_game_data_handles[session->get_info()->type];
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    multi_game_data* data = Mp_game_data->data[handle & 0xFFFF] + (handle >> 16);
    if (!data->in_use)
    {
        data->linger_time.valid();
        return;
    }
    human_teams team = data->team;
    if (Mp_server_options.get_mode() < MGPM_NUM_MODES && Multi_gameplay_mode_funcs[Mp_server_options.get_mode()].is_team_mode)
    {
        if (team == HUMAN_TEAM_MARAUDER)
        {
            multi_game_player_change_my_team(session, HUMAN_TEAM_GUERILLA);
            return;
        }
        team = (human_teams)(team + 1);
        if ((int)team > 1)
        {
            multi_game_player_change_my_team(session, HUMAN_TEAM_MARAUDER);
            return;
        }
    }
    else
    {
        multi_game_character_get_my_base_team(); // the result is unused
        team = team != HUMAN_TEAM_MARAUDER ? HUMAN_TEAM_MARAUDER : HUMAN_TEAM_CIVILIAN;
    }
    multi_game_player_change_my_team(session, team);
}

// 0x009F6CD0 (:2046): the previous team
void multi_game_player_decrement_my_team(multi_session* session)
{
    if (Mp_game_data == nullptr)
    {
        return;
    }
    const unsigned int handle = Local_game_data_handles[session->get_info()->type];
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    multi_game_data* data = Mp_game_data->data[handle & 0xFFFF] + (handle >> 16);
    if (!data->in_use)
    {
        data->linger_time.valid();
        return;
    }
    human_teams team = data->team;
    if (Mp_server_options.get_mode() < MGPM_NUM_MODES && Multi_gameplay_mode_funcs[Mp_server_options.get_mode()].is_team_mode)
    {
        if (team == HUMAN_TEAM_MARAUDER)
        {
            multi_game_player_change_my_team(session, HUMAN_TEAM_EDF);
            return;
        }
        team = (human_teams)(team - 1);
        if ((int)team < 0)
        {
            multi_game_player_change_my_team(session, HUMAN_TEAM_MARAUDER);
            return;
        }
    }
    else
    {
        multi_game_character_get_my_base_team(); // the result is unused
        team = team != HUMAN_TEAM_MARAUDER ? HUMAN_TEAM_MARAUDER : HUMAN_TEAM_CIVILIAN;
    }
    multi_game_player_change_my_team(session, team);
}

// 0x009E25D0 (:2081): the host sets and announces its team, a client asks the host
void multi_game_player_change_my_team(multi_session* session, human_teams team)
{
    if (team >= NUM_HUMAN_TEAMS)
    {
        return;
    }
    char text[256];
    if (session->am_i_host())
    {
        multi_game_data_set_player_team(Local_game_data_handles[session->get_info()->type], team);
        multi_game_player_send_team_change(session->get_host_connection(), team);
        // Original: the text goes to a thread local buffer.
        sprintf(text, "Player team changed to '%ls'\n", human_team_get_display_name(team));
        console_output(text, nullptr, false);
        return;
    }
    multi_game_player_send_team_request(session, team);
    sprintf(text, "Requested player team change to '%ls'\n", human_team_get_display_name(team));
    console_output(text, nullptr, false);
}

// 0x009D0DA0 (:2103): PT_PLAYER_REQUEST_TEAM to the host
void multi_game_player_send_team_request(multi_session* session, human_teams team)
{
    unsigned char packet[Packet_buffer_size];
    packet[0] = PT_PLAYER_REQUEST_TEAM;
    unsigned short size = packet_header_size(PT_PLAYER_REQUEST_TEAM);
    packet[size++] = (unsigned char)team;
    packet_set_size(packet, size);
    multi_io_send_reliable(session->get_host_connection(), packet, size, nullptr, nullptr, nullptr);
}

// 0x009D0E30 (:2118): PT_TEAM_CHANGE to everyone: the player's data handle and team
void multi_game_player_send_team_change(const multi_connection* changed_player, human_teams new_team)
{
    unsigned char packet[Packet_buffer_size];
    packet[0] = PT_TEAM_CHANGE;
    unsigned short size = packet_header_size(PT_TEAM_CHANGE);
    write_u32_be(packet, size, changed_player->game_data);
    packet[size++] = (unsigned char)new_team;
    packet_set_size(packet, size);
    multi_io_send_to_all_reliable(changed_player->owner, packet, size);
}

// 0x009D0F10 (:2133): the host accepts a team (civilian outside team modes, EDF for a civilian in a team mode) and
// announces a change
void multi_game_player_process_team_request(unsigned char* packet_data, const sender& from)
{
    human_teams new_team = (human_teams)packet_data[0];
    multi_connection* cp = from.connection;
    if (new_team < HUMAN_TEAM_CIVILIAN)
    {
        // Original: the mode index is checked against the last mode with a team row (demolition).
        if (Mp_server_options.get_mode() > MGPM_DEMOLITION || !Multi_gameplay_mode_funcs[Mp_server_options.get_mode()].is_team_mode)
        {
            new_team = HUMAN_TEAM_CIVILIAN;
        }
    }
    else if (new_team == HUMAN_TEAM_CIVILIAN && Mp_server_options.get_mode() < MGPM_NUM_MODES && Multi_gameplay_mode_funcs[Mp_server_options.get_mode()].is_team_mode)
    {
        new_team = HUMAN_TEAM_EDF;
    }
    // Original: the data is read through without a null check.
    const multi_game_data* data = multi_game_data_get(cp->game_data, false, false); // inlined
    if (data->team == new_team)
    {
        return;
    }
    const unsigned int handle = cp->game_data;
    if (handle != 0xFFFFFFFF)
    {
        Mp_game_data->data[handle & 0xFFFF][handle >> 16].team = new_team;
        if (Local_game_data_handles[handle >> 16] == handle)
        {
            Spectating = new_team == HUMAN_TEAM_MARAUDER;
        }
    }
    multi_game_player_send_team_change(cp, new_team);
}

// 0x00982F60 (:2172)
void multi_game_player_process_team_change(unsigned char* packet_data, const sender& from)
{
    int offset = 0;
    const unsigned int handle = read_u32_be(packet_data, offset);
    const human_teams team = (human_teams)packet_data[4];
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    Mp_game_data->data[handle & 0xFFFF][handle >> 16].team = team;
    if (Local_game_data_handles[handle >> 16] == handle)
    {
        Spectating = team == HUMAN_TEAM_MARAUDER;
    }
}

// 0x00983000 (:2189): the count, then each player's data handle and team (0xFF: none)
void multi_game_player_append_teams_to_packet(unsigned char* packet_data, unsigned short& packet_size, session_types session_type)
{
    packet_data[packet_size] = (unsigned char)multi_game_data_count_players(session_type);
    packet_size += 1;
    for (unsigned int handle = multi_game_data_handles_begin(session_type, false, false); handle != 0xFFFFFFFF; handle = multi_game_data_handles_next(handle, false, false))
    {
        write_u32_be(packet_data, packet_size, handle);
        // Original: the data is read through without a null check.
        const multi_game_data* data = multi_game_data_get(handle, false, false);
        packet_data[packet_size] = data->team == HUMAN_TEAM_NONE ? 0xFF : (unsigned char)data->team;
        packet_size += 1;
    }
}

// 0x00983150 (:2212)
void multi_game_player_read_teams_from_packet(unsigned char* packet_data, int& packet_offset)
{
    const unsigned int count = packet_data[packet_offset];
    packet_offset += 1;
    for (unsigned int i = 0; i < count; i++)
    {
        const unsigned int handle = read_u32_be(packet_data, packet_offset);
        const unsigned char team_byte = packet_data[packet_offset];
        packet_offset += 1;
        const human_teams team = team_byte == 0xFF ? HUMAN_TEAM_NONE : (human_teams)team_byte;
        if (handle == 0xFFFFFFFF)
        {
            continue;
        }
        Mp_game_data->data[handle & 0xFFFF][handle >> 16].team = team;
        if (Local_game_data_handles[handle >> 16] == handle)
        {
            Spectating = team == HUMAN_TEAM_MARAUDER;
        }
    }
}

// 0x009E2690 (:2237)
void multi_game_player_set_my_desired_weapon(multi_session* session, unsigned int desired_weapon_inv_item_info_handle)
{
    const unsigned int handle = Local_game_data_handles[session->get_info()->type];
    if (handle != 0xFFFFFFFF)
    {
        multi_game_player_set_desired_weapon(session, handle, desired_weapon_inv_item_info_handle);
    }
}

// 0x009D1000 (:2244): the lobby host sets and announces a player's desired weapon (PT_DESIRED_WEAPON_CHANGE), a
// client asks for its own (PT_REQUEST_DESIRED_WEAPON_CHANGE)
void multi_game_player_set_desired_weapon(multi_session* session, unsigned int multi_game_data_handle, unsigned int desired_weapon_inv_item_info_handle)
{
    if (Mp_game_data == nullptr || multi_game_data_handle == 0xFFFFFFFF)
    {
        return;
    }
    multi_game_data* data = Mp_game_data->data[multi_game_data_handle & 0xFFFF] + (multi_game_data_handle >> 16);
    if (!data->in_use)
    {
        data->linger_time.valid();
        return;
    }
    if (data->desired_weapon_inv_item_info_handle == desired_weapon_inv_item_info_handle)
    {
        return;
    }
    unsigned char packet[Packet_buffer_size];
    if (Lobby_session == nullptr || !Lobby_session->am_i_host())
    {
        if (multi_game_data_handle != Local_game_data_handles[session->get_info()->type])
        {
            return;
        }
        packet[0] = PT_REQUEST_DESIRED_WEAPON_CHANGE;
        unsigned short size = packet_header_size(PT_REQUEST_DESIRED_WEAPON_CHANGE);
        write_u32_be(packet, size, multi_game_data_handle);
        write_u32_be(packet, size, desired_weapon_inv_item_info_handle);
        packet_set_size(packet, size);
        multi_io_send_reliable(session->get_host_connection(), packet, size, nullptr, nullptr, nullptr);
        return;
    }
    multi_game_data_set_desired_weapon(multi_game_data_handle, desired_weapon_inv_item_info_handle);
    packet[0] = PT_DESIRED_WEAPON_CHANGE;
    unsigned short size = packet_header_size(PT_DESIRED_WEAPON_CHANGE);
    write_u32_be(packet, size, multi_game_data_handle);
    write_u32_be(packet, size, desired_weapon_inv_item_info_handle);
    packet_set_size(packet, size);
    multi_io_send_to_all_reliable(session, packet, size);
}

// 0x009D12A0 (:2285): a weapon the level offers, else none
void multi_game_player_process_desired_weapon_request(unsigned char* packet_data, const sender& from)
{
    int offset = 0;
    const unsigned int data_handle = read_u32_be(packet_data, offset);
    unsigned int weapon_handle = read_u32_be(packet_data, offset);
    const multi_level_info* level = Mp_server_options.get_level();
    if (level == nullptr)
    {
        return;
    }
    bool offered = false;
    if (level->player_weapons != nullptr)
    {
        for (int i = 0; i < level->num_player_weapons; i++)
        {
            if (weapon_handle == level->player_weapons[i])
            {
                offered = true;
                break;
            }
        }
    }
    if (!offered)
    {
        weapon_handle = 0xFFFFFFFF;
    }
    multi_game_player_set_desired_weapon(from.connection->owner, data_handle, weapon_handle);
}

// 0x0095FD30 (:2306)
void multi_game_player_process_desired_weapon_change(unsigned char* packet_data, const sender& from)
{
    int offset = 0;
    const unsigned int data_handle = read_u32_be(packet_data, offset);
    const unsigned int weapon_handle = read_u32_be(packet_data, offset);
    if (data_handle != 0xFFFFFFFF)
    {
        Mp_game_data->data[data_handle & 0xFFFF][data_handle >> 16].desired_weapon_inv_item_info_handle = weapon_handle;
    }
}

// 0x00983230 (:2319): the count, then each player's data handle and desired weapon
void multi_game_player_append_desired_weapons_to_packet(unsigned char* packet_data, unsigned short& packet_size, multi_session* session)
{
    const session_types type = session->get_info()->type;
    packet_data[packet_size] = (unsigned char)multi_game_data_count_players(type);
    packet_size += 1;
    for (unsigned int handle = multi_game_data_handles_begin(type, false, false); handle != 0xFFFFFFFF; handle = multi_game_data_handles_next(handle, false, false))
    {
        const multi_game_data* data = multi_game_data_get(handle, false, false);
        if (data == nullptr)
        {
            return;
        }
        write_u32_be(packet_data, packet_size, data->handle);
        write_u32_be(packet_data, packet_size, data->desired_weapon_inv_item_info_handle);
    }
}

// 0x0095FE00 (:2329)
void multi_game_player_read_desired_weapons_from_packet(unsigned char* packet_data, int& packet_offset)
{
    const unsigned int count = packet_data[packet_offset];
    packet_offset += 1;
    for (unsigned int i = 0; i < count; i++)
    {
        const unsigned int data_handle = read_u32_be(packet_data, packet_offset);
        const unsigned int weapon_handle = read_u32_be(packet_data, packet_offset);
        if (data_handle != 0xFFFFFFFF)
        {
            Mp_game_data->data[data_handle & 0xFFFF][data_handle >> 16].desired_weapon_inv_item_info_handle = weapon_handle;
        }
    }
}

// 0x009D13A0 (:2345): the mode's starting weapon when it replaces the default, else the level's only player weapon
const inv_item_info* multi_game_player_get_default_weapon(unsigned int multi_game_data_handle)
{
    multi_game_equipment_loadout loadout;
    loadout.replace_starting_weapon = Mp_server_options.generic_options.replace_starting_weapon;
    loadout.starting_weapon = Mp_server_options.generic_options.starting_weapon;
    player* pp = multi_game_data_get_player_instance(multi_game_data_handle);
    if (pp != nullptr)
    {
        multi_gameplay_mode_get_default_equipment(pp, &loadout);
    }
    if (loadout.replace_starting_weapon)
    {
        const weapon_info* w_info = weapons_get_weapon_info(loadout.starting_weapon);
        return w_info != nullptr ? w_info->weapon_inv_item_info : nullptr;
    }
    if (Mp_game_data == nullptr || multi_game_data_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    multi_game_data* data = Mp_game_data->data[multi_game_data_handle & 0xFFFF] + (multi_game_data_handle >> 16);
    if (!data->in_use)
    {
        data->linger_time.valid();
        return nullptr;
    }
    if (Mp_server_options.get_level() != nullptr && Mp_server_options.get_level()->num_player_weapons == 1)
    {
        return inv_item_info_from_handle(Mp_server_options.get_level()->player_weapons[0]);
    }
    return nullptr;
}

// 0x009D14A0 (:2378): the local player's damage from itself (the server applies it, a client asks with
// PT_REQUEST_DAMAGE: the damage in 1/16 and the reason)
void multi_game_player_request_damage(float damage, request_damage_reason reason)
{
    player* pp = player_get_local_player();
    if (pp == nullptr || Game_session == nullptr)
    {
        return;
    }
    if (Multiplayer && Server)
    {
        human_damage(pp, pp, damage, DBT_NONE, nullptr, nullptr, true, false);
        return;
    }
    unsigned char packet[Packet_buffer_size];
    packet[0] = PT_REQUEST_DAMAGE;
    unsigned short size = packet_header_size(PT_REQUEST_DAMAGE);
    // Magic number: 16 steps per hit point.
    write_u16_be(packet, size, (unsigned short)(int)(damage * 16.0f));
    packet[size++] = (unsigned char)reason;
    packet_set_size(packet, size);
    multi_io_send_reliable(Game_session->get_host_connection(), packet, size, nullptr, nullptr, nullptr);
}

// 0x009D15D0 (:2403): a fall kills with the cliff weapon (no attacker), anything else is the player's own damage
void multi_game_player_process_request_damage(unsigned char* packet_data, const sender& from)
{
    if (from.connection == nullptr)
    {
        return;
    }
    player* pp = multi_game_data_get_player_instance(from.connection->game_data);
    if (pp == nullptr)
    {
        return;
    }
    const short packed = (short)(packet_data[0] << 8 | packet_data[1]);
    const weapon_info* w_info = nullptr;
    human* attacker = pp;
    if (packet_data[2] == RDR_FALL)
    {
        w_info = weapons_get_weapon_info(Multi_custom_weapon_names[9]); // "mp_killzone_cliff_suicide"
        attacker = nullptr;
    }
    human_damage(pp, attacker, (float)(int)packed * 0.0625f, DBT_NONE, w_info, nullptr, true, false);
}

// The PT_PLAYER_RAGDOLL velocities: a magnitude and two angles (the heading atan2(y, x) and the pitch acos(z / |v|))
// packed into a 40 bit field (linear 13 + 6 + 5 bits, angular 5 + 6 + 5).
// Magic numbers: linear speeds up to 250 m/s, angular up to 25 rad/s; headings in [-pi, pi], pitches in [0, pi] (the
// float pi as a double).
static const double RAGDOLL_PI = (double)3.14159274f;
static const double RAGDOLL_HALF_PI = (double)1.57079637f;

static void multi_game_player_write_ragdoll_vector(bit_array& bits, int& bit, const vector& v, double max_magnitude, double magnitude_bias, int magnitude_bits)
{
    const float magnitude = (float)sqrt((double)(float)((double)v.x * (double)v.x + (double)v.y * (double)v.y + (double)v.z * (double)v.z));
    float heading = 0.0f;
    float pitch = 0.0f;
    if (magnitude != 0.0f)
    {
        heading = (float)atan2((double)v.y, (double)v.x);
        float c = (float)((double)v.z / (double)magnitude);
        if ((float)((double)c - -1.0) < 0.0f)
        {
            c = -1.0f;
        }
        if ((float)((double)c - 1.0) >= 0.0f)
        {
            c = 1.0f;
        }
        pitch = fl_acos(c);
    }
    unsigned int packed[3];
    packed[0] = compress_float(magnitude, 0.0, max_magnitude, magnitude_bias, 1.0, magnitude_bits);
    packed[1] = compress_float(heading, -RAGDOLL_PI, RAGDOLL_PI, 0.0, 1.0, 6);
    packed[2] = compress_float(pitch, 0.0, RAGDOLL_PI, RAGDOLL_HALF_PI, 1.0, 5);
    const int widths[3] = {magnitude_bits, 6, 5};
    for (int f = 0; f < 3; f++)
    {
        // a field is skipped (the bits left as they are) when it would not fit the 40 bits
        if ((unsigned int)(bit + widths[f]) <= 40)
        {
            unsigned int value = packed[f];
            for (int i = 0; i < widths[f]; i++)
            {
                bits.set(bit + i, value & 1);
                value >>= 1;
            }
        }
        bit += widths[f];
    }
}

static vector multi_game_player_read_ragdoll_vector(const bit_array& bits, int& bit, double max_magnitude, double magnitude_bias, int magnitude_bits)
{
    unsigned int packed[3] = {0, 0, 0};
    const int widths[3] = {magnitude_bits, 6, 5};
    for (int f = 0; f < 3; f++)
    {
        for (int i = 0; i < widths[f]; i++)
        {
            packed[f] |= bits.get(bit + i) << i;
        }
        bit += widths[f];
    }
    const float magnitude = (float)decompress_double(packed[0], 0.0, max_magnitude, magnitude_bias, 1.0, magnitude_bits);
    const float heading = (float)decompress_double(packed[1], -RAGDOLL_PI, RAGDOLL_PI, 0.0, 1.0, 6);
    const float pitch = (float)decompress_double(packed[2], 0.0, RAGDOLL_PI, RAGDOLL_HALF_PI, 1.0, 5);
    const double horizontal = (double)((float)sin((double)pitch) * magnitude);
    return vector((float)((double)(float)cos((double)heading) * horizontal), (float)((double)(float)sin((double)heading) * horizontal), (float)cos((double)pitch) * magnitude);
}

// 0x0095FF20 (:2432)
// The server (or single player) collects a player's ragdoll pushes; multi_game_player_do_frame sends their sums.
void multi_game_player_ragdoll(player* pp, const vector& linear_velocity, const vector& angular_velocity)
{
    if ((Multiplayer && !Server) || pp == nullptr)
    {
        return;
    }
    pp->mp_ragdoll_linear_velocity_aggregate.x = pp->mp_ragdoll_linear_velocity_aggregate.x + linear_velocity.x;
    pp->mp_ragdoll_linear_velocity_aggregate.y = linear_velocity.y + pp->mp_ragdoll_linear_velocity_aggregate.y;
    pp->mp_ragdoll_linear_velocity_aggregate.z = linear_velocity.z + pp->mp_ragdoll_linear_velocity_aggregate.z;
    pp->mp_ragdoll_angular_velocity_aggregate.x = pp->mp_ragdoll_angular_velocity_aggregate.x + angular_velocity.x;
    pp->mp_ragdoll_angular_velocity_aggregate.y = angular_velocity.y + pp->mp_ragdoll_angular_velocity_aggregate.y;
    pp->mp_ragdoll_angular_velocity_aggregate.z = angular_velocity.z + pp->mp_ragdoll_angular_velocity_aggregate.z;
}

// 0x009D1680 (:2448)
// A client ragdolls the player (unless it already is) and pushes it: the linear velocity capped, the angular one on
// the body parts 0x3B.
void multi_game_player_process_player_ragdoll(unsigned char* packet_data, const sender& from)
{
    const unsigned short net_handle = (unsigned short)(packet_data[0] << 8 | packet_data[1]);
    const unsigned int handle = net_handle == 0xFFFF ? 0xFFFFFFFF : multi_core_game_handle_from_network_handle(net_handle, true);
    unsigned char field[5];
    memcpy(field, packet_data + 2, sizeof(field));
    bit_array bits;
    bits.init(field, 40);
    int bit = 0;
    // Original: the linear speed decompresses with a bias of 75 where multi_game_player_do_frame compressed with 50.
    const vector linear = multi_game_player_read_ragdoll_vector(bits, bit, 250.0, 75.0, 13);
    const vector angular = multi_game_player_read_ragdoll_vector(bits, bit, 25.0, 25.0, 5);
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_PLAYER)
    {
        return;
    }
    human* hp = (human*)objp;
    if (hp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE || human_ragdoll_start(hp, true, 0xFFFFFFFF))
    {
        havok_ragdoll_apply_velocity(hp->ragdoll_idx, linear, true);
        havok_ragdoll_apply_angular_velocity(hp->ragdoll_idx, angular, 0x3B);
    }
}

// 0x009D1BB0 (:2492)
// The server sends a point impulse on a player's ragdoll (PT_PLAYER_RAGDOLL_POINT: the position relative to the
// player, the impulse) to every client.
void multi_game_player_ragdoll_point_impulse(const player* pp, const vector& relative_impulse_position, const vector& linear_impulse)
{
    if ((Multiplayer && !Server) || pp == nullptr)
    {
        return;
    }
    unsigned char packet[Packet_buffer_size];
    packet[0] = PT_PLAYER_RAGDOLL_POINT;
    unsigned short size = packet_header_size(PT_PLAYER_RAGDOLL_POINT);
    const unsigned int handle = pp->get_handle();
    write_u16_be(packet, size, handle == 0xFFFFFFFF ? 0xFFFF : multi_core_network_handle_from_game_handle(handle, true));
    const float values[6] = {relative_impulse_position.x, relative_impulse_position.y, relative_impulse_position.z, linear_impulse.x, linear_impulse.y, linear_impulse.z};
    for (int i = 0; i < 6; i++)
    {
        unsigned int bits;
        memcpy(&bits, &values[i], 4);
        write_u32_be(packet, size, bits);
    }
    packet_set_size(packet, size);
    multi_session* session = multi_session_get_by_type(ST_GAME);
    if (session == nullptr)
    {
        return;
    }
    multi_connection* first = session->get_connection_list();
    for (multi_connection* cp = first; cp != nullptr;)
    {
        if (cp != session->get_host_connection())
        {
            multi_io_send(cp, packet, size);
        }
        cp = cp->next;
        if (cp == first)
        {
            break;
        }
    }
}

// 0x009D1F30 (:2521)
// A client ragdolls the player (unless it already is) and applies the impulse at the position.
void multi_game_player_process_player_ragdoll_point_impulse(unsigned char* packet_data, const sender& from)
{
    int offset = 0;
    const unsigned short net_handle = read_u16_be(packet_data, offset);
    const unsigned int handle = net_handle == 0xFFFF ? 0xFFFFFFFF : multi_core_game_handle_from_network_handle(net_handle, true);
    float values[6];
    for (int i = 0; i < 6; i++)
    {
        const unsigned int bits = read_u32_be(packet_data, offset);
        memcpy(&values[i], &bits, 4);
    }
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_PLAYER)
    {
        return;
    }
    human* hp = (human*)objp;
    if (hp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE || human_ragdoll_start(hp, true, 0xFFFFFFFF))
    {
        const vector impulse(values[3], values[4], values[5]);
        const vector pos(hp->pos.x + values[0], hp->pos.y + values[1], hp->pos.z + values[2]);
        havok_ragdoll_apply_point_impulse(hp->ragdoll_idx, impulse, pos);
    }
}

// 0x009D22D0 (:2692): the received update events are dropped
void multi_game_player_weapon_update_clear()
{
    for (player_weapon_event_data* data = Weapon_sync_data->m_event_player_weapon_allocator.get_first(0); data != nullptr;)
    {
        data = Weapon_sync_data->m_event_player_weapon_allocator.free(data);
    }
}
