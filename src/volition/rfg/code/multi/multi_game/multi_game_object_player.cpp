// Original: volition/rfg/code/multi/multi_game/multi_game_object_player.cpp
//
// Decompiled whole (multiplayer.md MP6). The replication of players: the server sends every player's state to every
// client as delta compressed updates (a client sends its own player only through its control packet, see
// multi_game_client_predict.cpp). The masks: ragdoll state 0x0001, position 0x000E (7 sizes, 16 bits absolute), camera
// angular velocity 0x0010, linear velocity 0x00E0 (7 sizes), hit points 0x0100, the flags word (weapon slot, action ids
// of human::mp_flags, ladder, reload, crouch...) 0x0200, the vehicle 0x0400, the camera orientation 0x0800 (big) or
// 0x1000 (small delta), the ladder 0x2000, the ladder rung 0x4000, the turret 0x8000. A client keeps the received states
// in the player's mp_sim_state; the registration's move (multi_game_player_process_state) plays them back. The local
// player's charge/running attack/detonation/jetpack flags are latched between the simulation and the handshake (the
// mp flags snapshot and handshake callbacks) so that a flag set for one frame reaches the transmit groups.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_object_player.h"

#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/gamewide/gamewide.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_compression.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_delta_compression.h"
#include "volition/rfg/code/multi/multi_core/include/multi_pool.inl"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_callbacks.h"
#include "volition/rfg/code/multi/multi_game/multi_game_character.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_player.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_move.h"
#include "volition/rfg/code/objects/humans/human_ragdoll.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/bit_util.h"

#include <math.h>
#include <string.h>

// ---- file-private types (rfg.pdb)
// The local player's mp flags between the frame's snapshot and handshake: per flag the value the simulation saw
// (cache), a value to set at the handshake (set_val with set) and a reset once the value went out in 3 transmit groups.
// size 0x4
struct player_mp_flag_update_info
{
    unsigned int charge_weapon_cache : 2; // 0x00:0
    unsigned int charge_weapon_set_val : 2; // 0x00:2
    unsigned int charge_weapon_set : 1; // 0x00:4
    unsigned int charge_weapon_reset : 1; // 0x00:5
    unsigned int running_attack_cache : 1; // 0x00:6
    unsigned int running_attack_set_val : 1; // 0x00:7
    unsigned int running_attack_set : 1; // 0x00:8
    unsigned int running_attack_reset : 1; // 0x00:9
    unsigned int detonated_charges_cache : 1; // 0x00:10
    unsigned int detonated_charges_set_val : 1; // 0x00:11
    unsigned int detonated_charges_set : 1; // 0x00:12
    unsigned int detonated_charges_reset : 1; // 0x00:13
    unsigned int in_air_detonate_cache : 1; // 0x00:14
    unsigned int in_air_detonate_set_val : 1; // 0x00:15
    unsigned int in_air_detonate_set : 1; // 0x00:16
    unsigned int in_air_detonate_reset : 1; // 0x00:17
    unsigned int jetpack_cache : 1; // 0x00:18
    unsigned int jetpack_set_val : 1; // 0x00:19
    unsigned int jetpack_set : 1; // 0x00:20
    unsigned int jetpack_reset : 1; // 0x00:21
};

// ---- file-static globals
// const float PI (0x01309C38) is used here too; placed in multi_game_client_predict.cpp.
// Unread in the shipped build:
static const float PI2 = 6.28318548f; // 0x01309C3C
// int Num_player_bandwidths (0x0167DE5C) is used here too; placed in multi_core_game_bandwidth.cpp.
// multi_gameplay_mode Multi_gameplay_mode (0x0167DEE0) is used here too; placed in multi_gameplay_mode.cpp.
// human_voice_persona_handle Multi_general_announcer (0x0167DEE4) is used here too; placed in multi_game_mode_bagman.cpp.
// unsigned int Local_game_data_handles[4] (0x0167E5F4) is used here too; placed in multi_join.cpp.
// The server sends a remote player's last received state instead of its simulated one (the client is authoritative
// for its own movement while it is not ragdolled).
static bool Mp_sync_player_transmitted_state = true; // 0x0167F015
// The base priority of each data set, in State_mask_player order: ragdoll, vehicle, position, linear velocity, the
// ragdoll state (again), hit points, the camera, the ladder, the camera angular velocity.
static multi_core_state_priority State_priority_base_player[9] = {MULTI_CORE_STATE_PRIORITY_EXTREME, MULTI_CORE_STATE_PRIORITY_EXTREME, MULTI_CORE_STATE_PRIORITY_HIGH, MULTI_CORE_STATE_PRIORITY_HIGH, MULTI_CORE_STATE_PRIORITY_MEDIUM, MULTI_CORE_STATE_PRIORITY_HIGH, MULTI_CORE_STATE_PRIORITY_HIGH, MULTI_CORE_STATE_PRIORITY_HIGH, MULTI_CORE_STATE_PRIORITY_HIGH}; // 0x0167F0BC
// The data sets: ragdoll... (see above); 0x6200 is the flags word with the ladder and its rung, 0x1800 the camera.
static unsigned int State_mask_player[9] = {0x400, 0x8000, 0xE, 0xE0, 1, 0x100, 0x6200, 0x1800, 0x10}; // 0x0167F0E0
// The data sets only the server may send to the player's own client (the client owns the rest of its own state).
static bool State_server_authoritative[9] = {true, true, false, false, false, true, false, false, false}; // 0x0167F104
// dynamic_uint_hash_table<remote_object*> Game_handle_to_rop_hash (0x01691B40) is used here too; placed in multi_core_delta_compression.cpp.
// dynamic_uint_hash_table<network_game_handle*> Reserved_game_to_network_hash (0x01691C18) is used here too; placed in multi_core_objects.cpp.
static multi_pool<object_state_data<player_data_bundle>, 400> Player_object_allocator; // 0x01692F10
// object_pool<rfg_mover,4000,1> rfg_moverpool (0x01F59580) is used here too; placed in rfg_mover.cpp.
static player_multi_registration* Player_mr; // 0x02FE5AE0
static player_mp_flag_update_info Player_mp_flag_udpate; // 0x02FE8D0C (sic)
// unsigned int Clock_current_time (0x02FEB538) is used here too; placed in multi.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.
// Unset in the shipped build (a debug switch): every update is a full one.
static bool Mp_player_force_full_sends; // 0x02FEB6EB
// object_pool<multi_object_backpack,76,1> multi_object_backpackpool (0x02FEDC90) is used here too; placed in multi_game_mode_king_of_the_hill.cpp.

// The state data of a player: the base state, then what each connection's client has (indexed by index_in_session).
static player_data_bundle& player_transmitted(dc_state_list* dcsl, const multi_connection* cp)
{
    return ((object_state_data<player_data_bundle>*)dcsl->m_data)->transmitted_data[cp->index_in_session];
}

static player_data_bundle& player_base(dc_state_list* dcsl)
{
    return ((object_state_data<player_data_bundle>*)dcsl->m_data)->base_data;
}

// The player of a game handle (null for any other object).
static player* player_from_handle(unsigned int game_handle)
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

