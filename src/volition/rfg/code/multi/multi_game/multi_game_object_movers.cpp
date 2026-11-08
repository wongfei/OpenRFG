// Original: volition/rfg/code/multi/multi_game/multi_game_object_movers.cpp
//
// Decompiled whole (multiplayer.md MP5). The replication of general movers (props, barrels, doors...): the level's
// movers are remote objects on every peer under reserved network handles (except those with mover flag 0x4); the state
// data and masks are the rfg_movers' (multi_game_object_rfg_movers.cpp) without the center of mass offset; a client
// steers its mover towards the interpolated server state with Havok's soft keyframe. Damage and activation go to the
// server as packets, and the server keeps the list of destroyed movers (sent to a migrating host).
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_object_movers.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkrotationf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/dynamics/keyframe/hkpkeyframeutility.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_convert.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_compression.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_delta_compression.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_pool.inl"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_migration.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_rfg_movers.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/level_object_properties.h"
#include "volition/rfg/code/objects/movers/general_mover.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/util/bit_util.h"

#include <math.h>
#include <string.h>

// ---- file-static globals
// multi_core_state_priority State_priority_base_movers[5] (0x0167F090) is used here too; placed in multi_game_object_marker.cpp.
static unsigned int State_mask_movers[5] = {0xF, 0xC00, 0x70, 0x380, 0x1000}; // 0x0167F0A4
static float Mp_max_mover_correction_distance = 6.0f; // 0x0167F0B8
// dynamic_uint_hash_table<remote_object*> Game_handle_to_rop_hash (0x01691B40) is used here too; placed in multi_core_delta_compression.cpp.
// dynamic_uint_hash_table<remote_object*> Network_handle_to_rop_hash (0x01691B88) is used here too; placed in multi_core_delta_compression.cpp.
// dynamic_uint_hash_table<network_game_handle*> Reserved_game_to_network_hash (0x01691C18) is used here too; placed in multi_core_objects.cpp.
// dynamic_uint_hash_table<network_game_handle*> Reserved_network_to_game_hash (0x01691C60) is used here too; placed in multi_core_delta_compression.cpp.
static multi_pool<object_state_data<mover_data_bundle>, 3200> General_mover_allocator; // 0x01692E90
// multi_pool<multi_sim_state<multi_mover_state>,128> General_mover_sim_state_allocator (0x01692ED0) is used here too; placed in multi_game_destruction.cpp.
// multi_pool<object_mover_error_info,256> Object_mover_error_info_allocator (0x01693050) is used here too; placed in multi_game_destruction.cpp.
static multi_mover_registration* Mover_mr; // 0x02FE5BF0
static unsigned short Movers_destroyed_network_handle[100]; // 0x02FE87E0
// unsigned int Clock_current_time (0x02FEB538) is used here too; placed in multi.cpp.
// int Registered_level_load_remote_object_local_create_types[10] (0x02FEB53C) is used here too; placed in multi_core_objects.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
// int Num_registered_level_load_remote_object_local_create_types (0x02FEB604) is used here too; placed in multi_core_objects.cpp.
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.
// migration_state Migration_state (0x02FEB6C4) is used here too; placed in multi.cpp.
static int Movers_destroyed_count; // 0x02FEB6E0

