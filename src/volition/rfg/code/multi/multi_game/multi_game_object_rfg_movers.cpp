// Original: volition/rfg/code/multi/multi_game/multi_game_object_rfg_movers.cpp
//
// Decompiled whole (multiplayer.md MP5). The replication of rfg_movers (building pieces): every mover gets its
// multiplayer info (damage history, destroy causes, the client's received states); the level's movers are remote
// objects on every peer under reserved network handles, the debris a destruction creates is matched by its destruction
// uid. The server sends delta compressed position, orientation and velocities (masks: position 0x00F, orientation
// 0xC00, linear velocity 0x070, angular velocity 0x380, the fade flag 0x1000); a client steers its own simulated mover
// towards the interpolated server state with a soft keyframe and lets it sleep when close enough.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_object_rfg_movers.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkrotationf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/dynamics/keyframe/hkpkeyframeutility.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/destruction/rfg_dynamics.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/destruction/rfg_shapes.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_convert.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_compression.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_delta_compression.h"
#include "volition/rfg/code/multi/multi_core/include/multi_pool.inl"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/util/bit_util.h"

#include <math.h>
#include <string.h>
#include <xmmintrin.h>

// ---- file-static globals
static float Object_mover_update_position_sleep_tolerance = 0.15f; // 0x0167F154
static float Object_mover_update_orient_sleep_tolerance = 0.087265f; // 0x0167F158 (5 degrees)
// Unread in the shipped build:
static float Object_mover_stopped_linear_vel_tol = 0.1f; // 0x0167F15C
static float Object_mover_teleport_pos_tol = 0.15f; // 0x0167F160
static float Object_mover_teleport_orient_tol = 0.174533f; // 0x0167F164 (10 degrees)
static int Object_mover_teleport_min_time = 750; // 0x0167F168
static int Object_mover_teleport_max_time = 1500; // 0x0167F16C
// The time (ms) a client's mover must stay within the sleep tolerances before it may sleep.
static int MP_om_move_hold_time = 750; // 0x0167F170
static multi_core_state_priority State_priority_base_rfg_movers[5] = {MULTI_CORE_STATE_PRIORITY_LOW, MULTI_CORE_STATE_PRIORITY_LOW, MULTI_CORE_STATE_PRIORITY_LOW, MULTI_CORE_STATE_PRIORITY_LOW, MULTI_CORE_STATE_PRIORITY_LOW}; // 0x0167F174
static unsigned int State_mask_rfg_movers[5] = {0xF, 0xC00, 0x70, 0x380, 0x1000}; // 0x0167F188
static float Rfg_mp_max_mover_correction_distance = 6.0f; // 0x0167F19C
// The longest extrapolation of a received state (ms; multi_sim_state).
static unsigned int Max_extrap_time_ms = 0x3E8; // 0x0167F4C4
// dynamic_uint_hash_table<remote_object*> Game_handle_to_rop_hash (0x01691B40) is used here too; placed in multi_core_delta_compression.cpp.
// dynamic_uint_hash_table<network_game_handle*> Reserved_game_to_network_hash (0x01691C18) is used here too; placed in multi_core_objects.cpp.
static multi_pool<object_state_data<rfg_mover_data_bundle>, 3200> Rfg_object_mover_allocator; // 0x01692FD0
// multi_pool<multi_sim_state<multi_mover_state>,128> Rfg_mover_sim_state_allocator (0x01693010) is used here too; placed in multi_game_destruction.cpp.
// multi_pool<object_mover_error_info,256> Object_mover_error_info_allocator (0x01693050) is used here too; placed in multi_game_destruction.cpp.
static multi_pool<multi_rfg_mover_info, 4000> Rfg_mover_info_allocator; // 0x01693090
// unsigned short Playback_next_client_destruction_frame (0x02FE6C74) is used here too; placed in multi_game_destruction.cpp.
static rfg_mover_multi_registration* Rfg_mover_mr; // 0x02FE88F8
// unsigned int Clock_current_time (0x02FEB538) is used here too; placed in multi.cpp.
// int Registered_level_load_remote_object_local_create_types[10] (0x02FEB53C) is used here too; placed in multi_core_objects.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
// int Num_registered_level_load_remote_object_local_create_types (0x02FEB604) is used here too; placed in multi_core_objects.cpp.

// The state data of a mover: the base state, then what each connection's client has (indexed by index_in_session).
static rfg_mover_data_bundle& rfg_mover_transmitted(dc_state_list* dcsl, const multi_connection* cp)
{
    return ((object_state_data<rfg_mover_data_bundle>*)dcsl->m_data)->transmitted_data[cp->index_in_session];
}

static rfg_mover_data_bundle& rfg_mover_base(dc_state_list* dcsl)
{
    return ((object_state_data<rfg_mover_data_bundle>*)dcsl->m_data)->base_data;
}