// The multiplayer data of a connection's player (the getter is inlined everywhere; it reads through an entry that is
// no longer in use only to check its linger time, then returns null).
static multi_game_data* player_connection_game_data(const multi_connection* cp)
{
    const unsigned int handle = cp->game_data;
    if (Mp_game_data == nullptr || handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    multi_game_data* data = Mp_game_data->data[handle & 0xFFFF] + (handle >> 16);
    if (!data->in_use)
    {
        data->linger_time.valid();
        return nullptr;
    }
    return data;
}

// Writes a float in network order (the packet's float helpers are inlined as byte stores).
static void player_append_float(unsigned char* packet_data, unsigned short& packet_size, float value)
{
    unsigned int bits;
    memcpy(&bits, &value, sizeof(bits));
    packet_data[packet_size] = (unsigned char)(bits >> 24);
    packet_data[packet_size + 1] = (unsigned char)(bits >> 16);
    packet_data[packet_size + 2] = (unsigned char)(bits >> 8);
    packet_data[packet_size + 3] = (unsigned char)bits;
    packet_size += 4;
}

static unsigned int player_read_uint(const unsigned char* packet_data, int& packet_offset)
{
    const unsigned int value = (unsigned int)packet_data[packet_offset] << 24 | (unsigned int)packet_data[packet_offset + 1] << 16 |
                               (unsigned int)packet_data[packet_offset + 2] << 8 | (unsigned int)packet_data[packet_offset + 3];
    packet_offset += 4;
    return value;
}

static float player_read_float(const unsigned char* packet_data, int& packet_offset)
{
    const unsigned int bits = player_read_uint(packet_data, packet_offset);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void player_append_ushort(unsigned char* packet_data, unsigned short& packet_size, unsigned short value)
{
    packet_data[packet_size] = (unsigned char)(value >> 8);
    packet_data[packet_size + 1] = (unsigned char)value;
    packet_size += 2;
}

// ---- internal functions
static void clear_remote_player_flags(); // 0x0099C4C0 (:699)
static void clear_local_player_flags(); // 0x0095B030 (:717)
static void multi_game_player_clear_player_flags(); // 0x0099C520 (:783)
static object_state_data<player_data_bundle>* player_state_data_alloc(); // 0x009F4C20 (:793)
static void spew_mask(unsigned short mask_bits); // 0x0095B100 (:806)
static void multi_game_player_mp_flags_snapshot(); // 0x0095B750 (:3352)
static void multi_game_player_mp_flags_handshake(); // 0x0095B7E0 (:3366)

// 0x0099C4C0 (:699): the server clears the one-frame action flags of the remote players once they went out
// (human::mp_flags charge_weapon, running_attack, in_air_detonate, detonated_charges and jetpack: bits 20-24)
static void clear_remote_player_flags()
{
    for (int i = 0; i < World.get_num_objects<player>(); i++)
    {
        player* pp = World.get_object<player>(i);
        if (!object_is_local_player(pp))
        {
            pp->mp_flags.set_charge_weapon(0);
            pp->mp_flags.set_running_attack(0);
            pp->mp_flags.set_in_air_detonate(0);
            pp->mp_flags.set_detonated_charges(0);
            // Original: the mask 0xFE0FFFFF clears bits 20-24 and keeps jetpack (bit 25).
        }
    }
}

// 0x0095B030 (:717): the local player's latched flags are reset once they went out in 3 transmit groups
static void clear_local_player_flags()
{
    // The groups each latched flag went out in (function statics).
    static int charge_weapon_count; // 0x03017B14
    static int running_attack_count; // 0x03017B10
    static int detonated_charges_count; // 0x03017B0C
    static int in_air_detonate_count; // 0x03017B08
    if (player_get_local_player() == nullptr || Transmit_list.num_transmit_groups <= 0)
    {
        return;
    }
    // Magic number: a flag is reset after 3 groups (more than 2).
    if (Player_mp_flag_udpate.charge_weapon_cache != 0)
    {
        charge_weapon_count += Transmit_list.num_transmit_groups;
        if (charge_weapon_count > 2)
        {
            Player_mp_flag_udpate.charge_weapon_reset = 1;
            charge_weapon_count = 0;
        }
    }
    if (Player_mp_flag_udpate.running_attack_cache != 0)
    {
        running_attack_count += Transmit_list.num_transmit_groups;
        if (running_attack_count > 2)
        {
            Player_mp_flag_udpate.running_attack_reset = 1;
            running_attack_count = 0;
        }
    }
    if (Player_mp_flag_udpate.detonated_charges_cache != 0)
    {
        detonated_charges_count += Transmit_list.num_transmit_groups;
        if (detonated_charges_count > 2)
        {
            Player_mp_flag_udpate.detonated_charges_reset = 1;
            detonated_charges_count = 0;
        }
    }
    if (Player_mp_flag_udpate.in_air_detonate_cache != 0)
    {
        in_air_detonate_count += Transmit_list.num_transmit_groups;
        if (in_air_detonate_count > 2)
        {
            Player_mp_flag_udpate.in_air_detonate_reset = 1;
            in_air_detonate_count = 0;
        }
    }
}

// 0x0099C520 (:783)
static void multi_game_player_clear_player_flags()
{
    if (Multiplayer && Server)
    {
        clear_remote_player_flags();
        clear_local_player_flags();
    }
}

// 0x009F4C20 (:793): a state data, after dropping the old states when the pool is full
static object_state_data<player_data_bundle>* player_state_data_alloc()
{
    object_state_data<player_data_bundle>* data = Player_object_allocator.alloc(0);
    if (data == nullptr)
    {
        multi_core_dc_flush_old_object_states();
        data = Player_object_allocator.alloc(0);
    }
    return data;
}

// 0x0095B100 (:806): empty in the shipped build (debug output of the mask bits)
static void spew_mask(unsigned short mask_bits)
{
}

// 0x0095B110 (:841): a movement stick as its radius (8 bits, 0 to 1) and angle (8 bits, -pi to pi; forward is the sine)
void get_stick_compression(unsigned char& radius, unsigned char& angle, float forward, float sideways)
{
    // Original: the sum of the squares in double, rounded, then the root in double; the clamps compare in double.
    float r = (float)sqrt((double)(float)((double)sideways * (double)sideways + (double)forward * (double)forward));
    if ((float)((double)r - 0.0) < 0.0f)
    {
        r = 0.0f;
    }
    if ((float)((double)r - 1.0) >= 0.0f)
    {
        r = 1.0f;
    }
    radius = (unsigned char)(int)((double)r * 255.0);
    // Magic numbers: pi as a float constant widened to double (the range of compress_float).
    const float stick_angle = (float)atan2((double)forward, (double)sideways);
    angle = (unsigned char)compress_float(stick_angle, -(double)3.14159274f, (double)3.14159274f, 0.0, 1.0, 8);
}

// 0x0095B1F0 (:852)
void get_stick_decompression(float& forward, float& sideways, unsigned char radius, unsigned char angle)
{
    const float r = (float)((double)radius / 255.0);
    const float stick_angle = (float)decompress_double(angle, -(double)3.14159274f, (double)3.14159274f, 0.0, 1.0, 8);
    forward = (float)sin((double)stick_angle) * r;
    sideways = (float)cos((double)stick_angle) * r;
}

// ---- player_multi_registration

// The largest per-component distance between two vectors (the registration's thresholds compare components).
static void player_component_distances(const vector& a, const vector& b, float& dx, float& dy, float& dz)
{
    dx = fabsf(a.x - b.x);
    dy = fabsf(a.y - b.y);
    dz = fabsf(a.z - b.z);
}

static bool player_any_above(float dx, float dy, float dz, float limit)
{
    return dx > limit || dy > limit || dz > limit;
}

// 0x0097E110 (:863)
// The data sets that changed between what the connection's client has (the reference's record for it) and the
// current state. The position is compared with the client's extrapolation of its last position; the smallest of 7
// sizes that holds the difference is chosen (and the position the client will have after it goes into the velocity
// estimate). The player's own client gets only what the server owns of it; a player in a vehicle sends no position
// or velocity. Nothing changed for 1.5 s: the hit points go out (a keep-alive).
unsigned short player_multi_registration::query_possible_mask_bits(dc_state_list* current_state, dc_state_list* reference_state, multi_connection* cp) const
{
    if (current_state == nullptr)
    {
        return 0;
    }
    const player_data_bundle& cur = player_base(current_state);
    if (reference_state == nullptr || Mp_player_force_full_sends)
    {
        return 0xFFFF;
    }
    const player_data_bundle& ref = player_transmitted(reference_state, cp);
    unsigned short mask = (unsigned short)~ref.transmitted_data_set;
    vector extrapolated = ref.position;
    vector client_pos = ref.position;
    extrapolate_position(extrapolated, ref.position, ref.linear_velocity, ref.network_time, cur.network_time);
    float dx;
    float dy;
    float dz;
    player_component_distances(cur.position, extrapolated, dx, dy, dz);
    // Magic number: a position within 1/64 m of the extrapolation needs no update.
    const bool position_close = !player_any_above(dx, dy, dz, 0.015625f);
    if ((mask & 0xE) == 0 && !position_close)
    {
        if (player_any_above(dx, dy, dz, 1.0f))
        {
            if (player_any_above(dx, dy, dz, 8.0f))
            {
                mask |= 0xE;
            }
            else if (player_any_above(dx, dy, dz, 2.0f))
            {
                mask |= 0xC;
            }
            else
            {
                mask |= 0xA;
            }
        }
        else if (player_any_above(dx, dy, dz, 0.25f))
        {
            mask |= player_any_above(dx, dy, dz, 0.5f) ? 8 : 6;
        }
        else if (player_any_above(dx, dy, dz, 0.125f))
        {
            mask |= 4;
        }
        else
        {
            mask |= 2;
        }
        unsigned char buffer[64];
        bit_packer packer(buffer, 64);
        int pack_size;
        float pack_amount;
        switch (mask & 0xE)
        {
        case 2:
            pack_amount = 0.125f;
            pack_size = 4;
            break;
        case 4:
            pack_amount = 0.25f;
            pack_size = 5;
            break;
        case 6:
            pack_amount = 0.5f;
            pack_size = 6;
            break;
        case 8:
            pack_amount = 1.0f;
            pack_size = 7;
            break;
        case 0xA:
            pack_amount = 2.0f;
            pack_size = 8;
            break;
        case 0xC:
            pack_amount = 8.0f;
            pack_size = 10;
            break;
        default: // 0xE (original: compressed from the extrapolation like the others, unlike the send)
            pack_amount = 512.0f;
            pack_size = 16;
            break;
        }
        vec_delta_compress_decompress(packer, extrapolated, cur.position, client_pos, pack_size, pack_amount, 0.0f, 1.0f);
    }
    // Magic number: a velocity within 0.04 m/s needs no update.
    const bool velocity_changed = player_any_above(fabsf(cur.linear_velocity.x - ref.linear_velocity.x), fabsf(cur.linear_velocity.y - ref.linear_velocity.y),
                                                   fabsf(cur.linear_velocity.z - ref.linear_velocity.z), 0.0390625f) ||
                                  (mask & 0xE) != 0;
    if ((mask & 0xE0) == 0 && velocity_changed)
    {
        vector estimated = ref.linear_velocity;
        if ((mask & 0xE) == 0)
        {
            // A velocity goes out with a position (the smallest size).
            mask |= 2;
            unsigned char buffer[64];
            bit_packer packer(buffer, 64);
            vec_delta_compress_decompress(packer, extrapolated, cur.position, client_pos, 4, 0.125f, 0.0f, 1.0f);
        }
        estimate_velocity(estimated, ref.position, client_pos, ref.network_time, cur.network_time);
        player_component_distances(cur.linear_velocity, estimated, dx, dy, dz);
        if (player_any_above(dx, dy, dz, 5.0f))
        {
            if (player_any_above(dx, dy, dz, 20.0f))
            {
                mask |= player_any_above(dx, dy, dz, 320.0f) ? 0xE0 : 0xC0;
            }
            else
            {
                mask |= 0xA0;
            }
        }
        else if (player_any_above(dx, dy, dz, 0.625f))
        {
            mask |= player_any_above(dx, dy, dz, 1.25f) ? 0x80 : 0x60;
        }
        else if (player_any_above(dx, dy, dz, 0.078125f))
        {
            mask |= 0x40;
        }
        else
        {
            mask |= 0x20;
        }
    }
    if (fabsf(cur.hit_points_pct - ref.hit_points_pct) > 0.01f)
    {
        mask |= 0x100;
    }
    if (cur.data != ref.data)
    {
        // The flags go to the player's own client only when its dont_deform_bones bit changed.
        multi_game_data* data = player_connection_game_data(cp);
        if (data->player_network_handle != current_state->m_network_handle || ((cur.data ^ ref.data) & 0x400) != 0)
        {
            mask |= 0x200;
        }
    }
    float dp;
    float dh;
    mat_ph_get_dpdh(ref.camera_orient, cur.camera_orient, dp, dh);
    // Magic numbers: 2*pi / 8192 and pi / 16 (the small camera delta's resolution and range).
    if (fabsf(dh) >= 0.0007669904f || fabsf(dp) >= 0.0007669904f)
    {
        mask |= (fabsf(dh) >= 0.19634955f || fabsf(dp) >= 0.19634955f) ? 0x800 : 0x1000;
    }
    if (cur.vehicle_handle.get_network_handle() != 0xFFFF &&
        (fabsf(ref.camera_angular_velocity.x - cur.camera_angular_velocity.x) > 0.01f || fabsf(ref.camera_angular_velocity.y - cur.camera_angular_velocity.y) > 0.01f))
    {
        mask |= 0x10;
    }
    if (cur.ladder_handle != ref.ladder_handle)
    {
        mask |= 0x2000;
    }
    if (cur.ladder_state.data != ref.ladder_state.data)
    {
        mask |= 0x4000;
    }
    if (cur.vehicle_handle.get_network_handle() != ref.vehicle_handle.get_network_handle())
    {
        mask |= 0x400;
    }
    if (cur.turret_handle.get_network_handle() != ref.turret_handle.get_network_handle())
    {
        mask |= 0x8000;
    }
    if (cur.ragdoll_state != ref.ragdoll_state)
    {
        mask |= 1;
    }
    // Original: the connection's data is read through without a null check.
    if (player_connection_game_data(cp)->player_network_handle == current_state->m_network_handle)
    {
        mask &= 0xE700; // the player's own client: hit points, flags, vehicle, ladder, rung, turret
    }
    if (cur.vehicle_handle.get_network_handle() != 0xFFFF)
    {
        mask &= 0x7F10; // in a vehicle: no position, velocity or turret
    }
    // Magic number: 1.5 s without an update.
    if (mask == 0 && cur.network_time - ref.network_time > 1500)
    {
        mask = 0x100;
    }
    return mask;
}

// 0x0097EB00 (:1173)
void player_multi_registration::create(unsigned int game_handle) const
{
    if (!Multiplayer || !Server)
    {
        return;
    }
    multi_game_player_clear_weapon_changes(player_from_handle(game_handle));
}

// 0x009C8B70 (:1199)
// The create packet: position, heading, the multiplayer data handle, the player's network handle (from the data
// handle's entry; 0xFFFF without), the team, the player selection, then the vehicle seat (0xFF: none) and the
// vehicle's network handle. The object becomes a remote object.
void player_multi_registration::send_create_packet(const remote_object* rop, unsigned char* packet_data, unsigned short& packet_size, multi_connection* dest_conn) const
{
    // Original: the object is not checked.
    player* pp = player_from_handle(rop->m_game_handle);
    player_append_float(packet_data, packet_size, pp->pos.x);
    player_append_float(packet_data, packet_size, pp->pos.y);
    player_append_float(packet_data, packet_size, pp->pos.z);
    float heading;
    pp->orient.extract_heading(heading);
    player_append_float(packet_data, packet_size, heading);
    const unsigned int mp_data = pp->mp_data;
    player_append_float(packet_data, packet_size, *(const float*)&mp_data); // the handle's bits
    player_append_ushort(packet_data, packet_size, multi_core_network_handle_from_game_handle(rop->m_game_handle, true));
    packet_data[packet_size] = (unsigned char)pp->current_team;
    packet_size += 1;
    const unsigned int selection = mp_data == 0xFFFFFFFF ? 0 : Mp_game_data->data[mp_data & 0xFFFF][mp_data >> 16].player_selection_preference;
    packet_data[packet_size] = (unsigned char)(selection >> 24);
    packet_data[packet_size + 1] = (unsigned char)(selection >> 16);
    packet_data[packet_size + 2] = (unsigned char)(selection >> 8);
    packet_data[packet_size + 3] = (unsigned char)selection;
    packet_size += 4;
    pp->remote_obj_flags.remote_object = 1;
    if (pp->vehicle_handle == 0xFFFFFFFF)
    {
        packet_data[packet_size] = 0xFF;
        packet_size += 1;
        return;
    }
    packet_data[packet_size] = (unsigned char)pp->vehicle_seat_idx;
    packet_size += 1;
    player_append_ushort(packet_data, packet_size, multi_core_network_handle_from_game_handle(pp->vehicle_handle, true));
}

// 0x009E05A0 (:1243)
// A client makes the player of a create packet: its multiplayer data entry gets the player's network handle; the
// local player's own create only sets its data handle. A remote player is created as in multi_game_join_initialize_player
// (the human info of its team and selection, the props, standing idle, zoom off, hidden in pregame and countdown) and
// joins the mode. Its team comes from its data entry (or its human info). In a vehicle it enters its seat.
void player_multi_registration::process_create_packet(unsigned int& game_handle, unsigned short net_handle, int& packet_offset, unsigned char* packet_data) const
{
    vector pos;
    pos.x = player_read_float(packet_data, packet_offset);
    pos.y = player_read_float(packet_data, packet_offset);
    pos.z = player_read_float(packet_data, packet_offset);
    const float heading = player_read_float(packet_data, packet_offset);
    matrix orient;
    orient.set(0.0f, 0.0f, heading);
    const unsigned int mp_data = player_read_uint(packet_data, packet_offset);
    const unsigned short player_net_handle = (unsigned short)(packet_data[packet_offset] << 8 | packet_data[packet_offset + 1]);
    packet_offset += 2;
    const human_teams team_in = (human_teams)packet_data[packet_offset];
    packet_offset += 1;
    const unsigned int selection = player_read_uint(packet_data, packet_offset);
    multi_game_data* data = nullptr;
    if (Mp_game_data != nullptr)
    {
        if (mp_data != 0xFFFFFFFF)
        {
            data = Mp_game_data->data[mp_data & 0xFFFF] + (mp_data >> 16);
            if (!data->in_use)
            {
                data->linger_time.valid();
                data = nullptr;
            }
        }
    }
    if (Mp_game_data != nullptr || mp_data != 0xFFFFFFFF)
    {
        if (mp_data != 0xFFFFFFFF)
        {
            // Original: written whether or not the entry is in use.
            Mp_game_data->data[mp_data & 0xFFFF][mp_data >> 16].player_network_handle = player_net_handle;
        }
    }
    player* pp;
    if (mp_data == Local_game_data_handles[2])
    {
        pp = player_get_local_player();
        pp->mp_data = mp_data;
    }
    else
    {
        human_teams team = HUMAN_TEAM_CIVILIAN;
        if (Multiplayer && Multi_gameplay_mode >= 0 && Multi_gameplay_mode <= 7 && Multi_gameplay_mode_funcs[Multi_gameplay_mode].is_team_mode)
        {
            team = team_in;
        }
        const human_info* h_info = multi_game_charcter_get_human_info(team, selection);
        obj_human_props props(h_info, pos, orient, nullptr);
        props.set<unsigned int>(MULTI_PLAYER_HANDLE, mp_data);
        props.set<int>(MULTI_TEAM, (int)team_in);
        pp = World.create_object<player>(props, 0xFFFFFFFF);
        // Magic number: -1 keeps the move speed.
        human_set_state(pp, HS_STANDARD, HMS_IDLE, -1.0f);
        pp->update_pos_and_orient(pos, orient, true);
        human_set_state(pp, HS_STANDARD, HMS_IDLE, -1.0f);
        player_set_zoom_state(pp, PZS_OFF, false);
        object_set_visibility_plus_children(pp, Multi_match_info.match_state != MGMS_PREGAME && Multi_match_info.match_state != MGMS_COUNTDOWN);
        if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].add_player != nullptr)
        {
            Multi_gameplay_mode_funcs[Multi_gameplay_mode].add_player(pp);
        }
    }
    if (data == nullptr || data->team == (human_teams)-1)
    {
        pp->set_team(pp->h_info->default_team);
    }
    else
    {
        pp->set_team(data->team);
    }
    const unsigned char seat = packet_data[packet_offset];
    packet_offset += 1;
    if (seat != 0xFF)
    {
        const unsigned short vehicle_net = (unsigned short)(packet_data[packet_offset] << 8 | packet_data[packet_offset + 1]);
        packet_offset += 2;
        if (vehicle_net != 0xFFFF)
        {
            const unsigned int vehicle_handle = multi_core_game_handle_from_network_handle(vehicle_net, true);
            object* objp = vehicle_handle != 0xFFFFFFFF ? World.object_from_handle(vehicle_handle) : nullptr;
            if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
            {
                if (human_is_ragdolled(pp))
                {
                    human_ragdoll_stop(pp, false);
                }
                vi_enter_data enter_data;
                enter_data.specified_seat = (vehicle_seat_index)seat;
                enter_data.seat_selection_method = VI_SEAT_SELECTION_METHOD_SPECIFIED;
                enter_data.destination_seat = VSI_INVALID;
                enter_data.flags.teleport = 1;
                vehicle_interaction_request_enter(pp, (vehicle*)objp, &enter_data);
            }
        }
    }
    pp->remote_obj_flags.remote_object = 1;
    game_handle = pp->get_handle();
}