// The general mover of a game handle (null for any other object).
static general_mover* general_mover_from_handle(unsigned int game_handle)
{
    if (game_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(game_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_MOVER || objp->get_sub_type() != OT_SUB_MOVER_GENERAL)
    {
        return nullptr;
    }
    return (general_mover*)objp;
}

static mover_data_bundle& mover_transmitted(dc_state_list* dcsl, const multi_connection* cp)
{
    return ((object_state_data<mover_data_bundle>*)dcsl->m_data)->transmitted_data[cp->index_in_session];
}

static mover_data_bundle& mover_base(dc_state_list* dcsl)
{
    return ((object_state_data<mover_data_bundle>*)dcsl->m_data)->base_data;
}

// Mover flag 0x4: a mover that is not replicated (no remote object, no destroyed-list entry; general_mover.cpp).
static bool general_mover_is_local_only(const general_mover* gmp)
{
    return (gmp->mover_flags & 0x4) != 0;
}

// ---- internal functions
static bool is_val_in_list(unsigned short val, unsigned short* list, int list_size); // 0x0095A2B0 (:320)
static bool is_pos_mask_ref_delta(unsigned int mask); // 0x0095A310 (:344)
static int get_pos_num_bits_from_mask(unsigned int mask); // 0x0095A320 (:360)
static int get_orient_num_bits_from_mask(unsigned int mask); // 0x0095A3A0 (:383)
static int get_linear_vel_num_bits_from_mask(unsigned int mask); // 0x0095A3E0 (:398)
static int get_angular_vel_num_bits_from_mask(unsigned int mask); // 0x0095A4C0 (:417)
static int get_state_num_bits_from_mask(unsigned int mask); // 0x0095A530 (:436)
static object_state_data<mover_data_bundle>* mover_state_data_alloc(); // 0x009F4A60 (:445)
static unsigned int get_position_no_ref_mask_bits(const mover_data_bundle& base_data); // 0x0095A540 (:488)
static unsigned int get_linear_velocity_no_ref_mask_bits(const mover_data_bundle& base_data); // 0x0095A680 (:512)
static unsigned int get_angular_velocity_no_ref_mask_bits(const mover_data_bundle& base_data); // 0x0095A6E0 (:526)
static unsigned int get_no_ref_mask_bits(const mover_data_bundle& base_data); // 0x0095A740 (:540)
static unsigned short query_possible_mask_bits_base(mover_data_bundle& cur_data, mover_data_bundle& ref_data); // 0x0095A800 (:562)
static bool should_create_remote_object(const general_mover* gmp); // 0x0095B010 (:1879)

// 0x0095A2B0 (:320)
// LTCG: list_size in ECX, list in EDX, val in SI (the port has no register convention).
static bool is_val_in_list(unsigned short val, unsigned short* list, int list_size)
{
    for (int i = 0; i < list_size; i++)
    {
        if (list[i] == val)
        {
            return true;
        }
    }
    return false;
}

// 0x0095A2D0 (:331): up to 100, each once
void add_destroyed_network_handle_to_mover_destroyed_list(unsigned short net_handle)
{
    for (int i = 0; i < Movers_destroyed_count; i++)
    {
        if (Movers_destroyed_network_handle[i] == net_handle)
        {
            return;
        }
    }
    if (Movers_destroyed_count < 100)
    {
        Movers_destroyed_network_handle[Movers_destroyed_count++] = net_handle;
    }
}

// 0x0095A310 (:344)
static bool is_pos_mask_ref_delta(unsigned int mask)
{
    return (mask & 0xF) - 5 < 5;
}

// 0x0095A320 (:360)
static int get_pos_num_bits_from_mask(unsigned int mask)
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

// 0x0095A3A0 (:383)
static int get_orient_num_bits_from_mask(unsigned int mask)
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

// 0x0095A3E0 (:398)
static int get_linear_vel_num_bits_from_mask(unsigned int mask)
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

// 0x0095A4C0 (:417)
static int get_angular_vel_num_bits_from_mask(unsigned int mask)
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

// 0x0095A530 (:436)
static int get_state_num_bits_from_mask(unsigned int mask)
{
    return (mask >> 12) & 1;
}

// 0x009F4A60 (:445)
static object_state_data<mover_data_bundle>* mover_state_data_alloc()
{
    object_state_data<mover_data_bundle>* data = General_mover_allocator.alloc(0);
    if (data == nullptr)
    {
        multi_core_dc_flush_old_object_states();
        data = General_mover_allocator.alloc(0);
    }
    return data;
}

// 0x0099C100 (:462)
multi_sim_state<multi_mover_state>* general_mover_alloc_sim_state()
{
    multi_sim_state<multi_mover_state>* sim_state = General_mover_sim_state_allocator.alloc(0);
    if (sim_state == nullptr)
    {
        keen::debug::nativePrintArgs("Out of general mover sim states!");
        return nullptr;
    }
    sim_state->reset();
    return sim_state;
}

// 0x009C7B40 (:474)
void general_mover_clean_up_mp_resources(general_mover* gmp)
{
    if (gmp->mp_sim_state != nullptr)
    {
        General_mover_sim_state_allocator.free(gmp->mp_sim_state);
        gmp->mp_sim_state = nullptr;
    }
    if (gmp->m_mp_error_info != nullptr)
    {
        Object_mover_error_info_allocator.free(gmp->m_mp_error_info);
        gmp->m_mp_error_info = nullptr;
    }
}

// 0x0095A540 (:488): as the rfg_movers'
// LTCG: base_data in EDI (the port has no register convention).
static unsigned int get_position_no_ref_mask_bits(const mover_data_bundle& base_data)
{
    vector reference_pos;
    object_mover_get_reference_pos(reference_pos, base_data.position_ref_index);
    const float threshold = Mover_mr->get_state_mask_pack_threshold(5);
    if ((float)fabs((double)reference_pos.x - (double)base_data.position.x) <= threshold && (float)fabs((double)reference_pos.y - (double)base_data.position.y) <= threshold && (float)fabs((double)reference_pos.z - (double)base_data.position.z) <= threshold)
    {
        return 5;
    }
    const unsigned int mask = Mover_mr->get_vector_mask(base_data.position, reference_pos, 5, 6, 7, 8, 9, 0xFFFFFFFF);
    return mask != 0 ? mask : 0xF;
}

// 0x0095A680 (:512)
static unsigned int get_linear_velocity_no_ref_mask_bits(const mover_data_bundle& base_data)
{
    const unsigned int mask = Mover_mr->get_vector_mask(base_data.linear_velocity, ZERO_VECTOR, 0x60, 0x70, 0xFFFFFFFF);
    return mask != 0 ? mask : 0x60;
}

// 0x0095A6E0 (:526)
static unsigned int get_angular_velocity_no_ref_mask_bits(const mover_data_bundle& base_data)
{
    const unsigned int mask = Mover_mr->get_vector_mask(base_data.angular_velocity, ZERO_VECTOR, 0x300, 0x380, 0xFFFFFFFF);
    return mask != 0 ? mask : 0x300;
}

// 0x0095A740 (:540)
// LTCG: base_data in EAX (the port has no register convention).
static unsigned int get_no_ref_mask_bits(const mover_data_bundle& base_data)
{
    unsigned int mask = get_position_no_ref_mask_bits(base_data);
    mask = (mask & 0xFFFFFF8F) | 0x1F80;
    mask |= get_linear_velocity_no_ref_mask_bits(base_data); // inlined
    mask &= 0xFFFFFC7F;
    return get_angular_velocity_no_ref_mask_bits(base_data) | mask; // inlined
}

// 0x0095A800 (:562)
// LTCG: cur_data in EDI, ref_data in ESI (the port has no register convention).
static unsigned short query_possible_mask_bits_base(mover_data_bundle& cur_data, mover_data_bundle& ref_data)
{
    const unsigned int pos_mask = Mover_mr->get_vector_mask(cur_data.position, ref_data.position, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0xFFFFFFFF);
    const unsigned int orient_mask = Mover_mr->get_orient_mask(cur_data.orientation, ref_data.orientation, 0xC00, 0x400, 0x800);
    unsigned short mask = (unsigned short)(orient_mask | pos_mask);
    mask |= (unsigned short)Mover_mr->get_vector_mask(cur_data.linear_velocity, ref_data.linear_velocity, 0x10, 0x20, 0x30, 0x40, 0x50, 0x70, 0xFFFFFFFF);
    mask |= (unsigned short)Mover_mr->get_vector_mask(cur_data.angular_velocity, ref_data.angular_velocity, 0x80, 0x100, 0x180, 0x200, 0x280, 0x380, 0xFFFFFFFF);
    if (cur_data.fade_delete != ref_data.fade_delete)
    {
        mask |= 0x1000;
    }
    return mask;
}

// 0x0095A900 (:604)
unsigned short multi_mover_registration::query_possible_mask_bits(dc_state_list* current_state, dc_state_list* reference_state, multi_connection* cp) const
{
    if (reference_state == nullptr)
    {
        return (unsigned short)get_no_ref_mask_bits(mover_base(current_state));
    }
    mover_data_bundle& client = mover_transmitted(reference_state, cp);
    const unsigned short never_sent = (unsigned short)(~client.transmitted_data_set & 0x1FFF);
    return query_possible_mask_bits_base(mover_base(current_state), client) | never_sent;
}

// 0x0097DC30 (:630)
void multi_mover_registration::create(unsigned int game_handle) const
{
    general_mover* gmp = general_mover_from_handle(game_handle);
    if (gmp != nullptr)
    {
        gmp->remote_obj_flags.remote_object = 1;
    }
}

static void mover_write_float(unsigned char* packet_data, unsigned short& packet_size, float value)
{
    const float swapped = PACKET_SWAP_FLOAT(value);
    memcpy(packet_data + packet_size, &swapped, 4);
    packet_size += 4;
}

static float mover_read_float(const unsigned char* packet_data, int& packet_offset)
{
    float value;
    memcpy(&value, packet_data + packet_offset, 4);
    packet_offset += 4;
    return PACKET_SWAP_FLOAT(value);
}

// 0x0097DC60 (:645): the position, the orientation quaternion and with a Havok body its velocities (network order)
void multi_mover_registration::send_create_packet(const remote_object* rop, unsigned char* packet_data, unsigned short& packet_size, multi_connection* dest_conn) const
{
    general_mover* gmp = general_mover_from_handle(rop->m_game_handle);
    if (gmp == nullptr)
    {
        return;
    }
    const quaternion q(gmp->orient);
    mover_write_float(packet_data, packet_size, gmp->pos.x);
    mover_write_float(packet_data, packet_size, gmp->pos.y);
    mover_write_float(packet_data, packet_size, gmp->pos.z);
    mover_write_float(packet_data, packet_size, q.x);
    mover_write_float(packet_data, packet_size, q.y);
    mover_write_float(packet_data, packet_size, q.z);
    mover_write_float(packet_data, packet_size, q.w);
    if (gmp->havok_handle != 0xFFFFFFFF)
    {
        vector velocity;
        havok_body_get_linear_velocity(gmp->havok_handle, velocity);
        mover_write_float(packet_data, packet_size, velocity.x);
        mover_write_float(packet_data, packet_size, velocity.y);
        mover_write_float(packet_data, packet_size, velocity.z);
        havok_body_get_angular_velocity(gmp->havok_handle, velocity);
        mover_write_float(packet_data, packet_size, velocity.x);
        mover_write_float(packet_data, packet_size, velocity.y);
        mover_write_float(packet_data, packet_size, velocity.z);
    }
}

// 0x009C7B90 (:677)
// The client's own mover (by network handle) gets its received states and error info; a resting mover takes the sent
// linear velocity. The position is skipped and the orientation is read but not used.
void multi_mover_registration::process_create_packet(unsigned int& game_handle, unsigned short net_handle, int& packet_offset, unsigned char* packet_data) const
{
    packet_offset += 12;
    quaternion q;
    const unsigned char* p = packet_data + packet_offset;
    const unsigned int q_bits[4] = {
        ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8) | p[3],
        ((unsigned int)p[4] << 24) | ((unsigned int)p[5] << 16) | ((unsigned int)p[6] << 8) | p[7],
        ((unsigned int)p[8] << 24) | ((unsigned int)p[9] << 16) | ((unsigned int)p[10] << 8) | p[11],
        ((unsigned int)p[12] << 24) | ((unsigned int)p[13] << 16) | ((unsigned int)p[14] << 8) | p[15],
    };
    memcpy(&q.x, &q_bits[0], 4);
    memcpy(&q.y, &q_bits[1], 4);
    memcpy(&q.z, &q_bits[2], 4);
    memcpy(&q.w, &q_bits[3], 4);
    packet_offset += 16;
    matrix orient;
    q.extract_matrix(orient);
    game_handle = multi_core_game_handle_from_network_handle(net_handle, false);
    general_mover* gmp = general_mover_from_handle(game_handle);
    if (gmp == nullptr)
    {
        return;
    }
    gmp->mp_sim_state = general_mover_alloc_sim_state(); // inlined
    gmp->m_mp_error_info = object_mover_allocate_error_info(); // inlined
    vector velocity;
    if (gmp->havok_handle != 0xFFFFFFFF)
    {
        havok_body_get_linear_velocity(gmp->havok_handle, velocity);
        if (velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z < 0.1f) // Magic number: at rest
        {
            velocity.x = mover_read_float(packet_data, packet_offset);
            velocity.y = mover_read_float(packet_data, packet_offset);
            velocity.z = mover_read_float(packet_data, packet_offset);
            havok_body_set_linear_velocity(gmp->havok_handle, velocity);
            velocity.x = mover_read_float(packet_data, packet_offset);
            velocity.y = mover_read_float(packet_data, packet_offset);
            velocity.z = mover_read_float(packet_data, packet_offset);
            // Original: the angular velocity is read back from the body instead of set.
            havok_body_get_angular_velocity(gmp->havok_handle, velocity);
        }
    }
    gmp->remote_obj_flags.remote_object = 1;
    game_handle = gmp->get_handle();
}