// The rfg_mover of a game handle (null for any other object).
static rfg_mover* rfg_mover_from_handle(unsigned int game_handle)
{
    if (game_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(game_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_MOVER || objp->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        return nullptr;
    }
    return (rfg_mover*)objp;
}

// ---- internal functions
static bool get_rfg_mover_center_of_mass_position(rfg_mover* rmp, vector& world_cm_position); // 0x0097F070 (:345)
static bool get_rfg_mover_world_position(rfg_mover* rmp, const vector& cm_position, const matrix& cm_orient, vector& world_position); // 0x0097F110 (:355)
static bool is_pos_mask_ref_delta_rfg_movers(unsigned int mask); // 0x0095CAD0 (:382)
static int get_pos_num_bits_from_mask_rfg_movers(unsigned int mask); // 0x0095CAE0 (:398)
static int get_orient_num_bits_from_mask_rfg_movers(unsigned int mask); // 0x0095CB60 (:421)
static int get_linear_vel_num_bits_from_mask_rfg_movers(unsigned int mask); // 0x0095CBA0 (:436)
static int get_angular_vel_num_bits_from_mask_rfg_movers(unsigned int mask); // 0x0095CC80 (:455)
static int get_state_num_bits_from_mask_rfg_movers(unsigned int mask); // 0x0095CCF0 (:474)
static object_state_data<rfg_mover_data_bundle>* rfg_mover_state_data_alloc(); // 0x009F5E10 (:483)
static void free_one_mp_state_data(rfg_mover* rmp); // 0x009B0E80 (:535)
static rfg_mover* rfg_mover_find_by_network_handle_and_destruction_uid(unsigned short net_handle, unsigned int destruction_uid); // 0x009CC4A0 (:555)
static unsigned int get_position_no_ref_mask_bits(const rfg_mover_data_bundle& base_data); // 0x0095CD00 (:579)
static unsigned int get_linear_velocity_no_ref_mask_bits(const rfg_mover_data_bundle& base_data); // 0x0095CE40 (:603)
static unsigned int get_angular_velocity_no_ref_mask_bits(const rfg_mover_data_bundle& base_data); // 0x0095CEA0 (:617)
static unsigned int get_no_ref_mask_bits(const rfg_mover_data_bundle& base_data); // 0x0095CF00 (:631)
static unsigned short query_possible_mask_bits_base(const rfg_mover_data_bundle& cur_data, const rfg_mover_data_bundle& ref_data); // 0x0095CFC0 (:653)
static void havok_custom_soft_keyframe(const rfg_mover_soft_key_info& soft_key_info, hkpKeyFrameUtility::AccelerationInfo& accelInfo, float deltaTime, float invDeltaTime, hkpRigidBody* body); // 0x0099C9E0 (:1053)
static bool rfg_mover_should_create_level_init_remote_object(const rfg_mover* rmp); // 0x0095D7D0 (:2237)

// 0x0097F070 (:345): the shape's center of mass in the world (false while the mover's resources are not loaded)
// LTCG: rmp in ESI, world_cm_position in EDI (the port has no register convention).
static bool get_rfg_mover_center_of_mass_position(rfg_mover* rmp, vector& world_cm_position)
{
    if (!rmp->rdd.is_loaded() || rmp->rdd.get_data()->shape == nullptr)
    {
        return false;
    }
    const vector offset = rmp->rdd.get_data()->shape->center_of_mass.copy_unrotate(rmp->orient);
    world_cm_position.x = offset.x + rmp->pos.x;
    world_cm_position.y = rmp->pos.y + offset.y;
    world_cm_position.z = rmp->pos.z + offset.z;
    return true;
}

// 0x0097F110 (:355): the mover's position for a center of mass pose
// LTCG: rmp in EAX, cm_orient in ECX, cm_position in ESI, world_position in EDI (the port has no register convention).
static bool get_rfg_mover_world_position(rfg_mover* rmp, const vector& cm_position, const matrix& cm_orient, vector& world_position)
{
    if (!rmp->rdd.is_loaded() || rmp->rdd.get_data()->shape == nullptr)
    {
        return false;
    }
    const vector offset = rmp->rdd.get_data()->shape->center_of_mass.copy_unrotate(cm_orient);
    world_position.x = cm_position.x - offset.x;
    world_position.y = cm_position.y - offset.y;
    world_position.z = cm_position.z - offset.z;
    return true;
}

// Position masks 5-9 are relative to the mover's reference position (2 to 13 bits per component), 10-14 deltas to the
// client's position, 15 absolute (16 bits).

// 0x0095CAD0 (:382)
static bool is_pos_mask_ref_delta_rfg_movers(unsigned int mask)
{
    return (mask & 0xF) - 5 < 5;
}

// 0x0095CAE0 (:398)
static int get_pos_num_bits_from_mask_rfg_movers(unsigned int mask)
{
    switch (mask & 0xF)
    {
    case 5:
        return 6;
    case 6:
    case 0xB:
        return 15;
    case 7:
    case 0xC:
        return 24;
    case 8:
    case 0xD:
        return 30;
    case 9:
    case 0xE:
        return 39;
    case 0xA:
        return 9;
    case 0xF:
        return 48;
    default:
        return 0;
    }
}

// 0x0095CB60 (:421)
static int get_orient_num_bits_from_mask_rfg_movers(unsigned int mask)
{
    switch (mask & 0xC00)
    {
    case 0x400:
        return 15;
    case 0x800:
        return 24;
    case 0xC00:
        return 30;
    default:
        return 0;
    }
}

// 0x0095CBA0 (:436)
static int get_linear_vel_num_bits_from_mask_rfg_movers(unsigned int mask)
{
    switch (mask & 0x70)
    {
    case 0x10:
        return 9;
    case 0x20:
        return 15;
    case 0x30:
    case 0x60:
        return 24;
    case 0x40:
        return 30;
    case 0x50:
        return 39;
    case 0x70:
        return 48;
    default:
        return 0;
    }
}

// 0x0095CC80 (:455)
static int get_angular_vel_num_bits_from_mask_rfg_movers(unsigned int mask)
{
    switch (mask & 0x380)
    {
    case 0x80:
        return 9;
    case 0x100:
        return 15;
    case 0x180:
    case 0x300:
        return 24;
    case 0x200:
        return 30;
    case 0x280:
        return 39;
    case 0x380:
        return 48;
    default:
        return 0;
    }
}

// 0x0095CCF0 (:474)
static int get_state_num_bits_from_mask_rfg_movers(unsigned int mask)
{
    return (mask >> 12) & 1;
}

// 0x009F5E10 (:483): when the pool is empty, old delta compression states are flushed once
static object_state_data<rfg_mover_data_bundle>* rfg_mover_state_data_alloc()
{
    object_state_data<rfg_mover_data_bundle>* data = Rfg_object_mover_allocator.alloc(0);
    if (data == nullptr)
    {
        multi_core_dc_flush_old_object_states();
        data = Rfg_object_mover_allocator.alloc(0);
    }
    return data;
}

// 0x0099C7A0 (:500)
multi_sim_state<multi_mover_state>* rfg_mover_allocate_sim_mover_state()
{
    multi_sim_state<multi_mover_state>* sim_state = Rfg_mover_sim_state_allocator.alloc(0);
    if (sim_state != nullptr)
    {
        sim_state->reset();
    }
    return sim_state;
}

// 0x0099C7D0 (:511)
object_mover_error_info* object_mover_allocate_error_info()
{
    object_mover_error_info* error_info = Object_mover_error_info_allocator.alloc(0);
    if (error_info != nullptr)
    {
        error_info->hold_time.invalidate();
        error_info->hold_count = 0;
    }
    return error_info;
}

// 0x009B0E60 (:528)
void object_mover_free_error_info(object_mover_error_info* error_info)
{
    if (error_info != nullptr)
    {
        Object_mover_error_info_allocator.free(error_info);
    }
}

// 0x009B0E80 (:535): the client's received states and the error info
// LTCG: rmp in ESI (the port has no register convention).
static void free_one_mp_state_data(rfg_mover* rmp)
{
    if (rmp->mp_info->sim_state != nullptr)
    {
        Rfg_mover_sim_state_allocator.free(rmp->mp_info->sim_state);
        rmp->mp_info->sim_state = nullptr;
    }
    if (rmp->m_mp_error_info != nullptr)
    {
        Object_mover_error_info_allocator.free(rmp->m_mp_error_info);
        rmp->m_mp_error_info = nullptr;
    }
}

// 0x009B0ED0 (:547): every rfg_mover's (free_one_mp_state_data inlined)
void rfg_mover_free_mp_state_data()
{
    const int count = World.get_num_objects<rfg_mover>();
    for (int i = 0; i < count; i++)
    {
        free_one_mp_state_data(World.get_object<rfg_mover>(i));
    }
}

// 0x009CC4A0 (:555): by destruction uid among the rfg_movers, else by network handle
static rfg_mover* rfg_mover_find_by_network_handle_and_destruction_uid(unsigned short net_handle, unsigned int destruction_uid)
{
    if (destruction_uid != 0xFFFFFFFF)
    {
        const int count = World.get_num_objects<rfg_mover>();
        for (int i = 0; i < count; i++)
        {
            rfg_mover* rmp = World.get_object<rfg_mover>(i);
            if (rmp->destruction_uid == destruction_uid)
            {
                return rmp;
            }
        }
    }
    const unsigned int handle = multi_core_game_handle_from_network_handle(net_handle, false);
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    return rfg_mover_from_handle(handle);
}

// 0x0095CD00 (:579): mask 5 when the mover is within its smallest reference delta, else the smallest reference or delta
// mask that holds it, else absolute (15)
// LTCG: base_data in EDI (the port has no register convention).
static unsigned int get_position_no_ref_mask_bits(const rfg_mover_data_bundle& base_data)
{
    vector reference_pos;
    object_mover_get_reference_pos(reference_pos, base_data.position_ref_index);
    const float threshold = Rfg_mover_mr->get_state_mask_pack_threshold(5);
    if ((float)fabs((double)reference_pos.x - (double)base_data.position.x) <= threshold && (float)fabs((double)reference_pos.y - (double)base_data.position.y) <= threshold && (float)fabs((double)reference_pos.z - (double)base_data.position.z) <= threshold)
    {
        return 5;
    }
    const unsigned int mask = Rfg_mover_mr->get_vector_mask(base_data.position, reference_pos, 5, 6, 7, 8, 9, 0xFFFFFFFF);
    return mask != 0 ? mask : 0xF;
}

// 0x0095CE40 (:603)
static unsigned int get_linear_velocity_no_ref_mask_bits(const rfg_mover_data_bundle& base_data)
{
    const unsigned int mask = Rfg_mover_mr->get_vector_mask(base_data.linear_velocity, ZERO_VECTOR, 0x60, 0x70, 0xFFFFFFFF);
    return mask != 0 ? mask : 0x60;
}

// 0x0095CEA0 (:617)
static unsigned int get_angular_velocity_no_ref_mask_bits(const rfg_mover_data_bundle& base_data)
{
    const unsigned int mask = Rfg_mover_mr->get_vector_mask(base_data.angular_velocity, ZERO_VECTOR, 0x300, 0x380, 0xFFFFFFFF);
    return mask != 0 ? mask : 0x300;
}

// 0x0095CF00 (:631): everything for a client without a reference state (the orientation and the fade flag always)
// LTCG: base_data in EAX (the port has no register convention).
static unsigned int get_no_ref_mask_bits(const rfg_mover_data_bundle& base_data)
{
    unsigned int mask = get_position_no_ref_mask_bits(base_data);
    mask = (mask & 0xFFFFFF8F) | 0x1F80;
    mask |= get_linear_velocity_no_ref_mask_bits(base_data); // inlined
    mask &= 0xFFFFFC7F;
    return get_angular_velocity_no_ref_mask_bits(base_data) | mask; // inlined
}

// 0x0095CFC0 (:653): what changed from the client's state (ref_data) to the current one (cur_data)
// LTCG: cur_data in EDI, ref_data in ESI (the port has no register convention).
static unsigned short query_possible_mask_bits_base(const rfg_mover_data_bundle& cur_data, const rfg_mover_data_bundle& ref_data)
{
    const unsigned int pos_mask = Rfg_mover_mr->get_vector_mask(cur_data.position, ref_data.position, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0xFFFFFFFF);
    const unsigned int orient_mask = Rfg_mover_mr->get_orient_mask(cur_data.orientation, ref_data.orientation, 0xC00, 0x400, 0x800);
    unsigned short mask = (unsigned short)(orient_mask | pos_mask);
    mask |= (unsigned short)Rfg_mover_mr->get_vector_mask(cur_data.linear_velocity, ref_data.linear_velocity, 0x10, 0x20, 0x30, 0x40, 0x50, 0x70, 0xFFFFFFFF);
    mask |= (unsigned short)Rfg_mover_mr->get_vector_mask(cur_data.angular_velocity, ref_data.angular_velocity, 0x80, 0x100, 0x180, 0x200, 0x280, 0x380, 0xFFFFFFFF);
    if (cur_data.fade_delete != ref_data.fade_delete)
    {
        mask |= 0x1000;
    }
    return mask;
}

// 0x0095D0C0 (:695): nothing for a fixed mover; the data sets the client never received are always possible
unsigned short rfg_mover_multi_registration::query_possible_mask_bits(dc_state_list* current_state, dc_state_list* reference_state, multi_connection* cp) const
{
    const rfg_mover_data_bundle& cur = rfg_mover_base(current_state);
    if (cur.movement_fixed)
    {
        return 0;
    }
    if (reference_state == nullptr)
    {
        return (unsigned short)get_no_ref_mask_bits(cur);
    }
    const rfg_mover_data_bundle& client = rfg_mover_transmitted(reference_state, cp);
    const unsigned short never_sent = (unsigned short)(~client.transmitted_data_set & 0x1FFF);
    return never_sent | query_possible_mask_bits_base(cur, client);
}

// 0x0097F1B0 (:724)
void rfg_mover_multi_registration::create(unsigned int game_handle) const
{
    rfg_mover* rmp = rfg_mover_from_handle(game_handle);
    if (rmp != nullptr)
    {
        rmp->remote_obj_flags.remote_object = 1;
    }
}

// Appends a float in network byte order (PACKET_SWAP_FLOAT, as rfg.exe calls it here).
static void rfg_mover_write_float(unsigned char* packet_data, unsigned short& packet_size, float value)
{
    const float swapped = PACKET_SWAP_FLOAT(value);
    memcpy(packet_data + packet_size, &swapped, 4);
    packet_size += 4;
}

// 0x0097F1E0 (:739)
// The destruction uid (32 bits big endian), the position and the orientation quaternion, and with a Havok body its
// linear and angular velocity (floats in network order).
void rfg_mover_multi_registration::send_create_packet(const remote_object* rop, unsigned char* packet_data, unsigned short& packet_size, multi_connection* dest_conn) const
{
    rfg_mover* rmp = rfg_mover_from_handle(rop->m_game_handle);
    if (rmp == nullptr)
    {
        return;
    }
    const unsigned int uid = rmp->destruction_uid;
    packet_data[packet_size] = (unsigned char)(uid >> 24);
    packet_data[packet_size + 1] = (unsigned char)(uid >> 16);
    packet_data[packet_size + 2] = (unsigned char)(uid >> 8);
    packet_data[packet_size + 3] = (unsigned char)uid;
    packet_size += 4;
    const quaternion q(rmp->orient);
    rfg_mover_write_float(packet_data, packet_size, rmp->pos.x);
    rfg_mover_write_float(packet_data, packet_size, rmp->pos.y);
    rfg_mover_write_float(packet_data, packet_size, rmp->pos.z);
    rfg_mover_write_float(packet_data, packet_size, q.x);
    rfg_mover_write_float(packet_data, packet_size, q.y);
    rfg_mover_write_float(packet_data, packet_size, q.z);
    rfg_mover_write_float(packet_data, packet_size, q.w);
    if (rmp->havok_handle != 0xFFFFFFFF)
    {
        vector velocity;
        havok_body_get_linear_velocity(rmp->havok_handle, velocity);
        rfg_mover_write_float(packet_data, packet_size, velocity.x);
        rfg_mover_write_float(packet_data, packet_size, velocity.y);
        rfg_mover_write_float(packet_data, packet_size, velocity.z);
        havok_body_get_angular_velocity(rmp->havok_handle, velocity);
        rfg_mover_write_float(packet_data, packet_size, velocity.x);
        rfg_mover_write_float(packet_data, packet_size, velocity.y);
        rfg_mover_write_float(packet_data, packet_size, velocity.z);
    }
}

// A big endian float read with an integer byte swap (rfg.exe inlines it here; no x87 load).
static float rfg_mover_read_float(const unsigned char* p)
{
    const unsigned int bits = ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8) | p[3];
    float value;
    memcpy(&value, &bits, 4);
    return value;
}

