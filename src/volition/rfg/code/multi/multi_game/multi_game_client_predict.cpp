// Original: volition/rfg/code/multi/multi_game/multi_game_client_predict.cpp
//
// Decompiled whole (multiplayer.md MP6). A client simulates its own player and sends its controls and state to the
// server every network frame (PT_CLIENT_CONTROLS, unreliable): the client time, 25 packed bytes (ragdoll state and
// knockdown time, the action ids of human::mp_flags, the charge, the running attack and jump, the desired weapon slot
// and its ammo, the aim as a direction from the head, the sticks relative to the aim, its position and velocity), the
// control flags (crouch, reload, ladder, detonations, jetpack, bone deform, vehicle and turret use, zoom, weapon swap)
// with their handles, and the ladder state. The server equips the slot, applies the flags at once and adds the rest to
// the player's received states (played back by multi_game_player_process_state from player_do_frame).
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_client_predict.h"

#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_compression.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_move.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/util/bit_util.h"

#include <string.h>

// ---- file-static globals
// Unread in the shipped build:
static const float PI = 3.14159274f; // 0x01309C38
// dynamic_uint_hash_table<remote_object*> Game_handle_to_rop_hash (0x01691B40) is used here too; placed in multi_core_delta_compression.cpp.
// dynamic_uint_hash_table<remote_object*> Network_handle_to_rop_hash (0x01691B88) is used here too; placed in multi_core_delta_compression.cpp.
// dynamic_uint_hash_table<network_game_handle*> Reserved_game_to_network_hash (0x01691C18) is used here too; placed in multi_core_objects.cpp.
// dynamic_uint_hash_table<network_game_handle*> Reserved_network_to_game_hash (0x01691C60) is used here too; placed in multi_core_delta_compression.cpp.
// unsigned int Clock_current_time (0x02FEB538) is used here too; placed in multi.cpp.
// migration_state Migration_state (0x02FEB6C4) is used here too; placed in multi.cpp.
// object_pool<multi_object_backpack,76,1> multi_object_backpackpool (0x02FEDC90) is used here too; placed in multi_game_mode_king_of_the_hill.cpp.

// The packed part of the controls: 200 bits.
static const int Client_controls_packed_bytes = 25;

// ---- internal functions
static void fill_control_flags(unsigned short& flags_out, player* pp); // 0x00951380 (:156)
static void append_control_data_to_packet(unsigned char* packet_data, unsigned short& packet_size, unsigned short control_flags, const player* pp); // 0x009BD280 (:213)
static void read_control_flags(multi_player_state& mis, player* pp, const unsigned short flags_in, unsigned char* packet_data, int& packet_offset); // 0x009DD870 (:239)

// 0x00951380 (:156): the flags of the control packet; the one-shot detonation, vehicle, turret and swap requests are
// cleared once taken
// LTCG: flags_out in EDI, pp in ESI (the port has no register convention).
static void fill_control_flags(unsigned short& flags_out, player* pp)
{
    flags_out = 0;
    if (human_move_is_crouching(pp))
    {
        flags_out |= 1;
    }
    if (human_weapons_is_reloading(pp, false))
    {
        flags_out |= 2;
    }
    if (pp->hflags.is_on_ladder)
    {
        flags_out |= 4;
    }
    if (pp->mp_flags.get_detonated_charges() != 0)
    {
        flags_out |= 8;
        pp->mp_flags.set_detonated_charges(0);
    }
    if (pp->mp_flags.get_in_air_detonate() != 0)
    {
        flags_out |= 0x100;
        pp->mp_flags.set_in_air_detonate(0);
    }
    if (pp->pflags.jetpack_started)
    {
        flags_out |= 0x10;
    }
    if (pp->hflags.dont_deform_bones)
    {
        flags_out |= 0x20;
    }
    if (pp->mp_flags.vehicle)
    {
        flags_out |= 0x40;
        pp->mp_flags.vehicle = 0;
    }
    if (pp->mp_flags.turret)
    {
        flags_out |= 0x80;
        pp->mp_flags.turret = 0;
    }
    if (player_is_zoomed(pp))
    {
        flags_out |= 0x200;
    }
    if (pp->mp_flags.swap_weapon)
    {
        if (player_get_swap_weapon(pp) != nullptr && World.object_from_handle(pp->mp_action_object_handle) != nullptr)
        {
            flags_out |= 0x400;
        }
        pp->mp_flags.swap_weapon = 0;
    }
}