// 0x009C8010 (:723): nothing is sent; the mover goes to the destroyed list
void multi_mover_registration::send_destroy_packet(unsigned int game_handle, unsigned char* packet_data, unsigned short& packet_size) const
{
    general_mover* gmp = general_mover_from_handle(game_handle);
    if (gmp != nullptr && !general_mover_is_local_only(gmp))
    {
        add_destroyed_network_handle_to_mover_destroyed_list(multi_core_network_handle_from_game_handle(gmp->get_handle(), false));
    }
}

// 0x009C8060 (:741): with its death effect
void multi_mover_registration::process_destroy_packet(unsigned int game_handle, int& packet_offset, unsigned char* packet_data) const
{
    general_mover* gmp = general_mover_from_handle(game_handle);
    if (gmp == nullptr)
    {
        return;
    }
    gmp->remote_obj_flags.client_destroy_permission = 1;
    const unsigned int death_effect = gmp->m_level_obj_properties->m_base_properties->m_death_effect;
    if (death_effect != 0xFFFFFFFF)
    {
        effects_play(death_effect, gmp->pos, gmp->orient, false, gmp->get_handle(), -1, 0xFFFFFFFF, 1.0f, false, (object_effect_category)0);
    }
    if (!general_mover_is_local_only(gmp))
    {
        add_destroyed_network_handle_to_mover_destroyed_list(multi_core_network_handle_from_game_handle(gmp->get_handle(), false));
    }
    World.destroy_object(gmp, false);
}