// 0x009CC520 (:769)
// The client's own mover (made by its own destruction, or the level's) is found by the destruction uid; it gets its
// received states and error info. The position is skipped, the orientation is read but not used, and the velocities
// are left unread.
void rfg_mover_multi_registration::process_create_packet(unsigned int& game_handle, unsigned short net_handle, int& packet_offset, unsigned char* packet_data) const
{
    const unsigned char* p = packet_data + packet_offset;
    const unsigned int uid = ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8) | p[3];
    packet_offset += 16;
    quaternion q;
    q.x = rfg_mover_read_float(packet_data + packet_offset);
    q.y = rfg_mover_read_float(packet_data + packet_offset + 4);
    q.z = rfg_mover_read_float(packet_data + packet_offset + 8);
    q.w = rfg_mover_read_float(packet_data + packet_offset + 12);
    packet_offset += 16;
    matrix orient;
    q.extract_matrix(orient);
    rfg_mover* rmp = rfg_mover_find_by_network_handle_and_destruction_uid(net_handle, uid);
    if (rmp == nullptr)
    {
        return;
    }
    rmp->mp_info->sim_state = rfg_mover_allocate_sim_mover_state(); // inlined
    object_mover_error_info* error_info = object_mover_allocate_error_info(); // inlined
    rmp->remote_obj_flags.remote_object = 1;
    rmp->m_mp_error_info = error_info;
    game_handle = rmp->get_handle();
}

// 0x0097F660 (:801): the active destruction frame (big endian)
void rfg_mover_multi_registration::send_destroy_packet(unsigned int game_handle, unsigned char* packet_data, unsigned short& packet_size) const
{
    if (game_handle != 0xFFFFFFFF)
    {
        World.object_from_handle(game_handle); // unused
    }
    const unsigned short frame_hi = rfg_dyn_get_active_destruction_frame();
    const unsigned short frame_lo = rfg_dyn_get_active_destruction_frame();
    packet_data[packet_size] = (unsigned char)(frame_hi >> 8);
    packet_data[packet_size + 1] = (unsigned char)frame_lo;
    packet_size += 2;
}