// 0x0095B290 (:1345): a player's destroy packet has no data
void player_multi_registration::send_destroy_packet(unsigned int game_handle, unsigned char* packet_data, unsigned short& packet_size) const
{
}

// 0x009C9010 (:1356): a remote player leaves its vehicle and the game
void player_multi_registration::process_destroy_packet(unsigned int game_handle, int& packet_offset, unsigned char* packet_data) const
{
    player* pp = player_from_handle(game_handle);
    if (pp == nullptr || pp->obj_flags.destroyed || object_is_local_player(pp))
    {
        return;
    }
    if (pp->vehicle_handle != 0xFFFFFFFF)
    {
        vi_exit_data exit_data;
        exit_data.flags.teleport = 1;
        vehicle_interaction_request_exit(pp, &exit_data);
    }
    multi_game_remove_player(pp);
}

// 0x009F4C50 (:1380)
void player_multi_registration::process_object_free(object* obj) const
{
    multi_game_process_remote_object_free_common(obj);
}

// 0x0099C540 (:1389)
void player_multi_registration::post_update_cleanup() const
{
    multi_game_player_clear_player_flags(); // inlined
}

// 0x009F4C60 (:1394): a client plays back the received states
void player_multi_registration::move(unsigned int game_handle) const
{
    multi_game_player_process_state(player_from_handle(game_handle));
}