// 0x009F4A90 (:770)
void multi_mover_registration::process_object_free(object* obj) const
{
    if (obj == nullptr || obj->get_obj_type() != OT_MOVER || obj->get_sub_type() != OT_SUB_MOVER_GENERAL)
    {
        return;
    }
    general_mover_clean_up_mp_resources((general_mover*)obj); // inlined
    multi_game_process_remote_object_free_common(obj);
}

// 0x0095A960 (:791)
void multi_mover_registration::post_update_cleanup() const
{
}

// 0x0099C140 (:797)
// A client's dynamic mover follows the server state at the current network time (at most every 100 ms): unless it may
// sleep, its velocities are set to the state's and Havok's soft keyframe (factors min(1 / frame time, 4), correction
// distance 6 m) steers it towards the state's pose.
void multi_mover_registration::move(unsigned int game_handle) const
{
    general_mover* gmp = general_mover_from_handle(game_handle);
    if (gmp == nullptr || gmp->mp_sim_state == nullptr || gmp->havok_handle == 0xFFFFFFFF)
    {
        return;
    }
    const int motion_type = havok_body_get_motion_type(gmp->havok_handle);
    if (motion_type == 5 || motion_type == 4) // Magic numbers: hkpMotion::MOTION_KEYFRAMED (4) and MOTION_FIXED (5)
    {
        return;
    }
    multi_sim_state<multi_mover_state>* sim_state = gmp->mp_sim_state;
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
        object_mover_mark_for_fade(gmp, 0);
    }
    if (object_mover_is_fading_for_delete(gmp))
    {
        return;
    }
    vector pos;
    matrix orient;
    havok_body_get_pos_orient(gmp->havok_handle, pos, orient);
    const float pos_error = vec_dist(pos, state.position);
    const matrix delta = orient * state.orientation.copy_transpose();
    vector axis;
    float angle_error;
    delta.extract_axis_and_angle(axis, angle_error);
    if (object_mover_should_sleep(gmp, pos_error, angle_error))
    {
        return;
    }
    hkVector4f cur_pos;
    game_to_havok_vector(cur_pos, pos);
    hkRotationf rotation;
    game_to_havok_matrix(rotation, orient);
    hkQuaternionf cur_orient;
    cur_orient.set(rotation);
    hkVector4f target_pos;
    game_to_havok_vector(target_pos, state.position);
    hkRotationf target_rotation;
    game_to_havok_matrix(target_rotation, state.orientation);
    hkQuaternionf target_orient;
    target_orient.set(target_rotation);
    const float inv_frametime = 1.0f / Frametime;
    hkpKeyFrameUtility::KeyFrameInfo key_frame_info;
    key_frame_info.setUsingPositionOrientationPair(cur_pos, cur_orient, target_pos, target_orient, inv_frametime);
    const float factor = 4.0f - inv_frametime < 0.0f ? 4.0f : inv_frametime; // min(1 / frame time, 4)
    hkpKeyFrameUtility::AccelerationInfo accel_info;
    const __m128 factors = _mm_setr_ps(factor, factor, factor, 0.0f);
    accel_info.m_linearPositionFactor.m_quad = factors;
    accel_info.m_angularPositionFactor.m_quad = factors;
    accel_info.m_linearVelocityFactor.m_quad = factors;
    accel_info.m_angularVelocityFactor.m_quad = factors;
    accel_info.m_maxAllowedDistance = Mp_max_mover_correction_distance;
    havok_body_set_linear_velocity(gmp->havok_handle, state.linear_velocity);
    havok_body_set_angular_velocity(gmp->havok_handle, state.angular_velocity);
    hkpKeyFrameUtility::applySoftKeyFrame(key_frame_info, accel_info, Frametime, inv_frametime, havok_body_get_pointer(gmp->havok_handle));
}