// 0x0097F6B0 (:814): a client applies the destroy once its destruction playback has reached that frame
bool rfg_mover_multi_registration::allow_destroy_packet(unsigned int game_handle, int& packet_offset, unsigned char* packet_data) const
{
    if (game_handle != 0xFFFFFFFF)
    {
        World.object_from_handle(game_handle); // unused
    }
    const unsigned short destruction_frame = (unsigned short)((packet_data[packet_offset] << 8) | packet_data[packet_offset + 1]);
    packet_offset += 2;
    if (Playback_next_client_destruction_frame == 0)
    {
        return true;
    }
    return (unsigned short)(Playback_next_client_destruction_frame - 1) >= destruction_frame;
}

// 0x0097F720 (:832)
void rfg_mover_multi_registration::process_destroy_packet(unsigned int game_handle, int& packet_offset, unsigned char* packet_data) const
{
    rfg_mover* rmp = rfg_mover_from_handle(game_handle);
    if (rmp != nullptr)
    {
        rmp->remote_obj_flags.client_destroy_permission = 1;
        World.destroy_object(rmp, false);
    }
}

// 0x009F5E40 (:844)
// Original: the object is not checked to be an rfg_mover before its fields are read (a null object crashes).
void rfg_mover_multi_registration::process_object_free(object* obj) const
{
    rfg_mover* rmp = nullptr;
    if (obj != nullptr && obj->get_obj_type() == OT_MOVER && obj->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        rmp = (rfg_mover*)obj;
    }
    free_one_mp_state_data(rmp); // inlined
    multi_game_process_remote_object_free_common(obj);
}

// 0x0095D130 (:862)
void rfg_mover_multi_registration::post_update_cleanup() const
{
}

// 0x0099C7F0 (:973): the distance and angle between the client's mover and the server state at the current network
// time (multi_sim_state::obtain_state inlined)
bool rfg_mover_get_client_sync_info(rfg_mover* rmp, float& pos_error, float& angle_error)
{
    remote_object* rop = Remote_objects_list;
    if (rop == nullptr)
    {
        return false;
    }
    while (rop->m_game_handle != rmp->get_handle())
    {
        rop = rop->next;
        if (rop == Remote_objects_list || rop == nullptr)
        {
            return false;
        }
    }
    multi_mover_state state;
    bool exceeded_extrap_time;
    if (!rmp->mp_info->sim_state->obtain_state(Clock_current_time, &state, exceeded_extrap_time))
    {
        return false;
    }
    vector pos;
    matrix orient;
    havok_body_get_pos_orient(rmp->havok_handle, pos, orient);
    pos_error = vec_dist(pos, state.position);
    const matrix delta = orient * state.orientation.copy_transpose();
    vector axis;
    delta.extract_axis_and_angle(axis, angle_error);
    return true;
}

// 0x0099C9E0 (:1053)
// RFG's soft keyframe: beyond the allowed distance the body is put at the target at once; then the velocities move
// towards the target's (velocity factors * dt) and towards the target pose (position factors), each change limited by
// the maximum acceleration * dt. SSE as rfg.exe computes it; invDeltaTime is unused.
// LTCG: soft_key_info in ECX, body in EDI, accelInfo and deltaTime on the stack (the port has no register convention).
static void havok_custom_soft_keyframe(const rfg_mover_soft_key_info& soft_key_info, hkpKeyFrameUtility::AccelerationInfo& accelInfo, float deltaTime, float invDeltaTime, hkpRigidBody* body)
{
    const __m128 dt = _mm_set1_ps(deltaTime);
    __m128 d = _mm_sub_ps(soft_key_info.target_center_of_mass_position.m_quad, soft_key_info.cur_center_of_mass_position.m_quad);
    // |d|^2 as (y + x) + z
    const __m128 d2 = _mm_mul_ps(d, d);
    const float len2 = _mm_cvtss_f32(_mm_add_ps(_mm_add_ps(_mm_shuffle_ps(d2, d2, 0x55), _mm_shuffle_ps(d2, d2, 0x00)), _mm_shuffle_ps(d2, d2, 0xAA)));
    const float max_dist2 = accelInfo.m_maxAllowedDistance * accelInfo.m_maxAllowedDistance;
    if (len2 > max_dist2)
    {
        d = _mm_setzero_ps();
        body->setPositionAndRotation(soft_key_info.target_pos_world, soft_key_info.target_orient);
        body->setAngularVelocity(soft_key_info.target_angular_velocity);
        body->setLinearVelocity(soft_key_info.target_linear_velocity);
    }
    // The rotation from the current orientation to the target's: (cur x tgt - tgt.w cur + cur.w tgt, cur . tgt), times 2,
    // negated when its w is negative.
    const __m128 cur = soft_key_info.cur_orient.m_vec.m_quad;
    const __m128 tgt = soft_key_info.target_orient.m_vec.m_quad;
    __m128 xyz = _mm_sub_ps(_mm_mul_ps(_mm_shuffle_ps(tgt, tgt, 0xC9), cur), _mm_mul_ps(_mm_shuffle_ps(cur, cur, 0xC9), tgt));
    xyz = _mm_shuffle_ps(xyz, xyz, 0xC9);
    xyz = _mm_sub_ps(xyz, _mm_mul_ps(_mm_shuffle_ps(tgt, tgt, 0xFF), cur));
    xyz = _mm_add_ps(xyz, _mm_mul_ps(_mm_shuffle_ps(cur, cur, 0xFF), tgt));
    const __m128 ct = _mm_mul_ps(cur, tgt);
    const __m128 ct_pairs = _mm_add_ps(_mm_shuffle_ps(ct, ct, 0x4E), ct);
    const __m128 dot = _mm_add_ps(_mm_shuffle_ps(ct_pairs, ct_pairs, 0xB1), ct_pairs);
    const __m128 diff = _mm_shuffle_ps(xyz, _mm_unpackhi_ps(xyz, dot), 0xC4);
    __m128 rotation = _mm_mul_ps(_mm_set1_ps(2.0f), diff);
    if (0.0f > _mm_cvtss_f32(_mm_shuffle_ps(diff, diff, 0xFF)))
    {
        rotation = _mm_xor_ps(rotation, _mm_castsi128_ps(_mm_set1_epi32((int)0x80000000)));
    }
    const __m128 linear_velocity = body->getLinearVelocity().m_quad;
    __m128 linear_change = _mm_mul_ps(_mm_sub_ps(soft_key_info.target_linear_velocity.m_quad, linear_velocity), _mm_mul_ps(accelInfo.m_linearVelocityFactor.m_quad, dt));
    linear_change = _mm_add_ps(linear_change, _mm_mul_ps(accelInfo.m_linearPositionFactor.m_quad, d));
    const __m128 angular_velocity = body->getAngularVelocity().m_quad;
    __m128 angular_change = _mm_mul_ps(_mm_sub_ps(soft_key_info.target_angular_velocity.m_quad, angular_velocity), _mm_mul_ps(accelInfo.m_angularVelocityFactor.m_quad, dt));
    angular_change = _mm_add_ps(angular_change, _mm_mul_ps(accelInfo.m_angularPositionFactor.m_quad, rotation));
    // Each change limited to max acceleration * dt (compared in double; the scale 1 / |change| * max in float).
    struct limit_change
    {
        static __m128 apply(__m128 change, float max_acceleration, float delta_time)
        {
            const float max_change = (float)((double)max_acceleration * (double)delta_time);
            const __m128 c2 = _mm_mul_ps(change, change);
            const float change2 = _mm_cvtss_f32(_mm_add_ps(_mm_add_ps(_mm_shuffle_ps(c2, c2, 0x55), _mm_shuffle_ps(c2, c2, 0x00)), _mm_shuffle_ps(c2, c2, 0xAA)));
            if ((double)change2 > (double)max_change * (double)max_change)
            {
                const float inv_len = _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(1.0f), _mm_sqrt_ss(_mm_set_ss(change2))));
                const float scale = inv_len * max_change;
                change = _mm_mul_ps(_mm_set1_ps(scale), change);
            }
            return change;
        }
    };
    linear_change = limit_change::apply(linear_change, accelInfo.m_maxLinearAcceleration, deltaTime);
    angular_change = limit_change::apply(angular_change, accelInfo.m_maxAngularAcceleration, deltaTime);
    hkVector4f new_angular_velocity;
    new_angular_velocity.m_quad = _mm_add_ps(angular_velocity, angular_change);
    hkVector4f new_linear_velocity;
    new_linear_velocity.m_quad = _mm_add_ps(linear_velocity, linear_change);
    body->setAngularVelocity(new_angular_velocity);
    body->setLinearVelocity(new_linear_velocity);
}