// 0x009BD280 (:213): the network handles the flags need (ladder, vehicle, turret, the swap's pickup and slot)
// LTCG: packet_size in ESI, the rest on the stack (the port has no register convention).
static void append_control_data_to_packet(unsigned char* packet_data, unsigned short& packet_size, unsigned short control_flags, const player* pp)
{
    if ((control_flags & 4) != 0)
    {
        write_u16_be(packet_data, packet_size, pp->ladder_handle == 0xFFFFFFFF ? 0xFFFF : multi_core_network_handle_from_game_handle(pp->ladder_handle, true));
    }
    if ((control_flags & 0x40) != 0)
    {
        write_u16_be(packet_data, packet_size, multi_core_network_handle_from_game_handle(pp->mp_action_object_handle, true));
    }
    if ((control_flags & 0x80) != 0)
    {
        write_u16_be(packet_data, packet_size, multi_core_network_handle_from_game_handle(pp->mp_action_object_handle, true));
    }
    if ((control_flags & 0x400) == 0)
    {
        return;
    }
    write_u16_be(packet_data, packet_size, multi_core_network_handle_from_game_handle(pp->mp_action_object_handle, true));
    // Original: the swap weapon is read through without a null check (the flag is only set with one).
    packet_data[packet_size] = (unsigned char)player_get_swap_weapon(pp)->selection_slot;
    packet_size += 1;
}

// 0x009DD870 (:239)
// The server applies a client's control flags: crouch, reload, the ladder (by network handle), the detonations (at
// once), jetpack, bone deform, the vehicle and turret use requests (at once), fine aim, and a weapon swap.
// LTCG: mis in EDI, the rest on the stack (the port has no register convention).
static void read_control_flags(multi_player_state& mis, player* pp, const unsigned short flags_in, unsigned char* packet_data, int& packet_offset)
{
    mis.is_crouched = (flags_in & 1) != 0 ? 1 : 0;
    mis.reload = (flags_in & 2) != 0 ? 1 : 0;
    if ((flags_in & 4) == 0)
    {
        mis.is_on_ladder = 0;
        mis.ladder_handle = 0xFFFFFFFF;
    }
    else
    {
        // A ladder that is not (or no longer) a remote object leaves the state's (zeroed) ladder.
        const unsigned short ladder_net = read_u16_be(packet_data, packet_offset);
        if (ladder_net != 0xFFFF)
        {
            const unsigned int ladder_handle = multi_core_game_handle_from_network_handle(ladder_net, true); // inlined
            if (ladder_handle != 0xFFFFFFFF)
            {
                mis.is_on_ladder = 1;
                mis.ladder_handle = ladder_handle;
            }
        }
    }
    if ((flags_in & 8) == 0)
    {
        pp->mp_flags.set_detonated_charges(0);
    }
    else
    {
        pp->mp_flags.set_detonated_charges(1);
        detonate_charges(pp->get_handle());
    }
    if ((flags_in & 0x100) == 0)
    {
        pp->mp_flags.set_in_air_detonate(0);
    }
    else
    {
        pp->mp_flags.set_in_air_detonate(1);
        detonate_in_air(pp->get_handle());
    }
    mis.jetpack = (flags_in & 0x10) != 0 ? 1 : 0;
    mis.dont_deform_bones = (flags_in & 0x20) != 0 ? 1 : 0;
    if ((flags_in & 0x40) != 0)
    {
        const unsigned short net = read_u16_be(packet_data, packet_offset);
        pp->action_object.handle = net == 0xFFFF ? 0xFFFFFFFF : multi_core_game_handle_from_network_handle(net, true);
        player_use_vehicle(pp, true);
    }
    if ((flags_in & 0x80) != 0)
    {
        const unsigned short net = read_u16_be(packet_data, packet_offset);
        pp->action_object.handle = net == 0xFFFF ? 0xFFFFFFFF : multi_core_game_handle_from_network_handle(net, true);
        player_use_turret(pp, true);
    }
    if ((flags_in & 0x200) == 0)
    {
        if (player_is_fine_aimed(pp))
        {
            player_set_zoom_state(pp, PZS_OFF, false);
        }
    }
    else if (!player_is_fine_aimed(pp))
    {
        player_set_zoom_state(pp, PZS_FINE_AIM, false);
    }
    if ((flags_in & 0x400) != 0)
    {
        const unsigned short net = read_u16_be(packet_data, packet_offset);
        const unsigned char swap_slot = packet_data[packet_offset];
        packet_offset += 1;
        player_do_weapon_swap(pp, net == 0xFFFF ? 0xFFFFFFFF : multi_core_game_handle_from_network_handle(net, true), swap_slot);
    }
}