// 0x009F4AF0 (:903)
// The mover's state (no center of mass offset, no fixed flag, reference index left as the pool had it).
// Original: the state is allocated before the object is checked, and a missing mover is dereferenced.
void multi_mover_registration::get_state(unsigned int game_handle, dc_state_list* dcsl) const
{
    dcsl->m_data = nullptr;
    dcsl->m_size = 0;
    object_state_data<mover_data_bundle>* data = mover_state_data_alloc(); // inlined
    if (data == nullptr)
    {
        return;
    }
    data->initialize();
    dcsl->m_data = (unsigned char*)data;
    dcsl->m_size = sizeof(object_state_data<mover_data_bundle>);
    general_mover* gmp = general_mover_from_handle(game_handle);
    mover_data_bundle& base = data->base_data;
    base.fade_delete = object_mover_is_fading_for_delete(gmp);
    if (gmp->havok_handle != 0xFFFFFFFF)
    {
        havok_body_get_pos_orient(gmp->havok_handle, base.position, base.orientation);
        havok_body_get_linear_velocity(gmp->havok_handle, base.linear_velocity);
        havok_body_get_angular_velocity(gmp->havok_handle, base.angular_velocity);
        return;
    }
    base.position = gmp->pos;
    base.orientation = gmp->orient;
    base.linear_velocity.zero();
    base.angular_velocity.zero();
}

// 0x009B0C60 (:942)
void multi_mover_registration::free_state(dc_state_list* dcsl) const
{
    General_mover_allocator.free((object_state_data<mover_data_bundle>*)dcsl->m_data);
    dcsl->m_data = nullptr;
    dcsl->m_size = 0;
}

// 0x0097E090 (:950)
bool multi_mover_registration::check_object_is_dirty(dc_state_list* ref_state, dc_state_list* cur_state, const remote_object* rop) const
{
    if (ref_state == nullptr || cur_state == nullptr)
    {
        return true;
    }
    if (general_mover_from_handle(rop->m_game_handle) == nullptr)
    {
        return false;
    }
    return query_possible_mask_bits_base(mover_base(cur_state), mover_base(ref_state)) != 0;
}

// 0x0095A980 (:974): as the rfg_movers'
void multi_mover_registration::get_compressed_state(dc_state_list* current_state, dc_state_list* reference_state, dc_state_list* compressed_state, multi_connection* cp, multi_core_state_priority priority, unsigned short mask_bits) const
{
    if (mask_bits == 0)
    {
        compressed_state->m_size = 0;
        return;
    }
    mover_data_bundle& cur = mover_base(current_state);
    const mover_data_bundle* client_source = reference_state != nullptr ? &mover_transmitted(reference_state, cp) : &cur;
    mover_data_bundle& client = mover_transmitted(current_state, cp);
    memcpy(&client, client_source, sizeof(mover_data_bundle));
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
    compressed_state->m_size = (unsigned long)(long long)(float)ceil((double)((float)(double)packer.get_start_bit() * 0.125f));
}