// 0x0097F760 (:1155): true once the mover has been within the sleep tolerances for MP_om_move_hold_time; any time
// outside them restarts the hold
bool object_mover_should_sleep(object_mover* omp, float pos_error, float angle_error)
{
    object_mover_error_info* error_info = omp->m_mp_error_info;
    if (!error_info->hold_time.valid())
    {
        error_info->hold_time.set(0);
        error_info->hold_count++;
    }
    if (error_info->hold_time.time_since() >= MP_om_move_hold_time)
    {
        if (pos_error < Object_mover_update_position_sleep_tolerance && angle_error < Object_mover_update_orient_sleep_tolerance)
        {
            return true;
        }
        error_info->hold_time.set(0);
        error_info->hold_count++;
    }
    return false;
}

// 0x0099CD30 (:1185)
// A client's mover follows the server state at the current network time (at most every 100 ms): a mover marked for
// fading starts its fade; otherwise, unless it may sleep, its velocities are set to the state's and a soft keyframe
// (position and velocity factors min(1 / frame time, 4), correction distance 6 m) steers its center of mass towards
// the state's.
void rfg_mover_multi_registration::move(unsigned int game_handle) const
{
    rfg_mover* rmp = rfg_mover_from_handle(game_handle);
    if (rmp == nullptr || rmp->mp_info->sim_state == nullptr || rmp->havok_handle == 0xFFFFFFFF || rmp->get_move_type() == RFGM_FIXED)
    {
        return;
    }
    multi_sim_state<multi_mover_state>* sim_state = rmp->mp_info->sim_state;
    if (!sim_state->should_apply_correction())
    {
        return;
    }
    sim_state->set_correction_timer(100); // Magic number: 100 ms between corrections
    multi_mover_state state;
    bool exceeded_extrap_time;
    if (!sim_state->obtain_state(Clock_current_time, &state, exceeded_extrap_time))
    {
        return;
    }
    if (state.fade_delete)
    {
        object_mover_mark_for_fade(rmp, 0);
    }
    if (object_mover_is_fading_for_delete(rmp))
    {
        return;
    }
    vector pos;
    matrix orient;
    havok_body_get_pos_orient(rmp->havok_handle, pos, orient);
    vector cm_pos;
    if (!get_rfg_mover_center_of_mass_position(rmp, cm_pos))
    {
        return;
    }
    const float pos_error = vec_dist(cm_pos, state.position);
    const matrix delta = orient * state.orientation.copy_transpose();
    vector axis;
    float angle_error;
    delta.extract_axis_and_angle(axis, angle_error);
    if (object_mover_should_sleep(rmp, pos_error, angle_error))
    {
        return;
    }
    rfg_mover_soft_key_info soft_key_info;
    game_to_havok_vector(soft_key_info.cur_center_of_mass_position, cm_pos);
    hkRotationf rotation;
    game_to_havok_matrix(rotation, orient);
    soft_key_info.cur_orient.set(rotation);
    game_to_havok_vector(soft_key_info.target_center_of_mass_position, state.position);
    hkRotationf target_rotation;
    game_to_havok_matrix(target_rotation, state.orientation);
    soft_key_info.target_orient.set(target_rotation);
    vector world_pos;
    get_rfg_mover_world_position(rmp, state.position, state.orientation, world_pos);
    game_to_havok_vector(soft_key_info.target_pos_world, world_pos);
    hkpKeyFrameUtility::KeyFrameInfo key_frame_info;
    key_frame_info.setUsingPositionOrientationPair(soft_key_info.cur_center_of_mass_position, soft_key_info.cur_orient, soft_key_info.target_center_of_mass_position, soft_key_info.target_orient, 4.0f);
    // min(1 / frame time, 4); the 4 is first clamped at 0 (as compiled)
    const float inv_frametime = (float)(1.0 / (double)Frametime);
    float clamp = (float)(4.0 - 0.0) >= 0.0f ? 4.0f : 0.0f;
    const float factor = (float)((double)clamp - (double)inv_frametime) >= 0.0f ? inv_frametime : clamp;
    hkpKeyFrameUtility::AccelerationInfo accel_info;
    const __m128 factors = _mm_setr_ps(factor, factor, factor, 0.0f);
    accel_info.m_linearPositionFactor.m_quad = factors;
    accel_info.m_angularPositionFactor.m_quad = factors;
    accel_info.m_linearVelocityFactor.m_quad = factors;
    accel_info.m_angularVelocityFactor.m_quad = factors;
    accel_info.m_maxAllowedDistance = Rfg_mp_max_mover_correction_distance;
    havok_body_set_linear_velocity(rmp->havok_handle, state.linear_velocity);
    havok_body_set_angular_velocity(rmp->havok_handle, state.angular_velocity);
    soft_key_info.target_linear_velocity = key_frame_info.m_linearVelocity;
    soft_key_info.target_angular_velocity = key_frame_info.m_angularVelocity;
    havok_custom_soft_keyframe(soft_key_info, accel_info, Frametime, 0.0f, havok_body_get_pointer(rmp->havok_handle));
}

// 0x009F5EB0 (:1314)
// The mover's state: the center of mass position (the body's position, or the object's without a body, plus the
// rotated center of mass offset), the orientation, the velocities, the fixed and fading flags and the reference
// position index. Nothing (size 0) while the mover's resources are not loaded.
void rfg_mover_multi_registration::get_state(unsigned int game_handle, dc_state_list* dcsl) const
{
    dcsl->m_data = nullptr;
    dcsl->m_size = 0;
    rfg_mover* rmp = rfg_mover_from_handle(game_handle);
    if (rmp == nullptr || !rmp->rdd.is_loaded() || rmp->rdd.get_data()->shape == nullptr)
    {
        return;
    }
    const vector offset = rmp->rdd.get_data()->shape->center_of_mass.copy_unrotate(rmp->orient);
    const float object_x = rmp->pos.x;
    const float object_y = rmp->pos.y;
    const float object_z = rmp->pos.z;
    object_state_data<rfg_mover_data_bundle>* data = rfg_mover_state_data_alloc(); // inlined
    if (data == nullptr)
    {
        return;
    }
    data->initialize();
    dcsl->m_data = (unsigned char*)data;
    dcsl->m_size = sizeof(object_state_data<rfg_mover_data_bundle>);
    rfg_mover_data_bundle& base = data->base_data;
    base.movement_fixed = rmp->get_move_type() == RFGM_FIXED;
    base.fade_delete = object_mover_is_fading_for_delete(rmp);
    base.position_ref_index = rmp->reference_position_index;
    if (rmp->havok_handle == 0xFFFFFFFF)
    {
        base.position = rmp->pos;
        base.orientation = rmp->orient;
        base.linear_velocity.zero();
        base.angular_velocity.zero();
    }
    else
    {
        havok_body_get_pos_orient(rmp->havok_handle, base.position, base.orientation);
        havok_body_get_linear_velocity(rmp->havok_handle, base.linear_velocity);
        havok_body_get_angular_velocity(rmp->havok_handle, base.angular_velocity);
    }
    // Original: the offset is added to the object's position read before the body's (the same unless the body moved
    // since the object was synced).
    base.position.x = offset.x + object_x;
    base.position.y = object_y + offset.y;
    base.position.z = object_z + offset.z;
}