// 0x009BD560 (:627): the local player's controls to the server (not while the session migrates)
void multi_game_cp_send_client_controls()
{
    multi_session* session = multi_session_get_by_type(ST_GAME);
    if (session == nullptr || session->get_state() == MSS_MIGRATING)
    {
        return;
    }
    multi_connection* host = session->get_host_connection();
    if (host == nullptr || host->connection_status == CS_DISCONNECTED)
    {
        return;
    }
    player* pp = player_get_local_player();
    if (pp == nullptr || Migration_state != MIGRATION_STATE_NONE)
    {
        return;
    }
    unsigned char packet[Packet_buffer_size + 4];
    packet[0] = PT_CLIENT_CONTROLS;
    unsigned short size = packet_header_size(PT_CLIENT_CONTROLS);
    write_u32_be(packet, size, Clock_current_time);
    unsigned char packed[28];
    bit_packer packer(packed, Client_controls_packed_bytes);
    unsigned int knockdown_left = (unsigned int)pp->knockdown_timestamp.time_until();
    if (knockdown_left > 0xFFFF)
    {
        knockdown_left = 0xFFFF;
    }
    packer.set_next_bits(pp->ragdoll_state, 3);
    packer.set_next_bits(knockdown_left, 16);
    packer.set_next_bits(pp->mp_flags.melee_primary_id, 2);
    packer.set_next_bits(pp->mp_flags.melee_secondary_id, 2);
    packer.set_next_bits(pp->mp_flags.melee_tertiary_id, 2);
    packer.set_next_bits(pp->mp_flags.melee_firearm_id, 2);
    packer.set_next_bits(pp->mp_flags.firearm_trigger_down_id, 2);
    packer.set_next_bits(pp->mp_flags.firearm_trigger_up_id, 2);
    packer.set_next_bits(pp->mp_flags.throw_attack_id, 2);
    packer.set_next_bits(pp->mp_flags.get_charge_weapon(), 2);
    packer.set_next_bits(pp->mp_flags.get_running_attack(), 1);
    packer.set_next_bits(pp->mp_flags.jump_id, 2);
    packer.set_next_bits(pp->mp_desired_weapon_slot, 3);
    unsigned short magazine = 0;
    unsigned short reserve = 0;
    if (pp->desired_equipped_inv_item != nullptr)
    {
        weapon* wp = World.object_from_handle<weapon>(pp->desired_equipped_inv_item->weapon_handle);
        if (wp != nullptr)
        {
            magazine = (unsigned short)wp->get_rounds_in_magazine();
            reserve = (unsigned short)wp->get_rounds_in_reserve();
        }
    }
    // Original: 7 and 9 bits (more rounds are sent modulo 128 and 512).
    packer.set_next_bits(magazine, 7);
    packer.set_next_bits(reserve, 9);
    // The aim: the direction from the head to the aim point.
    matrix aim(IDENTITY_MATRIX);
    vector head;
    human_get_head_pos(pp, head);
    vector aim_dir(pp->aim_pos.x - head.x, pp->aim_pos.y - head.y, pp->aim_pos.z - head.z);
    aim_dir.normalize_safe();
    aim.make_quick(aim_dir);
    // Magic numbers: pi in 12 bits (the camera's big delta).
    mat_ph_delta_compress(packer, IDENTITY_MATRIX, aim, 12, 3.14159274f, 0.0f, 1.0f);
    // The sticks: the camera relative movement in the aim's frame (8 bits each, -1 to 1).
    vector camera_forward = Rfg_camera.real_orient.fvec;
    vector camera_right = Rfg_camera.real_orient.rvec;
    camera_forward.flatten_normalize_safe();
    camera_right.flatten_normalize_safe();
    const float sideways = pp->mp_current_command.sideways;
    const float forward = pp->mp_current_command.forward;
    vector movement;
    movement.y = camera_forward.y * forward + camera_right.y * sideways;
    movement.x = camera_forward.x * forward + camera_right.x * sideways;
    movement.z = camera_forward.z * forward + camera_right.z * sideways;
    vector aim_forward = aim.fvec;
    vector aim_right = aim.rvec;
    aim_forward.flatten_normalize_safe();
    aim_right.flatten_normalize_safe();
    const float aim_sideways = aim_right.x * movement.x + aim_right.y * movement.y + aim_right.z * movement.z;
    delta_compress(packer, 0.0f, aim_forward.y * movement.y + movement.x * aim_forward.x + aim_forward.z * movement.z, 8, 1.0f, 0.0f, 1.0f); // inlined
    delta_compress(packer, 0.0f, aim_sideways, 8, 1.0f, 0.0f, 1.0f); // inlined
    vector pos;
    vector vel;
    if (pp->ragdoll_state == HUMAN_RAGDOLL_STATE_NONE || pp->ragdoll_idx == -1)
    {
        pos = pp->pos;
        vel = pp->actual_velocity;
    }
    else
    {
        havok_ragdoll_get_pos(pp->ragdoll_idx, pos, RAGDOLL_BODY);
        havok_ragdoll_get_vel(pp->ragdoll_idx, vel);
    }
    // Magic numbers: 16 bits over 512 m (m/s).
    vec_delta_compress(packer, ZERO_VECTOR, pos, 16, 512.0f, 0.0f, 1.0f);
    vec_delta_compress(packer, ZERO_VECTOR, vel, 16, 512.0f, 0.0f, 1.0f);
    // Unused bits to the byte.
    packer.set_next_bits(0, 7);
    for (int i = 0; i < Client_controls_packed_bytes; i++)
    {
        packet[size++] = packed[i];
    }
    unsigned short control_flags;
    fill_control_flags(control_flags, pp);
    write_u16_be(packet, size, control_flags);
    append_control_data_to_packet(packet, size, control_flags, pp);
    if (pp->hflags.is_on_ladder)
    {
        packet[size] = pp->mp_ladder_state.l_rung;
        packet[size + 1] = pp->mp_ladder_state.l_state;
        size += 2;
    }
    packet_set_size(packet, size);
    multi_io_send(host, packet, size);
    pp->mp_flags.set_charge_weapon(0);
    pp->mp_flags.set_running_attack(0);
}