// 0x009F8DE0 (:1098): as the rfg_movers' (a missing reference reads as zero / the identity; no position check)
bool multi_mover_registration::apply_compressed_state(unsigned int game_handle, dc_state_list* reference_state, dc_state_list* compressed_state, dc_state_list** resultant_state, unsigned int network_time) const
{
    general_mover* gmp = general_mover_from_handle(game_handle);
    if (gmp == nullptr)
    {
        return false;
    }
    dc_state_list* dcsl = multi_core_dc_alloc_state_list(); // inlined
    *resultant_state = dcsl;
    if (dcsl == nullptr)
    {
        return false;
    }
    dcsl->m_reference_ackid = 0;
    dcsl->m_data = nullptr;
    dcsl->m_size = 0;
    object_state_data<mover_data_bundle>* data;
    const mover_data_bundle* reference = nullptr;
    if (reference_state == nullptr)
    {
        get_state(game_handle, dcsl);
        data = (object_state_data<mover_data_bundle>*)dcsl->m_data;
        if (data == nullptr)
        {
            return false;
        }
        data->base_data.network_time = 0;
    }
    else
    {
        reference = &mover_base(reference_state);
        data = mover_state_data_alloc(); // inlined
        if (data == nullptr)
        {
            multi_core_dc_free_state_list(dcsl);
            *resultant_state = nullptr;
            return false;
        }
        data->initialize();
        memcpy(&data->base_data, reference, sizeof(mover_data_bundle));
        dcsl->m_data = (unsigned char*)data;
        dcsl->m_size = sizeof(mover_data_bundle);
    }
    mover_data_bundle& state = data->base_data;
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
            apply_delta_compressed_vector(packer, pos_mask, state.position, (pos_mask == 0xF || reference == nullptr) ? ZERO_VECTOR : reference->position);
        }
    }
    if ((mask & 0xC00) != 0)
    {
        apply_delta_compressed_pbh(packer, mask & 0xC00, state.orientation, ((mask & 0xC00) == 0xC00 || reference == nullptr) ? IDENTITY_MATRIX : reference->orientation);
    }
    if ((mask & 0x70) != 0)
    {
        const unsigned int vel_mask = mask & 0x70;
        apply_delta_compressed_vector(packer, vel_mask, state.linear_velocity, (vel_mask == 0x70 || vel_mask == 0x60 || reference == nullptr) ? ZERO_VECTOR : reference->linear_velocity);
    }
    if ((mask & 0x380) != 0)
    {
        const unsigned int vel_mask = mask & 0x380;
        apply_delta_compressed_vector(packer, vel_mask, state.angular_velocity, (vel_mask == 0x380 || vel_mask == 0x300 || reference == nullptr) ? ZERO_VECTOR : reference->angular_velocity);
    }
    if ((mask & 0x1000) != 0)
    {
        state.fade_delete = packer.get_bool();
    }
    gmp->mp_sim_state->add_state(state);
    gmp->mp_sim_state->set_correction_timer(0);
    return true;
}

// 0x0095ACF0 (:1219)
void multi_mover_registration::query_space_for_mask(query_size_results* qs_results, void* additional_data) const
{
    for (int i = 0; i < 5; i++)
    {
        const unsigned int mask = qs_results[i].mask;
        const int bits = get_pos_num_bits_from_mask(mask) + get_orient_num_bits_from_mask(mask) + get_linear_vel_num_bits_from_mask(mask) + get_angular_vel_num_bits_from_mask(mask) + get_state_num_bits_from_mask(mask);
        qs_results[i].num_bytes = bits == 0 ? 0 : (unsigned short)((bits + 20) >> 3);
    }
}

// 0x009C8100 (:1251): as the rfg_movers' (State_priority_base_movers)
void multi_mover_registration::query_space_for_priority(const remote_object* rop, dc_state_list* current_state, dc_state_list* reference_state, unsigned short possible_mask_bits, multi_connection* cp, query_size_results* qs_results) const
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
        const mover_data_bundle& cur = mover_base(current_state);
        const mover_data_bundle* ref = reference_state != nullptr ? &mover_base(reference_state) : nullptr;
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
        int priority = (int)(bundle->state_data[i].time_since_update >> 1) + State_priority_base_movers[i] + distance_bonus;
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
                qs_results[level].mask |= (unsigned short)State_mask_movers[i];
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

// 0x0095AF50 (:1345)
void multi_mover_registration::notify_transmitted_updated(const remote_object* rop, dc_state_list* current_state, multi_connection* recipient, unsigned short possible_mask_bits, unsigned short mask_bits_transmitted) const
{
    state_priority_bundle* bundle = const_cast<state_priority_bundle*>(&rop->m_state_priority_bundle[recipient->index_in_session]);
    for (int i = 0; i < 5; i++)
    {
        if ((State_mask_movers[i] & possible_mask_bits) == 0 || (State_mask_movers[i] & mask_bits_transmitted) != 0)
        {
            bundle->state_data[i].time_since_update = 0;
        }
        else
        {
            bundle->state_data[i].time_since_update++;
        }
    }
    mover_transmitted(current_state, recipient).full_update = possible_mask_bits == 0 || possible_mask_bits == mask_bits_transmitted;
}

// 0x0095AFE0 (:1368)
bool multi_mover_registration::check_full_update(dc_state_list* ref_state, multi_connection* cp) const
{
    return mover_transmitted(ref_state, cp).full_update;
}

// 0x009C8410 (:1379)
void multi_mover_registration::reset_state_data() const
{
    for (object_state_data<mover_data_bundle>* data = General_mover_allocator.get_first(0); data != nullptr;)
    {
        data = General_mover_allocator.free(data);
    }
}

// 0x009C8460 (:1384)
void multi_mover_registration::free_mp_state_data() const
{
    remote_object* rop = Remote_objects_list;
    while (rop != nullptr)
    {
        if (rop->current_type == registration_type)
        {
            general_mover* gmp = general_mover_from_handle(rop->m_game_handle);
            if (gmp != nullptr)
            {
                general_mover_clean_up_mp_resources(gmp); // inlined
            }
        }
        rop = rop->next;
        if (rop == Remote_objects_list)
        {
            break;
        }
    }
}

// 0x0096BC60
multi_mover_registration::multi_mover_registration()
{
    num_masks = 0;
}

// 0x0096BD60
multi_mover_registration::~multi_mover_registration()
{
}