// 0x009B0F60 (:1364)
void rfg_mover_multi_registration::free_state(dc_state_list* dcsl) const
{
    Rfg_object_mover_allocator.free((object_state_data<rfg_mover_data_bundle>*)dcsl->m_data);
    dcsl->m_data = nullptr;
    dcsl->m_size = 0;
}

// 0x0097F7E0 (:1372): a fixed mover is never dirty
bool rfg_mover_multi_registration::check_object_is_dirty(dc_state_list* ref_state, dc_state_list* cur_state, const remote_object* rop) const
{
    if (ref_state == nullptr || cur_state == nullptr)
    {
        return true;
    }
    if (rfg_mover_from_handle(rop->m_game_handle) == nullptr)
    {
        return false;
    }
    const rfg_mover_data_bundle& cur = rfg_mover_base(cur_state);
    if (cur.movement_fixed)
    {
        return false;
    }
    return query_possible_mask_bits_base(cur, rfg_mover_base(ref_state)) != 0;
}

// 0x0095D150 (:1402)
// The 13 mask bits, then each masked data set compressed against the client's value (the reference state's transmitted
// record for this connection, or the current base state without one); the current state's record for the connection
// becomes what the client will have.
void rfg_mover_multi_registration::get_compressed_state(dc_state_list* current_state, dc_state_list* reference_state, dc_state_list* compressed_state, multi_connection* cp, multi_core_state_priority priority, unsigned short mask_bits) const
{
    if (mask_bits == 0)
    {
        compressed_state->m_size = 0;
        return;
    }
    rfg_mover_data_bundle& cur = rfg_mover_base(current_state);
    const rfg_mover_data_bundle* client_source = reference_state != nullptr ? &rfg_mover_transmitted(reference_state, cp) : &cur;
    rfg_mover_data_bundle& client = rfg_mover_transmitted(current_state, cp);
    memcpy(&client, client_source, sizeof(rfg_mover_data_bundle));
    bit_packer packer(compressed_state->m_data, 1200);
    const unsigned int mask = mask_bits;
    packer.set_next_bits(mask, 13);
    vector client_vec;
    if ((mask & 0xF) != 0)
    {
        const unsigned int pos_mask = mask & 0xF;
        if (pos_mask - 5 < 5)
        {
            vector reference_pos;
            object_mover_get_reference_pos(reference_pos, cur.position_ref_index);
            buffer_delta_compressed_vector(packer, pos_mask, client_vec, reference_pos, cur.position);
        }
        else
        {
            buffer_delta_compressed_vector(packer, pos_mask, client_vec, pos_mask != 0xF ? client.position : ZERO_VECTOR, cur.position);
        }
        client.transmitted_data_set |= 0xF;
        client.position = client_vec;
    }
    if ((mask & 0xC00) != 0)
    {
        matrix client_matrix;
        buffer_delta_compressed_pbh(packer, mask & 0xC00, client_matrix, (mask & 0xC00) != 0xC00 ? client.orientation : IDENTITY_MATRIX, cur.orientation);
        client.orientation = client_matrix;
        client.transmitted_data_set |= 0xC00;
    }
    if ((mask & 0x70) != 0)
    {
        const unsigned int vel_mask = mask & 0x70;
        buffer_delta_compressed_vector(packer, vel_mask, client_vec, (vel_mask == 0x70 || vel_mask == 0x60) ? ZERO_VECTOR : client.linear_velocity, cur.linear_velocity);
        client.transmitted_data_set |= 0x70;
        client.linear_velocity = client_vec;
    }
    if ((mask & 0x380) != 0)
    {
        const unsigned int vel_mask = mask & 0x380;
        buffer_delta_compressed_vector(packer, vel_mask, client_vec, (vel_mask == 0x380 || vel_mask == 0x300) ? ZERO_VECTOR : client.angular_velocity, cur.angular_velocity);
        client.transmitted_data_set |= 0x380;
        client.angular_velocity = client_vec;
    }
    if ((mask & 0x1000) != 0)
    {
        packer.add_bool(cur.fade_delete);
        client.fade_delete = cur.fade_delete;
        client.transmitted_data_set |= 0x1000;
    }
    // The bytes used, rounded up (float * 0.125, ceil in double, rounded to an integer).
    compressed_state->m_size = (unsigned long)(long long)(float)ceil((double)((float)(double)packer.get_start_bit() * 0.125f));
}

// 0x009FA4C0 (:1525)
// The received state is decompressed against the reference state (a copy of it, a 0x54-byte state), or against the
// client's own current state when there is none; a valid state goes to the mover's received states at its network time
// and the next correction is due at once. The new state (resultant_state) becomes a stored state for later references.
bool rfg_mover_multi_registration::apply_compressed_state(unsigned int game_handle, dc_state_list* reference_state, dc_state_list* compressed_state, dc_state_list** resultant_state, unsigned int network_time) const
{
    rfg_mover* rmp = rfg_mover_from_handle(game_handle);
    if (rmp == nullptr)
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
    object_state_data<rfg_mover_data_bundle>* data;
    const rfg_mover_data_bundle* reference = nullptr;
    if (reference_state == nullptr)
    {
        get_state(game_handle, dcsl);
        data = (object_state_data<rfg_mover_data_bundle>*)dcsl->m_data;
        if (data == nullptr)
        {
            return false;
        }
        data->base_data.network_time = 0;
    }
    else
    {
        reference = &rfg_mover_base(reference_state);
        data = rfg_mover_state_data_alloc(); // inlined
        if (data == nullptr)
        {
            multi_core_dc_free_state_list(dcsl);
            *resultant_state = nullptr;
            return false;
        }
        data->initialize();
        memcpy(&data->base_data, reference, sizeof(rfg_mover_data_bundle));
        dcsl->m_data = (unsigned char*)data;
        dcsl->m_size = sizeof(rfg_mover_data_bundle);
    }
    rfg_mover_data_bundle& state = data->base_data;
    state.network_time = network_time;
    bit_packer packer(compressed_state->m_data, compressed_state->m_size);
    const unsigned int mask = packer.get_next_bits(13);
    if ((mask & 0xF) != 0)
    {
        const unsigned int pos_mask = mask & 0xF;
        if (pos_mask - 5 < 5)
        {
            vector reference_pos;
            object_mover_get_reference_pos(reference_pos, state.position_ref_index);
            apply_delta_compressed_vector(packer, pos_mask, state.position, reference_pos);
        }
        else
        {
            // Original: with a reference state the base is the reference's position (no reference: the mask is 15).
            apply_delta_compressed_vector(packer, pos_mask, state.position, pos_mask == 0xF ? ZERO_VECTOR : reference->position);
        }
    }
    if ((mask & 0xC00) != 0)
    {
        apply_delta_compressed_pbh(packer, mask & 0xC00, state.orientation, (mask & 0xC00) == 0xC00 ? IDENTITY_MATRIX : reference->orientation);
    }
    if ((mask & 0x70) != 0)
    {
        const unsigned int vel_mask = mask & 0x70;
        apply_delta_compressed_vector(packer, vel_mask, state.linear_velocity, (vel_mask == 0x70 || vel_mask == 0x60) ? ZERO_VECTOR : reference->linear_velocity);
    }
    if ((mask & 0x380) != 0)
    {
        const unsigned int vel_mask = mask & 0x380;
        apply_delta_compressed_vector(packer, vel_mask, state.angular_velocity, (vel_mask == 0x380 || vel_mask == 0x300) ? ZERO_VECTOR : reference->angular_velocity);
    }
    if ((mask & 0x1000) != 0)
    {
        state.fade_delete = packer.get_bool();
    }
    if (check_if_pos_is_invalid(state.position, rmp, "multi_game_object_rfg_movers.cpp:1636"))
    {
        return false;
    }
    rmp->mp_info->sim_state->add_state(state);
    rmp->mp_info->sim_state->set_correction_timer(0);
    return true;
}