// 0x009F4C90 (:1402)
// The current state: the body (a remote player on the server: its last received state, when it is not ragdolled),
// the flags word, the ladder, hit points, the camera, the sticks, the vehicle seat, the turret and the ragdoll. A
// position beyond 3 km from the origin ends the game (a broken state).
void player_multi_registration::get_state(unsigned int game_handle, dc_state_list* dcsl) const
{
    object_state_data<player_data_bundle>* data = player_state_data_alloc(); // inlined
    if (data == nullptr)
    {
        dcsl->m_data = nullptr;
        dcsl->m_size = 0;
        return;
    }
    data->initialize();
    player_data_bundle& state = data->base_data;
    state.network_time = Clock_current_time;
    dcsl->m_data = (unsigned char*)data;
    dcsl->m_size = sizeof(object_state_data<player_data_bundle>);
    // Original: the player is not checked.
    player* pp = player_from_handle(game_handle);
    multi_player_state last_received;
    unsigned int last_received_time;
    const bool has_received = pp->mp_sim_state.get_latest_state(last_received_time, &last_received); // inlined
    switch (pp->ragdoll_state)
    {
    case HUMAN_RAGDOLL_STATE_NONE:
    case HUMAN_RAGDOLL_STATE_BLEND_OUT:
    case HUMAN_RAGDOLL_STATE_FLINCH:
    case HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT:
    case HUMAN_RAGDOLL_STATE_DEACTIVATED:
        if (Multiplayer && Server && !object_is_local_player(pp) && Mp_sync_player_transmitted_state && has_received)
        {
            // The angular velocity keeps its (unset) value.
            state.position = last_received.position;
            state.orientation = last_received.orientation;
            state.linear_velocity = last_received.linear_velocity;
            break;
        }
        state.position = pp->pos;
        state.orientation = pp->orient;
        state.linear_velocity = pp->actual_velocity;
        state.angular_velocity.zero();
        break;
    case HUMAN_RAGDOLL_STATE_ACTIVE:
        havok_ragdoll_get_pos(pp->ragdoll_idx, state.position, RAGDOLL_BODY);
        state.orientation = pp->orient;
        havok_ragdoll_get_vel(pp->ragdoll_idx, state.linear_velocity);
        state.angular_velocity.zero();
        break;
    default:
        break;
    }
    const unsigned int slot = pp->desired_equipped_inv_item != nullptr ? pp->desired_equipped_inv_item->selection_slot : 7;
    state.data = 0;
    state.jump_id = pp->mp_flags.jump_id;
    state.is_on_ladder = pp->hflags.is_on_ladder;
    state.ladder_handle = pp->ladder_handle;
    state.melee_primary_id = pp->mp_flags.melee_primary_id;
    state.melee_secondary_id = pp->mp_flags.melee_secondary_id;
    state.melee_tertiary_id = pp->mp_flags.melee_tertiary_id;
    state.melee_firearm_id = pp->mp_flags.melee_firearm_id;
    state.firearm_trigger_down_id = pp->mp_flags.firearm_trigger_down_id;
    state.firearm_trigger_up_id = pp->mp_flags.firearm_trigger_up_id;
    state.throw_attack_id = pp->mp_flags.throw_attack_id;
    // The charge is the last received state's (the server does not charge a remote weapon itself).
    state.charge_weapon = has_received ? last_received.charge_weapon : 0;
    state.running_attack = pp->mp_flags.get_running_attack();
    state.equip_weapon_slot = slot & 7;
    state.dont_deform_bones = pp->hflags.dont_deform_bones;
    state.reload = human_weapons_is_reloading(pp, false) ? 1 : 0;
    state.do_sprint = pp->pflags.do_sprint;
    state.jetpack = pp->pflags.jetpack_started;
    state.is_zoomed = player_is_zoomed(pp) ? 1 : 0;
    state.is_crouched = human_move_is_crouching(pp) ? 1 : 0;
    state.detonated_charges = pp->mp_flags.get_detonated_charges();
    state.in_air_detonate = pp->mp_flags.get_in_air_detonate();
    if (pp->hflags.is_on_ladder)
    {
        state.ladder_state.data = pp->mp_ladder_state.data;
    }
    else
    {
        state.ladder_state.data = 0;
    }
    const float max_hit_points = (float)pp->max_hit_points;
    state.hit_points_pct = max_hit_points == 0.0f ? 1.0f : pp->hit_points / max_hit_points;
    state.camera_orient = pp->mp_camera_orient;
    state.camera_angular_velocity = pp->mp_camera_angular_velocity;
    state.forward = pp->mp_current_command.forward;
    state.sideways = pp->mp_current_command.sideways;
    state.vehicle_handle.clear();
    state.vehicle_seat = 0;
    object* vehicle_objp = pp->vehicle_handle != 0xFFFFFFFF ? World.object_from_handle(pp->vehicle_handle) : nullptr;
    if (vehicle_objp != nullptr && vehicle_objp->get_obj_type() == OT_VEHICLE)
    {
        vehicle* vp = (vehicle*)vehicle_objp;
        const int num_seats = vehicle_get_num_seats(vp);
        int seat = 0;
        while (seat < num_seats && vehicle_passenger_in_seat(vp, (vehicle_seat_index)seat) != pp)
        {
            seat++;
        }
        if (seat != num_seats)
        {
            state.vehicle_handle.set_network_handle(multi_core_network_handle_from_game_handle(pp->vehicle_handle, false));
            state.vehicle_seat = (unsigned char)seat;
        }
    }
    state.turret_handle.clear();
    const unsigned int turret_handle = pp->combat.reserved_turret;
    object* turret_objp = turret_handle != 0xFFFFFFFF ? World.object_from_handle(turret_handle) : nullptr;
    if (turret_objp != nullptr && turret_objp->get_obj_type() == OT_TURRET)
    {
        state.turret_handle.set_network_handle(multi_core_network_handle_from_game_handle(pp->combat.reserved_turret, true));
    }
    state.ragdoll_state = pp->ragdoll_state;
    state.knockdown_timestamp = pp->knockdown_timestamp.time_until();
    // Magic number: 3 km from the origin.
    if (fabsf(state.position.x - ZERO_VECTOR.x) > 3000.0f || fabsf(state.position.y - ZERO_VECTOR.y) > 3000.0f || fabsf(state.position.z - ZERO_VECTOR.z) > 3000.0f)
    {
        game_boot_to_main_menu(BC_GAME_MIGRATION_FAIL);
    }
}

// 0x009B0CB0 (:1581)
void player_multi_registration::free_state(dc_state_list* dcsl) const
{
    Player_object_allocator.free((object_state_data<player_data_bundle>*)dcsl->m_data);
    dcsl->m_data = nullptr;
    dcsl->m_size = 0;
}

// 0x0095B2A0 (:1589)
bool player_multi_registration::check_object_is_dirty(dc_state_list* reference_state, dc_state_list* cur_state, const remote_object* rop) const
{
    return true;
}

// 0x0095B2B0 (:1595): not used for players
bool player_multi_registration::check_full_update(dc_state_list* ref_state, multi_connection* cp) const
{
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    return false;
}