// 0x009DDB90 (:835)
// The server reads a client's controls into a state of its player (a zeroed state: no turret or vehicle, the player's
// own orientation, no camera angular velocity), equips the slot at once and adds the state to the received states. A
// packet older than 1 s (with cheat detection on) only makes the server ignore the connection's standby for 500 ms.
void multi_game_cp_process_client_controls(unsigned char* packet_data, const sender& from)
{
    multi_connection* cp = from.connection;
    if (cp == nullptr || Migration_state != MIGRATION_STATE_NONE)
    {
        return;
    }
    multi_player_state state;
    memset(&state, 0, sizeof(state));
    player* pp = multi_game_data_get_player_instance(cp->game_data);
    const unsigned int now = Clock_current_time;
    if (pp == nullptr)
    {
        return;
    }
    int offset = 0;
    state.network_time = read_u32_be(packet_data, offset);
    // Magic numbers: 1 s late, 500 ms of standby ignore.
    if (multi_core_cheat_detection_enabled() && multi_core_cheat_detection_should_check() && state.network_time < now && now - state.network_time > 1000)
    {
        cp->standby_ignore_time.set(500);
        return;
    }
    unsigned char packed[28];
    for (int i = 0; i < Client_controls_packed_bytes; i++)
    {
        packed[i] = packet_data[i + 4];
    }
    bit_packer packer(packed, Client_controls_packed_bytes);
    state.ragdoll_state = (human_ragdoll_state)packer.get_next_bits(3);
    state.knockdown_timestamp = (int)packer.get_next_bits(16);
    state.melee_primary_id = packer.get_next_bits(2);
    state.melee_secondary_id = packer.get_next_bits(2);
    state.melee_tertiary_id = packer.get_next_bits(2);
    state.melee_firearm_id = packer.get_next_bits(2);
    state.firearm_trigger_down_id = packer.get_next_bits(2);
    state.firearm_trigger_up_id = packer.get_next_bits(2);
    state.throw_attack_id = packer.get_next_bits(2);
    state.charge_weapon = packer.get_next_bits(2);
    state.running_attack = packer.get_next_bits(1);
    state.jump_id = packer.get_next_bits(2);
    const unsigned int slot = packer.get_next_bits(3);
    player_equip_item(pp, inv_item_get_item_in_slot(pp, slot), nullptr, true);
    state.weapon_magazine_ammo = (unsigned short)packer.get_next_bits(7);
    state.weapon_reserve_ammo = (unsigned short)packer.get_next_bits(9);
    mat_ph_delta_decompress(packer, state.camera_orient, IDENTITY_MATRIX, 12, 3.14159274f, 0.0f, 1.0f);
    state.camera_angular_velocity = ZERO_VECTOR2;
    state.forward = delta_decompress(packer, 0.0f, 8, 1.0f, 0.0f, 1.0f);
    state.sideways = delta_decompress(packer, 0.0f, 8, 1.0f, 0.0f, 1.0f);
    vec_delta_decompress(packer, state.position, ZERO_VECTOR, 16, 512.0f, 0.0f, 1.0f);
    vec_delta_decompress(packer, state.linear_velocity, ZERO_VECTOR, 16, 512.0f, 0.0f, 1.0f);
    state.orientation = pp->orient;
    packer.get_next_bits(7);
    // The flags follow the packed bytes (offset 4 + 25).
    offset = 4 + Client_controls_packed_bytes;
    const unsigned short control_flags = read_u16_be(packet_data, offset);
    read_control_flags(state, pp, control_flags, packet_data, offset);
    state.ladder_state.data = 0;
    if (state.is_on_ladder)
    {
        // The client sends the rung, then the state.
        state.ladder_state.l_rung = packet_data[offset];
        state.ladder_state.l_state = packet_data[offset + 1];
    }
    pp->mp_sim_state.add_state(state);
}