// 0x009DFB40 (:1412)
// With clear_allocators the pools are emptied (a level change). Otherwise the pools (3200 state datas, 128 received state
// sets, 256 error infos) and the registration ("mover", type 6, created locally on level load; the rfg_movers' masks).
void multi_game_object_mover_init(bool clear_allocators)
{
    if (clear_allocators)
    {
        for (object_state_data<mover_data_bundle>* data = General_mover_allocator.get_first(0); data != nullptr;)
        {
            data = General_mover_allocator.free(data);
        }
        for (multi_sim_state<multi_mover_state>* sim_state = General_mover_sim_state_allocator.get_first(0); sim_state != nullptr;)
        {
            sim_state = General_mover_sim_state_allocator.free(sim_state);
        }
        return;
    }
    General_mover_allocator.init(MP_allocators_mempool);
    General_mover_sim_state_allocator.init(MP_allocators_mempool);
    Object_mover_error_info_allocator.init(MP_allocators_mempool);
    // Original: the allocation is not checked before the masks are written.
    multi_mover_registration* registration = new (MP_allocators_mempool.alloc(sizeof(multi_mover_registration), 0)) multi_mover_registration();
    registration->name = "mover";
    registration->update_rate = 100;
    Mover_mr = registration;
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
    multi_core_object_register_object_type(MULTI_OBJECT_TYPE_GEN_MOVER, registration);
    multi_core_objects_register_level_load_remote_object_local_create_types(MULTI_OBJECT_TYPE_GEN_MOVER);
}

// 0x0095B000 (:1697)
void multi_game_general_mover_reset_destroyed_count()
{
    Movers_destroyed_count = 0;
}

// 0x009C84F0 (:1704)
// PT_MOVER_APPLY_DAMAGE to everyone: the network handle, the damage (16 bits, big endian) and the damage type.
void multi_game_object_mover_apply_damage(unsigned int game_handle, int damage_hitpoints, damaged_by_types from_type)
{
    if (general_mover_from_handle(game_handle) == nullptr)
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_MOVER_APPLY_DAMAGE;
    const unsigned int header = Multi_packets[PT_MOVER_APPLY_DAMAGE].group_id != 0 ? 5 : 3;
    const unsigned short net_handle = multi_core_network_handle_from_game_handle(game_handle, false);
    packet[header] = (unsigned char)(net_handle >> 8);
    packet[header + 1] = (unsigned char)net_handle;
    packet[header + 2] = (unsigned char)((unsigned int)damage_hitpoints >> 8);
    packet[header + 3] = (unsigned char)damage_hitpoints;
    packet[header + 4] = (unsigned char)from_type;
    const unsigned short payload = (unsigned short)(header + 2);
    packet[1] = (unsigned char)(payload >> 8);
    packet[2] = (unsigned char)payload;
    multi_io_send_to_all_reliable(multi_session_get_by_type(ST_GAME), packet, header + 5);
}

// The game handle of a network handle (multi_core_game_handle_from_network_handle inlined).
static unsigned int mover_game_handle_from_packet(const unsigned char* packet_data)
{
    return multi_core_game_handle_from_network_handle((unsigned short)((packet_data[0] << 8) | packet_data[1]), true);
}

// 0x009C85F0 (:1738)
// Original: the damage is applied with hit position and normal vectors that are never set (uninitialized stack).
void multi_game_object_mover_process_apply_damage(unsigned char* packet_data, const sender& from)
{
    const unsigned int game_handle = mover_game_handle_from_packet(packet_data);
    const unsigned short damage = (unsigned short)((packet_data[2] << 8) | packet_data[3]);
    const damaged_by_types from_type = (damaged_by_types)packet_data[4];
    general_mover* gmp = general_mover_from_handle(game_handle);
    if (gmp == nullptr || general_mover_is_local_only(gmp))
    {
        return;
    }
    vector hit_pos;
    vector hit_normal;
    gmp->apply_damage((float)damage, from_type, hit_pos, hit_normal, nullptr, 0xFFFFFFFF, true);
}

// 0x009C8720 (:1765): the server knocks the mover loose (its dislodge hit points plus one, as a car)
void multi_game_object_mover_process_activated(unsigned char* packet_data, const sender& from)
{
    const unsigned short net_handle = (unsigned short)((packet_data[0] << 8) | packet_data[1]);
    if (net_handle == 0xFFFF)
    {
        return;
    }
    remote_object* rop = nullptr;
    network_game_handle* reserved = Reserved_network_to_game_hash.get(net_handle);
    if (reserved != nullptr)
    {
        // (the reserved entry is read as a remote object here: its game handle is at the same offset)
        rop = (remote_object*)reserved;
    }
    else
    {
        rop = Network_handle_to_rop_hash.get(net_handle);
    }
    if (rop == nullptr)
    {
        return;
    }
    general_mover* gmp = general_mover_from_handle(rop->m_game_handle);
    if (gmp == nullptr || gmp->m_level_obj_properties->m_dislodge_hitpoints < 0)
    {
        return;
    }
    vector hit_pos;
    vector hit_normal;
    gmp->apply_damage((float)(gmp->m_level_obj_properties->m_dislodge_hitpoints + 1), DBT_CAR, hit_pos, hit_normal, nullptr, 0xFFFFFFFF, true);
}