// 0x009C9080 (:1605)
// The update of the mask's data sets for one connection. The client's record starts as a copy of what it had (the
// reference's record, or the current state without a reference); each data set sent goes into it as the client will
// decompress it (the camera and its angular velocity as the exact current values). The player's own client gets zero
// position and velocity in its record for the data sets that were not sent. 1200 bytes at most.
void player_multi_registration::get_compressed_state(dc_state_list* current_state, dc_state_list* reference_state, dc_state_list* compressed_state, multi_connection* cp, multi_core_state_priority priority, unsigned short mask_bits) const
{
    player_data_bundle& cur = player_base(current_state);
    const player_data_bundle& source = reference_state != nullptr ? player_transmitted(reference_state, cp) : cur;
    player_data_bundle& client = player_transmitted(current_state, cp);
    memcpy(&client, &source, sizeof(player_data_bundle));
    if (mask_bits == 0)
    {
        compressed_state->m_size = 0;
        return;
    }
    // Original: the connection's data is read through without a null check.
    const bool own_player = player_connection_game_data(cp)->player_network_handle == current_state->m_network_handle;
    // Magic number: 0x2580 bits (1200 bytes).
    bit_packer packer(compressed_state->m_data, 1200);
    const unsigned int mask = mask_bits;
    packer.set_next_bits(mask, 16);
    if ((mask & 1) != 0)
    {
        packer.add_uint8((unsigned char)cur.ragdoll_state);
        packer.add_uint((unsigned int)cur.knockdown_timestamp);
        client.ragdoll_state = cur.ragdoll_state;
        client.knockdown_timestamp = cur.knockdown_timestamp;
        client.transmitted_data_set |= 1;
    }
    vector extrapolated = source.position;
    extrapolate_position(extrapolated, source.position, source.linear_velocity, source.network_time, cur.network_time);
    if ((mask & 0xE) == 0)
    {
        client.position = own_player ? ZERO_VECTOR : extrapolated;
    }
    else
    {
        vector result = extrapolated;
        int pack_size = 0;
        float pack_amount = 0.0f;
        const vector* from = &extrapolated;
        switch (mask & 0xE)
        {
        case 2:
            pack_amount = 0.125f;
            pack_size = 4;
            break;
        case 4:
            pack_amount = 0.25f;
            pack_size = 5;
            break;
        case 6:
            pack_amount = 0.5f;
            pack_size = 6;
            break;
        case 8:
            pack_amount = 1.0f;
            pack_size = 7;
            break;
        case 0xA:
            pack_amount = 2.0f;
            pack_size = 8;
            break;
        case 0xC:
            pack_amount = 8.0f;
            pack_size = 10;
            break;
        case 0xE:
            pack_amount = 512.0f;
            pack_size = 16;
            from = &ZERO_VECTOR;
            break;
        }
        vec_delta_compress_decompress(packer, *from, cur.position, result, pack_size, pack_amount, 0.0f, 1.0f);
        client.transmitted_data_set |= 0xE;
        client.position = result;
    }
    if ((mask & 0xE0) == 0)
    {
        if (own_player)
        {
            client.linear_velocity = ZERO_VECTOR;
        }
    }
    else
    {
        vector estimated = client.linear_velocity;
        estimate_velocity(estimated, source.position, client.position, source.network_time, cur.network_time);
        int pack_size = 0;
        float pack_amount = 0.0f;
        const vector* from = &estimated;
        switch (mask & 0xE0)
        {
        case 0x20:
            pack_amount = 0.078125f;
            pack_size = 2;
            break;
        case 0x40:
            pack_amount = 0.625f;
            pack_size = 5;
            break;
        case 0x60:
            pack_amount = 1.25f;
            pack_size = 6;
            break;
        case 0x80:
            pack_amount = 5.0f;
            pack_size = 8;
            break;
        case 0xA0:
            pack_amount = 20.0f;
            pack_size = 10;
            break;
        case 0xC0:
            pack_amount = 320.0f;
            pack_size = 13;
            break;
        case 0xE0:
            pack_amount = 1280.0f;
            pack_size = 16;
            from = &ZERO_VECTOR;
            break;
        }
        vector result = extrapolated; // the switch has no default: every case is listed
        vec_delta_compress_decompress(packer, *from, cur.linear_velocity, result, pack_size, pack_amount, 0.0f, 1.0f);
        client.transmitted_data_set |= 0xE0;
        client.linear_velocity = result;
    }
    if ((mask & 0x100) != 0)
    {
        // Magic numbers: 0 to 1 in 8 bits, linear (bias value 0.5, bias 1).
        const unsigned int packed = compress_float(cur.hit_points_pct, 0.0, 1.0, 0.5, 1.0, 8);
        packer.add_uint8((unsigned char)packed);
        client.hit_points_pct = (float)decompress_double(packed & 0xFF, 0.0, 1.0, 0.5, 1.0, 8);
        if (client.hit_points_pct > 0.99f)
        {
            client.hit_points_pct = 1.0f;
        }
        client.transmitted_data_set |= 0x100;
    }
    if ((mask & 0x200) != 0)
    {
        packer.set_next_bits(cur.data, 32);
        client.transmitted_data_set |= 0x200;
        client.data = cur.data;
    }
    if ((mask & 0x400) != 0)
    {
        packer.add_uint16(cur.vehicle_handle.get_network_handle());
        packer.add_uint8(cur.vehicle_seat);
        client.vehicle_handle = cur.vehicle_handle;
        client.vehicle_seat = cur.vehicle_seat;
        client.transmitted_data_set |= 0x400;
    }
    if ((mask & 0x1800) != 0)
    {
        // Magic numbers: pi in 12 bits (big), pi / 16 in 8 bits (small).
        if ((mask & 0x800) != 0)
        {
            mat_ph_delta_compress(packer, IDENTITY_MATRIX, cur.camera_orient, 12, 3.14159274f, 0.0f, 1.0f);
        }
        else
        {
            mat_ph_delta_compress(packer, source.camera_orient, cur.camera_orient, 8, 0.19634955f, 0.0f, 1.0f);
        }
        client.camera_orient = cur.camera_orient;
        client.transmitted_data_set |= 0x1800;
    }
    if ((mask & 0x10) == 0)
    {
        client.camera_angular_velocity = ZERO_VECTOR2;
    }
    else
    {
        Player_mr->buffer_delta_compressed_vector2(packer, 0x10, client.camera_angular_velocity, ZERO_VECTOR2, cur.camera_angular_velocity);
        client.camera_angular_velocity = cur.camera_angular_velocity;
    }
    // Original: set whether or not the angular velocity was sent.
    client.transmitted_data_set |= 0x10;
    if ((mask & 0x2000) != 0)
    {
        packer.add_uint16(multi_core_network_handle_from_game_handle(cur.ladder_handle, false));
        client.transmitted_data_set |= 0x2000;
        client.ladder_handle = cur.ladder_handle;
    }
    if ((mask & 0x4000) != 0)
    {
        packer.add_uint16(cur.ladder_state.data);
        client.transmitted_data_set |= 0x4000;
        client.ladder_state.data = cur.ladder_state.data;
    }
    if ((mask & 0x8000) != 0)
    {
        packer.add_uint16(cur.turret_handle.get_network_handle());
        client.turret_handle = cur.turret_handle;
        client.transmitted_data_set |= 0x8000;
    }
    // Magic number: a velocity within 0.04 m/s is zero.
    if (fabsf(client.linear_velocity.x - ZERO_VECTOR.x) <= 0.0390625f && fabsf(client.linear_velocity.y - ZERO_VECTOR.y) <= 0.0390625f &&
        fabsf(client.linear_velocity.z - ZERO_VECTOR.z) <= 0.0390625f)
    {
        client.linear_velocity.zero();
    }
    client.network_time = cur.network_time;
    if (fabsf(client.position.x - ZERO_VECTOR.x) > 3000.0f || fabsf(client.position.y - ZERO_VECTOR.y) > 3000.0f || fabsf(client.position.z - ZERO_VECTOR.z) > 3000.0f)
    {
        game_boot_to_main_menu(BC_GAME_MIGRATION_FAIL);
    }
    // The bytes used, rounded up (float * 0.125, ceil in double, rounded to an integer).
    compressed_state->m_size = (unsigned long)(long long)(float)ceil((double)((float)(double)packer.get_start_bit() * 0.125f));
}