// 0x0095D4C0 (:1649): the bytes of each priority's mask: the bits of its data sets plus the 13 mask bits, rounded up
void rfg_mover_multi_registration::query_space_for_mask(query_size_results* qs_results, void* additional_data) const
{
    for (int i = 0; i < 5; i++)
    {
        const unsigned int mask = qs_results[i].mask;
        const int bits = get_pos_num_bits_from_mask_rfg_movers(mask) + get_orient_num_bits_from_mask_rfg_movers(mask) + get_linear_vel_num_bits_from_mask_rfg_movers(mask) + get_angular_vel_num_bits_from_mask_rfg_movers(mask) + get_state_num_bits_from_mask_rfg_movers(mask);
        qs_results[i].num_bytes = bits == 0 ? 0 : (unsigned short)((bits + 20) >> 3);
    }
}

// 0x009CC790 (:1682)
// The priority of each data set for this client: its base (low), plus half the updates it missed, plus the distance
// to the client's camera (closer than 3 m +2, 5 m +1, 10 m 0, 20 m -1, else -2), clamped to [abysmal, medium]; priority
// level i sends the data sets whose priority is at least i.
void rfg_mover_multi_registration::query_space_for_priority(const remote_object* rop, dc_state_list* current_state, dc_state_list* reference_state, unsigned short possible_mask_bits, multi_connection* cp, query_size_results* qs_results) const
{
    if (possible_mask_bits == 0)
    {
        return;
    }
    player* viewer = multi_game_data_get_player_instance(cp->game_data);
    state_priority_bundle* bundle = const_cast<state_priority_bundle*>(&rop->m_state_priority_bundle[cp->index_in_session]);
    float dist2 = 0.0f;
    if (viewer != nullptr)
    {
        const rfg_mover_data_bundle& cur = rfg_mover_base(current_state);
        const rfg_mover_data_bundle* ref = reference_state != nullptr ? &rfg_mover_base(reference_state) : nullptr;
        const vector camera_pos = object_is_local_player(viewer) ? Rfg_camera.real_pos : viewer->mp_camera_pos;
        dist2 = (camera_pos.x - cur.position.x) * (camera_pos.x - cur.position.x) + (camera_pos.y - cur.position.y) * (camera_pos.y - cur.position.y) + (camera_pos.z - cur.position.z) * (camera_pos.z - cur.position.z);
        if (ref != nullptr)
        {
            const float ref_dist2 = (camera_pos.x - ref->position.x) * (camera_pos.x - ref->position.x) + (camera_pos.y - ref->position.y) * (camera_pos.y - ref->position.y) + (camera_pos.z - ref->position.z) * (camera_pos.z - ref->position.z);
            if (dist2 - ref_dist2 >= 0.0f)
            {
                dist2 = ref_dist2;
            }
        }
    }
    int distance_bonus;
    if (dist2 > 400.0f) // Magic numbers: squared distances 20, 10, 5 and 3 m
    {
        distance_bonus = -2;
    }
    else if (dist2 > 100.0f)
    {
        distance_bonus = -1;
    }
    else if (dist2 > 25.0f)
    {
        distance_bonus = 0;
    }
    else if (dist2 > 9.0f)
    {
        distance_bonus = 1;
    }
    else
    {
        distance_bonus = 2;
    }
    for (int i = 0; i < 5; i++)
    {
        int priority = (int)(bundle->state_data[i].time_since_update >> 1) + State_priority_base_rfg_movers[i] + distance_bonus;
        if (priority < 0)
        {
            priority = MULTI_CORE_STATE_PRIORITY_ABYSMAL;
        }
        else if (priority > 2)
        {
            priority = MULTI_CORE_STATE_PRIORITY_MEDIUM;
        }
        bundle->state_data[i].priority = (multi_core_state_priority)priority;
    }
    for (int level = 0; level < 5; level++)
    {
        qs_results[level].mask = 0;
        for (int i = 0; i < 5; i++)
        {
            if (level <= (int)bundle->state_data[i].priority)
            {
                qs_results[level].mask |= (unsigned short)State_mask_rfg_movers[i];
            }
        }
    }
    for (int level = 0; level < 5; level++)
    {
        qs_results[level].mask &= possible_mask_bits;
    }
    // Original: the sizes are computed 5 times over.
    for (int i = 0; i < 5; i++)
    {
        query_space_for_mask(qs_results, nullptr);
    }
}

// 0x0095D720 (:1779): a data set that could have been sent but was not has missed an update; the client's record is a
// full update when everything possible was sent
void rfg_mover_multi_registration::notify_transmitted_updated(const remote_object* rop, dc_state_list* current_state, multi_connection* recipient, unsigned short possible_mask_bits, unsigned short mask_bits_transmitted) const
{
    state_priority_bundle* bundle = const_cast<state_priority_bundle*>(&rop->m_state_priority_bundle[recipient->index_in_session]);
    for (int i = 0; i < 5; i++)
    {
        if ((State_mask_rfg_movers[i] & possible_mask_bits) == 0 || (State_mask_rfg_movers[i] & mask_bits_transmitted) != 0)
        {
            bundle->state_data[i].time_since_update = 0;
        }
        else
        {
            bundle->state_data[i].time_since_update++;
        }
    }
    rfg_mover_transmitted(current_state, recipient).full_update = possible_mask_bits == 0 || possible_mask_bits == mask_bits_transmitted;
}

// 0x0095D7B0 (:1801)
bool rfg_mover_multi_registration::check_full_update(dc_state_list* ref_state, multi_connection* cp) const
{
    return rfg_mover_transmitted(ref_state, cp).full_update;
}

// 0x009CCAA0 (:1812): the state data and the error infos (the received states stay)
void rfg_mover_multi_registration::reset_state_data() const
{
    for (object_state_data<rfg_mover_data_bundle>* data = Rfg_object_mover_allocator.get_first(0); data != nullptr;)
    {
        data = Rfg_object_mover_allocator.free(data);
    }
    for (object_mover_error_info* error_info = Object_mover_error_info_allocator.get_first(0); error_info != nullptr;)
    {
        error_info = Object_mover_error_info_allocator.free(error_info);
    }
}

// 0x009B0F80 (:1818): every remote rfg_mover's received states and error info
void rfg_mover_multi_registration::free_mp_state_data() const
{
    remote_object* rop = Remote_objects_list;
    while (rop != nullptr)
    {
        if (rop->current_type == registration_type)
        {
            rfg_mover* rmp = rfg_mover_from_handle(rop->m_game_handle);
            if (rmp != nullptr)
            {
                free_one_mp_state_data(rmp); // inlined
            }
        }
        rop = rop->next;
        if (rop == Remote_objects_list)
        {
            break;
        }
    }
}

// 0x0096BCB0
rfg_mover_multi_registration::rfg_mover_multi_registration()
{
    num_masks = 0;
}

// 0x0096BD90
rfg_mover_multi_registration::~rfg_mover_multi_registration()
{
}