// 0x009C8820 (:1787): a client tells its host that it knocked a mover loose (PT_MOVER_ACTIVATED)
void multi_game_object_mover_send_activated(const general_mover* gmp)
{
    if (!Multiplayer || Server)
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_MOVER_ACTIVATED;
    const unsigned int header = Multi_packets[PT_MOVER_ACTIVATED].group_id != 0 ? 5 : 3;
    const unsigned short net_handle = multi_core_network_handle_from_game_handle(gmp->get_handle(), false);
    packet[header] = (unsigned char)(net_handle >> 8);
    packet[header + 1] = (unsigned char)net_handle;
    const unsigned short payload = (unsigned short)(header - 1);
    packet[1] = (unsigned char)(payload >> 8);
    packet[2] = (unsigned char)payload;
    multi_io_send_reliable(multi_session_game_host_connection(), packet, header + 2, nullptr, nullptr, nullptr);
}

// 0x009C88E0 (:1812): PT_DESTROYED_MOVERS_LIST: the count and the network handles (16 bits each)
unsigned int multi_game_object_mover_send_destroyed_movers_list(multi_connection* cp)
{
    unsigned char packet[1200];
    packet[0] = PT_DESTROYED_MOVERS_LIST;
    unsigned int size = Multi_packets[PT_DESTROYED_MOVERS_LIST].group_id != 0 ? 5 : 3;
    packet[size] = (unsigned char)((unsigned int)Movers_destroyed_count >> 8);
    packet[size + 1] = (unsigned char)Movers_destroyed_count;
    size += 2;
    for (int i = 0; i < Movers_destroyed_count; i++)
    {
        packet[size & 0xFFFF] = (unsigned char)(Movers_destroyed_network_handle[i] >> 8);
        packet[(size & 0xFFFF) + 1] = (unsigned char)Movers_destroyed_network_handle[i];
        size += 2;
    }
    const unsigned short payload = (unsigned short)(size - 3);
    packet[1] = (unsigned char)(payload >> 8);
    packet[2] = (unsigned char)payload;
    return multi_io_send_reliable(cp, packet, size & 0xFFFF, nullptr, nullptr, nullptr);
}

// 0x009C89B0 (:1835)
// Every listed mover is destroyed here without a destroy packet; a server waiting for a migrating client's list notes
// that it arrived.
void multi_game_object_process_destroyed_movers_list(unsigned char* packet_data, const sender& from)
{
    unsigned int count = (unsigned int)((packet_data[0] << 8) | packet_data[1]);
    if (Multiplayer && Server)
    {
        if (Migration_state != MIGRATION_SERVER_WAIT_DESTRUCTION_STATUS || from.connection == nullptr)
        {
            return;
        }
        const unsigned int handle = from.connection->game_data;
        if (handle == 0xFFFFFFFF)
        {
            return;
        }
        Mp_game_data->data[handle & 0xFFFF][handle >> 16].dm_data.destroyed_general_movers_recvd = true;
    }
    const unsigned char* entry = packet_data + 2;
    for (; count != 0; count--, entry += 2)
    {
        const unsigned short net_handle = (unsigned short)((entry[0] << 8) | entry[1]);
        add_destroyed_network_handle_to_mover_destroyed_list(net_handle);
        general_mover* gmp = World.object_from_handle<general_mover>(multi_core_game_handle_from_network_handle(net_handle, true));
        if (gmp != nullptr)
        {
            gmp->remote_obj_flags.client_destroy_permission = 1;
            gmp->remote_obj_flags.no_destroy_packet = 1;
            World.destroy_object(gmp, false);
        }
    }
}

// 0x0095B010 (:1879)
// LTCG: gmp in EAX (the port has no register convention).
static bool should_create_remote_object(const general_mover* gmp)
{
    return !general_mover_is_local_only(gmp);
}

// 0x009E0440 (:1889): every replicated level mover becomes a remote object under its reserved network handle; a client
// also gives it received states and an error info
void create_level_general_mover_remote_objects()
{
    const int count = World.get_num_objects<general_mover>();
    for (int i = 0; i < count; i++)
    {
        general_mover* gmp = World.get_object<general_mover>(i);
        if (!should_create_remote_object(gmp)) // inlined
        {
            continue;
        }
        const unsigned int handle = gmp->get_handle();
        if (handle != 0xFFFFFFFF)
        {
            const unsigned short net_handle = multi_core_network_handle_from_game_handle(handle, true); // inlined
            if (net_handle != 0xFFFF)
            {
                multi_core_create_remote_object_core_internal(handle, net_handle, MULTI_OBJECT_TYPE_GEN_MOVER);
            }
        }
        if (Multiplayer && !Server)
        {
            gmp->mp_sim_state = general_mover_alloc_sim_state(); // inlined
            gmp->m_mp_error_info = object_mover_allocate_error_info(); // inlined
        }
    }
}

// 0x009C8AA0 (:1905): a new host destroys the movers its list says are gone
void multi_game_migration_destroy_general_mover_destroyed_list()
{
    for (int i = 0; i < Movers_destroyed_count; i++)
    {
        const unsigned short net_handle = Movers_destroyed_network_handle[i];
        if (net_handle == 0xFFFF)
        {
            continue;
        }
        general_mover* gmp = general_mover_from_handle(multi_core_game_handle_from_network_handle(net_handle, true));
        if (gmp != nullptr)
        {
            gmp->remote_obj_flags.client_destroy_permission = 1;
            gmp->remote_obj_flags.no_destroy_packet = 1;
            World.destroy_object(gmp, false);
        }
    }
}