// 0x009F90D0 (:2083)
// A client applies a received update: decompressed against the reference state (the client's extrapolation of it for
// the position), or against the player's own current state without a reference. The local player keeps zero position
// and velocity (its client owns them). A remote player equips the state's weapon slot and zoom at once; the sticks are
// the velocity in the player's camera frame (signed square roots of the components over the top speed). The state
// goes to the player's received states. A remote player's aim follows its camera (a test bullet to the far clip with a
// weapon, 10 m without), its charge and running attack flags come from the state and a charging grinder plays its
// effects.
bool player_multi_registration::apply_compressed_state(unsigned int game_handle, dc_state_list* reference_state, dc_state_list* compressed_state, dc_state_list** resultant_state, unsigned int network_time) const
{
    player* pp = player_from_handle(game_handle);
    if (pp == nullptr)
    {
        return false;
    }
    dc_state_list* dcsl = multi_core_dc_alloc_state_list(); // inlined
    *resultant_state = dcsl;
    if (dcsl == nullptr)
    {
        return false;
    }
    dcsl->m_data = nullptr;
    dcsl->m_reference_ackid = 0;
    dcsl->m_size = 0;
    object_state_data<player_data_bundle>* data;
    const player_data_bundle* start;
    if (reference_state == nullptr || reference_state->m_data == nullptr)
    {
        get_state(game_handle, dcsl);
        data = (object_state_data<player_data_bundle>*)dcsl->m_data;
        // Original: the new state is not checked.
        data->base_data.network_time = 0;
        start = &data->base_data;
    }
    else
    {
        const player_data_bundle& reference = player_base(reference_state);
        data = player_state_data_alloc(); // inlined
        if (data == nullptr)
        {
            multi_core_dc_free_state_list(dcsl);
            *resultant_state = nullptr;
            return false;
        }
        data->initialize();
        memcpy(&data->base_data, &reference, sizeof(player_data_bundle));
        dcsl->m_data = (unsigned char*)data;
        dcsl->m_size = sizeof(object_state_data<player_data_bundle>);
        start = &reference;
    }
    player_data_bundle& state = data->base_data;
    bit_packer packer(compressed_state->m_data, compressed_state->m_size);
    const unsigned int mask = packer.get_next_bits(16);
    if ((mask & 1) != 0)
    {
        state.ragdoll_state = (human_ragdoll_state)packer.get_uint8();
        state.knockdown_timestamp = (int)packer.get_uint();
    }
    vector extrapolated = state.position;
    extrapolate_position(extrapolated, state.position, state.linear_velocity, state.network_time, network_time);
    if ((mask & 0xE) == 0)
    {
        state.position = extrapolated;
    }
    else
    {
        int pack_size = 0;
        float pack_amount = 0.0f;
        const vector* from = &extrapolated;
        switch (mask & 0xE)
        {
        case 2:
            pack_amount = 0.125f;
            pack_size = 4;
            break;
        case 4:
            pack_amount = 0.25f;
            pack_size = 5;
            break;
        case 6:
            pack_amount = 0.5f;
            pack_size = 6;
            break;
        case 8:
            pack_amount = 1.0f;
            pack_size = 7;
            break;
        case 0xA:
            pack_amount = 2.0f;
            pack_size = 8;
            break;
        case 0xC:
            pack_amount = 8.0f;
            pack_size = 10;
            break;
        case 0xE:
            pack_amount = 512.0f;
            pack_size = 16;
            from = &ZERO_VECTOR;
            break;
        }
        vec_delta_decompress(packer, state.position, *from, pack_size, pack_amount, 0.0f, 1.0f);
    }
    if ((mask & 0xE0) != 0)
    {
        vector estimated = state.linear_velocity;
        estimate_velocity(estimated, start->position, state.position, state.network_time, network_time);
        int pack_size = 0;
        float pack_amount = 0.0f;
        const vector* from = &estimated;
        switch (mask & 0xE0)
        {
        case 0x20:
            pack_amount = 0.078125f;
            pack_size = 2;
            break;
        case 0x40:
            pack_amount = 0.625f;
            pack_size = 5;
            break;
        case 0x60:
            pack_amount = 1.25f;
            pack_size = 6;
            break;
        case 0x80:
            pack_amount = 5.0f;
            pack_size = 8;
            break;
        case 0xA0:
            pack_amount = 20.0f;
            pack_size = 10;
            break;
        case 0xC0:
            pack_amount = 320.0f;
            pack_size = 13;
            break;
        case 0xE0:
            pack_amount = 1280.0f;
            pack_size = 16;
            from = &ZERO_VECTOR;
            break;
        }
        vec_delta_decompress(packer, state.linear_velocity, *from, pack_size, pack_amount, 0.0f, 1.0f);
    }
    if (object_is_local_player(pp))
    {
        state.position = ZERO_VECTOR;
        state.linear_velocity = ZERO_VECTOR;
    }
    if ((mask & 0x100) != 0)
    {
        state.hit_points_pct = (float)decompress_double(packer.get_uint8(), 0.0, 1.0, 0.5, 1.0, 8);
        if (state.hit_points_pct > 0.99f)
        {
            state.hit_points_pct = 1.0f;
        }
    }
    if ((mask & 0x200) != 0)
    {
        state.data = packer.get_next_bits(32);
    }
    if ((mask & 0x400) != 0)
    {
        state.vehicle_handle.set_network_handle(packer.get_uint16());
        state.vehicle_seat = packer.get_uint8();
        multi_game_player_apply_vehicle_state(pp, state);
    }
    if ((mask & 0x1800) != 0)
    {
        matrix camera_orient;
        if ((mask & 0x800) != 0)
        {
            mat_ph_delta_decompress(packer, camera_orient, IDENTITY_MATRIX, 12, 3.14159274f, 0.0f, 1.0f);
        }
        else
        {
            mat_ph_delta_decompress(packer, camera_orient, start->camera_orient, 8, 0.19634955f, 0.0f, 1.0f);
        }
        state.camera_orient = camera_orient;
    }
    if ((mask & 0x10) == 0)
    {
        state.camera_angular_velocity = ZERO_VECTOR2;
    }
    else
    {
        Player_mr->apply_delta_compressed_vector2(packer, 0x10, state.camera_angular_velocity, ZERO_VECTOR2);
    }
    if ((mask & 0x2000) != 0)
    {
        state.ladder_handle = multi_core_game_handle_from_network_handle(packer.get_uint16(), true);
    }
    if ((mask & 0x4000) != 0)
    {
        state.ladder_state.data = packer.get_uint16();
    }
    if ((mask & 0x8000) != 0)
    {
        state.turret_handle.set_network_handle(packer.get_uint16());
    }
    state.network_time = network_time;
    if (!object_is_local_player(pp))
    {
        const unsigned int slot = state.equip_weapon_slot;
        inventory_item* ip = slot == 7 ? nullptr : inv_item_get_item_in_slot(pp, slot);
        player_equip_item(pp, ip, nullptr, true);
        if (state.is_zoomed != 0)
        {
            if (!player_is_zoomed(pp))
            {
                player_set_zoom_state(pp, (player_zoom_state)2, false);
            }
        }
        else if (player_is_zoomed(pp))
        {
            player_set_zoom_state(pp, PZS_OFF, false);
        }
    }
    if (fabsf(state.linear_velocity.x - ZERO_VECTOR.x) <= 0.0390625f && fabsf(state.linear_velocity.y - ZERO_VECTOR.y) <= 0.0390625f &&
        fabsf(state.linear_velocity.z - ZERO_VECTOR.z) <= 0.0390625f)
    {
        state.linear_velocity.zero();
    }
    vector velocity(state.linear_velocity.x, 0.0f, state.linear_velocity.z);
    float max_speed;
    float max_speed_base;
    player_get_max_movement_speeds(pp, max_speed, max_speed_base);
    if (max_speed > 0.0f)
    {
        const float inv_max_speed = 1.0f / max_speed;
        velocity.x = inv_max_speed * velocity.x;
        velocity.y = inv_max_speed * 0.0f;
        velocity.z = velocity.z * inv_max_speed;
        vector forward_dir = pp->mp_camera_orient.fvec;
        vector right_dir = pp->mp_camera_orient.rvec;
        forward_dir.flatten_normalize_safe();
        right_dir.flatten_normalize_safe();
        float forward = forward_dir.y * velocity.y + forward_dir.x * velocity.x + forward_dir.z * velocity.z;
        state.forward = forward;
        float sideways = right_dir.y * velocity.y + right_dir.x * velocity.x + right_dir.z * velocity.z;
        state.sideways = sideways;
        state.forward = forward > 0.0f ? sqrtf(forward) : -sqrtf(-forward);
        state.sideways = sideways > 0.0f ? sqrtf(sideways) : -sqrtf(-sideways);
    }
    else
    {
        state.forward = 0.0f;
        state.sideways = 0.0f;
    }
    if (fabsf(state.position.x - ZERO_VECTOR.x) > 3000.0f || fabsf(state.position.y - ZERO_VECTOR.y) > 3000.0f || fabsf(state.position.z - ZERO_VECTOR.z) > 3000.0f)
    {
        game_boot_to_main_menu(BC_GAME_MIGRATION_FAIL);
    }
    pp->mp_sim_state.add_state(state);
    if (!object_is_local_player(pp->get_handle()))
    {
        // Magic number: 10 m of aim without a weapon.
        float aim_distance = 10.0f;
        if (human_weapons_get_weapon(pp) != nullptr)
        {
            aim_distance = Rfg_camera.m_far_clip_dist;
        }
        weapon_fire_info fire_info;
        fire_info.end_pos.x = aim_distance * pp->mp_camera_orient.fvec.x + pp->mp_camera_pos.x;
        fire_info.end_pos.y = pp->mp_camera_pos.y + pp->mp_camera_orient.fvec.y * aim_distance;
        fire_info.end_pos.z = pp->mp_camera_pos.z + pp->mp_camera_orient.fvec.z * aim_distance;
        fire_info.start_pos = pp->mp_camera_pos;
        fire_info.w_info = nullptr;
        fire_info.hit_handle = 0xFFFFFFFF;
        if (weapons_test_bullet(fire_info, pp, true, 0.0f, vector(0.0f, 0.0f, 0.0f)))
        {
            object* hit = World.object_from_handle(fire_info.hit_handle);
            if (hit != nullptr && hit->get_obj_type() == OT_HUMAN && human_is_dead((human*)hit))
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
        weapon* wp = pp->get_equipped_weapon();
        if (pp->cur_state != HS_DEAD && wp != nullptr)
        {
            pp->mp_flags.set_charge_weapon(state.charge_weapon);
            pp->mp_flags.set_running_attack(state.running_attack);
            if (wp->w_info->weapon_class == WEAPON_CLASS_GRINDER)
            {
                const unsigned int charge = pp->mp_flags.get_charge_weapon();
                if (charge == 1 || charge == 2)
                {
                    wp->process_grinder_effects(charge == 1);
                    pp->pflags.mp_remote_player_charged_this_frame = 1;
                }
            }
        }
    }
    return true;
}

// 0x0095B2D0 (:2504)
// The byte sizes of the 5 priority levels' masks. For the player's own client (additional_data[0]) the client-owned
// data sets are dropped; a position goes with a velocity and the other way round (else both are dropped, keeping the
// ragdoll state, the camera angular velocity and the upper byte).
void player_multi_registration::query_space_for_mask(query_size_results* qs_results, void* additional_data) const
{
    if (additional_data != nullptr)
    {
        if (*(const bool*)additional_data)
        {
            for (int i = 0; i < 9; i++)
            {
                if (!State_server_authoritative[i])
                {
                    for (int level = 0; level < 5; level++)
                    {
                        qs_results[level].mask &= (unsigned short)~State_mask_player[i];
                    }
                }
            }
        }
        for (int level = 0; level < 5; level++)
        {
            const unsigned short level_mask = qs_results[level].mask;
            if (((level_mask & 0xE) != 0) != ((level_mask & 0xE0) != 0))
            {
                qs_results[level].mask = level_mask & 0xFF11;
            }
        }
    }
    for (int level = 0; level < 5; level++)
    {
        const unsigned short level_mask = qs_results[level].mask;
        int bits = (level_mask & 0x400) != 0 ? 24 : 0;
        switch (level_mask & 0xE)
        {
        case 2:
            bits += 12;
            break;
        case 4:
            bits += 15;
            break;
        case 6:
            bits += 18;
            break;
        case 8:
            bits += 21;
            break;
        case 0xA:
            bits += 24;
            break;
        case 0xC:
            bits += 30;
            break;
        case 0xE:
            bits += 48;
            break;
        }
        switch (level_mask & 0xE0)
        {
        case 0x20:
            bits += 6;
            break;
        case 0x40:
            bits += 15;
            break;
        case 0x60:
            bits += 18;
            break;
        case 0x80:
            bits += 24;
            break;
        case 0xA0:
            bits += 30;
            break;
        case 0xC0:
            bits += 39;
            break;
        case 0xE0:
            bits += 48;
            break;
        }
        if ((level_mask & 0x800) != 0)
        {
            bits += 24;
        }
        else if ((level_mask & 0x1000) != 0)
        {
            bits += 16;
        }
        if ((level_mask & 0x10) != 0)
        {
            bits += 16;
        }
        if ((level_mask & 1) != 0)
        {
            bits += 40;
        }
        if ((level_mask & 0x200) != 0)
        {
            bits += 32;
        }
        if ((level_mask & 0x2000) != 0)
        {
            bits += 16;
        }
        if ((level_mask & 0x4000) != 0)
        {
            bits += 16;
        }
        if ((level_mask & 0x100) != 0)
        {
            bits += 8;
        }
        if ((level_mask & 0x8000) != 0)
        {
            bits += 16;
        }
        // The mask word (16 bits) rounded up to bytes.
        qs_results[level].num_bytes = bits == 0 ? 0 : (unsigned short)((bits + 0x17) >> 3);
    }
}

// 0x009C9C00 (:2661)
// The data sets' priorities for one connection: the base priority plus half the frames since the set was last sent,
// plus a distance bonus (from the viewer's camera to the nearer of the current and the reference position: -2 beyond
// 20 m ... +2 within 3 m) and +2 when the player is in front of the camera (the cosine to the client's record of the
// position at least 0.99). The position is extreme in front of the camera or within 5 m (else abysmal); within 20 m in
// front of the camera everything from the position on is extreme (beyond it the camera). In a vehicle the position,
// velocity and ragdoll sets are abysmal, hit points medium, the flags high and the camera medium. The viewer's own
// player is extreme throughout, a ragdolled player's camera abysmal, the ragdoll set always extreme.
void player_multi_registration::query_space_for_priority(const remote_object* rop, dc_state_list* current_state, dc_state_list* reference_state, unsigned short possible_mask_bits, multi_connection* cp, query_size_results* qs_results) const
{
    if (possible_mask_bits == 0)
    {
        return;
    }
    player* pp = player_from_handle(rop->m_game_handle);
    player* viewer = multi_game_data_get_player_instance(cp->game_data);
    state_priority_bundle* bundle = const_cast<state_priority_bundle*>(&rop->m_state_priority_bundle[cp->index_in_session]);
    float dist2 = 0.0f;
    float facing = 1.0f;
    if (viewer != nullptr)
    {
        const player_data_bundle& cur = player_base(current_state);
        const unsigned char* ref_data = reference_state != nullptr ? reference_state->m_data : nullptr;
        vector camera_pos;
        const matrix* camera_orient;
        if (object_is_local_player(viewer))
        {
            camera_pos = Rfg_camera.real_pos;
            camera_orient = &Rfg_camera.real_orient;
        }
        else
        {
            camera_pos = viewer->mp_camera_pos;
            camera_orient = &viewer->mp_camera_orient;
        }
        dist2 = (camera_pos.x - cur.position.x) * (camera_pos.x - cur.position.x) + (camera_pos.y - cur.position.y) * (camera_pos.y - cur.position.y) +
                (camera_pos.z - cur.position.z) * (camera_pos.z - cur.position.z);
        vector client_pos = cur.position;
        if (ref_data != nullptr)
        {
            const player_data_bundle& ref = player_base(reference_state);
            const float ref_dist2 = (camera_pos.x - ref.position.x) * (camera_pos.x - ref.position.x) + (camera_pos.y - ref.position.y) * (camera_pos.y - ref.position.y) +
                                    (camera_pos.z - ref.position.z) * (camera_pos.z - ref.position.z);
            if (dist2 - ref_dist2 >= 0.0f)
            {
                dist2 = ref_dist2;
            }
            client_pos = player_transmitted(reference_state, cp).position;
        }
        facing = 0.0f;
        vector to_player(client_pos.x - camera_pos.x, client_pos.y - camera_pos.y, client_pos.z - camera_pos.z);
        if (to_player.mag_and_normalize_safe() > 0.0f && viewer != pp)
        {
            facing = camera_orient->fvec.y * to_player.y + camera_orient->fvec.x * to_player.x + camera_orient->fvec.z * to_player.z;
        }
    }
    // Magic numbers: squared distances 20, 10, 5 and 3 m.
    int bonus;
    if (dist2 > 400.0f)
    {
        bonus = -2;
    }
    else if (dist2 > 100.0f)
    {
        bonus = -1;
    }
    else if (dist2 > 25.0f)
    {
        bonus = 0;
    }
    else if (dist2 > 9.0f)
    {
        bonus = 1;
    }
    else
    {
        bonus = 2;
    }
    // Magic number: in front of the camera (cosine 0.99).
    if (facing >= 0.99f)
    {
        bonus += 2;
    }
    for (int i = 0; i < 9; i++)
    {
        state_priority& set = bundle->state_data[i];
        set.priority = (multi_core_state_priority)(State_priority_base_player[i] + (int)(set.time_since_update >> 1) + bonus);
        if (i == 2)
        {
            set.priority = (facing >= 0.0f || dist2 <= 25.0f) ? MULTI_CORE_STATE_PRIORITY_EXTREME : MULTI_CORE_STATE_PRIORITY_ABYSMAL;
        }
        if (pp->vehicle_handle == 0xFFFFFFFF)
        {
            if (facing >= 0.0f)
            {
                if (dist2 < 400.0f)
                {
                    if (i >= 2 && i <= 7)
                    {
                        set.priority = MULTI_CORE_STATE_PRIORITY_EXTREME;
                    }
                }
                else
                {
                    bundle->state_data[7].priority = MULTI_CORE_STATE_PRIORITY_EXTREME;
                }
            }
        }
        else
        {
            switch (i)
            {
            case 2:
            case 3:
            case 4:
                set.priority = MULTI_CORE_STATE_PRIORITY_ABYSMAL;
                break;
            case 5:
                set.priority = MULTI_CORE_STATE_PRIORITY_MEDIUM;
                break;
            case 6:
                set.priority = MULTI_CORE_STATE_PRIORITY_HIGH;
                break;
            case 7:
                set.priority = MULTI_CORE_STATE_PRIORITY_MEDIUM;
                break;
            default:
                break;
            }
            // Original: the age and the bonus are added a second time to every set (the in-vehicle path).
            set.priority = (multi_core_state_priority)(set.priority + (int)(set.time_since_update >> 1) + bonus);
        }
        if (pp == viewer)
        {
            set.priority = MULTI_CORE_STATE_PRIORITY_EXTREME;
        }
        if (human_is_ragdolled(pp))
        {
            bundle->state_data[7].priority = MULTI_CORE_STATE_PRIORITY_ABYSMAL;
        }
        bundle->state_data[0].priority = MULTI_CORE_STATE_PRIORITY_EXTREME;
        if ((int)set.priority < 0)
        {
            set.priority = MULTI_CORE_STATE_PRIORITY_ABYSMAL;
        }
        else if ((int)set.priority > 4)
        {
            set.priority = MULTI_CORE_STATE_PRIORITY_EXTREME;
        }
    }
    for (int level = 0; level < 5; level++)
    {
        qs_results[level].mask = 0;
        for (int i = 0; i < 9; i++)
        {
            if (level <= (int)bundle->state_data[i].priority)
            {
                qs_results[level].mask |= (unsigned short)State_mask_player[i];
            }
        }
    }
    for (int level = 0; level < 5; level++)
    {
        qs_results[level].mask &= possible_mask_bits;
    }
    // The mask query's data: the viewer's own player, a reference exists.
    bool mask_data[2] = {pp == viewer, reference_state != nullptr};
    query_space_for_mask(qs_results, mask_data);
}

// 0x0095B6E0 (:2876): a data set's age grows while it could have been sent and was not
void player_multi_registration::notify_transmitted_updated(const remote_object* rop, dc_state_list* current_state, multi_connection* recipient, unsigned short possible_mask_bits, unsigned short mask_bits_transmitted) const
{
    state_priority_bundle* bundle = const_cast<state_priority_bundle*>(&rop->m_state_priority_bundle[recipient->index_in_session]);
    for (int i = 0; i < 9; i++)
    {
        if ((State_mask_player[i] & possible_mask_bits) == 0 || (State_mask_player[i] & mask_bits_transmitted) != 0)
        {
            bundle->state_data[i].time_since_update = 0;
        }
        else
        {
            bundle->state_data[i].time_since_update++;
        }
    }
}

// 0x009CA2B0 (:2896)
void player_multi_registration::reset_state_data() const
{
    for (object_state_data<player_data_bundle>* data = Player_object_allocator.get_first(0); data != nullptr;)
    {
        data = Player_object_allocator.free(data);
    }
}

// 0x0095B740 (:2902)
void player_multi_registration::free_mp_state_data() const
{
}

// 0x0096BC70
player_multi_registration::player_multi_registration()
{
    num_masks = 0;
}

// 0x0096BD70
player_multi_registration::~player_multi_registration()
{
}

// 0x009F53C0 (:2915)
// With clear_allocators the state pool is emptied (a level change). Otherwise the pool (400 state datas) and the
// registration are made: type 2, 100 ms. The masks: name (bits), group, mask, bits, epsilon, pack threshold, bias
// value, bias.
void multi_game_object_player_init(bool clear_allocators)
{
    if (clear_allocators)
    {
        for (object_state_data<player_data_bundle>* data = Player_object_allocator.get_first(0); data != nullptr;)
        {
            data = Player_object_allocator.free(data);
        }
        return;
    }
    Player_object_allocator.init(MP_allocators_mempool);
    // Original: the allocation is not checked before the masks are written.
    player_multi_registration* registration = new (MP_allocators_mempool.alloc(sizeof(player_multi_registration), 0)) player_multi_registration();
    registration->name = "player";
    registration->update_rate = 100;
    Player_mr = registration;
    registration->add_state_mask("RAGDOLLING", "RAGDOLLING", 1, 40, 0.0f, 0.0f, 0.0f, 1.0f);
    registration->add_state_mask("4BITS_D", "POS", 2, 12, 0.0f, 0.125f, 0.0f, 1.0f);
    registration->add_state_mask("5BITS_D", "POS", 4, 15, 0.0f, 0.25f, 0.0f, 1.0f);
    registration->add_state_mask("6BITS_D", "POS", 6, 18, 0.0f, 0.5f, 0.0f, 1.0f);
    registration->add_state_mask("7BITS_D", "POS", 8, 21, 0.0f, 1.0f, 0.0f, 1.0f);
    registration->add_state_mask("8BITS_D", "POS", 0xA, 24, 0.0f, 2.0f, 0.0f, 1.0f);
    registration->add_state_mask("10BITS_D", "POS", 0xC, 30, 0.0f, 8.0f, 0.0f, 1.0f);
    registration->add_state_mask("16BITS", "POS", 0xE, 48, 0.0f, 512.0f, 0.0f, 1.0f);
    registration->add_state_mask("2BITS_D", "LVEL", 0x20, 6, 0.0f, 0.078125f, 0.0f, 1.0f);
    registration->add_state_mask("5BITS_D", "LVEL", 0x40, 15, 0.0f, 0.625f, 0.0f, 1.0f);
    registration->add_state_mask("6BITS_D", "LVEL", 0x60, 18, 0.0f, 1.25f, 0.0f, 1.0f);
    registration->add_state_mask("8BITS_D", "LVEL", 0x80, 24, 0.0f, 5.0f, 0.0f, 1.0f);
    registration->add_state_mask("10BITS_D", "LVEL", 0xA0, 30, 0.0f, 20.0f, 0.0f, 1.0f);
    registration->add_state_mask("13BITS_D", "LVEL", 0xC0, 39, 0.0f, 320.0f, 0.0f, 1.0f);
    registration->add_state_mask("16BITS", "LVEL", 0xE0, 48, 0.0f, 1280.0f, 0.0f, 1.0f);
    registration->add_state_mask("HIT_POINTS", "HIT_POINTS", 0x100, 8, 0.0f, 0.0f, 0.0f, 1.0f);
    registration->add_state_mask("VEHICLE", "VEHICLE", 0x400, 24, 0.0f, 0.0f, 0.0f, 1.0f);
    registration->add_state_mask("BIG", "CAM", 0x800, 24, 0.0f, 0.0f, 0.0f, 1.0f);
    registration->add_state_mask("SMALL", "CAM", 0x1000, 16, 0.0f, 0.0f, 0.0f, 1.0f);
    registration->add_state_mask("CAM_AVEL", "CAM_AVEL", 0x10, 16, 0.0f, 6.28318548f, 0.0f, 1.0f);
    registration->add_state_mask("LADDER", "LADDER", 0x2000, 16, 0.0f, 0.0f, 0.0f, 1.0f);
    registration->add_state_mask("LADDER_RUNG", "LADDER_RUNG", 0x4000, 16, 0.0f, 0.0f, 0.0f, 1.0f);
    registration->add_state_mask("TURRET", "TURRET", 0x8000, 16, 0.0f, 0.0f, 0.0f, 1.0f);
    multi_core_object_register_object_type(MULTI_OBJECT_TYPE_PLAYER, registration);
}

// 0x0097EB50 (:3337): the last received state's time, position and velocity
bool multi_game_player_get_last_pos_velocity_update(const player* pp, unsigned int& update_time, vector& pos, vector& linear_velocity)
{
    multi_player_state last;
    if (pp == nullptr || !pp->mp_sim_state.get_latest_state(update_time, &last)) // inlined
    {
        return false;
    }
    pos = last.position;
    linear_velocity = last.linear_velocity;
    return true;
}

// 0x0095B750 (:3352): the frame's view of the local player's flags (before the simulation)
static void multi_game_player_mp_flags_snapshot()
{
    memset(&Player_mp_flag_udpate, 0, sizeof(Player_mp_flag_udpate));
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    Player_mp_flag_udpate.charge_weapon_cache = pp->mp_flags.get_charge_weapon();
    Player_mp_flag_udpate.running_attack_cache = pp->mp_flags.get_running_attack();
    Player_mp_flag_udpate.detonated_charges_cache = pp->mp_flags.get_detonated_charges();
    Player_mp_flag_udpate.in_air_detonate_cache = pp->mp_flags.get_in_air_detonate();
    Player_mp_flag_udpate.jetpack_cache = pp->mp_flags.get_jetpack();
}

// 0x0095B7E0 (:3366): the server applies the latched resets and sets to its local player (in the handshake)
static void multi_game_player_mp_flags_handshake()
{
    if (!Multiplayer || !Server)
    {
        return;
    }
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    if (Player_mp_flag_udpate.charge_weapon_reset != 0)
    {
        pp->mp_flags.set_charge_weapon(0);
    }
    if (Player_mp_flag_udpate.charge_weapon_set != 0)
    {
        pp->mp_flags.set_charge_weapon(Player_mp_flag_udpate.charge_weapon_set_val);
    }
    if (Player_mp_flag_udpate.running_attack_reset != 0)
    {
        pp->mp_flags.set_running_attack(0);
    }
    if (Player_mp_flag_udpate.running_attack_set != 0)
    {
        pp->mp_flags.set_running_attack(Player_mp_flag_udpate.running_attack_set_val);
    }
    if (Player_mp_flag_udpate.detonated_charges_reset != 0)
    {
        pp->mp_flags.set_detonated_charges(0);
    }
    if (Player_mp_flag_udpate.detonated_charges_set != 0)
    {
        pp->mp_flags.set_detonated_charges(Player_mp_flag_udpate.detonated_charges_set_val);
    }
    if (Player_mp_flag_udpate.in_air_detonate_reset != 0)
    {
        pp->mp_flags.set_in_air_detonate(0);
    }
    if (Player_mp_flag_udpate.in_air_detonate_set != 0)
    {
        pp->mp_flags.set_in_air_detonate(Player_mp_flag_udpate.in_air_detonate_set_val);
    }
    if (Player_mp_flag_udpate.jetpack_reset != 0)
    {
        pp->mp_flags.set_jetpack(0);
    }
    if (Player_mp_flag_udpate.jetpack_set != 0)
    {
        pp->mp_flags.set_jetpack(Player_mp_flag_udpate.jetpack_set_val);
    }
}

// 0x0095B930 (:3418): the local player's flag goes to the handshake (other humans' flags are set directly)
void multi_game_player_set_mp_flag(human* hp, player_mp_flag_types flag, unsigned int val)
{
    if (!object_is_local_player(hp))
    {
        return;
    }
    switch (flag)
    {
    case PMPF_CHARGE_WEAPON:
        Player_mp_flag_udpate.charge_weapon_set_val = val & 3;
        Player_mp_flag_udpate.charge_weapon_set = 1;
        break;
    case PMPF_RUNNING_ATTACK:
        Player_mp_flag_udpate.running_attack_set_val = val & 1;
        Player_mp_flag_udpate.running_attack_set = 1;
        break;
    case PMPF_DETONATED_CHARGES:
        Player_mp_flag_udpate.detonated_charges_set_val = val & 1;
        Player_mp_flag_udpate.detonated_charges_set = 1;
        break;
    case PMPF_IN_AIR_DETONATE:
        Player_mp_flag_udpate.in_air_detonate_set_val = val & 1;
        Player_mp_flag_udpate.in_air_detonate_set = 1;
        break;
    case PMPF_JETPACK:
        Player_mp_flag_udpate.jetpack_set_val = val & 1;
        Player_mp_flag_udpate.jetpack_set = 1;
        break;
    default:
        break;
    }
}

// 0x0095BA20 (:3461)
void (*multi_game_player_get_mp_flags_snapshot())()
{
    return multi_game_player_mp_flags_snapshot;
}

// 0x0095BA30 (:3466)
void (*multi_game_player_get_mp_flags_handshake())()
{
    return multi_game_player_mp_flags_handshake;
}