// 0x009E0F30 (:1909)
// With clear_allocators the pools are emptied (a level change). Otherwise the pools (3200 state datas, 128 received state
// sets, 256 error infos, 4000 mover infos) and the registration are made: type 7, created locally on level load. The
// masks: name (bits per vector), group, mask, bits, epsilon, pack threshold (the range), bias value, bias.
void multi_game_object_rfg_mover_init(bool clear_allocators)
{
    if (clear_allocators)
    {
        for (object_state_data<rfg_mover_data_bundle>* data = Rfg_object_mover_allocator.get_first(0); data != nullptr;)
        {
            data = Rfg_object_mover_allocator.free(data);
        }
        for (multi_sim_state<multi_mover_state>* sim_state = Rfg_mover_sim_state_allocator.get_first(0); sim_state != nullptr;)
        {
            sim_state = Rfg_mover_sim_state_allocator.free(sim_state);
        }
        for (object_mover_error_info* error_info = Object_mover_error_info_allocator.get_first(0); error_info != nullptr;)
        {
            error_info = Object_mover_error_info_allocator.free(error_info);
        }
        return;
    }
    Rfg_object_mover_allocator.init(MP_allocators_mempool);
    Rfg_mover_sim_state_allocator.init(MP_allocators_mempool);
    Object_mover_error_info_allocator.init(MP_allocators_mempool);
    Rfg_mover_info_allocator.init(MP_allocators_mempool);
    // Original: the allocation is not checked before the masks are written.
    rfg_mover_multi_registration* registration = new (MP_allocators_mempool.alloc(sizeof(rfg_mover_multi_registration), 0)) rfg_mover_multi_registration();
    registration->name = "rfg mover";
    registration->update_rate = 100;
    Rfg_mover_mr = registration;
    // Position: absolute and deltas (Z: from zero), then deltas from the reference position (R).
    registration->add_state_mask("16ZBD", "POS", 0xF, 48, 0.015625f, 512.0f, 0.0f, 1.0f);
    registration->add_state_mask("13BD", "POS", 0xE, 39, 0.015625f, 64.0f, 0.0f, 1.0f);
    registration->add_state_mask("10BD", "POS", 0xD, 30, 0.015625f, 8.0f, 0.0f, 1.0f);
    registration->add_state_mask("8BD", "POS", 0xC, 24, 0.015625f, 2.0f, 0.0f, 1.0f);
    registration->add_state_mask("5BD", "POS", 0xB, 15, 0.015625f, 0.25f, 0.0f, 1.0f);
    registration->add_state_mask("3BD", "POS", 0xA, 9, 0.015625f, 0.0625f, 0.0f, 1.0f);
    registration->add_state_mask("13BRD", "POS", 0x9, 39, 0.015625f, 64.0f, 0.0f, 1.0f);
    registration->add_state_mask("10BRD", "POS", 0x8, 30, 0.015625f, 8.0f, 0.0f, 1.0f);
    registration->add_state_mask("8BRD", "POS", 0x7, 24, 0.015625f, 2.0f, 0.0f, 1.0f);
    registration->add_state_mask("5BRD", "POS", 0x6, 15, 0.015625f, 0.25f, 0.0f, 1.0f);
    registration->add_state_mask("2BRD", "POS", 0x5, 6, 0.015625f, 0.03125f, 0.0f, 1.0f);
    // Orientation (pitch, bank, heading deltas): epsilon 0.008590292 (about 0.5 degrees), ranges 1.4 pi, 0.35 pi, 0.04375 pi.
    registration->add_state_mask("10BZD", "ORIENT", 0xC00, 30, 0.008590292f, 4.3982296f, 0.0f, 1.0f);
    registration->add_state_mask("8BD", "ORIENT", 0x800, 24, 0.008590292f, 1.0995574f, 0.0f, 1.0f);
    registration->add_state_mask("5BD", "ORIENT", 0x400, 15, 0.008590292f, 0.13744467f, 0.0f, 1.0f);
    registration->add_state_mask("16BZD", "L_VEL", 0x70, 48, 0.0390625f, 1280.0f, 0.0f, 1.0f);
    registration->add_state_mask("8BZD", "L_VEL", 0x60, 24, 0.0390625f, 5.0f, 0.0f, 1.0f);
    registration->add_state_mask("13BD", "L_VEL", 0x50, 39, 0.0390625f, 320.0f, 0.0f, 1.0f);
    registration->add_state_mask("10BD", "L_VEL", 0x40, 30, 0.0390625f, 20.0f, 0.0f, 1.0f);
    registration->add_state_mask("8BD", "L_VEL", 0x30, 24, 0.0390625f, 5.0f, 0.0f, 1.0f);
    registration->add_state_mask("5BD", "L_VEL", 0x20, 15, 0.0390625f, 0.625f, 0.0f, 1.0f);
    registration->add_state_mask("3BD", "L_VEL", 0x10, 9, 0.0390625f, 0.15625f, 0.0f, 1.0f);
    registration->add_state_mask("16BZD", "A_VEL", 0x380, 48, 0.0390625f, 1280.0f, 0.0f, 1.0f);
    registration->add_state_mask("8BZD", "A_VEL", 0x300, 24, 0.0390625f, 5.0f, 0.0f, 1.0f);
    registration->add_state_mask("13BD", "A_VEL", 0x280, 39, 0.0390625f, 320.0f, 0.0f, 1.0f);
    registration->add_state_mask("10BD", "A_VEL", 0x200, 30, 0.0390625f, 20.0f, 0.0f, 1.0f);
    registration->add_state_mask("8BD", "A_VEL", 0x180, 24, 0.0390625f, 5.0f, 0.0f, 1.0f);
    registration->add_state_mask("5BD", "A_VEL", 0x100, 15, 0.0390625f, 0.625f, 0.0f, 1.0f);
    registration->add_state_mask("3BD", "A_VEL", 0x80, 9, 0.0390625f, 0.15625f, 0.0f, 1.0f);
    registration->add_state_mask("STATE_DATA", "STATE_DATA", 0x1000, 1, 0.0f, 0.0f, 0.0f, 1.0f);
    multi_core_object_register_object_type(MULTI_OBJECT_TYPE_RFG_MOVER, registration);
    multi_core_objects_register_level_load_remote_object_local_create_types(MULTI_OBJECT_TYPE_RFG_MOVER);
}

// 0x0099D130 (:2211)
void multi_game_object_rfg_mover_created(rfg_mover* mover)
{
    mover->mp_info = Rfg_mover_info_allocator.alloc(0);
    if (mover->mp_info == nullptr)
    {
        keen::debug::nativePrintArgs("Cannot allocate MP info for an rfg_mover!");
        return;
    }
    memset(mover->mp_info, 0, sizeof(multi_rfg_mover_info));
    mover->mp_info->last_destruction_frame = 0xFFFF;
}

// 0x009E18B0 (:2224)
void multi_game_object_rfg_mover_destroyed(rfg_mover* mover)
{
    multi_core_object_process_object_free(mover); // inlined
    Rfg_mover_info_allocator.free(mover->mp_info);
    mover->mp_info = nullptr;
}

// 0x0095D7D0 (:2237)
// LTCG: rmp in EAX (the port has no register convention).
static bool rfg_mover_should_create_level_init_remote_object(const rfg_mover* rmp)
{
    return !rfg_mover_is_world_anchor(rmp);
}

// 0x009E1900 (:2243)
// Every level mover but the world anchors becomes a remote object under its reserved network handle; a client also
// gives it received states and an error info.
void create_level_rfg_mover_remote_objects()
{
    const int count = World.get_num_objects<rfg_mover>();
    for (int i = 0; i < count; i++)
    {
        rfg_mover* rmp = World.get_object<rfg_mover>(i);
        if (!rfg_mover_should_create_level_init_remote_object(rmp)) // inlined
        {
            continue;
        }
        const unsigned int handle = rmp->get_handle();
        if (handle != 0xFFFFFFFF)
        {
            const unsigned short net_handle = multi_core_network_handle_from_game_handle(handle, true); // inlined
            if (net_handle != 0xFFFF)
            {
                multi_core_create_remote_object_core_internal(handle, net_handle, MULTI_OBJECT_TYPE_RFG_MOVER);
            }
        }
        if (Multiplayer && !Server)
        {
            rmp->mp_info->sim_state = rfg_mover_allocate_sim_mover_state(); // inlined
            rmp->m_mp_error_info = object_mover_allocate_error_info(); // inlined
        }
    }
}

// 0x0095D7F0 (:2259)
void object_mover_get_sleep_tolerances(float& pos_sleep_tol, float& angle_sleep_tol)
{
    pos_sleep_tol = Object_mover_update_position_sleep_tolerance;
    angle_sleep_tol = Object_mover_update_orient_sleep_tolerance;
}
